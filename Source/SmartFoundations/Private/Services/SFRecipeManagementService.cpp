// Copyright Epic Games, Inc. All Rights Reserved.

#include "SFRecipeManagementService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Data/SFHologramDataRegistry.h"
#include "FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGRecipeManager.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableFactory.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGPowerShardDescriptor.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGFactoryClipboard.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// ========================================
// Initialization & Lifecycle
// ========================================

void USFRecipeManagementService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	ClearAllRecipes();
	SmartBuildingRegistry.Empty();
	CurrentPlacementBuildings.Empty();
	CurrentPlacementGroupID = 0;
	bBlueprintProxyRecentlySpawned = false;
	UE_LOG(LogSmartFoundations, Log, TEXT("Recipe Management Service: Initialized"));
}

void USFRecipeManagementService::Cleanup()
{
	ClearAllRecipes();
	SmartBuildingRegistry.Empty();
	CurrentPlacementBuildings.Empty();
	if (Subsystem && Subsystem->GetWorld())
	{
		Subsystem->GetWorld()->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
	}
	Subsystem = nullptr;
	UE_LOG(LogSmartFoundations, Log, TEXT("Recipe Management Service: Cleaned up"));
}

// ========================================
// Recipe Mode (U Key)
// ========================================

void USFRecipeManagementService::ActivateRecipeMode()
{
	bRecipeModeActive = true;
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Recipe mode activated"));
}

void USFRecipeManagementService::DeactivateRecipeMode()
{
	bRecipeModeActive = false;
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Recipe mode deactivated"));
}

// ========================================
// Recipe Selection & Cycling
// ========================================

void USFRecipeManagementService::CycleRecipeForward(int32 AccumulatedSteps)
{
	// Ensure we have a cached list for the current hologram
	if (SortedFilteredRecipes.Num() == 0)
	{
		SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
	}

	const int32 Total = SortedFilteredRecipes.Num();
	if (Total == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Cannot cycle recipes: %d available"), Total);
		return;
	}

	// If only one recipe exists, select it if none is active; otherwise nothing to cycle
	if (Total == 1)
	{
		if (!ActiveRecipe)
		{
			SetActiveRecipeByIndex(0);
			ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
		}
		return;
	}

	// When no active recipe is selected, the first forward step should select index 0
	if (!ActiveRecipe)
	{
		SetActiveRecipeByIndex(0);
		ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
		return;
	}

	int32 NewIndex = (CurrentRecipeIndex + AccumulatedSteps) % Total;
	SetActiveRecipeByIndex(NewIndex);
	ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
}

void USFRecipeManagementService::CycleRecipeBackward(int32 AccumulatedSteps)
{
	// Ensure we have a cached list for the current hologram
	if (SortedFilteredRecipes.Num() == 0)
	{
		SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
	}

	const int32 Total = SortedFilteredRecipes.Num();
	if (Total == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Cannot cycle recipes: %d available"), Total);
		return;
	}

	// If only one recipe exists, select it if none is active; otherwise nothing to cycle
	if (Total == 1)
	{
		if (!ActiveRecipe)
		{
			SetActiveRecipeByIndex(0);
			ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
		}
		return;
	}

	// When no active recipe is selected, the first backward step should select the last index
	if (!ActiveRecipe)
	{
		SetActiveRecipeByIndex(Total - 1);
		ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
		return;
	}

	int32 NewIndex = (CurrentRecipeIndex - AccumulatedSteps) % Total;
	if (NewIndex < 0) NewIndex += Total;
	SetActiveRecipeByIndex(NewIndex);
	ActiveRecipeSource = ESFRecipeSource::ManuallySelected;
}

void USFRecipeManagementService::SetActiveRecipeByIndex(int32 Index)
{
	// Use cached sorted recipes directly (don't rebuild - it resets CurrentRecipeIndex!)
	if (SortedFilteredRecipes.Num() == 0)
	{
		// Cache empty - initialize it first (first time use or cache stale)
		SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
		if (SortedFilteredRecipes.Num() == 0) return; // Still empty, no recipes available
	}
	
	// Clamp index to valid range
	Index = FMath::Clamp(Index, 0, SortedFilteredRecipes.Num() - 1);
	
	ActiveRecipe = SortedFilteredRecipes[Index];
	CurrentRecipeIndex = Index;
	
	// Update stored recipe variables for compatibility with existing system
	StoredProductionRecipe = ActiveRecipe;
	StoredRecipeDisplayName = GetRecipeDisplayName(ActiveRecipe);
	bHasStoredProductionRecipe = (ActiveRecipe != nullptr);
	
	// Apply recipe to parent hologram if available
	ApplyRecipeToParentHologram();
	
	// Debounced regeneration - only if children exist and recipe actually changed
	AFGHologram* Hologram = Subsystem ? Subsystem->GetActiveHologram() : nullptr;
	if (Hologram)
	{
		if (Hologram->GetHologramChildren().Num() > 0)
		{
			if (UWorld* World = Subsystem->GetWorld())
			{
				World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
				World->GetTimerManager().SetTimer(
					RecipeRegenerationTimer,
					FTimerDelegate::CreateLambda([this]()
					{
						if (Subsystem)
						{
							Subsystem->RegenerateChildHologramGrid();
						}
					}),
					0.2f,  // 200ms debounce
					false
				);
				UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Recipe changed - scheduled child regeneration in 200ms"));
			}
		}
	}
	
	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Active recipe set: %s [%d/%d]"), 
		*StoredRecipeDisplayName, Index + 1, SortedFilteredRecipes.Num());
}

