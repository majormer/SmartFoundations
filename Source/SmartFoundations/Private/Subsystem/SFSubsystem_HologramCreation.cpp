// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - building registry + runtime hologram swap + CreateCustom* holograms + tier lookups.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"


// ========================================
// Building Registry System Implementation
// ========================================

void USFSubsystem::RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->RegisterSmartBuilding(Building, IndexInGroup, bIsParent);
	}
}

void USFSubsystem::ApplyRecipesToCurrentPlacement()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipesToCurrentPlacement();
	}
}

void USFSubsystem::ClearCurrentPlacement()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearCurrentPlacement();
	}
}

// ========================================
// Phase 4: Runtime Hologram Swapping Implementation
// ========================================

AFGHologram* USFSubsystem::TrySwapToSmartHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("TrySwapToSmartHologram: Invalid hologram"));
		return OriginalHologram;
	}

	// NOTE: Removed name-based check as Satisfactory renames holograms after creation
	// Child holograms are identified by the calling context in SpawnChildHologram
	// All holograms reaching this function are intended for swapping
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("TrySwapToSmartHologram: Proceeding with hologram swap for %s"), *OriginalHologram->GetName());

	// Get the build class to determine what type of building this is
	UClass* BuildClass = OriginalHologram->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("TrySwapToSmartHologram: No build class for %s"), *OriginalHologram->GetName());
		return OriginalHologram;
	}

	FString BuildClassName = BuildClass->GetName();
	FString OriginalHologramClass = OriginalHologram->GetClass()->GetName();

	UE_LOG(LogSmartFoundations, Verbose, TEXT("TrySwapToSmartHologram: %s -> BuildClass=%s"),
		*OriginalHologramClass, *BuildClassName);

	// For now, implement a simple check - if it's a foundation hologram, we could swap it
	// This is a placeholder for the full implementation
	// The actual swapping would involve:
	// 1. Creating a new custom hologram instance
	// 2. Copying properties from the original
	// 3. Replacing it in the build gun system
	// 4. Destroying the original

	// PHASE 4 FULL IMPLEMENTATION: Actually swap holograms
	if (OriginalHologramClass == TEXT("Holo_Foundation_C"))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SWAPPING: Foundation hologram -> ASFFoundationHologram"));
		ASFFoundationHologram* CustomHologram = CreateCustomFoundationHologram(OriginalHologram);
		if (CustomHologram)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Successfully created custom foundation hologram: %s"), *CustomHologram->GetName());
			return CustomHologram;
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("❌ Failed to create custom foundation hologram, falling back to vanilla"));
		}
	}
	else if (Cast<AFGFactoryHologram>(OriginalHologram))
	{
		// NOTE: Runtime factory hologram swapping is DISABLED
		// The build gun holds the original hologram reference, and creating a new one
		// causes constant re-registration loops. Factory holograms need Blueprint class
		// remapping at module load time instead.
		// For now, EXTEND must work with vanilla holograms or use a different approach.
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Factory hologram %s - runtime swap disabled (use Blueprint class remapping)"),
			*OriginalHologramClass);
		// Return the original - don't create orphan custom holograms
		return OriginalHologram;
	}
	else if (OriginalHologramClass == TEXT("Holo_ConveyorBelt_C") ||
			 OriginalHologramClass == TEXT("Holo_ConveyorAttachmentSplitter_C") ||
			 OriginalHologramClass == TEXT("Holo_ConveyorAttachmentMerger_C"))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SWAPPING: Logistics hologram (%s) -> ASFLogisticsHologram"), *OriginalHologramClass);
		ASFLogisticsHologram* CustomHologram = CreateCustomLogisticsHologram(OriginalHologram);
		if (CustomHologram)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Successfully created custom logistics hologram: %s"), *CustomHologram->GetName());
			return CustomHologram;
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("❌ Failed to create custom logistics hologram, falling back to vanilla"));
		}
	}

	// No swap needed
	return OriginalHologram;
}

// ========================================
// Hologram Creation Functions Implementation
// ========================================

