// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FGRecipe.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGBuildGun.h"
#include "InputActionValue.h"
#include "SFRecipeManagementService.generated.h"

/**
 * Metadata for tracking buildings placed with Smart!
 * Used for recipe application after placement.
 */
USTRUCT()
struct FSFBuildingMetadata
{
	GENERATED_BODY()

	/** Unique ID for this placement group (incremented each placement) */
	int32 PlacementGroupID = 0;

	/** Index within the placement group (0 = parent, 1+ = children) */
	int32 IndexInGroup = 0;

	/** Whether this is the parent building in the group */
	bool bIsParent = false;

	/** Recipe that should be applied to this building (if any) */
	UPROPERTY()
	TSubclassOf<UFGRecipe> AppliedRecipe = nullptr;

	/** Timestamp when building was created */
	FDateTime CreationTime;
};

/**
 * Recipe source tracking for priority system
 * Determines which recipe takes precedence when multiple sources exist
 */
UENUM()
enum class ESFRecipeSource : uint8
{
	None,              // No recipe set
	Copied,            // From middle-click sampling (Ctrl+C)
	ManuallySelected   // From U + Num8/5 selector (highest priority)
};

/**
 * Service responsible for managing factory crafting recipes in Smart!
 * 
 * Handles two key workflows:
 * 1. Recipe Selection: User selects a recipe via U key + Num8/5 or middle-click sampling
 * 2. Recipe Application: Selected recipe is applied to all buildings in a placement group
 * 
 * This is DISTINCT from build gun recipes (hologram types), which determine
 * WHAT building is placed. This service manages WHAT that building produces.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFRecipeManagementService : public UObject
{
	GENERATED_BODY()

public:
	// ========================================
	// Initialization & Lifecycle
	// ========================================

	/** Initialize service with owning subsystem reference */
	void Initialize(class USFSubsystem* InSubsystem);

	/** Cleanup service resources */
	void Cleanup();

	// ========================================
	// Recipe Mode (U Key)
	// ========================================

	/** Activate recipe selection mode (U key pressed) */
	void ActivateRecipeMode();

	/** Deactivate recipe selection mode (U key released) */
	void DeactivateRecipeMode();

	/** Check if recipe mode is currently active */
	bool IsRecipeModeActive() const { return bRecipeModeActive; }

	// ========================================
	// Recipe Selection & Cycling
	// ========================================

	/** Cycle to next recipe in filtered list */
	void CycleRecipeForward(int32 AccumulatedSteps = 1);

	/** Cycle to previous recipe in filtered list */
	void CycleRecipeBackward(int32 AccumulatedSteps = 1);

	/** Set active recipe by index in filtered list */
	void SetActiveRecipeByIndex(int32 Index);

	/** Set active recipe by class reference — finds the correct index in SortedFilteredRecipes */
	bool SetActiveRecipeByClass(TSubclassOf<UFGRecipe> RecipeClass);

	/** Store a production recipe directly when the filtered hologram list is not settled yet. */
	bool StoreProductionRecipeClass(TSubclassOf<UFGRecipe> RecipeClass, ESFRecipeSource Source = ESFRecipeSource::Copied);

	/** Add a recipe to the unlocked recipes list */
	void AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe);

	/** Get filtered recipes compatible with current hologram */
	TArray<TSubclassOf<UFGRecipe>> GetFilteredRecipesForCurrentHologram();

	/** Clear all recipe state */
	void ClearAllRecipes();

	// ========================================
	// Recipe Sampling (Middle-Click)
	// ========================================

	/** Store production recipe from source building during middle mouse sampling */
	void StoreProductionRecipeFromBuilding(AFGBuildable* SourceBuilding);

	/** Called when build gun samples a recipe (middle-mouse, Ctrl+C, or Copy Settings button) */
	void OnBuildGunRecipeSampled(TSubclassOf<UFGRecipe> SampledRecipe);

	/** Clear stored production recipe (clears everything: recipe + shards + somersloops) */
	void ClearStoredProductionRecipe();

	/** Clear only stored shard/somersloop state (does NOT clear recipe) */
	void ClearStoredShardState();

	/** Notify that a new build session started (new parent hologram registered).
	 * Increments the session ID so stale shard state from previous sessions won't apply.
	 * @param NewBuildClass The build class of the new hologram — used to detect build type changes */
	void OnNewBuildSession(UClass* NewBuildClass);

	// ========================================
	// Recipe Application (On Placement)
	// ========================================

	/** Register a Smart-created building in the session registry */
	void RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent);

	/** Apply recipes to all buildings in current placement group */
	void ApplyRecipesToCurrentPlacement();

	/** Handle actor spawn to apply stored recipes from child holograms */
	void OnActorSpawned(AActor* SpawnedActor);

	/** Apply active recipe to parent hologram data registry */
	void ApplyRecipeToParentHologram();

	/** Handle recipe mode toggle (U key) */
	void OnRecipeModeChanged(const FInputActionValue& Value);

	/** Apply stored recipe to target building */
	void ApplyStoredProductionRecipeToBuilding(AFGBuildable* TargetBuilding);

	/** Apply recipe after delay (timer callback) */
	UFUNCTION()
	void ApplyRecipeDelayed(AFGBuildableManufacturer* ManufacturerBuilding, TSubclassOf<UFGRecipe> Recipe);

	/** Clear blueprint proxy flag (timer callback) */
	UFUNCTION()
	void ClearBlueprintProxyFlag();

	/** Clear current placement tracking */
	void ClearCurrentPlacement();

	// ========================================
	// State Queries
	// ========================================

	/** Get current active recipe */
	TSubclassOf<UFGRecipe> GetActiveRecipe() const { return ActiveRecipe; }

	/** Check if a blueprint proxy was recently spawned (for blocking Smart! features during vanilla blueprint placement) */
	bool IsBlueprintProxyRecentlySpawned() const { return bBlueprintProxyRecentlySpawned; }

	/** Get active recipe source */
	ESFRecipeSource GetActiveRecipeSource() const { return ActiveRecipeSource; }

	/** Check if a recipe is compatible with a hologram */
	bool IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass);

	/** Check if a recipe is compatible with a specific building instance */
	bool IsRecipeCompatibleWithBuilding(TSubclassOf<UFGRecipe> Recipe, AFGBuildable* Building) const;

	/** Check if building is a production building that supports recipes */
	bool IsProductionBuilding(AFGBuildable* Building) const;

	/** Get whether we have a valid stored production recipe */
	bool HasStoredProductionRecipe() const { return bHasStoredProductionRecipe; }

	/** Get stored production recipe (legacy compatibility) */
	TSubclassOf<UFGRecipe> GetStoredProductionRecipe() const { return StoredProductionRecipe; }

	// ========================================
	// Display Helpers
	// ========================================

	/** Get clean display name for recipe (removes Recipe_ prefix and _C suffix) */
	FString GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get the primary product's icon texture for a recipe */
	UTexture2D* GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get recipe display name with first ingredient shown */
	FString GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get recipe display name with inputs and outputs as indented lines */
	FString GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get compact recipe label for ComboBox display: "ProductName (Input1, Input2...)" */
	FString GetRecipeComboBoxLabel(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get the sorted filtered recipes array for UI population */
	const TArray<TSubclassOf<UFGRecipe>>& GetSortedFilteredRecipes() const { return SortedFilteredRecipes; }

	/** Get current active recipe display name */
	FString GetActiveRecipeDisplayName() const;

	/** Get current recipe index and total count for HUD display */
	void GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const;

private:
	// ========================================
	// Internal Helpers
	// ========================================

	/** Find recipe for spawned building using fuzzy matching (class + spatial proximity) */
	TSubclassOf<UFGRecipe> FindRecipeForSpawnedBuilding(AFGBuildableManufacturer* SpawnedBuilding);

	/** Mirror recipe service state into legacy subsystem fields used by older placement paths. */
	void SyncSubsystemRecipeState() const;

	
	// ========================================
	// State - Recipe Selection
	// ========================================

	/** Whether recipe mode is currently active (U key held) */
	bool bRecipeModeActive = false;

	/** Unified active recipe (single source of truth) */
	UPROPERTY(Transient)
	TSubclassOf<UFGRecipe> ActiveRecipe = nullptr;

	/** Source of the current active recipe */
	ESFRecipeSource ActiveRecipeSource = ESFRecipeSource::None;

	/** Array of all discovered/unlocked recipes */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UFGRecipe>> UnlockedRecipes;

	/** Cached sorted recipes for current hologram type */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UFGRecipe>> SortedFilteredRecipes;

	/** Current index in the filtered unlocked recipes array */
	int32 CurrentRecipeIndex = 0;

	// ========================================
	// State - Legacy Compatibility
	// ========================================

	/** Stored production recipe from sampled building (legacy) */
	UPROPERTY(Transient)
	TSubclassOf<UFGRecipe> StoredProductionRecipe = nullptr;

	/** Cached display name for stored production recipe */
	FString StoredRecipeDisplayName;

	/** Whether we have a valid stored production recipe */
	bool bHasStoredProductionRecipe = false;

	// ========================================
	// State - Power Shard & Somersloop (Issue #208/#209)
	// ========================================

	/** Build session ID when shard state was captured — only apply if current session matches */
	int32 ShardSessionId = 0;

	/** Current build session ID — incremented each time a new parent hologram registers */
	int32 CurrentBuildSessionId = 0;

	/** Build class that shards were captured from — used to detect build type changes */
	UPROPERTY(Transient)
	UClass* ShardSourceBuildClass = nullptr;

	/** Stored overclock potential from sampled building (1.0 = 100%, 2.5 = 250%) */
	float StoredPotential = 1.0f;

	/** Stored production boost from sampled building (1.0 = no boost, 2.0 = doubled) */
	float StoredProductionBoost = 1.0f;

	/** Whether the sampled building had Power Shards installed */
	bool bHasStoredPotential = false;

	/** Whether the sampled building had a Somersloop installed */
	bool bHasStoredProductionBoost = false;

	/** Actual Power Shard descriptor class from sampled building (needed for TryFillPotentialInventory) */
	UPROPERTY(Transient)
	TSubclassOf<class UFGPowerShardDescriptor> StoredOverclockShardClass = nullptr;

	/** Number of overclock shards in the source building (for direct inventory transfer) */
	int32 StoredOverclockShardCount = 0;

	/** Actual Somersloop descriptor class from sampled building (needed for TryFillPotentialInventory) */
	UPROPERTY(Transient)
	TSubclassOf<class UFGPowerShardDescriptor> StoredProductionBoostShardClass = nullptr;

public:
	/** Get stored overclock potential */
	float GetStoredPotential() const { return StoredPotential; }

	/** Get stored production boost */
	float GetStoredProductionBoost() const { return StoredProductionBoost; }

	/** Whether the sampled building had overclock/Power Shards */
	bool HasStoredPotential() const { return bHasStoredPotential; }

	/** Whether the sampled building had Somersloop/production boost */
	bool HasStoredProductionBoost() const { return bHasStoredProductionBoost; }

	/** Apply stored Power Shard and Somersloop configuration to a placed building (Issue #208/#209)
	 * Transfers items from player inventory — will not duplicate items.
	 * @param TargetBuilding The newly placed building to configure
	 * @param Player The player whose inventory to consume shards/somersloops from
	 * @return true if any configuration was applied
	 */
	bool ApplyStoredPotentialToBuilding(AFGBuildable* TargetBuilding, class AFGCharacterPlayer* Player);

private:

	// ========================================
	// State - Building Registry
	// ========================================

	/** Registry of all Smart-placed buildings with metadata */
	UPROPERTY(Transient)
	TMap<AFGBuildable*, FSFBuildingMetadata> SmartBuildingRegistry;

	/** Buildings placed in current placement operation */
	UPROPERTY(Transient)
	TArray<AFGBuildable*> CurrentPlacementBuildings;

	/** Current placement group ID (incremented each placement) */
	int32 CurrentPlacementGroupID = 0;

	/** Flag to detect blueprint proxy spawns (blueprint buildings preserve recipes) */
	bool bBlueprintProxyRecentlySpawned = false;

	// ========================================
	// References
	// ========================================

	/** Owning subsystem reference */
	UPROPERTY(Transient)
	class USFSubsystem* Subsystem = nullptr;

	/** Timer for debounced recipe regeneration */
	FTimerHandle RecipeRegenerationTimer;
};
