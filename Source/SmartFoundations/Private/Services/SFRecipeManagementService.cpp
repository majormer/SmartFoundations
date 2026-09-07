// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

// SP/MP DIVERGENCE MAP - copied factory settings
// QueueFactorySettingsApplication / ApplyFactorySettingsSnapshot [MP-AUTH]:
// SP and server authority apply the shared snapshot; remote clients never mutate
// the constructed factory's recipe or transfer its shards locally.

#include "SFRecipeManagementService.h"
#include "SmartFoundations.h"
#include "Core/Construction/SFFactorySettingsApplyPolicy.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Data/SFHologramDataRegistry.h"
#include "Equipment/FGBuildGun.h"
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
#include "FGItemPickup_Spawnable.h"
#include "FGFactoryClipboard.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Module/SFGameInstanceModule.h"  // [#368] SetBuildStateClipboardRecipe (clipboard sync)
#include "Core/Net/SFRCO.h"               // [#368] Server_SetClipboardRecipe

// ========================================
// Initialization & Lifecycle
// ========================================

void USFRecipeManagementService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	ClearAllRecipes();
	SmartBuildingRegistry.Empty();
	CurrentPlacementBuildings.Empty();
	PendingFactorySettingsApplications.Empty();
	CurrentPlacementGroupID = 0;
	bBlueprintProxyRecentlySpawned = false;
	UE_LOG(LogSmartFoundations, Verbose, TEXT("Recipe Management Service: Initialized"));
}

void USFRecipeManagementService::SyncSubsystemRecipeState() const
{
	if (!Subsystem)
	{
		return;
	}

	Subsystem->StoredProductionRecipe = StoredProductionRecipe;
	Subsystem->StoredRecipeDisplayName = StoredRecipeDisplayName;
	Subsystem->bHasStoredProductionRecipe = bHasStoredProductionRecipe;
}

void USFRecipeManagementService::SyncClipboardRecipe(TSubclassOf<UFGRecipe> Recipe)
{
	// [#368] Keep the player's vanilla build-gun clipboard in sync with Smart's chosen recipe so
	// vanilla's PasteSettings applies the SAME recipe Smart's spec-construction does (otherwise a
	// stale sampled clipboard overrides a U/Panel pick on the authoritative build). Recipe-pick runs
	// on the owning client; set the local copy AND ask the server to set its copy (the field is not
	// replicated). On SP/listen-host the local set is the authority; the RCO is a harmless no-op/echo.
	if (!Subsystem)
	{
		return;
	}
	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		return;
	}
	AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
	if (!Player)
	{
		return;
	}
	USFGameInstanceModule::SetBuildStateClipboardRecipe(Player, Recipe);

	if (AFGPlayerController* PC = Cast<AFGPlayerController>(Player->GetController()))
	{
		if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
		{
			RCO->Server_SetClipboardRecipe(Recipe);
		}
	}
}

void USFRecipeManagementService::Cleanup()
{
	ClearAllRecipes();
	SmartBuildingRegistry.Empty();
	CurrentPlacementBuildings.Empty();
	PendingFactorySettingsApplications.Empty();
	if (Subsystem && Subsystem->GetWorld())
	{
		Subsystem->GetWorld()->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
		Subsystem->GetWorld()->GetTimerManager().ClearTimer(FactorySettingsApplyTimer);
	}
	Subsystem = nullptr;
	UE_LOG(LogSmartFoundations, Verbose, TEXT("Recipe Management Service: Cleaned up"));
}

// ========================================
// Recipe Mode (U Key)
// ========================================

void USFRecipeManagementService::ActivateRecipeMode()
{
	bRecipeModeActive = true;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe mode activated"));
}

void USFRecipeManagementService::DeactivateRecipeMode()
{
	bRecipeModeActive = false;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe mode deactivated"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Cannot cycle recipes: %d available"), Total);
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Cannot cycle recipes: %d available"), Total);
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
	SyncSubsystemRecipeState();
	
	// Apply recipe to parent hologram if available
	ApplyRecipeToParentHologram();

	// [#368] Mirror the pick into the vanilla build-gun clipboard (client + server) so vanilla pastes
	// the same recipe Smart does and a stale sampled clipboard can't override this selection.
	SyncClipboardRecipe(ActiveRecipe);

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
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe changed - scheduled child regeneration in 200ms"));
			}
		}
	}
	
	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Active recipe set: %s [%d/%d]"), 
		*StoredRecipeDisplayName, Index + 1, SortedFilteredRecipes.Num());
}