void USFRecipeManagementService::AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe)
{
	if (!Recipe) return;
	
	// Check if already unlocked
	if (UnlockedRecipes.Contains(Recipe)) return;
	
	// Add to unlocked recipes
	UnlockedRecipes.Add(Recipe);
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Recipe unlocked: %s (%d total)"), 
		*GetRecipeDisplayName(Recipe), UnlockedRecipes.Num());
}

TArray<TSubclassOf<UFGRecipe>> USFRecipeManagementService::GetFilteredRecipesForCurrentHologram()
{
	if (!Subsystem || !Subsystem->GetActiveHologram()) 
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ No active hologram for recipe filtering"));
		SortedFilteredRecipes.Empty();
		return TArray<TSubclassOf<UFGRecipe>>();
	}
	
	// Get recipe manager
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(Subsystem->GetWorld());
	if (!RecipeManager) 
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Cannot get RecipeManager"));
		SortedFilteredRecipes.Empty();
		return TArray<TSubclassOf<UFGRecipe>>();
	}
	
	AFGHologram* ActiveHologram = Subsystem->GetActiveHologram();
	
	// Get hologram's buildable class
	UClass* HologramBuildClass = ActiveHologram->GetBuildClass();
	if (!HologramBuildClass) 
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Cannot get hologram buildable class (non-buildable hologram?)"));
		SortedFilteredRecipes.Empty();
		return TArray<TSubclassOf<UFGRecipe>>();
	}
	
	// Get ALL available recipes for this building type from SML
	TArray<TSubclassOf<UFGRecipe>> AvailableRecipes;
	RecipeManager->GetAvailableRecipesForProducer(HologramBuildClass, AvailableRecipes);
	
	// Sort recipes alphabetically by product name
	AvailableRecipes.Sort([this](const TSubclassOf<UFGRecipe>& A, const TSubclassOf<UFGRecipe>& B)
	{
		FString NameA = GetRecipeDisplayName(A);
		FString NameB = GetRecipeDisplayName(B);
		return NameA < NameB;
	});
	
	// Cache the sorted results
	SortedFilteredRecipes = AvailableRecipes;
	
	// Reset recipe index when hologram type changes (prevent stale index)
	CurrentRecipeIndex = 0;
	
	return SortedFilteredRecipes;
}

void USFRecipeManagementService::ClearAllRecipes()
{
	// Clear unified state
	ActiveRecipe = nullptr;
	ActiveRecipeSource = ESFRecipeSource::None;
	CurrentRecipeIndex = 0;
	
	// Clear legacy variables for compatibility
	StoredProductionRecipe = nullptr;
	StoredRecipeDisplayName = TEXT("");
	bHasStoredProductionRecipe = false;
	
	// Clear unlocked recipes
	UnlockedRecipes.Empty();
	SortedFilteredRecipes.Empty();
	
	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ All recipes and unlocked list cleared"));
}

// ========================================
// Recipe Sampling (Middle-Click)
// ========================================

