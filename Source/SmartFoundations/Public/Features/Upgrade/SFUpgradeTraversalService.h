// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SFUpgradeAuditService.h"
#include "SFUpgradeTraversalService.generated.h"

// Forward declarations
class AFGBuildable;
class AFGBuildableConveyorBase;
class AFGBuildablePipeline;
class AFGBuildablePowerPole;
class UFGFactoryConnectionComponent;
class UFGPipeConnectionComponent;
class UFGPowerConnectionComponent;

/**
 * Configuration for network traversal behavior
 */
USTRUCT(BlueprintType)
struct FSFTraversalConfig
{
	GENERATED_BODY()

	/** Cross splitters/mergers when traversing belts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrossSplitters = true;

	/** Cross storage containers when traversing belts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrossStorage = false;

	/** Cross train cargo platforms when traversing belts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrossTrainPlatforms = false;

	/** Cross floor holes/passthroughs when traversing lifts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrossFloorHoles = true;

	/** Cross pumps when traversing pipes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCrossPumps = true;

	/** Maximum number of buildables to traverse (safety limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxTraversalCount = 10000;
};

/**
 * Result of a network traversal scan
 */
USTRUCT(BlueprintType)
struct FSFTraversalResult
{
	GENERATED_BODY()

	/** The anchor buildable that started the traversal */
	UPROPERTY()
	TWeakObjectPtr<AFGBuildable> AnchorBuildable;

	/** Family of the anchor buildable */
	UPROPERTY()
	ESFUpgradeFamily Family = ESFUpgradeFamily::None;

	/** All buildables found in the network */
	UPROPERTY()
	TArray<FSFUpgradeAuditEntry> Entries;

	/** Count by tier */
	UPROPERTY()
	TMap<int32, int32> CountByTier;

	/** Total count of buildables in network */
	UPROPERTY()
	int32 TotalCount = 0;

	/** Count of upgradeable buildables */
	UPROPERTY()
	int32 UpgradeableCount = 0;

	/** Whether traversal hit the max limit */
	UPROPERTY()
	bool bHitMaxLimit = false;

	/** Error message if traversal failed */
	UPROPERTY()
	FString ErrorMessage;

	/** Whether traversal was successful */
	bool IsValid() const { return Family != ESFUpgradeFamily::None && ErrorMessage.IsEmpty(); }
};

/**
 * Service for traversing connected buildable networks
 * Used by the Upgrade Panel to find all buildables in a connected system
 */
UCLASS()
class SMARTFOUNDATIONS_API USFUpgradeTraversalService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Traverse a network starting from an anchor buildable
	 * @param AnchorBuildable The starting point for traversal
	 * @param Config Traversal configuration options
	 * @param PlayerController Player controller for unlock checks
	 * @return Traversal result containing all found buildables
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	FSFTraversalResult TraverseNetwork(
		AFGBuildable* AnchorBuildable,
		const FSFTraversalConfig& Config,
		class AFGPlayerController* PlayerController);

	/**
	 * Get the upgrade family for a buildable
	 * @param Buildable The buildable to classify
	 * @return The upgrade family, or None if not upgradeable
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	static ESFUpgradeFamily GetUpgradeFamily(AFGBuildable* Buildable);

	/**
	 * Get the current tier of a buildable
	 * @param Buildable The buildable to check
	 * @return The tier (1-based), or 0 if unknown
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	static int32 GetBuildableTier(AFGBuildable* Buildable);

private:
	/** Traverse conveyor belt/lift network */
	void TraverseConveyorNetwork(
		AFGBuildableConveyorBase* StartConveyor,
		const FSFTraversalConfig& Config,
		TSet<AFGBuildable*>& VisitedSet,
		TArray<AFGBuildable*>& OutBuildables);

	/** Traverse pipeline network */
	void TraversePipelineNetwork(
		AFGBuildablePipeline* StartPipeline,
		const FSFTraversalConfig& Config,
		TSet<AFGBuildable*>& VisitedSet,
		TArray<AFGBuildable*>& OutBuildables);

	/** Traverse power pole network */
	void TraversePowerNetwork(
		AFGBuildablePowerPole* StartPole,
		const FSFTraversalConfig& Config,
		TSet<AFGBuildable*>& VisitedSet,
		TArray<AFGBuildable*>& OutBuildables);

	/** Get connected buildable from a factory connection */
	AFGBuildable* GetConnectedBuildable(UFGFactoryConnectionComponent* Connection);

	/** Get connected buildable from a pipe connection */
	AFGBuildable* GetConnectedBuildable(UFGPipeConnectionComponent* Connection);

	/** Check if a buildable should be crossed during traversal */
	bool ShouldCrossBuildable(AFGBuildable* Buildable, const FSFTraversalConfig& Config);

	/** Get all factory connections on a buildable */
	TArray<UFGFactoryConnectionComponent*> GetFactoryConnections(AFGBuildable* Buildable);

	/** Get all pipe connections on a buildable */
	TArray<UFGPipeConnectionComponent*> GetPipeConnections(AFGBuildable* Buildable);
};
