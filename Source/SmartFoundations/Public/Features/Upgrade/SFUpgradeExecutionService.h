#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SFUpgradeAuditService.h"
#include "SFUpgradeExecutionService.generated.h"

class USFSubsystem;
class AFGBuildable;
class AFGPlayerController;
class AFGCharacterPlayer;

/**
 * Parameters for upgrade execution
 */
USTRUCT(BlueprintType)
struct FSFUpgradeExecutionParams
{
	GENERATED_BODY()

	/** Family to upgrade */
	UPROPERTY()
	ESFUpgradeFamily Family = ESFUpgradeFamily::None;

	/** Source tier to upgrade FROM */
	UPROPERTY()
	int32 SourceTier = 0;

	/** Target tier to upgrade TO */
	UPROPERTY()
	int32 TargetTier = 0;

	/** Origin point for radius-based selection */
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	/** Radius in cm (0 = save-wide) */
	UPROPERTY()
	float Radius = 0.0f;

	/** Max items to upgrade (0 = unlimited) */
	UPROPERTY()
	int32 MaxItems = 0;

	/** Player controller for inventory access (required for cost deduction) */
	UPROPERTY()
	AFGPlayerController* PlayerController = nullptr;

	/** Specific buildables to upgrade (if provided, ignores Origin/Radius and upgrades these directly) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AFGBuildable>> SpecificBuildables;

	/** Whether to use SpecificBuildables instead of radius scan */
	bool HasSpecificBuildables() const { return SpecificBuildables.Num() > 0; }
};

/**
 * Result of upgrade execution
 */
USTRUCT(BlueprintType)
struct FSFUpgradeExecutionResult
{
	GENERATED_BODY()

	/** Number of items successfully upgraded */
	UPROPERTY()
	int32 SuccessCount = 0;

	/** Number of items that failed to upgrade */
	UPROPERTY()
	int32 FailCount = 0;

	/** Number of items skipped (already target tier, etc.) */
	UPROPERTY()
	int32 SkipCount = 0;

	/** Total items processed */
	UPROPERTY()
	int32 TotalProcessed = 0;

	/** Whether execution completed (vs cancelled) */
	UPROPERTY()
	bool bCompleted = false;

	/** Error message if any */
	UPROPERTY()
	FString ErrorMessage;

	/** Number of connections validated against the pre-upgrade manifest */
	UPROPERTY()
	int32 ValidatedConnectionCount = 0;

	/** Number of connections that were missing after upgrade and successfully repaired */
	UPROPERTY()
	int32 RepairedConnectionCount = 0;

	/** Number of connections that were missing after upgrade and could not be repaired */
	UPROPERTY()
	int32 BrokenConnectionCount = 0;
};

/** Delegate for progress updates during execution */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUpgradeProgressUpdated, float, ProgressPercent, int32, SuccessCount, int32, TotalCount);

/** Delegate for execution completion */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradeExecutionCompleted, const FSFUpgradeExecutionResult&, Result);

/**
 * Service for executing batch upgrades using sequential vanilla upgrade path
 */
UCLASS()
class SMARTFOUNDATIONS_API USFUpgradeExecutionService : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize the service */
	void Initialize(USFSubsystem* InSubsystem);

	/** Cleanup */
	void Cleanup();

	/** Tick for batch processing */
	void Tick(float DeltaTime);

	/** Start upgrade execution */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void StartUpgrade(const FSFUpgradeExecutionParams& Params);

	/** Cancel in-progress upgrade */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void CancelUpgrade();

	/** Check if upgrade is in progress */
	UFUNCTION(BlueprintPure, Category = "SmartFoundations|Upgrade")
	bool IsUpgradeInProgress() const { return bUpgradeInProgress; }

	/** Get the last execution result */
	UFUNCTION(BlueprintPure, Category = "SmartFoundations|Upgrade")
	const FSFUpgradeExecutionResult& GetLastResult() const { return LastResult; }

	/** Progress update delegate */
	UPROPERTY(BlueprintAssignable, Category = "SmartFoundations|Upgrade")
	FOnUpgradeProgressUpdated OnUpgradeProgressUpdated;

	/** Completion delegate */
	UPROPERTY(BlueprintAssignable, Category = "SmartFoundations|Upgrade")
	FOnUpgradeExecutionCompleted OnUpgradeExecutionCompleted;

	/** Get target recipe for upgrading a buildable to a specific tier */
	TSubclassOf<class UFGRecipe> GetUpgradeRecipe(ESFUpgradeFamily Family, int32 TargetTier) const;

	/** Get the buildable class for a family/tier combination */
	TSubclassOf<AFGBuildable> GetBuildableClass(ESFUpgradeFamily Family, int32 Tier) const;