void USFRecipeManagementService::StoreProductionRecipeFromBuilding(AFGBuildable* SourceBuilding)
{
	if (!SourceBuilding || !IsValid(SourceBuilding))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Cannot store recipe - source building is invalid"));
		return;
	}
	
	// Clear any existing stored recipe
	ClearStoredProductionRecipe();
	
	// Check if this is a production building that supports recipes
	if (!IsProductionBuilding(SourceBuilding))
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Building is not a production building, skipping recipe storage"));
		return;
	}
	
	// Try to get the production recipe from the building
	TSubclassOf<UFGRecipe> ProductionRecipe = nullptr;
	
	if (auto Manufacturer = Cast<AFGBuildableManufacturer>(SourceBuilding))
	{
		ProductionRecipe = Manufacturer->GetCurrentRecipe();
	}
	
	if (ProductionRecipe)
	{
		// Update unified state
		ActiveRecipe = ProductionRecipe;
		ActiveRecipeSource = ESFRecipeSource::Copied;
		
		// Legacy compatibility
		StoredProductionRecipe = ProductionRecipe;
		StoredRecipeDisplayName = GetRecipeDisplayName(ProductionRecipe);
		bHasStoredProductionRecipe = true;
		
		// Add to unlocked recipes
		AddRecipeToUnlocked(ProductionRecipe);
		
		// Update cached filtered recipes
		if (SortedFilteredRecipes.Num() == 0)
		{
			SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
		}
		
		// Find index of sampled recipe in filtered list
		CurrentRecipeIndex = SortedFilteredRecipes.IndexOfByKey(ProductionRecipe);
		if (CurrentRecipeIndex == INDEX_NONE)
		{
			CurrentRecipeIndex = 0;
		}
		
		// Apply to parent hologram
		ApplyRecipeToParentHologram();
		
		if (Subsystem)
		{
			Subsystem->UpdateCounterDisplay();
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Stored recipe from building: %s"), *StoredRecipeDisplayName);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Building %s has no recipe set"), *SourceBuilding->GetName());
	}
	
	// Issue #208/#209: Capture Power Shard and Somersloop state from source building
	if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(SourceBuilding))
	{
		float SourcePotential = FMath::Max(Factory->GetPendingPotential(), Factory->GetCurrentPotential());
		float SourceBoost = FMath::Max(Factory->GetPendingProductionBoost(), Factory->GetCurrentProductionBoost());
		
		// First, extract actual shard descriptor classes from the building's potential inventory
		StoredOverclockShardClass = nullptr;
		StoredOverclockShardCount = 0;
		StoredProductionBoostShardClass = nullptr;
		UFGInventoryComponent* PotentialInv = Factory->GetPotentialInventory();
		if (PotentialInv)
		{
			TArray<FInventoryStack> Stacks;
			PotentialInv->GetInventoryStacks(Stacks);
			for (const FInventoryStack& Stack : Stacks)
			{
				if (!Stack.HasItems()) continue;
				TSubclassOf<UFGPowerShardDescriptor> ShardClass = TSubclassOf<UFGPowerShardDescriptor>(Stack.Item.GetItemClass());
				if (!ShardClass) continue;
				
				EPowerShardType ShardType = UFGPowerShardDescriptor::GetPowerShardType(ShardClass);
				if (ShardType == EPowerShardType::PST_Overclock)
				{
					if (!StoredOverclockShardClass)
					{
						StoredOverclockShardClass = ShardClass;
					}
					StoredOverclockShardCount += Stack.NumItems;
					UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Captured overclock shard: %s x%d (total: %d)"), *ShardClass->GetName(), Stack.NumItems, StoredOverclockShardCount);
				}
				else if (ShardType == EPowerShardType::PST_ProductionBoost && !StoredProductionBoostShardClass)
				{
					StoredProductionBoostShardClass = ShardClass;
					UE_LOG(LogSmartFoundations, Log, TEXT("🔮 Captured production boost shard class: %s"), *ShardClass->GetName());
				}
			}
		}
		
		// Store overclock potential (Power Shards)
		if (Factory->GetCanChangePotential() && SourcePotential > 1.0f && StoredOverclockShardClass)
		{
			StoredPotential = SourcePotential;
			bHasStoredPotential = true;
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Stored overclock potential: %.0f%% from %s (shard class: %s)"), 
				SourcePotential * 100.0f, *SourceBuilding->GetName(), *StoredOverclockShardClass->GetName());
		}
		else
		{
			StoredPotential = 1.0f;
			bHasStoredPotential = false;
		}
		
		// Store production boost (Somersloop)
		if (Factory->CanChangeProductionBoost() && SourceBoost > 1.0f && StoredProductionBoostShardClass)
		{
			StoredProductionBoost = SourceBoost;
			bHasStoredProductionBoost = true;
			UE_LOG(LogSmartFoundations, Log, TEXT("🔮 Stored production boost: %.0f%% from %s (shard class: %s)"), 
				SourceBoost * 100.0f, *SourceBuilding->GetName(), *StoredProductionBoostShardClass->GetName());
		}
		else
		{
			StoredProductionBoost = 1.0f;
			bHasStoredProductionBoost = false;
		}
		
		// Tag shard state with a fresh session ID and sync both counters
		// This ensures shards always match the current session, regardless of
		// whether RegisterActiveHologram has bumped yet or not
		if (bHasStoredPotential || bHasStoredProductionBoost)
		{
			++CurrentBuildSessionId;
			ShardSessionId = CurrentBuildSessionId;
			ShardSourceBuildClass = SourceBuilding->GetClass();
			UE_LOG(LogSmartFoundations, Log, TEXT("🏷️ Tagged shard state with session ID=%d, source class=%s"), 
				ShardSessionId, *GetNameSafe(ShardSourceBuildClass));
		}
	}
}

void USFRecipeManagementService::OnRecipeModeChanged(const FInputActionValue& Value)
{
	// Toggle recipe mode state
	bRecipeModeActive = Value.Get<bool>();
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Recipe mode %s"), 
		bRecipeModeActive ? TEXT("activated") : TEXT("deactivated"));
}

void USFRecipeManagementService::ApplyStoredProductionRecipeToBuilding(AFGBuildable* TargetBuilding)
{
	if (!TargetBuilding || !bHasStoredProductionRecipe || !StoredProductionRecipe)
	{
		return;
	}
	
	// Only apply to production buildings
	if (!IsProductionBuilding(TargetBuilding))
	{
		return;
	}
	
	AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(TargetBuilding);
	if (Manufacturer)
	{
		Manufacturer->SetRecipe(StoredProductionRecipe);
		UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Applied stored recipe %s to building %s"), 
			*StoredRecipeDisplayName, *TargetBuilding->GetName());
	}
}

void USFRecipeManagementService::OnBuildGunRecipeSampled(TSubclassOf<UFGRecipe> SampledRecipe)
{
	if (!SampledRecipe || !Subsystem)
	{
		return;
	}
	
	// The delegate gives us the construction recipe, but we need the production recipe
	// Find the building under the player's crosshair
	AFGPlayerController* PC = Subsystem->GetLastPlayerController();
	if (!PC)
	{
		return;
	}
	
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		return;
	}
	
	// Trace from camera to find the building
	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector TraceEnd = CameraLocation + (CameraRotation.Vector() * 10000.0f); // 100m range
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	
	if (Subsystem->GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, TraceEnd, ECC_Visibility, QueryParams))
	{
		// Check if we hit a production building
		AFGBuildable* HitBuilding = Cast<AFGBuildable>(HitResult.GetActor());
		if (HitBuilding && IsProductionBuilding(HitBuilding))
		{
			// Store the building's current production recipe
			StoreProductionRecipeFromBuilding(HitBuilding);
		}
	}
}