bool USFRecipeManagementService::SetActiveRecipeByClass(TSubclassOf<UFGRecipe> RecipeClass)
{
	if (!RecipeClass) return false;

	// Force-rebuild the recipe cache since this is typically called after a
	// build gun switch where the cached list is stale (old hologram's recipes).
	// The index reset from rebuilding is acceptable here because we look up
	// by class, not by index.
	SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();

	// Find the recipe in SortedFilteredRecipes by class
	for (int32 i = 0; i < SortedFilteredRecipes.Num(); ++i)
	{
		if (SortedFilteredRecipes[i] == RecipeClass)
		{
			SetActiveRecipeByIndex(i);
			return true;
		}
	}

	return false;
}

bool USFRecipeManagementService::StoreProductionRecipeClass(TSubclassOf<UFGRecipe> RecipeClass, ESFRecipeSource Source)
{
	if (!RecipeClass)
	{
		return false;
	}

	ActiveRecipe = RecipeClass;
	ActiveRecipeSource = Source;
	StoredProductionRecipe = RecipeClass;
	StoredRecipeDisplayName = GetRecipeDisplayName(RecipeClass);
	bHasStoredProductionRecipe = true;

	AddRecipeToUnlocked(RecipeClass);
	SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
	CurrentRecipeIndex = SortedFilteredRecipes.IndexOfByKey(RecipeClass);
	if (CurrentRecipeIndex == INDEX_NONE)
	{
		CurrentRecipeIndex = 0;
	}

	SyncSubsystemRecipeState();
	ApplyRecipeToParentHologram();

	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Stored production recipe class directly: %s (source=%d, filteredIndex=%d, filteredCount=%d)"),
		*RecipeClass->GetName(),
		static_cast<int32>(Source),
		CurrentRecipeIndex,
		SortedFilteredRecipes.Num());
	return true;
}

void USFRecipeManagementService::AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe)
{
	if (!Recipe) return;
	
	// Check if already unlocked
	if (UnlockedRecipes.Contains(Recipe)) return;
	
	// Add to unlocked recipes
	UnlockedRecipes.Add(Recipe);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe unlocked: %s (%d total)"), 
		*GetRecipeDisplayName(Recipe), UnlockedRecipes.Num());
}

TArray<TSubclassOf<UFGRecipe>> USFRecipeManagementService::GetFilteredRecipesForCurrentHologram()
{
	if (!Subsystem || !Subsystem->GetActiveHologram()) 
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ No active hologram for recipe filtering"));
		SortedFilteredRecipes.Empty();
		return TArray<TSubclassOf<UFGRecipe>>();
	}
	
	// Get recipe manager
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(Subsystem->GetWorld());
	if (!RecipeManager) 
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Cannot get RecipeManager"));
		SortedFilteredRecipes.Empty();
		return TArray<TSubclassOf<UFGRecipe>>();
	}
	
	AFGHologram* ActiveHologram = Subsystem->GetActiveHologram();
	
	// Get hologram's buildable class
	UClass* HologramBuildClass = ActiveHologram->GetBuildClass();
	if (!HologramBuildClass) 
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Cannot get hologram buildable class (non-buildable hologram?)"));
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
	SyncSubsystemRecipeState();

	// [#368] Explicit recipe clear (Num0 / panel Clear) -> drop the vanilla clipboard recipe too, so
	// the next placement is recipe-less (vanilla behavior). No-op during init/cleanup (no build gun).
	SyncClipboardRecipe(nullptr);

	// Clear unlocked recipes
	UnlockedRecipes.Empty();
	SortedFilteredRecipes.Empty();
	
	if (Subsystem)
	{
		Subsystem->UpdateCounterDisplay();
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ All recipes and unlocked list cleared"));
}

// ========================================
// Implicit Settings Sampling (Middle-Click)
// ========================================

void USFRecipeManagementService::BeginImplicitSettingsSample()
{
	bCapturedImplicitSettingsThisSample = false;

	// MMB with vanilla setting-copy disabled must not inherit settings from a previous MMB sample.
	// A recipe deliberately chosen through U/the Smart Panel is a separate contract and survives.
	if (ActiveRecipeSource == ESFRecipeSource::Copied)
	{
		ClearStoredProductionRecipe();
	}
	else
	{
		ClearStoredShardState();
		if (Subsystem)
		{
			Subsystem->UpdateCounterDisplay();
		}
	}
}