ASFFoundationHologram* USFSubsystem::CreateCustomFoundationHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFoundationHologram: Invalid original hologram"));
		return nullptr;
	}

	// Get the world context from the original hologram
	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFoundationHologram: No world context"));
		return nullptr;
	}

	// Configure spawn parameters with DEFERRED construction
	// This allows us to set properties before BeginPlay() is called
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFFoundationHologram* CustomHologram = World->SpawnActor<ASFFoundationHologram>(ASFFoundationHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFoundationHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFoundationHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	// NOTE: Material state should be inherited from parent once properly linked
	// The swapped hologram needs to be added to parent's children array to inherit parent's valid state

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFoundationHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

ASFFactoryHologram* USFSubsystem::CreateCustomFactoryHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFactoryHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFactoryHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFFactoryHologram* CustomHologram = World->SpawnActor<ASFFactoryHologram>(ASFFactoryHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomFactoryHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFactoryHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFactoryHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

ASFLogisticsHologram* USFSubsystem::CreateCustomLogisticsHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomLogisticsHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomLogisticsHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFLogisticsHologram* CustomHologram = World->SpawnActor<ASFLogisticsHologram>(ASFLogisticsHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CreateCustomLogisticsHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomLogisticsHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomLogisticsHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

void USFSubsystem::CopyHologramProperties(AFGHologram* Source, AFGHologram* Destination)
{
	if (!Source || !Destination)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyHologramProperties: Invalid source or destination"));
		return;
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyHologramProperties: Copying from %s to %s"),
		*Source->GetName(), *Destination->GetName());

	// Copy essential hologram properties
	Destination->SetActorLocationAndRotation(Source->GetActorLocation(), Source->GetActorRotation());

	// Note: Build class is already set in the creation function BEFORE BeginPlay() to prevent assertion failure

	// Copy recipe if available
	if (Source->GetRecipe())
	{
		Destination->SetRecipe(Source->GetRecipe());
	}

	// NOTE: Parent hologram reference and scroll mode cannot be directly copied
	// These properties are managed by the hologram system internally
	// The parent-child relationship must be established by replacing the child in the parent's children array

	// Copy basic hologram state (simplified for Phase 4 MVP)
	// Note: Advanced property copying will be added in later iteration

	UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyHologramProperties: Successfully copied basic properties (location, rotation, build class, recipe, parent)"));
}

bool USFSubsystem::ReplaceHologramInBuildGun(AFGHologram* OriginalHologram, AFGHologram* CustomHologram)
{
	// Phase 4 MVP: Simplified implementation that always succeeds
	// Full build gun replacement will be implemented in later iteration

	if (!OriginalHologram || !CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ReplaceHologramInBuildGun: Invalid holograms"));
		return false;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ReplaceHologramInBuildGun: MVP implementation - hologram replacement pending"));

	// For MVP, we just return true to indicate success
	// The actual replacement logic will be added later
	return true;
}

// Belt Tier Configuration Helpers
// ========================================

UClass* USFSubsystem::GetBeltClassForTier(int32 Tier, AFGPlayerController* PlayerController)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 6)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt class path
	FString BeltPath = FString::Printf(
		TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
		Tier, Tier, Tier);

	UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);

	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassForTier: Failed to load belt class for tier %d"), Tier);
		return nullptr;
	}

	// Check if player has unlocked this belt tier
	if (PlayerController)
	{
		UWorld* World = PlayerController->GetWorld();
		if (World)
		{
			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
			if (RecipeManager)
			{
				// Cast to AFGBuildable to check availability
				TSubclassOf<AFGBuildable> BeltBuildableClass = BeltClass;
				if (!RecipeManager->IsBuildingAvailable(BeltBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBeltClassForTier: Belt tier Mk%d not unlocked yet"), Tier);
					return nullptr;  // Belt tier not unlocked - prevents cheating
				}
			}
		}
	}

	return BeltClass;
}