void USFRecipeManagementService::ClearStoredProductionRecipe()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🧹 ClearStoredProductionRecipe: bHasStoredPotential=%s, bHasStoredProductionBoost=%s, ShardCount=%d"),
		bHasStoredPotential ? TEXT("true") : TEXT("false"),
		bHasStoredProductionBoost ? TEXT("true") : TEXT("false"),
		StoredOverclockShardCount);
	
	// Clear unified state
	ActiveRecipe = nullptr;
	ActiveRecipeSource = ESFRecipeSource::None;
	CurrentRecipeIndex = 0;
	
	// Legacy compatibility
	StoredProductionRecipe = nullptr;
	StoredRecipeDisplayName = TEXT("");
	bHasStoredProductionRecipe = false;
	
	// Clear stored overclock and boost states
	StoredPotential = 1.0f;
	StoredProductionBoost = 1.0f;
	bHasStoredPotential = false;
	bHasStoredProductionBoost = false;
	StoredOverclockShardClass = nullptr;
	StoredOverclockShardCount = 0;
	StoredProductionBoostShardClass = nullptr;
	
	// Apply clear to hologram registry and trigger regeneration
	ApplyRecipeToParentHologram();
	
	AFGHologram* Hologram = Subsystem ? Subsystem->GetActiveHologram() : nullptr;
	if (Hologram)
	{
		if (Hologram->GetHologramChildren().Num() > 0)
		{
			if (UWorld* World = Subsystem->GetWorld())
			{
				World->GetTimerManager().SetTimer(
					RecipeRegenerationTimer,
					FTimerDelegate::CreateLambda([this]()
					{
						if (Subsystem)
						{
							Subsystem->RegenerateChildHologramGrid();
						}
					}),
					0.1f,
					false
				);
			}
		}
	}
	
	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Cleared stored production recipe"));
}

// ========================================
// Building Registry System
// ========================================

void USFRecipeManagementService::ApplyRecipeToParentHologram()
{
	// NOTE: We cannot change the recipe on a hologram after it's been set during construction.
	// The AFGHologram::SetRecipe() method has an assertion: !mRecipe (recipe must be null).
	// Instead, we update the hologram data registry so the recipe will be applied to BUILDINGS after placement.
	
	if (!Subsystem || !Subsystem->GetActiveHologram())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ No active hologram to apply recipe to"));
		return;
	}
	
	// Update the parent hologram's stored recipe in the data registry
	AFGHologram* ParentHologram = Subsystem->GetActiveHologram();
	if (AFGHologram* Parent = ParentHologram->GetParentHologram())
	{
		ParentHologram = Parent;
	}
	
	// Attach data structure to parent hologram if it doesn't exist
	FSFHologramData* HologramData = USFHologramDataRegistry::GetData(ParentHologram);
	if (!HologramData)
	{
		HologramData = USFHologramDataRegistry::AttachData(ParentHologram);
	}
	
	// Update the stored recipe (supports null recipe for clearing)
	if (HologramData)
	{
		HologramData->StoredRecipe = ActiveRecipe;
		
		if (ActiveRecipe != nullptr)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Updated parent hologram %s data registry with recipe %s"), 
				*ParentHologram->GetName(), *GetRecipeDisplayName(ActiveRecipe));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("🍽️ Cleared parent hologram %s data registry (recipe cleared)"), 
				*ParentHologram->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Could not attach hologram data to parent %s"), 
			*ParentHologram->GetName());
	}
}

void USFRecipeManagementService::RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent)
{
	if (!Building || !IsValid(Building))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("REGISTRY: Cannot register null or invalid building"));
		return;
	}
	
	// Create metadata
	FSFBuildingMetadata Metadata;
	Metadata.PlacementGroupID = CurrentPlacementGroupID;
	Metadata.IndexInGroup = IndexInGroup;
	Metadata.bIsParent = bIsParent;
	Metadata.AppliedRecipe = ActiveRecipe;  // Use unified state
	Metadata.CreationTime = FDateTime::Now();
	
	// Add to registry
	SmartBuildingRegistry.Add(Building, Metadata);
	
	// Track for current placement
	CurrentPlacementBuildings.Add(Building);
	
	// Log registration
	UE_LOG(LogSmartFoundations, Log, 
		TEXT("REGISTRY: Registered Smart Building | Group=%d Index=%d Type=%s Class=%s Recipe=%s"),
		Metadata.PlacementGroupID,
		Metadata.IndexInGroup,
		bIsParent ? TEXT("Parent") : TEXT("Child"),
		*Building->GetClass()->GetName(),
		Metadata.AppliedRecipe ? *Metadata.AppliedRecipe->GetName() : TEXT("None"));
}

void USFRecipeManagementService::ApplyRecipesToCurrentPlacement()
{
	if (CurrentPlacementBuildings.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("REGISTRY: No buildings in current placement to apply recipes to"));
		return;
	}
	
	if (!bHasStoredProductionRecipe || !StoredProductionRecipe)
	{
		UE_LOG(LogSmartFoundations, Log, 
			TEXT("REGISTRY: No stored recipe - buildings registered but no recipes to apply (Group %d, %d buildings)"),
			CurrentPlacementGroupID, CurrentPlacementBuildings.Num());
		ClearCurrentPlacement();
		return;
	}
	
	UE_LOG(LogSmartFoundations, Log, 
		TEXT("REGISTRY: Applying recipe %s to %d buildings in group %d"),
		*GetRecipeDisplayName(StoredProductionRecipe),
		CurrentPlacementBuildings.Num(),
		CurrentPlacementGroupID);
	
	int32 AppliedCount = 0;
	int32 SkippedCount = 0;
	
	for (AFGBuildable* Building : CurrentPlacementBuildings)
	{
		if (!Building || !IsValid(Building))
		{
			SkippedCount++;
			continue;
		}
		
		// Only apply to manufacturer buildings
		AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Building);
		if (!Manufacturer)
		{
			SkippedCount++;
			continue;
		}
		
		// Apply recipe via delayed timer
		if (Subsystem && Subsystem->GetWorld())
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUFunction(this, TEXT("ApplyRecipeDelayed"), Manufacturer, StoredProductionRecipe);
			Subsystem->GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.1f, false);
			AppliedCount++;
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, 
		TEXT("REGISTRY: Recipe application scheduled - %d manufacturers, %d skipped"),
		AppliedCount, SkippedCount);
	
	// Clear current placement tracking
	ClearCurrentPlacement();
}