void USFRecipeManagementService::CaptureVanillaSampledProductionSettings(AFGBuildableManufacturer* SourceBuilding)
{
	if (!SourceBuilding || !IsValid(SourceBuilding))
	{
		return;
	}

	// The caller has already verified that vanilla produced manufacturer clipboard settings for
	// this exact actor. Do not perform a second trace or independently read the gameplay option.
	if (!IsProductionBuilding(SourceBuilding))
	{
		return;
	}

	bCapturedImplicitSettingsThisSample = true;
	
	// Try to get the production recipe from the building
	TSubclassOf<UFGRecipe> ProductionRecipe = nullptr;
	
	ProductionRecipe = SourceBuilding->GetCurrentRecipe();
	
	if (ProductionRecipe)
	{
		// Update unified state
		ActiveRecipe = ProductionRecipe;
		ActiveRecipeSource = ESFRecipeSource::Copied;
		
		// Legacy compatibility
		StoredProductionRecipe = ProductionRecipe;
		StoredRecipeDisplayName = GetRecipeDisplayName(ProductionRecipe);
		bHasStoredProductionRecipe = true;
		SyncSubsystemRecipeState();
		
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
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Stored recipe from building: %s"), *StoredRecipeDisplayName);
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Building %s has no recipe set"), *SourceBuilding->GetName());
	}
	
	// Issue #208/#209: Capture Power Shard and Somersloop state from source building
	if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(SourceBuilding))
	{
		const float RawPotential = FMath::Max(Factory->GetPendingPotential(), Factory->GetCurrentPotential());
		const float RawBoost = FMath::Max(Factory->GetPendingProductionBoost(), Factory->GetCurrentProductionBoost());
		const float SourcePotential = FMath::IsFinite(RawPotential) ? RawPotential : 1.0f;
		const float SourceBoost = FMath::IsFinite(RawBoost) ? RawBoost : 1.0f;
		
		// First, extract actual shard descriptor classes from the building's potential inventory
		StoredOverclockShardClass = nullptr;
		StoredOverclockShardCount = 0;
		StoredProductionBoostShardClass = nullptr;
		StoredProductionBoostShardCount = 0;
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
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ Captured overclock shard: %s x%d (total: %d)"), *ShardClass->GetName(), Stack.NumItems, StoredOverclockShardCount);
				}
				else if (ShardType == EPowerShardType::PST_ProductionBoost)
				{
					if (!StoredProductionBoostShardClass)
					{
						StoredProductionBoostShardClass = ShardClass;
					}
					if (StoredProductionBoostShardClass == ShardClass)
					{
						StoredProductionBoostShardCount += Stack.NumItems;
					}
				}
			}
		}
		
		// Store overclock potential (Power Shards)
		if (Factory->GetCanChangePotential() && SourcePotential > 1.0f && StoredOverclockShardClass)
		{
			StoredPotential = SourcePotential;
			bHasStoredPotential = true;
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ Stored overclock potential: %.0f%% from %s (shard class: %s)"), 
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔮 Stored production boost: %.0f%% from %s (shard class: %s)"), 
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🏷️ Tagged shard state with session ID=%d, source class=%s"), 
				ShardSessionId, *GetNameSafe(ShardSourceBuildClass));
		}
	}
}

void USFRecipeManagementService::CaptureFactorySettingsSnapshot(FSFFactorySettingsSnapshot& OutSnapshot) const
{
	OutSnapshot = FSFFactorySettingsSnapshot();
	if (bHasStoredProductionRecipe && StoredProductionRecipe)
	{
		OutSnapshot.bHasRecipe = true;
		OutSnapshot.Recipe = StoredProductionRecipe;
	}
	if (bHasStoredPotential && StoredOverclockShardClass)
	{
		OutSnapshot.bHasPotential = true;
		OutSnapshot.Potential = StoredPotential;
		OutSnapshot.OverclockShardClass = StoredOverclockShardClass;
		OutSnapshot.OverclockShardCount = StoredOverclockShardCount;
	}
	if (bHasStoredProductionBoost && StoredProductionBoostShardClass)
	{
		OutSnapshot.bHasProductionBoost = true;
		OutSnapshot.ProductionBoost = StoredProductionBoost;
		OutSnapshot.ProductionBoostShardClass = StoredProductionBoostShardClass;
		OutSnapshot.ProductionBoostShardCount = StoredProductionBoostShardCount;
	}
}