private:
	/** Process a single upgrade in the batch
	 * @return 1 = success, 0 = failed, -1 = out of funds (abort remaining)
	 */
	int32 ProcessSingleUpgrade(AFGBuildable* Buildable, TSubclassOf<class UFGRecipe> TargetRecipe);

	/** Gather buildables to upgrade based on params */
	void GatherUpgradeTargets();

	/** Expand belt/lift targets to safe connected conveyor cohorts. */
	void NormalizeConveyorUpgradeTargets(bool bRespectRadius);

	/** Collect connected belts/lifts that belong to one conveyor cohort. */
	void CollectConnectedConveyorCohort(class AFGBuildableConveyorBase* StartConveyor, TSet<class AFGBuildableConveyorBase*>& OutCohort) const;

	/** Whether a conveyor intersects the current radius selection. */
	bool ConveyorIntersectsRadius(class AFGBuildableConveyorBase* Conveyor) const;

	/** Whether a conveyor is fully inside the current radius selection. */
	bool ConveyorFullyInsideRadius(class AFGBuildableConveyorBase* Conveyor) const;

	/** Resolve the correct target recipe for a specific buildable in a mixed batch. */
	TSubclassOf<class UFGRecipe> GetTargetRecipeForBuildable(AFGBuildable* Buildable, TSubclassOf<class UFGRecipe> FallbackRecipe) const;

	/** Calculate total upgrade cost for all pending items
	 * @param OutNetCost - Output map of item class to net amount needed
	 * @return true if calculation succeeded
	 */
	bool CalculateTotalUpgradeCost(TMap<TSubclassOf<class UFGItemDescriptor>, int32>& OutNetCost) const;

	/** Check if player can afford the upgrade costs
	 * @param NetCost - Map of item class to net amount needed
	 * @return true if player has enough materials
	 */
	bool CanAffordUpgrade(const TMap<TSubclassOf<class UFGItemDescriptor>, int32>& NetCost) const;

	/** Deduct upgrade costs from player inventory
	 * @param NetCost - Map of item class to net amount to deduct
	 * @return true if deduction succeeded
	 */
	bool DeductUpgradeCosts(const TMap<TSubclassOf<class UFGItemDescriptor>, int32>& NetCost);

	/** Complete the upgrade batch */
	void CompleteUpgrade();

	/** Reference to subsystem */
	UPROPERTY()
	USFSubsystem* Subsystem = nullptr;

	/** Current execution parameters */
	FSFUpgradeExecutionParams CurrentParams;

	/** Working result */
	FSFUpgradeExecutionResult WorkingResult;

	/** Last completed result */
	FSFUpgradeExecutionResult LastResult;

	/** Whether upgrade is in progress */
	bool bUpgradeInProgress = false;

	/** Buildables queued for upgrade */
	UPROPERTY()
	TArray<AFGBuildable*> PendingUpgrades;

	/** Current index in pending upgrades */
	int32 CurrentUpgradeIndex = 0;

	/** Target recipe for current batch */
	UPROPERTY()
	TSubclassOf<class UFGRecipe> CurrentTargetRecipe;

	/** Items to process per timer callback */
	static constexpr int32 ITEMS_PER_TICK = 1;

	/** Timer handle for deferred processing */
	FTimerHandle UpgradeTimerHandle;

	/** Timer callback for processing upgrades outside parallel tick */
	void ProcessUpgradeTimer();

	/** Conveyors that were upgraded (for chain rebuild) */
	UPROPERTY()
	TArray<class AFGBuildableConveyorBase*> UpgradedConveyors;

	/** All conveyors in the original chain (cached at first upgrade) */
	UPROPERTY()
	TArray<class AFGBuildableConveyorBase*> CachedChainConveyors;

	/** Whether we've cached the chain conveyors */
	bool bChainCached = false;

	/** Accumulated costs during upgrade loop (positive = cost, negative = refund) */
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> AccumulatedCosts;

	/** Overflow items that couldn't fit in inventory (for crate spawning) */
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> OverflowItems;
	
	/** Map of old conveyor -> new conveyor for fixing inter-connected upgrades */
	TMap<class AFGBuildableConveyorBase*, class AFGBuildableConveyorBase*> OldToNewConveyorMap;

	/** General map of old buildable -> new buildable (all families). Used by Option B validation and pipe batch repair. */
	TMap<class AFGBuildable*, class AFGBuildable*> OldToNewBuildableMap;
	
	/** Chain actors captured from old belts BEFORE destruction (ensures stale chains are removed even if new belts haven't joined a chain yet) */
	TSet<class AFGConveyorChainActor*> PreDestroyChainActors;
	
	/** Saved connection pairs: [OldConveyor, ConnectionIndex] -> [PartnerOldConveyor, PartnerConnectionIndex] */
	TMap<TPair<class AFGBuildableConveyorBase*, int32>, TPair<class AFGBuildableConveyorBase*, int32>> SavedConnectionPairs;

	/** Saved pipe connection pairs (same shape as conveyor pairs, for pipe-to-pipe batch upgrades) */
	TMap<TPair<class AFGBuildablePipeline*, int32>, TPair<class AFGBuildablePipeline*, int32>> SavedPipeConnectionPairs;

	/** Kind of connector in an expected-connection edge */
	enum class EConnectionEdgeKind : uint8 { Factory, Pipe, Power };

	/** A single pre-upgrade connection edge captured before destruction */
	struct FConnectionEdge
	{
		FName LocalConnectorName;          // Component FName on the buildable being upgraded
		class AFGBuildable* PartnerOldPtr = nullptr; // Raw pointer identity (may be dangling after destruction; do NOT deref directly)
		FName PartnerConnectorName;        // Component FName on the partner
		EConnectionEdgeKind Kind = EConnectionEdgeKind::Factory;
	};

	/** Expected connections captured BEFORE destruction, keyed by old buildable pointer (pointer used as identity only) */
	TMap<class AFGBuildable*, TArray<FConnectionEdge>> ExpectedConnectionEdges;
	
	/** Save inter-connected pairs before any upgrades (Option A: belts + pipes) */
	void SaveBatchConnectionPairs();

	/** Save inter-connected pipe pairs before any upgrades (Option A) */
	void SaveBatchPipeConnectionPairs();
	
	/** Fix connections between conveyors that were both upgraded in the same batch */
	void FixBatchConnectionReferences();

	/** Fix connections between pipes that were both upgraded in the same batch */
	void FixBatchPipeConnectionReferences();

	/** Option B: capture every connector + partner identity on each pending buildable BEFORE destruction */
	void CaptureExpectedConnectionManifests();

	/** Option B: after the batch, verify expected edges exist and repair missing ones. Updates WorkingResult counts. */
	void ValidateAndRepairConnections();
};