void USFRecipeManagementService::OnActorSpawned(AActor* SpawnedActor)
{
	if (!SpawnedActor || !Subsystem)
	{
		return;
	}
	
	// Skip recipe logic during save game loading
	UWorld* World = Subsystem->GetWorld();
	if (World && !World->HasBegunPlay())
	{
		return;
	}
	
	// Check if this is a blueprint proxy
	if (SpawnedActor->GetClass()->GetName().Contains(TEXT("BlueprintProxy")))
	{
		bBlueprintProxyRecentlySpawned = true;
		UE_LOG(LogSmartFoundations, Log, TEXT("OnActorSpawned: Blueprint proxy %s detected"), 
			*SpawnedActor->GetName());
		
		// Clear the flag after 0.3 seconds
		if (World)
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUFunction(this, TEXT("ClearBlueprintProxyFlag"));
			World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.3f, false);
		}
		return;
	}
	
	// Issue #208/#209: Apply Power Shards and Somersloops to scaled child buildings
	// BOTH must go through a single delayed TryFillPotentialInventory call because:
	// 1. Immediate calls fail (building's potential inventory not initialized yet)
	// 2. Separate calls reset the inventory (overclock-only call clears Somersloop)
	// Guard: Only apply when Smart! is actively scaling (grid > 1x1)
	AFGBuildableFactory* FactoryBuilding = Cast<AFGBuildableFactory>(SpawnedActor);
	bool bIsSmartScaling = Subsystem->IsSmartScalingActive();
	bool bWantShards = bHasStoredPotential && StoredOverclockShardClass && StoredOverclockShardCount > 0;
	bool bWantBoost = bHasStoredProductionBoost && StoredProductionBoostShardClass && StoredProductionBoost > 1.0f;
	bool bSessionMatch = (ShardSessionId == CurrentBuildSessionId);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔍 OnActorSpawned SHARD CHECK: Factory=%s, Scaling=%s, Session=%s (%d==%d), WantShards=%s, WantBoost=%s"),
		FactoryBuilding ? TEXT("yes") : TEXT("no"), bIsSmartScaling ? TEXT("yes") : TEXT("no"),
		bSessionMatch ? TEXT("match") : TEXT("MISMATCH"), ShardSessionId, CurrentBuildSessionId,
		bWantShards ? TEXT("yes") : TEXT("no"), bWantBoost ? TEXT("yes") : TEXT("no"));
	
	if (FactoryBuilding && bIsSmartScaling && bSessionMatch && (bWantShards || bWantBoost))
	{
		if (UWorld* TimerWorld = Subsystem->GetWorld())
		{
			// Capture all values for the delayed lambda
			TWeakObjectPtr<AFGBuildableFactory> WeakFactory = FactoryBuilding;
			float CapturedPotential = StoredPotential;
			float CapturedBoost = StoredProductionBoost;
			TSubclassOf<UFGPowerShardDescriptor> CapturedOverclockClass = StoredOverclockShardClass;
			TSubclassOf<UFGPowerShardDescriptor> CapturedBoostClass = StoredProductionBoostShardClass;
			bool bCapturedWantShards = bWantShards;
			bool bCapturedWantBoost = bWantBoost;
			
			FTimerHandle TimerHandle;
			TimerWorld->GetTimerManager().SetTimer(TimerHandle, 
				[WeakFactory, CapturedPotential, CapturedBoost, CapturedOverclockClass, CapturedBoostClass, bCapturedWantShards, bCapturedWantBoost]()
			{
				AFGBuildableFactory* Factory = WeakFactory.Get();
				if (!Factory) return;
				
				AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(Factory->GetWorld(), 0));
				if (!Player) return;
				
				// Build a SINGLE combined map with both overclock and boost
				TMap<EPowerShardType, TPair<TSubclassOf<UFGPowerShardDescriptor>, float>> PotentialValues;
				
				if (bCapturedWantShards && Factory->GetCanChangePotential())
				{
					PotentialValues.Add(EPowerShardType::PST_Overclock, 
						TPair<TSubclassOf<UFGPowerShardDescriptor>, float>(CapturedOverclockClass, CapturedPotential));
				}
				if (bCapturedWantBoost && Factory->CanChangeProductionBoost())
				{
					PotentialValues.Add(EPowerShardType::PST_ProductionBoost, 
						TPair<TSubclassOf<UFGPowerShardDescriptor>, float>(CapturedBoostClass, CapturedBoost));
				}
				
				if (PotentialValues.Num() == 0) return;
				
				TMap<EPowerShardType, float> ReachedValues;
				bool bFilled = Factory->TryFillPotentialInventory(Player, PotentialValues, ReachedValues, false);
				
				// Apply overclock if shards were transferred
				if (bCapturedWantShards)
				{
					float NewMax = Factory->GetCurrentMaxPotential();
					if (NewMax > 1.0f)
					{
						Factory->SetPendingPotential(FMath::Min(CapturedPotential, NewMax));
						UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Delayed: Power Shards applied to %s, pending=%.0f%%, max=%.0f%%"),
							*Factory->GetName(), CapturedPotential * 100.0f, NewMax * 100.0f);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ Delayed: Power Shards failed for %s (max=%.0f%%)"),
							*Factory->GetName(), NewMax * 100.0f);
					}
				}
				
				// Apply production boost if somersloop was transferred
				if (bCapturedWantBoost)
				{
					float NewMaxBoost = Factory->GetCurrentMaxProductionBoost();
					if (NewMaxBoost > 1.0f)
					{
						Factory->SetPendingProductionBoost(FMath::Min(CapturedBoost, NewMaxBoost));
						UE_LOG(LogSmartFoundations, Log, TEXT("🔮 Delayed: Somersloop applied to %s, pending=%.0f%%, max=%.0f%%"),
							*Factory->GetName(), CapturedBoost * 100.0f, NewMaxBoost * 100.0f);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("🔮 Delayed: Somersloop failed for %s (max=%.0f%%)"),
							*Factory->GetName(), NewMaxBoost * 100.0f);
					}
				}
			}, 0.2f, false);
		}
	}
	
	// Recipe application: Only process manufacturer buildings
	AFGBuildableManufacturer* ManufacturerBuilding = Cast<AFGBuildableManufacturer>(SpawnedActor);
	if (!ManufacturerBuilding)
	{
		return;
	}
	
	// Apply the ActiveRecipe to the spawned building
	if (World)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUFunction(this, TEXT("ApplyRecipeDelayed"), ManufacturerBuilding, ActiveRecipe);
		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.1f, false);
		
		if (ActiveRecipe)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("OnActorSpawned: Will apply active recipe %s to manufacturer %s"), 
				*ActiveRecipe->GetName(), *ManufacturerBuilding->GetName());
		}
	}
}