void USFRecipeManagementService::InstallFactorySettingsSnapshot(const FSFFactorySettingsSnapshot& Snapshot, UClass* CommitBuildClass)
{
	// The snapshot is AUTHORITATIVE for this commit: absent settings mean the commit's factories
	// get NONE - never inherit whatever a previous commit installed. The additive-only first
	// version leaked state live (2026-07-14): with vanilla's sample setting OFF the snapshot
	// shipped empty, the install no-op'd, and the clones received the PREVIOUS test's recipe
	// straight from this service's stale stored state. On a listen host this install (like the
	// old Restore-recipe install before it) also overwrites the host player's own sampled state
	// when a remote client's commit lands - a pre-existing shared-service trade-off; the
	// reported environment (dedicated server) has no local player.
	if (Snapshot.bHasRecipe && Snapshot.Recipe)
	{
		// Same install the RESTORE commit already used - also syncs the subsystem mirror fields
		// (StoredProductionRecipe / bHasStoredProductionRecipe) the scaled spawner reads.
		ClearStoredShardState();  // shard fields re-install below only when the snapshot carries them
		StoreProductionRecipeClass(Snapshot.Recipe);
	}
	else
	{
		ClearStoredProductionRecipe();  // clears recipe AND shard/somersloop state
	}

	// Server-side sanity on client-shipped values: clamp to vanilla's reachable ranges rather
	// than trusting the floats/counts blindly. The descriptor classes are already constrained to
	// UFGPowerShardDescriptor by the property system.
	if (Snapshot.bHasPotential && Snapshot.OverclockShardClass && FMath::IsFinite(Snapshot.Potential))
	{
		StoredPotential = FMath::Clamp(Snapshot.Potential, 1.0f, 2.5f);
		StoredOverclockShardCount = FMath::Clamp(Snapshot.OverclockShardCount, 0, 3);
		bHasStoredPotential = StoredPotential > 1.0f && StoredOverclockShardCount > 0;
		StoredOverclockShardClass = bHasStoredPotential ? Snapshot.OverclockShardClass : nullptr;
	}
	if (Snapshot.bHasProductionBoost && Snapshot.ProductionBoostShardClass && FMath::IsFinite(Snapshot.ProductionBoost))
	{
		StoredProductionBoost = FMath::Clamp(Snapshot.ProductionBoost, 1.0f, 2.0f);
		StoredProductionBoostShardCount = FMath::Clamp(Snapshot.ProductionBoostShardCount, 0, 2);
		bHasStoredProductionBoost = StoredProductionBoost > 1.0f && StoredProductionBoostShardCount > 0;
		StoredProductionBoostShardClass = bHasStoredProductionBoost ? Snapshot.ProductionBoostShardClass : nullptr;
	}
	if (bHasStoredPotential || bHasStoredProductionBoost)
	{
		// Align the shard session to NOW and record the commit's build class, so the session
		// gating (and OnNewBuildSession's same-class carry-over) treats this exactly like a
		// local sample instead of discarding it as stale.
		++CurrentBuildSessionId;
		ShardSessionId = CurrentBuildSessionId;
		ShardSourceBuildClass = CommitBuildClass;
	}

	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[#484] Installed commit factory settings: recipe=%s potential=%s(%.2f, shards=%d) boost=%s(%.2f) buildClass=%s"),
		Snapshot.bHasRecipe ? *GetNameSafe(Snapshot.Recipe) : TEXT("(none)"),
		Snapshot.bHasPotential ? TEXT("yes") : TEXT("no"), Snapshot.Potential, Snapshot.OverclockShardCount,
		Snapshot.bHasProductionBoost ? TEXT("yes") : TEXT("no"), Snapshot.ProductionBoost,
		*GetNameSafe(CommitBuildClass));
}

void USFRecipeManagementService::OnRecipeModeChanged(const FInputActionValue& Value)
{
	// Toggle recipe mode state
	bRecipeModeActive = Value.Get<bool>();
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe mode %s"), 
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Applied stored recipe %s to building %s"), 
			*StoredRecipeDisplayName, *TargetBuilding->GetName());
	}
}