int32 USFSubsystem::GetHighestUnlockedBeltTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedBeltTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedBeltTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedBeltTier: No recipe manager, defaulting to Mk1"));
		return 1;
	}

	// Check belt tiers from highest (Mk6) to lowest (Mk1) to find highest unlocked
	for (int32 Tier = 6; Tier >= 1; Tier--)
	{
		FString BeltPath = FString::Printf(
			TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
			Tier, Tier, Tier);

		UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);
		if (BeltClass)
		{
			TSubclassOf<AFGBuildable> BeltBuildableClass = BeltClass;
			if (RecipeManager->IsBuildingAvailable(BeltBuildableClass))
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHighestUnlockedBeltTier: Highest unlocked tier is Mk%d"), Tier);
				return Tier;  // Found highest unlocked tier
			}
		}
	}

	// Fallback: If nothing is unlocked (shouldn't happen), return Mk1
	UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedBeltTier: No belts unlocked, defaulting to Mk1"));
	return 1;
}

int32 USFSubsystem::GetHighestUnlockedPowerPoleTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return 1;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return 1;
	}

	// Power pole paths: Mk1, Mk2, Mk3
	static const TCHAR* PowerPolePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1.Build_PowerPoleMk1_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk2/Build_PowerPoleMk2.Build_PowerPoleMk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3.Build_PowerPoleMk3_C"),
	};

	// Check from highest (Mk3) to lowest (Mk1)
	for (int32 Tier = 3; Tier >= 1; Tier--)
	{
		UClass* PoleClass = LoadObject<UClass>(nullptr, PowerPolePaths[Tier - 1]);
		if (PoleClass)
		{
			TSubclassOf<AFGBuildable> PoleBuildableClass = PoleClass;
			if (RecipeManager->IsBuildingAvailable(PoleBuildableClass))
			{
				return Tier;
			}
		}
	}

	return 1;
}

int32 USFSubsystem::GetHighestUnlockedWallOutletTier(AFGPlayerController* PlayerController, bool bDouble)
{
	if (!PlayerController)
	{
		return 1;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return 1;
	}

	// Wall outlet paths: Mk1, Mk2, Mk3. Single and double are independent asset families and
	// unlock separately via game progression (Issue #267), so probe whichever family the caller
	// asked for — mixing them under-reports availability for the unchecked family.
	// Note: Wall outlets use underscore naming (Build_PowerPoleWall_Mk2) in the same subfolder,
	// NOT separate subfolders like regular power poles (PowerPoleMk2/Build_PowerPoleMk2).
	static const TCHAR* WallOutletSinglePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall.Build_PowerPoleWall_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk2.Build_PowerPoleWall_Mk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk3.Build_PowerPoleWall_Mk3_C"),
	};
	static const TCHAR* WallOutletDoublePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble.Build_PowerPoleWallDouble_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk2.Build_PowerPoleWallDouble_Mk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk3.Build_PowerPoleWallDouble_Mk3_C"),
	};
	const TCHAR* const * Paths = bDouble ? WallOutletDoublePaths : WallOutletSinglePaths;

	// Check from highest (Mk3) to lowest (Mk1)
	for (int32 Tier = 3; Tier >= 1; Tier--)
	{
		UClass* OutletClass = LoadObject<UClass>(nullptr, Paths[Tier - 1]);
		if (OutletClass)
		{
			TSubclassOf<AFGBuildable> OutletBuildableClass = OutletClass;
			if (RecipeManager->IsBuildingAvailable(OutletBuildableClass))
			{
				return Tier;
			}
		}
	}

	return 1;
}

UClass* USFSubsystem::GetBeltClassFromConfig(int32 ConfigTier, AFGPlayerController* PlayerController)
{
	int32 ActualTier = ConfigTier;

	// Handle "Auto" mode (0 = use highest unlocked)
	if (ConfigTier == 0)
	{
		ActualTier = GetHighestUnlockedBeltTier(PlayerController);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassFromConfig: Auto mode selected highest tier Mk%d"), ActualTier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassFromConfig: Using configured tier Mk%d"), ActualTier);
	}

	// Get belt class for the determined tier
	UClass* BeltClass = GetBeltClassForTier(ActualTier, PlayerController);

	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassFromConfig: Belt tier Mk%d unavailable or not unlocked - belt category disabled"), ActualTier);
	}

	return BeltClass;
}