void USFRecipeManagementService::ClearCurrentPlacement()
{
	CurrentPlacementBuildings.Empty();
	CurrentPlacementGroupID++;
	
	UE_LOG(LogSmartFoundations, Log, 
		TEXT("REGISTRY: Cleared current placement tracking | Next GroupID=%d | Total registered buildings=%d"),
		CurrentPlacementGroupID, SmartBuildingRegistry.Num());
}

void USFRecipeManagementService::ClearBlueprintProxyFlag()
{
	bBlueprintProxyRecentlySpawned = false;
	UE_LOG(LogSmartFoundations, Verbose, TEXT("ClearBlueprintProxyFlag: Blueprint proxy flag cleared"));
}

// ========================================
// Building Detection Functions
// ========================================

bool USFRecipeManagementService::IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass)
{
	if (!Recipe || !HologramBuildClass)
	{
		return false;
	}
	
	// Get recipe default object
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO)
	{
		return false;
	}
	
	// Get the building class that can produce this recipe
	TArray<TSubclassOf<UObject>> ProducedIn;
	RecipeCDO->GetProducedIn(ProducedIn);
	if (ProducedIn.Num() == 0)
	{
		return false;
	}
	UClass* RecipeProducerClass = ProducedIn[0];
	if (!RecipeProducerClass)
	{
		return false;
	}
	
	// Check if the hologram's buildable class can produce this recipe
	// This handles both direct matches and inheritance (for modded buildings)
	return HologramBuildClass->IsChildOf(RecipeProducerClass);
}

bool USFRecipeManagementService::IsRecipeCompatibleWithBuilding(TSubclassOf<UFGRecipe> Recipe, AFGBuildable* Building) const
{
	if (!Recipe || !Building)
	{
		return false;
	}
	
	// Get recipe default object
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO)
	{
		return false;
	}
	
	// Get the building classes that can produce this recipe
	TArray<TSubclassOf<UObject>> ProducedIn;
	RecipeCDO->GetProducedIn(ProducedIn);
	if (ProducedIn.Num() == 0)
	{
		return false;
	}
	
	// Check if this building's class is compatible with any of the recipe's producer classes
	UClass* BuildingClass = Building->GetClass();
	for (const TSubclassOf<UObject>& ProducerClass : ProducedIn)
	{
		if (ProducerClass && BuildingClass->IsChildOf(ProducerClass))
		{
			return true; // Building can produce this recipe
		}
	}
	
	return false; // Building cannot produce this recipe
}

bool USFRecipeManagementService::IsProductionBuilding(AFGBuildable* Building) const
{
	if (!Building)
	{
		return false;
	}

	// Check if building is a production building type that supports recipes
	// AFGBuildableManufacturer is the base class for most production buildings
	return Cast<AFGBuildableManufacturer>(Building) != nullptr ||
		   Building->IsA(AFGBuildableFactory::StaticClass()); // Generic factory check for modded buildings
}

// ========================================
// Display Helper Functions
// ========================================

FString USFRecipeManagementService::GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const
{
	if (!Recipe) return TEXT("None");
	
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO) return TEXT("Invalid");
	
	// Check if recipe has products using public getter
	TArray<FItemAmount> Products = RecipeCDO->GetProducts();
	if (Products.Num() == 0) return TEXT("No Product");
	
	// Get the first product's display name (handles localization internally)
	return UFGItemDescriptor::GetItemName(Products[0].ItemClass).ToString();
}

UTexture2D* USFRecipeManagementService::GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const
{
	if (!Recipe) return nullptr;
	
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO) return nullptr;
	
	// Get primary product
	TArray<FItemAmount> Products = RecipeCDO->GetProducts();
	if (Products.Num() == 0) return nullptr;
	
	// Get the first product's icon using the correct Satisfactory API
	return UFGItemDescriptor::GetSmallIcon(Products[0].ItemClass);
}