void USFRecipeManagementService::FinishImplicitSettingsSample(AActor* SampledActor) const
{
	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[#489] MMB settings sample: actor=%s vanillaCopy=%s recipe=%s shards=%d potential=%.0f%% somersloop=%s boost=%.0f%%"),
		*GetNameSafe(SampledActor),
		bCapturedImplicitSettingsThisSample ? TEXT("YES") : TEXT("NO"),
		bCapturedImplicitSettingsThisSample ? *GetNameSafe(StoredProductionRecipe) : TEXT("None"),
		bCapturedImplicitSettingsThisSample ? StoredOverclockShardCount : 0,
		bCapturedImplicitSettingsThisSample && bHasStoredPotential ? StoredPotential * 100.0f : 100.0f,
		bCapturedImplicitSettingsThisSample ? *GetNameSafe(StoredProductionBoostShardClass) : TEXT("None"),
		bCapturedImplicitSettingsThisSample && bHasStoredProductionBoost ? StoredProductionBoost * 100.0f : 100.0f);
}

void USFRecipeManagementService::ClearStoredProductionRecipe()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🧹 ClearStoredProductionRecipe: bHasStoredPotential=%s, bHasStoredProductionBoost=%s, ShardCount=%d"),
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
	SyncSubsystemRecipeState();
	
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

	// [#368] NOTE: intentionally does NOT touch the vanilla build-gun clipboard here. The middle-click
	// sample flow routes through this reset, and clearing the
	// clipboard would wipe the recipe vanilla's own sample just populated (breaking vanilla Ctrl+V).
	// The clipboard is cleared at the deliberate sites instead: ClearAllRecipes (Num0/panel) and the
	// holster path (OnBuildGunUnequipped).

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Cleared stored production recipe"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ No active hologram to apply recipe to"));
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Updated parent hologram %s data registry with recipe %s"), 
				*ParentHologram->GetName(), *GetRecipeDisplayName(ActiveRecipe));
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Cleared parent hologram %s data registry (recipe cleared)"), 
				*ParentHologram->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Could not attach hologram data to parent %s"),
			*ParentHologram->GetName());
	}
}

void USFRecipeManagementService::RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent)
{
	if (!Building || !IsValid(Building))
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("REGISTRY: Cannot register null or invalid building"));
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
	UE_LOG(LogSmartFoundations, Verbose,
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("REGISTRY: No buildings in current placement to apply recipes to"));
		return;
	}
	
	if (!bHasStoredProductionRecipe || !StoredProductionRecipe)
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("REGISTRY: No stored recipe - buildings registered but no recipes to apply (Group %d, %d buildings)"),
			CurrentPlacementGroupID, CurrentPlacementBuildings.Num());
		ClearCurrentPlacement();
		return;
	}
	
	UE_LOG(LogSmartFoundations, Verbose,
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
	
	UE_LOG(LogSmartFoundations, Verbose,
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("OnActorSpawned: Blueprint proxy %s detected"),
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
	
	// [#515-#517] Local SP/listen construction uses the same immutable queue as the
	// dedicated-server spec seam. Capture now; never let a delayed callback consult mutable service state.
	AFGBuildableFactory* FactoryBuilding = Cast<AFGBuildableFactory>(SpawnedActor);
	AFGHologram* ActiveHologram = Subsystem->GetActiveHologram();
	if (FactoryBuilding && ActiveHologram)
	{
		FSFFactorySettingsSnapshot Snapshot;
		CaptureFactorySettingsSnapshot(Snapshot);
		AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(ActiveHologram->GetConstructionInstigator());
		QueueFactorySettingsApplication(FactoryBuilding, Player, Snapshot);
	}

}

void USFRecipeManagementService::ClearCurrentPlacement()
{
	CurrentPlacementBuildings.Empty();
	CurrentPlacementGroupID++;
	
	UE_LOG(LogSmartFoundations, Verbose, 
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Building is invalid - cannot apply/clear recipe"));
		return;
	}
	
	// CRITICAL: Skip recipe changes for blueprint buildings (they retain their preloaded recipes)
	if (bBlueprintProxyRecentlySpawned)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Skipping recipe change for %s - building spawned from blueprint (preserving preloaded recipe)"), 
			*ManufacturerBuilding->GetName());
		return;
	}
	
	// CRITICAL: Check if building has begun play before applying recipe
	if (!ManufacturerBuilding->HasActorBegunPlay())
	{
		if (Recipe)
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Building %s not ready for recipe (HasActorBegunPlay=false) - retrying with longer delay"),
				*ManufacturerBuilding->GetName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Building %s not ready for recipe clear (HasActorBegunPlay=false) - retrying with longer delay"),
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
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("ApplyRecipeDelayed: ❌ Recipe %s is NOT compatible with building %s (class: %s) - skipping application"),
				*Recipe->GetName(),
				*ManufacturerBuilding->GetName(),
				*ManufacturerBuilding->GetClass()->GetName());
			return; // Skip applying incompatible recipe
		}
		
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Building %s is ready - applying recipe %s"), 
			*ManufacturerBuilding->GetName(), *Recipe->GetName());
		
		// Apply the recipe to the building
		ManufacturerBuilding->SetRecipe(Recipe);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Building %s is ready - clearing recipe (user cleared with Num0)"), 
			*ManufacturerBuilding->GetName());
		
		// Clear the recipe from the building
		ManufacturerBuilding->SetRecipe(nullptr);
	}
	
	// Verify the recipe was actually applied or cleared
	TSubclassOf<UFGRecipe> AppliedRecipe = ManufacturerBuilding->GetCurrentRecipe();
	UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: Recipe verification - Expected: %s, Applied: %s"), 
		Recipe ? *Recipe->GetName() : TEXT("NULL"), 
		AppliedRecipe ? *AppliedRecipe->GetName() : TEXT("NULL"));
	
	if (AppliedRecipe == Recipe)
	{
		if (Recipe)
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: ✅ Recipe successfully applied and verified"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: ✅ Recipe successfully cleared and verified"));
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyRecipeDelayed: ❌ Recipe verification failed - expected/applied mismatch"));
	}
}

