// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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
	TObjectPtr<AFGPlayerController> PlayerController = nullptr;

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

	/** [#376] Shared static conveyor-radius helpers - PUBLIC so the audit service can run the IDENTICAL
	 *  cohort grouping + fully-inside test the radius upgrade uses. Keeping them in one place is what
	 *  prevents the audit's "upgradeable" count from drifting away from what the execution will do. */
	static void CollectConnectedConveyorCohort(class AFGBuildableConveyorBase* StartConveyor, TSet<class AFGBuildableConveyorBase*>& OutCohort);
	static bool IsConveyorFullyInsideRadius(class AFGBuildableConveyorBase* Conveyor, const FVector& Origin, float RadiusSq);

	/** Start upgrade execution */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void StartUpgrade(const FSFUpgradeExecutionParams& Params);

	/** Cancel in-progress upgrade */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void CancelUpgrade();

	/** [UPGRADE-MP] Client-side: install a server-executed result and broadcast completion, so
	 *  panel delegates bound to this (local) service fire exactly as in SP. Mirrors
	 *  USFUpgradeAuditService::InjectAuditResult. */
	void InjectUpgradeResult(const FSFUpgradeExecutionResult& Result);

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

	/** [#485] Exact, geometry-derived settlement for upgrading one buildable to TargetRecipe.
	 *  Charge = target recipe ingredients x the buildable's own vanilla dismantle-refund
	 *  multiplier (the per-length cost factor for belts/pipes/lifts; 1 for poles/outlets).
	 *  Refund = the buildable's actual GetDismantleRefundReturns (already length-multiplied,
	 *  build cost only - contents are a separate vanilla API and are never refunded here).
	 *  The SINGLE pricing source for both the panel estimate and execution settlement, so the
	 *  displayed cost can never drift from what execution charges. Must be called while the
	 *  old buildable is intact (before PreUpgrade/Dismantle). */
	bool ComputeUpgradeSettlement(AFGBuildable* Old, TSubclassOf<class UFGRecipe> TargetRecipe,
		TMap<TSubclassOf<class UFGItemDescriptor>, int32>& OutCharge,
		TMap<TSubclassOf<class UFGItemDescriptor>, int32>& OutRefund) const;

private:
	/** [#485] One target's inventory settlement, split for transactional ordering:
	 *  net-positive items are reserved (deducted) BEFORE Construct and rolled back if it fails;
	 *  net-negative items (refunds) are granted only AFTER the replacement is built and the old
	 *  target retired. Prevents the charge/refund leak where a failed construct kept the refund. */
	struct FSFUpgradeSettlementLedger
	{
		TMap<TSubclassOf<class UFGItemDescriptor>, int32> NetCharge;
		TMap<TSubclassOf<class UFGItemDescriptor>, int32> NetRefund;
		bool bFreeBuild = true;
	};

	/** Price one target and check net affordability against inventory + Dimensional Depot.
	 *  @return 1 = ready (or free build), 0 = pricing failed, -1 = cannot afford (abort batch) */
	int32 PrepareUpgradeSettlement(AFGBuildable* Old, TSubclassOf<class UFGRecipe> TargetRecipe,
		class AFGCharacterPlayer* PlayerChar, FSFUpgradeSettlementLedger& OutLedger) const;

	/** Deduct the ledger's net charge (inventory-vs-depot order follows the player preference). */
	void ReserveUpgradeCharge(const FSFUpgradeSettlementLedger& Ledger, class AFGCharacterPlayer* PlayerChar);

	/** Return a reserved charge after a failed construct (overflow goes to the crate). */
	void RollbackUpgradeCharge(const FSFUpgradeSettlementLedger& Ledger, class AFGCharacterPlayer* PlayerChar);

	/** Grant the ledger's net refund after a successful upgrade (overflow goes to the crate). */
	void GrantUpgradeRefund(const FSFUpgradeSettlementLedger& Ledger, class AFGCharacterPlayer* PlayerChar);

	/** Process a single upgrade in the batch
	 * @return 1 = success, 0 = failed, -1 = out of funds (abort remaining)
	 */
	int32 ProcessSingleUpgrade(AFGBuildable* Buildable, TSubclassOf<class UFGRecipe> TargetRecipe);

	/** Gather buildables to upgrade based on params */
	void GatherUpgradeTargets();

	/** Expand belt/lift targets to safe connected conveyor cohorts. */
	void NormalizeConveyorUpgradeTargets(bool bRespectRadius);

	/** Whether a conveyor intersects the current radius selection. */
	bool ConveyorIntersectsRadius(class AFGBuildableConveyorBase* Conveyor) const;

	/** Whether a conveyor is fully inside the current radius selection (delegates to the static helper). */
	bool ConveyorFullyInsideRadius(class AFGBuildableConveyorBase* Conveyor) const;

	/** Resolve the correct target recipe for a specific buildable in a mixed batch. */
	TSubclassOf<class UFGRecipe> GetTargetRecipeForBuildable(AFGBuildable* Buildable, TSubclassOf<class UFGRecipe> FallbackRecipe) const;

	/** Complete the upgrade batch */
	void CompleteUpgrade();

	/** Reference to subsystem */
	UPROPERTY()
	TObjectPtr<USFSubsystem> Subsystem = nullptr;

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
	TArray<TObjectPtr<AFGBuildable>> PendingUpgrades;

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
	TArray<TObjectPtr<class AFGBuildableConveyorBase>> UpgradedConveyors;

	/** All conveyors in the original chain (cached at first upgrade) */
	UPROPERTY()
	TArray<TObjectPtr<class AFGBuildableConveyorBase>> CachedChainConveyors;

	/** Whether we've cached the chain conveyors */
	bool bChainCached = false;

	/** Accumulated costs during upgrade loop (positive = cost, negative = refund) */
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> AccumulatedCosts;

	/** Overflow items that couldn't fit in inventory (for crate spawning) */
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> OverflowItems;

	/** [#485 temp] Batch material ledger for the accounting capture (Log level — Shipping strips
	 *  Verbose). Charged = items actually deducted; Refunded = items actually credited (incl.
	 *  overflow). Remove with the rest of the #485 diagnostics once the planner ships. */
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> BatchChargedTotals;
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> BatchRefundedTotals;
	
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