FString USFRecipeManagementService::GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const
{
	if (!Recipe) return TEXT("None");
	
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO) return TEXT("Invalid");
	
	// Get primary product name
	TArray<FItemAmount> Products = RecipeCDO->GetProducts();
	if (Products.Num() == 0) return TEXT("No Product");
	
	FString PrimaryProductName = UFGItemDescriptor::GetItemName(Products[0].ItemClass).ToString();
	FString Result = TEXT("");  // Start empty, primary will be first line
	
	// Enumerate ALL outputs including primary as indented lines
	for (int32 i = 0; i < Products.Num(); i++)
	{
		FString ProductName = UFGItemDescriptor::GetItemName(Products[i].ItemClass).ToString();
		int32 Amount = Products[i].Amount;
		
		if (i == 0)
		{
			// Primary output - show on main line AND in indented list
			if (Amount >= 1000)
			{
				int32 DisplayAmount = Amount / 1000;
				Result += FString::Printf(TEXT("%s x%d m³"), *ProductName, DisplayAmount);
				Result += FString::Printf(TEXT("\n  → Output: %s x%d m³"), *ProductName, DisplayAmount);
			}
			else
			{
				Result += FString::Printf(TEXT("%s x%d"), *ProductName, Amount);
				Result += FString::Printf(TEXT("\n  → Output: %s x%d"), *ProductName, Amount);
			}
		}
		else
		{
			// Additional outputs
			if (Amount >= 1000)
			{
				int32 DisplayAmount = Amount / 1000;
				Result += FString::Printf(TEXT("\n  → Output: %s x%d m³"), *ProductName, DisplayAmount);
			}
			else
			{
				Result += FString::Printf(TEXT("\n  → Output: %s x%d"), *ProductName, Amount);
			}
		}
	}
	
	// Enumerate inputs as indented lines
	TArray<FItemAmount> Ingredients = RecipeCDO->GetIngredients();
	if (Ingredients.Num() > 0)
	{
		for (int32 i = 0; i < Ingredients.Num(); i++)
		{
			FString IngredientName = UFGItemDescriptor::GetItemName(Ingredients[i].ItemClass).ToString();
			int32 Amount = Ingredients[i].Amount;
			
			// Liquids have amounts >= 1000, divide by 1000 and add m³ suffix
			if (Amount >= 1000)
			{
				int32 DisplayAmount = Amount / 1000;
				Result += FString::Printf(TEXT("\n  ← Input: %s x%d m³"), *IngredientName, DisplayAmount);
			}
			else
			{
				Result += FString::Printf(TEXT("\n  ← Input: %s x%d"), *IngredientName, Amount);
			}
		}
	}
	
	return Result;
}

FString USFRecipeManagementService::GetRecipeComboBoxLabel(TSubclassOf<UFGRecipe> Recipe) const
{
	if (!Recipe) return TEXT("None");
	
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO) return TEXT("Invalid");
	
	// Get primary product name
	FString ProductName = GetRecipeDisplayName(Recipe);
	
	// Build input list
	TArray<FItemAmount> Ingredients = RecipeCDO->GetIngredients();
	FString InputList;
	for (const FItemAmount& Ing : Ingredients)
	{
		if (!InputList.IsEmpty()) InputList += TEXT(", ");
		InputList += UFGItemDescriptor::GetItemName(Ing.ItemClass).ToString();
	}
	
	// Format as "ProductName (Input1, Input2...)"
	FString Label = FString::Printf(TEXT("%s (%s)"), *ProductName, *InputList);
	
	// Truncate if too long for ComboBox display
	if (Label.Len() > 45)
	{
		Label = Label.Left(42) + TEXT("...");
	}
	
	return Label;
}

FString USFRecipeManagementService::GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const
{
	if (!Recipe) return TEXT("None");
	
	UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
	if (!RecipeCDO) return TEXT("Invalid");
	
	// Get product name using existing function
	FString ProductName = GetRecipeDisplayName(Recipe);
	
	// Get first ingredient using public getter
	FString IngredientInfo = TEXT("");
	TArray<FItemAmount> Ingredients = RecipeCDO->GetIngredients();
	if (Ingredients.Num() > 0)
	{
		const FItemAmount& FirstIngredient = Ingredients[0];
		FString IngredientName = UFGItemDescriptor::GetItemName(FirstIngredient.ItemClass).ToString();
		IngredientInfo = FString::Printf(TEXT(" (%s x%d)"), *IngredientName, FirstIngredient.Amount);
	}
	
	return FString::Printf(TEXT("%s%s"), *ProductName, *IngredientInfo);
}

// ========================================
// Timer Callback Functions
// ========================================

void USFRecipeManagementService::ApplyRecipeDelayed(AFGBuildableManufacturer* ManufacturerBuilding, TSubclassOf<UFGRecipe> Recipe)
{
	if (!IsValid(ManufacturerBuilding))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyRecipeDelayed: Building is invalid - cannot apply/clear recipe"));
		return;
	}
	
	// CRITICAL: Skip recipe changes for blueprint buildings (they retain their preloaded recipes)
	if (bBlueprintProxyRecentlySpawned)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: Skipping recipe change for %s - building spawned from blueprint (preserving preloaded recipe)"), 
			*ManufacturerBuilding->GetName());
		return;
	}
	
	// CRITICAL: Check if building has begun play before applying recipe
	if (!ManufacturerBuilding->HasActorBegunPlay())
	{
		if (Recipe)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyRecipeDelayed: Building %s not ready for recipe (HasActorBegunPlay=false) - retrying with longer delay"), 
				*ManufacturerBuilding->GetName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyRecipeDelayed: Building %s not ready for recipe clear (HasActorBegunPlay=false) - retrying with longer delay"), 
				*ManufacturerBuilding->GetName());
		}
		
		// Retry with a longer delay (0.5s instead of 0.1s)
		FTimerHandle RetryTimerHandle;
		FTimerDelegate RetryTimerDelegate;
		RetryTimerDelegate.BindUFunction(this, TEXT("ApplyRecipeDelayed"), ManufacturerBuilding, Recipe);
		if (Subsystem && Subsystem->GetWorld())
		{
			Subsystem->GetWorld()->GetTimerManager().SetTimer(RetryTimerHandle, RetryTimerDelegate, 0.5f, false);
		}
		return;
	}
	
	if (Recipe)
	{
		// CRITICAL FIX FOR ISSUE #184: Validate recipe compatibility before applying
		if (!IsRecipeCompatibleWithBuilding(Recipe, ManufacturerBuilding))
		{
			UE_LOG(LogSmartFoundations, Warning, 
				TEXT("ApplyRecipeDelayed: ❌ Recipe %s is NOT compatible with building %s (class: %s) - skipping application"),
				*Recipe->GetName(),
				*ManufacturerBuilding->GetName(),
				*ManufacturerBuilding->GetClass()->GetName());
			return; // Skip applying incompatible recipe
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: Building %s is ready - applying recipe %s"), 
			*ManufacturerBuilding->GetName(), *Recipe->GetName());
		
		// Apply the recipe to the building
		ManufacturerBuilding->SetRecipe(Recipe);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: Building %s is ready - clearing recipe (user cleared with Num0)"), 
			*ManufacturerBuilding->GetName());
		
		// Clear the recipe from the building
		ManufacturerBuilding->SetRecipe(nullptr);
	}
	
	// Verify the recipe was actually applied or cleared
	TSubclassOf<UFGRecipe> AppliedRecipe = ManufacturerBuilding->GetCurrentRecipe();
	UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: Recipe verification - Expected: %s, Applied: %s"), 
		Recipe ? *Recipe->GetName() : TEXT("NULL"), 
		AppliedRecipe ? *AppliedRecipe->GetName() : TEXT("NULL"));
	
	if (AppliedRecipe == Recipe)
	{
		if (Recipe)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: ✅ Recipe successfully applied and verified"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("ApplyRecipeDelayed: ✅ Recipe successfully cleared and verified"));
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyRecipeDelayed: ❌ Recipe verification failed - expected/applied mismatch"));
	}
}