void USFRecipeManagementService::OnNewBuildSession(UClass* NewBuildClass)
{
	// Only skip the bump if shards are active AND the build class hasn't changed
	// (shard capture syncs both IDs — a bump for the SAME class would undo that sync)
	if (ShardSessionId == CurrentBuildSessionId && (bHasStoredPotential || bHasStoredProductionBoost)
		&& NewBuildClass == ShardSourceBuildClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🏷️ New build session: SKIPPED bump (shards active for same class in session %d)"), CurrentBuildSessionId);
		return;
	}
	
	++CurrentBuildSessionId;
	UE_LOG(LogSmartFoundations, Verbose, TEXT("🏷️ New build session: ID=%d (shard session=%d, match=%s)"),
		CurrentBuildSessionId, ShardSessionId,
		(CurrentBuildSessionId == ShardSessionId) ? TEXT("yes") : TEXT("no"));
}

void USFRecipeManagementService::ClearStoredShardState()
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("🧹 ClearStoredShardState: Clearing shard/somersloop state only (recipe preserved)"));
	StoredPotential = 1.0f;
	StoredProductionBoost = 1.0f;
	bHasStoredPotential = false;
	bHasStoredProductionBoost = false;
	StoredOverclockShardClass = nullptr;
	StoredOverclockShardCount = 0;
	StoredProductionBoostShardClass = nullptr;
	StoredProductionBoostShardCount = 0;
}

void USFRecipeManagementService::GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const
{
	OutCurrentIndex = CurrentRecipeIndex;
	OutTotalRecipes = SortedFilteredRecipes.Num();
}

namespace
{
	constexpr int32 SF_FACTORY_SETTINGS_MAX_ATTEMPTS = 25;
	constexpr float SF_FACTORY_SETTINGS_RETRY_INTERVAL = 0.2f;
}

void USFRecipeManagementService::QueueFactorySettingsApplication(AFGBuildable* TargetBuilding,
	AFGCharacterPlayer* Player, const FSFFactorySettingsSnapshot& Snapshot)
{
	AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(TargetBuilding);
	// [MP-AUTH] Only the construction authority may queue recipe/item mutation.
	if (!IsValid(Factory) || !Factory->HasAuthority() || !Snapshot.HasAnySettings()
		|| !Subsystem || !Subsystem->GetWorld())
	{
		return;
	}

	PendingFactorySettingsApplications.RemoveAllSwap([Factory](const FSFPendingFactorySettingsApplication& Pending)
	{
		return Pending.Factory.Get() == Factory;
	});
	FSFPendingFactorySettingsApplication& Pending = PendingFactorySettingsApplications.AddDefaulted_GetRef();
	Pending.Factory = Factory;
	Pending.Player = Player;
	Pending.Snapshot = Snapshot;

	UWorld* World = Subsystem->GetWorld();
	if (!World->GetTimerManager().IsTimerActive(FactorySettingsApplyTimer))
	{
		World->GetTimerManager().SetTimer(FactorySettingsApplyTimer, this,
			&USFRecipeManagementService::TickPendingFactorySettingsApplications,
			SF_FACTORY_SETTINGS_RETRY_INTERVAL, true, 0.0f);
	}
}

void USFRecipeManagementService::TickPendingFactorySettingsApplications()
{
	for (int32 Index = PendingFactorySettingsApplications.Num() - 1; Index >= 0; --Index)
	{
		FSFPendingFactorySettingsApplication& Pending = PendingFactorySettingsApplications[Index];
		AFGBuildableFactory* Factory = Pending.Factory.Get();
		AFGCharacterPlayer* Player = Pending.Player.Get();
		++Pending.AttemptNumber;
		const bool bNeedsInventory = Pending.Snapshot.NeedsInventoryTransfer();
		const bool bTargetInventoryReady = Factory && Factory->GetPotentialInventory();
		const bool bPlayerInventoryReady = Player && Player->GetInventory();
		const ESFFactorySettingsApplyDecision Decision = FSFFactorySettingsApplyPolicy::Decide(
			IsValid(Factory), Factory && Factory->HasActorBegunPlay(), bNeedsInventory,
			bTargetInventoryReady, bPlayerInventoryReady, Pending.AttemptNumber, SF_FACTORY_SETTINGS_MAX_ATTEMPTS);

		if (Decision == ESFFactorySettingsApplyDecision::Retry)
		{
			continue;
		}
		if (Decision == ESFFactorySettingsApplyDecision::Apply)
		{
			ApplyFactorySettingsSnapshot(Factory, Player, Pending.Snapshot);
		}
		else if (Decision == ESFFactorySettingsApplyDecision::GiveUp)
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[#515-#517] Factory settings readiness timed out for %s after %d attempts."),
				*GetNameSafe(Factory), Pending.AttemptNumber);
		}
		PendingFactorySettingsApplications.RemoveAtSwap(Index);
	}

	if (PendingFactorySettingsApplications.IsEmpty() && Subsystem && Subsystem->GetWorld())
	{
		Subsystem->GetWorld()->GetTimerManager().ClearTimer(FactorySettingsApplyTimer);
	}
}

// Protected accessor to call FillPotentialSlotsInternal on AFGBuildableFactory
// TryFillPotentialInventory silently fails for overclock shards, but the protected
// FillPotentialSlotsInternal takes a shard COUNT, but does not check supply (#524).
// Every caller must cap the target before invoking this add-then-remove helper.
class FFGBuildableFactoryAccessor : public AFGBuildableFactory
{
public:
	using AFGBuildableFactory::FillPotentialSlotsInternal;
};

bool USFRecipeManagementService::ApplyStoredPotentialToBuilding(AFGBuildable* TargetBuilding, AFGCharacterPlayer* Player)
{
	FSFFactorySettingsSnapshot Snapshot;
	CaptureFactorySettingsSnapshot(Snapshot);
	QueueFactorySettingsApplication(TargetBuilding, Player, Snapshot);
	return Snapshot.NeedsInventoryTransfer();
}