void USFRecipeManagementService::OnNewBuildSession(UClass* NewBuildClass)
{
	// Only skip the bump if shards are active AND the build class hasn't changed
	// (shard capture syncs both IDs — a bump for the SAME class would undo that sync)
	if (ShardSessionId == CurrentBuildSessionId && (bHasStoredPotential || bHasStoredProductionBoost)
		&& NewBuildClass == ShardSourceBuildClass)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🏷️ New build session: SKIPPED bump (shards active for same class in session %d)"), CurrentBuildSessionId);
		return;
	}
	
	++CurrentBuildSessionId;
	UE_LOG(LogSmartFoundations, Log, TEXT("🏷️ New build session: ID=%d (shard session=%d, match=%s)"),
		CurrentBuildSessionId, ShardSessionId,
		(CurrentBuildSessionId == ShardSessionId) ? TEXT("yes") : TEXT("no"));
}

void USFRecipeManagementService::ClearStoredShardState()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🧹 ClearStoredShardState: Clearing shard/somersloop state only (recipe preserved)"));
	StoredPotential = 1.0f;
	StoredProductionBoost = 1.0f;
	bHasStoredPotential = false;
	bHasStoredProductionBoost = false;
	StoredOverclockShardClass = nullptr;
	StoredOverclockShardCount = 0;
	StoredProductionBoostShardClass = nullptr;
}

void USFRecipeManagementService::GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const
{
	OutCurrentIndex = CurrentRecipeIndex;
	OutTotalRecipes = SortedFilteredRecipes.Num();
}

// Protected accessor to call FillPotentialSlotsInternal on AFGBuildableFactory
// TryFillPotentialInventory silently fails for overclock shards, but the protected
// FillPotentialSlotsInternal works correctly with a shard COUNT parameter
class FFGBuildableFactoryAccessor : public AFGBuildableFactory
{
public:
	using AFGBuildableFactory::FillPotentialSlotsInternal;
};

bool USFRecipeManagementService::ApplyStoredPotentialToBuilding(AFGBuildable* TargetBuilding, AFGCharacterPlayer* Player)
{
	if (!TargetBuilding || !Player)
	{
		return false;
	}
	
	AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(TargetBuilding);
	if (!Factory)
	{
		return false;
	}
	
	UFGInventoryComponent* PlayerInv = Player->GetInventory();
	if (!PlayerInv)
	{
		return false;
	}
	
	bool bAppliedAnything = false;
	
	// Issue #209: Apply Power Shards via FillPotentialSlotsInternal (protected accessor)
	// TryFillPotentialInventory silently fails for overclock, but this internal method works
	if (bHasStoredPotential && StoredPotential > 1.0f && Factory->GetCanChangePotential() 
		&& StoredOverclockShardClass && StoredOverclockShardCount > 0)
	{
		int32 TargetShards = StoredOverclockShardCount;
		TArray<FInventoryStack> ItemsToDrop;
		
		static_cast<FFGBuildableFactoryAccessor*>(Factory)->FillPotentialSlotsInternal(
			PlayerInv, EPowerShardType::PST_Overclock, StoredOverclockShardClass, TargetShards, ItemsToDrop);
		
		int32 ShardsTransferred = StoredOverclockShardCount - TargetShards;
		if (ShardsTransferred > 0)
		{
			Factory->SetPendingPotential(FMath::Min(StoredPotential, Factory->GetCurrentMaxPotential()));
			bAppliedAnything = true;
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Power Shards: Transferred %d/%d to %s, pending=%.0f%%, max=%.0f%%"),
				ShardsTransferred, StoredOverclockShardCount, *TargetBuilding->GetName(),
				StoredPotential * 100.0f, Factory->GetCurrentMaxPotential() * 100.0f);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ Power Shards: Could not transfer to %s (player may be out)"),
				*TargetBuilding->GetName());
		}
	}
	
	// Issue #208: Somersloops are handled by PasteSettings in OnActorSpawned (works correctly)
	// No need to handle them here — PasteSettings transfers Somersloops via the vanilla clipboard path
	
	return bAppliedAnything;
}