bool USFRecipeManagementService::ApplyFactorySettingsSnapshot(AFGBuildableFactory* Factory,
	AFGCharacterPlayer* Player, const FSFFactorySettingsSnapshot& Snapshot)
{
	// [MP-AUTH] Recipe and inventory changes belong only to the construction authority.
	if (!IsValid(Factory) || !Factory->HasAuthority())
	{
		return false;
	}

	bool bAppliedAnything = false;
	if (FSFFactorySettingsApplyPolicy::IsRecipeApplicationAllowed(
		Snapshot.bHasRecipe && Snapshot.Recipe != nullptr,
		IsRecipeCompatibleWithBuilding(Snapshot.Recipe, Factory)))
	{
		if (AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory))
		{
			Manufacturer->SetRecipe(Snapshot.Recipe);
			bAppliedAnything = true;
		}
	}

	UFGInventoryComponent* PlayerInventory = Player ? Player->GetInventory() : nullptr;
	if (!PlayerInventory)
	{
		return bAppliedAnything;
	}

	auto FillShardType = [Factory, Player, PlayerInventory](EPowerShardType Type,
		TSubclassOf<UFGPowerShardDescriptor> ShardClass, int32 RequestedCount) -> int32
	{
		if (!ShardClass || RequestedCount <= 0
			|| !FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(
				Type, UFGPowerShardDescriptor::GetPowerShardType(ShardClass)))
		{
			return 0;
		}

		UFGInventoryComponent* PotentialInventory = Factory->GetPotentialInventory();
		if (!PotentialInventory)
		{
			return 0;
		}

		// #524: CL 502094 FillPotentialSlotsInternal (RVA 0x4E5090) adds to the
		// machine before calling player Remove, without checking player supply.
		// Its ref argument is the TOTAL target, including matching installed shards.
		// Re-budget here for every queued machine, not once for the whole build.
		const int32 ExistingCount = PotentialInventory->GetNumItems(ShardClass);
		const int32 AffordableTarget = FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(
			RequestedCount, ExistingCount, PlayerInventory->GetNumItems(ShardClass));
		if (AffordableTarget <= ExistingCount)
		{
			// No new items can be funded, or the target is already satisfied. Do not
			// let a repeated application displace existing shards unnecessarily.
			return AffordableTarget;
		}
		int32 Remaining = AffordableTarget;
		TArray<FInventoryStack> ItemsToDrop;
		static_cast<FFGBuildableFactoryAccessor*>(Factory)->FillPotentialSlotsInternal(
			PlayerInventory, Type, ShardClass, Remaining, ItemsToDrop);

		for (const FInventoryStack& DisplacedStack : ItemsToDrop)
		{
			if (!DisplacedStack.HasItems())
			{
				continue;
			}

			const int32 AddedToPlayer = PlayerInventory->AddStack(DisplacedStack, true);
			const int32 UnreturnedCount = FSFFactorySettingsApplyPolicy::GetUnreturnedItemCount(
				DisplacedStack.NumItems, AddedToPlayer);
			if (UnreturnedCount > 0)
			{
				FInventoryStack WorldDrop = DisplacedStack;
				WorldDrop.NumItems = UnreturnedCount;
				AFGItemPickup_Spawnable::AddItemToWorldStackAtLocation(
					PlayerInventory, WorldDrop, Player->GetActorLocation(), Player->GetActorRotation());
			}
		}

		return AffordableTarget - Remaining;
	};

	if (Snapshot.bHasPotential && FMath::IsFinite(Snapshot.Potential)
		&& Snapshot.Potential > 1.0f && Factory->GetCanChangePotential())
	{
		const int32 Transferred = FillShardType(EPowerShardType::PST_Overclock,
			Snapshot.OverclockShardClass, FMath::Clamp(Snapshot.OverclockShardCount, 0, 3));
		float PendingValue = 1.0f;
		if (Transferred > 0 && FSFFactorySettingsApplyPolicy::TryResolvePendingValue(
			Snapshot.Potential, Factory->GetCurrentMaxPotential(), PendingValue))
		{
			Factory->SetPendingPotential(PendingValue);
			bAppliedAnything = true;
		}
	}

	if (Snapshot.bHasProductionBoost && FMath::IsFinite(Snapshot.ProductionBoost)
		&& Snapshot.ProductionBoost > 1.0f && Factory->CanChangeProductionBoost())
	{
		const int32 Transferred = FillShardType(EPowerShardType::PST_ProductionBoost,
			Snapshot.ProductionBoostShardClass, FMath::Clamp(Snapshot.ProductionBoostShardCount, 0, 2));
		float PendingValue = 1.0f;
		if (Transferred > 0 && FSFFactorySettingsApplyPolicy::TryResolvePendingValue(
			Snapshot.ProductionBoost, Factory->GetCurrentMaxProductionBoost(), PendingValue))
		{
			Factory->SetPendingProductionBoost(PendingValue);
			bAppliedAnything = true;
		}
	}

	return bAppliedAnything;
}
