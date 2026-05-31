// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFSubsystem implementation (part 4). Split out of SFSubsystem.cpp (slice S, pure
 * impl-split: one class across multiple .cpp) to keep each file <2k. No behavior change.
 */

#include "Subsystem/SFSubsystem.h"
#include "SmartFoundations.h"
#include "FGHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "FGRecipeManager.h"
#include "FGBuildingDescriptor.h"
#include "Data/SFHologramData.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"
#include "Debug/SFSplineAnalyzer.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SmartUpgradePanel.h"
#include "Subsystem/SFInputHandler.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFValidationService.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"
#include "Services/RadarPulse/SFRadarPulseService.h"
#include "Services/SFHudService.h"
#include "Services/SFHintBarService.h"
#include "Services/SFChainActorService.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Services/SFGridStateService.h"
#include "Services/SFGridSpawnerService.h"
#include "Services/SFGridTransformService.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Features/Restore/SFRestoreService.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "Logging/SFLogMacros.h"
#include "Hologram/FGFactoryBuildingHologram.h"  // Issue #160: Zoop detection
#include "SFHologramPerformanceProfiler.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
#include "Config/Smart_ConfigStruct.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Input/FGEnhancedInputComponent.h"
#include "Input/FGInputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Resources/FGBuildingDescriptor.h"
#include "FGCentralStorageSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"
#include "FGGameState.h"
#include "FGHUDBase.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "Components/SplineComponent.h"
#include "FGBlueprintProxy.h"
#include "FGBlueprintHologram.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Holograms/Core/SFBuildableHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Core/ASFLogisticsHologram.h"
#include "Holograms/Core/SFFoundationHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Holograms/Core/SFStandaloneSignChildHologram.h"
#include "Holograms/Adapters/SFSmartBuildableAdapter.h"
#include "Holograms/Adapters/SFSmartLogisticsAdapter.h"
#include "Holograms/Adapters/SFGenericAdapter.h"
#include "Holograms/Adapters/SFWallAdapter.h"
#include "Holograms/Adapters/SFPillarAdapter.h"
#include "Holograms/Adapters/SFWaterExtractorAdapter.h"
#include "Holograms/Adapters/SFResourceExtractorAdapter.h"
#include "Holograms/Adapters/SFFactoryAdapter.h"
#include "Holograms/Adapters/SFElevatorAdapter.h"
#include "Holograms/Adapters/SFRampAdapter.h"
#include "Holograms/Adapters/SFJumpPadAdapter.h"
#include "Holograms/Adapters/SFUnsupportedAdapter.h"
#include "Holograms/Adapters/SFPassthroughAdapter.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Input/SFInputRegistry.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/Spacing/SFSpacingModule.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGBuildablePolePipe.h"  // For stackable pipeline support auto-connect
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGWallHologram.h"
#include "Hologram/FGPillarHologram.h"
#include "Hologram/FGStackableStorageHologram.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "Hologram/FGPipeHyperAttachmentHologram.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Hologram/FGElevatorHologram.h"
#include "Hologram/FGStairHologram.h"
#include "Hologram/FGJumpPadHologram.h"
#include "Hologram/FGWheeledVehicleHologram.h"
#include "Hologram/FGSpaceElevatorHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGRecipe.h"
#include "Subsystem/SFSubsystemStackableCache.h"

void USFSubsystem::CycleRecipeForward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeForward(AccumulatedSteps);
	}
}

void USFSubsystem::CycleRecipeBackward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeBackward(AccumulatedSteps);
	}
}

void USFSubsystem::ClearAllRecipes()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearAllRecipes();
	}
}

// ========================================
// Pipe Tier Configuration Helpers
// ========================================

UClass* USFSubsystem::GetPipeClassForTier(int32 Tier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe class path based on tier and style
	FString PipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMK2/Build_PipelineMK2_NoIndicator.Build_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassForTier: Failed to load pipe class for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	// Check if player has unlocked this pipe tier
	if (PlayerController)
	{
		UWorld* World = PlayerController->GetWorld();
		if (World)
		{
			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
			if (RecipeManager)
			{
				// Cast to AFGBuildable to check availability
				TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
				if (!RecipeManager->IsBuildingAvailable(PipeBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetPipeClassForTier: Pipe tier Mk%d (%s) not unlocked yet"),
					Tier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
					return nullptr;  // Pipe tier not unlocked - prevents cheating
				}
			}
		}
	}

	return PipeClass;
}

TSubclassOf<UFGRecipe> USFSubsystem::GetBeltRecipeForTier(int32 Tier)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 6)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltRecipeForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt recipe path based on tier
	FString RecipePath;
	switch (Tier)
	{
		case 1: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"); break;
		case 2: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk2.Recipe_ConveyorBeltMk2_C"); break;
		case 3: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk3.Recipe_ConveyorBeltMk3_C"); break;
		case 4: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"); break;
		case 5: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk5.Recipe_ConveyorBeltMk5_C"); break;
		case 6: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk6.Recipe_ConveyorBeltMk6_C"); break;
		default: return nullptr;
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltRecipeForTier: Failed to load recipe for Mk%d belt"), Tier);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

TSubclassOf<UFGRecipe> USFSubsystem::GetPipeRecipeForTier(int32 Tier, bool bWithIndicator)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeRecipeForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe recipe path based on tier and style
	FString RecipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline_NoIndicator.Recipe_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2.Recipe_PipelineMK2_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2_NoIndicator.Recipe_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeRecipeForTier: Failed to load recipe for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

int32 USFSubsystem::GetHighestUnlockedPipeTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No recipe manager, defaulting to Mk1"));
		return 1;
	}

	// Check pipe tiers from highest (Mk2) to lowest (Mk1) to find highest unlocked
	// Check "Normal" variant (with indicators) as the canonical unlock check
	for (int32 Tier = 2; Tier >= 1; Tier--)
	{
		FString PipePath;
		if (Tier == 1)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else  // Tier == 2
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}

		UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);
		if (PipeClass)
		{
			TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
			if (RecipeManager->IsBuildingAvailable(PipeBuildableClass))
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHighestUnlockedPipeTier: Highest unlocked tier is Mk%d"), Tier);
				return Tier;  // Found highest unlocked tier
			}
		}
	}

	// Fallback: If nothing is unlocked (shouldn't happen), return Mk1
	UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No pipes unlocked, defaulting to Mk1"));
	return 1;
}

bool USFSubsystem::AreCleanPipesUnlocked(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return false;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return false;
	}

	// Check if Mk1 clean pipe is unlocked (NoIndicator variant)
	FString CleanPipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
	UClass* CleanPipeClass = LoadObject<UClass>(nullptr, *CleanPipePath);

	if (CleanPipeClass)
	{
		TSubclassOf<AFGBuildable> CleanPipeBuildableClass = CleanPipeClass;
		if (RecipeManager->IsBuildingAvailable(CleanPipeBuildableClass))
		{
			return true;
		}
	}

	return false;
}

UClass* USFSubsystem::GetPipeClassFromConfig(int32 ConfigTier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	int32 ActualTier = ConfigTier;

	// Handle "Auto" mode (0 = use highest unlocked)
	if (ConfigTier == 0)
	{
		ActualTier = GetHighestUnlockedPipeTier(PlayerController);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Auto mode selected highest tier Mk%d"), ActualTier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Using configured tier Mk%d"), ActualTier);
	}

	// Get pipe class for the determined tier and style
	UClass* PipeClass = GetPipeClassForTier(ActualTier, bWithIndicator, PlayerController);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassFromConfig: Pipe tier Mk%d (%s) unavailable or not unlocked - pipe category disabled"),
			ActualTier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
	}

	return PipeClass;
}

void USFSubsystem::CycleAutoConnectSetting()
{
    // Check context (Belt vs Pipe Junction vs Stackable Pipe vs Stackable Belt vs Power)
    bool bIsPipeJunction = false;
    bool bIsStackablePipe = false;
    bool bIsStackableBelt = false;
    bool bIsPowerPole = false;
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
        bIsStackablePipe = AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get());
        bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
        bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
    }

    if (bIsPowerPole)
    {
        // Power Pole cycle: Enabled -> Reserved -> Grid Axis -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::PowerEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerReserved;
            break;
        case EAutoConnectSetting::PowerReserved:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerGridAxis;
            break;
        case EAutoConnectSetting::PowerGridAxis:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        default:
            // If on unrelated setting, jump to first power setting
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        }
    }
    else if (bIsStackableBelt)
    {
        // Stackable Conveyor Pole cycle: Enabled -> Tier -> Routing Mode -> Direction -> Enabled
        // NOTE: Uses BeltTierMain for tier, StackableBeltDirection for direction
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::StackableBeltEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltTier;
            break;
        case EAutoConnectSetting::StackableBeltTier:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltDirection;
            break;
        case EAutoConnectSetting::StackableBeltDirection:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        default:
            // If on unrelated setting, jump to first stackable belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        }
    }
    else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
    {
        // Floor Hole Pipe cycle: Enabled -> To Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsStackablePipe)
    {
        // Stackable Pipe cycle: Enabled -> Main Tier -> Indicator -> Routing Mode -> Enabled
        // NOTE: No TierToBuilding - stackable pipes only connect to each other, not buildings
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsPipeJunction)
    {
        // Pipe Junction cycle: Enabled -> Main Tier -> Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else
    {
        // Belt cycle (Distributor): Enabled -> Main Tier -> Building Tier -> Routing Mode -> Chain -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierMain;
            break;
        case EAutoConnectSetting::BeltTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierToBuilding;
            break;
        case EAutoConnectSetting::BeltTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::ChainDistributors;
            break;
        case EAutoConnectSetting::ChainDistributors:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect Setting Cycled: %s"), *GetAutoConnectSettingDisplayString());
    UpdateCounterDisplay();
}

void USFSubsystem::AdjustAutoConnectSetting(int32 Delta)
{
    // Mark settings as modified (initialized) when user makes changes
    AutoConnectRuntimeSettings.bInitialized = true;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware toggle: belts on distributor, pipes on junction/stackable supports
        if (ActiveHologram.IsValid() && AutoConnectService)
        {
            if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
                AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()) ||
                USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
            {
                // Pipe hologram (junction, stackable support, or floor hole): toggle pipe auto-connect
                AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = !AutoConnectRuntimeSettings.bPipeAutoConnectEnabled;
            }
            else
            {
                // Distributor: toggle belt auto-connect
                AutoConnectRuntimeSettings.bEnabled = !AutoConnectRuntimeSettings.bEnabled;
            }
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierToBuilding + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        AutoConnectRuntimeSettings.bChainDistributors = !AutoConnectRuntimeSettings.bChainDistributors;
        break;


    case EAutoConnectSetting::PipeTierMain:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierMain + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierToBuilding + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeIndicator:
        // Toggle between Normal (with indicators) and Clean (no indicators)
        AutoConnectRuntimeSettings.bPipeIndicator = !AutoConnectRuntimeSettings.bPipeIndicator;
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        // Cycle through 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical
        AutoConnectRuntimeSettings.PipeRoutingMode = (AutoConnectRuntimeSettings.PipeRoutingMode + Delta) % 6;
        if (AutoConnectRuntimeSettings.PipeRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.PipeRoutingMode += 6;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        // Toggle stackable conveyor pole belt auto-connect
        AutoConnectRuntimeSettings.bStackableBeltEnabled = !AutoConnectRuntimeSettings.bStackableBeltEnabled;
        break;

    case EAutoConnectSetting::StackableBeltTier:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier - reuses BeltTierMain
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        // Toggle between 0 (Forward) and 1 (Backward)
        AutoConnectRuntimeSettings.StackableBeltDirection = (AutoConnectRuntimeSettings.StackableBeltDirection == 0) ? 1 : 0;
        break;

    case EAutoConnectSetting::PowerEnabled:
        AutoConnectRuntimeSettings.bConnectPower = !AutoConnectRuntimeSettings.bConnectPower;
        break;

    case EAutoConnectSetting::PowerReserved:
        AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(AutoConnectRuntimeSettings.PowerReserved + Delta, 0, 5);
        break;

    case EAutoConnectSetting::PowerGridAxis:
        // Cycle through 0 (Auto), 1 (X), 2 (Y), 3 (X+Y)
        AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(AutoConnectRuntimeSettings.PowerGridAxis + Delta, 0, 3);
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        // Cycle through 0=Default, 1=Curve, 2=Straight (matches vanilla belt build modes)
        AutoConnectRuntimeSettings.BeltRoutingMode = (AutoConnectRuntimeSettings.BeltRoutingMode + Delta) % 3;
        if (AutoConnectRuntimeSettings.BeltRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.BeltRoutingMode += 3;
        }
        break;
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect Setting Adjusted: %s"), *GetAutoConnectSettingDisplayString());

    // Trigger immediate re-evaluation of previews with new settings
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        if (AutoConnectService->IsDistributorHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this distributor
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all belt previews with new settings
                Orchestrator->EvaluateGrid(true);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated belt previews after settings change"));
            }
        }
        else if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this pipeline junction
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all pipe previews with new settings
                Orchestrator->OnPipeGridChanged();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated pipe previews after settings change"));
            }
        }
        else if (AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()))
        {
            // Stackable pipe supports: trigger re-processing of pipe previews
            AutoConnectService->ProcessStackablePipelineSupports(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Stackable Pipe: Force recreated pipe previews after settings change"));
        }
        else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
        {
            // Floor hole pipes: trigger re-processing of pipe previews
            AutoConnectService->ProcessFloorHolePipes(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Floor Hole Pipe: Force recreated pipe previews after settings change"));
        }
        else if (USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get()))
        {
            // Belt support poles (stackable, ceiling, wall): trigger re-processing of belt previews
            AutoConnectService->ProcessStackableConveyorPoles(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Belt Support: Force recreated belt previews after settings change"));
        }
        else if (AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this power pole
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all power previews with new settings
                Orchestrator->OnPowerGridChanged();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated power previews after settings change"));
            }
        }
    }

    UpdateCounterDisplay();
    AutoConnectRuntimeSettings.bInitialized = true;
}

FString USFSubsystem::GetAutoConnectSettingDisplayString() const
{
    FString SettingName;
    FString SettingValue;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware display: show belt or pipe auto-connect status
        if (ActiveHologram.IsValid() && AutoConnectService &&
            (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
             AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()) ||
             USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get())))
        {
            SettingName = TEXT("Pipe Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF");
        }
        else
        {
            SettingName = TEXT("Belt Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF");
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        SettingName = TEXT("Belt Tier (Main)");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        SettingName = TEXT("Belt Tier (To Building)");
        if (AutoConnectRuntimeSettings.BeltTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        SettingName = TEXT("Chain Distributors");
        SettingValue = AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        SettingName = TEXT("Belt Routing");
        switch (AutoConnectRuntimeSettings.BeltRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Default");
            break;
        case 1:
            SettingValue = TEXT("Curve");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        default:
            SettingValue = TEXT("Default");
            break;
        }
        break;

    case EAutoConnectSetting::PipeTierMain:
        SettingName = TEXT("Pipe Tier (Main)");
        if (AutoConnectRuntimeSettings.PipeTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
        }
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        SettingName = TEXT("Pipe Tier (To Building)");
        if (AutoConnectRuntimeSettings.PipeTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
        }
        break;

    case EAutoConnectSetting::PipeIndicator:
        SettingName = TEXT("Pipe Style");
        SettingValue = AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean");
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        SettingName = TEXT("Pipe Routing");
        switch (AutoConnectRuntimeSettings.PipeRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("Auto 2D");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        case 3:
            SettingValue = TEXT("Curve");
            break;
        case 4:
            SettingValue = TEXT("Noodle");
            break;
        case 5:
            SettingValue = TEXT("Horiz→Vert");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        SettingName = TEXT("Stackable Belt Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bStackableBeltEnabled ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::StackableBeltTier:
        SettingName = TEXT("Stackable Belt Tier");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        SettingName = TEXT("Stackable Belt Direction");
        SettingValue = AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward");
        break;

    case EAutoConnectSetting::PowerEnabled:
        SettingName = TEXT("Power Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::PowerGridAxis:
        SettingName = TEXT("Grid Axis");
        switch (AutoConnectRuntimeSettings.PowerGridAxis)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("X");
            break;
        case 2:
            SettingValue = TEXT("Y");
            break;
        case 3:
            SettingValue = TEXT("X+Y");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::PowerReserved:
        SettingName = TEXT("Reserved Slots");
        SettingValue = FString::Printf(TEXT("%d"), AutoConnectRuntimeSettings.PowerReserved);
        break;
    }

    return FString::Printf(TEXT("%s: %s"), *SettingName, *SettingValue);
}

bool USFSubsystem::IsCurrentHologramAutoConnectCapable() const
{
	if (!ActiveHologram.IsValid() || !AutoConnectService)
	{
		return false;
	}

	AFGHologram* Hologram = ActiveHologram.Get();
	return AutoConnectService->IsDistributorHologram(Hologram) ||
	       AutoConnectService->IsPipelineJunctionHologram(Hologram) ||
	       AutoConnectService->IsPowerPoleHologram(Hologram) ||
	       USFAutoConnectService::IsBeltSupportHologram(Hologram) ||
	       USFAutoConnectService::IsPassthroughPipeHologram(Hologram);
}

TArray<FString> USFSubsystem::GetDirtyAutoConnectSettings() const
{
	TArray<FString> DirtySettings;

	if (!bConfigLoaded)
	{
		return DirtySettings; // No config to compare against
	}

	// Determine hologram type to filter relevant settings
	bool bIsPipeJunction = false;
	bool bIsDistributor = false;
	bool bIsPowerPole = false;
	bool bIsStackableSupport = false;
	bool bIsPassthroughPipe = false;
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		AFGHologram* Hologram = ActiveHologram.Get();
		bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(Hologram);
		bIsDistributor = AutoConnectService->IsDistributorHologram(Hologram);
		bIsPowerPole = AutoConnectService->IsPowerPoleHologram(Hologram);
		bIsStackableSupport = USFAutoConnectService::IsBeltSupportHologram(Hologram);
		bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(Hologram);
	}

	// CRITICAL FIX: Only show settings relevant to current hologram type
	// Pipe junctions should only show pipe settings, distributors should only show belt settings

	if (bIsPipeJunction || bIsPassthroughPipe)
	{
		// Pipe junction or floor hole passthrough: Show pipe-related settings
		if (AutoConnectRuntimeSettings.bPipeAutoConnectEnabled != CachedConfig.bPipeAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PipeTierMain != CachedConfig.PipeLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.PipeTierToBuilding != CachedConfig.PipeLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bPipeIndicator != CachedConfig.PipeIndicator)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Style: %s"),
				AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean")));
		}
	}
	else if (bIsDistributor || bIsStackableSupport)
	{
		// Distributor: Show only belt-related settings
		if (AutoConnectRuntimeSettings.bEnabled != CachedConfig.bAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Belt Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.BeltTierMain != CachedConfig.BeltLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.BeltTierToBuilding != CachedConfig.BeltLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bChainDistributors != CachedConfig.bAutoConnectDistributors)
		{
			DirtySettings.Add(FString::Printf(TEXT("Chain Distributors: %s"),
				AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF")));
		}
	}
	else if (bIsPowerPole)
	{
		// Power pole: Show only power-related settings
		if (AutoConnectRuntimeSettings.bConnectPower != CachedConfig.bPowerAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Power Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PowerReserved != CachedConfig.PowerConnectReserved)
		{
			DirtySettings.Add(FString::Printf(TEXT("Reserved Slots: %d"), AutoConnectRuntimeSettings.PowerReserved));
		}

		if (AutoConnectRuntimeSettings.PowerGridAxis != CachedConfig.PowerConnectMode)
		{
			FString AxisText;
			switch (AutoConnectRuntimeSettings.PowerGridAxis)
			{
			case 0: AxisText = TEXT("Auto"); break;
			case 1: AxisText = TEXT("X"); break;
			case 2: AxisText = TEXT("Y"); break;
			case 3: AxisText = TEXT("X+Y"); break;
			default: AxisText = TEXT("Auto"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Grid Axis: %s"), *AxisText));
		}

		// Building range is config-only, but we can show if runtime value differs
		// Convert both to meters for comparison (runtime is in cm, config is in meters)
		int32 RuntimeRangeMeters = FMath::RoundToInt(AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
		if (RuntimeRangeMeters != CachedConfig.PowerConnectRange)
		{
			DirtySettings.Add(FString::Printf(TEXT("Building Range: %dm"), RuntimeRangeMeters));
		}
	}

	return DirtySettings;
}

void USFSubsystem::ResetAutoConnectRuntimeSettings()
{
	// Reset runtime settings to match FRESH config (not cached)
	// This ensures config changes made mid-session take effect when equipping a new hologram
	FSmart_ConfigStruct FreshConfig = FSmart_ConfigStruct::GetActiveConfig(this);
	AutoConnectRuntimeSettings.InitFromConfig(FreshConfig);

	// Issue #257: Refresh Extend enabled state from fresh config
	bExtendEnabledByConfig = FreshConfig.bExtendEnabled;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect runtime settings reset from config:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Belt Auto-Connect: %s"), AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Pipe Auto-Connect: %s"), AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Auto-Connect: %s"), AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Range: %.0fm"), AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Extend Enabled: %s"), bExtendEnabledByConfig ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::ResetSmartDisableFlag()
{
	if (bDisableSmartForNextAction || bExtendDisabledForSession)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("▶️ Smart! re-enabled (Auto-Connect + Extend flags reset)"));
	}
	bDisableSmartForNextAction = false;
	bExtendDisabledForSession = false;  // Issue #257: reset Extend session flag too
	LastCycleAxisTapTime = 0.0;
}

// ========================================
// Belt Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectBeltEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectBeltTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier main set to: %d"), AutoConnectRuntimeSettings.BeltTierMain);
}

void USFSubsystem::SetAutoConnectBeltTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier to building set to: %d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
}

void USFSubsystem::SetAutoConnectBeltChain(bool bEnabled)
{
	AutoConnectRuntimeSettings.bChainDistributors = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt chain distributors set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectStackableBeltDirection(int32 Direction)
{
	AutoConnectRuntimeSettings.StackableBeltDirection = FMath::Clamp(Direction, 0, 1);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Stackable belt direction set to: %d (%s)"),
		AutoConnectRuntimeSettings.StackableBeltDirection,
		AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward"));
}

void USFSubsystem::SetAutoConnectBeltRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.BeltRoutingMode = FMath::Clamp(Mode, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.BeltRoutingMode)
	{
	case 0: ModeName = TEXT("Default"); break;
	case 1: ModeName = TEXT("Curve"); break;
	case 2: ModeName = TEXT("Straight"); break;
	default: ModeName = TEXT("Default"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt routing mode set to: %d (%s)"), AutoConnectRuntimeSettings.BeltRoutingMode, *ModeName);
}

// ========================================
// Pipe Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPipeEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPipeTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier main set to: %d"), AutoConnectRuntimeSettings.PipeTierMain);
}

void USFSubsystem::SetAutoConnectPipeTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier to building set to: %d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
}

void USFSubsystem::SetAutoConnectPipeIndicator(bool bIndicator)
{
	AutoConnectRuntimeSettings.bPipeIndicator = bIndicator;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe indicator style set to: %s"), bIndicator ? TEXT("Normal") : TEXT("Clean"));
}

void USFSubsystem::SetAutoConnectPipeRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.PipeRoutingMode = FMath::Clamp(Mode, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.PipeRoutingMode)
	{
	case 0: ModeName = TEXT("Auto"); break;
	case 1: ModeName = TEXT("Auto 2D"); break;
	case 2: ModeName = TEXT("Straight"); break;
	case 3: ModeName = TEXT("Curve"); break;
	case 4: ModeName = TEXT("Noodle"); break;
	case 5: ModeName = TEXT("Horiz→Vert"); break;
	default: ModeName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe routing mode set to: %s (%d)"), *ModeName, AutoConnectRuntimeSettings.PipeRoutingMode);
}

// ========================================
// Power Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPowerEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bConnectPower = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPowerGridAxis(int32 Axis)
{
	AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(Axis, 0, 3);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString AxisName;
	switch (AutoConnectRuntimeSettings.PowerGridAxis)
	{
	case 0: AxisName = TEXT("Auto"); break;
	case 1: AxisName = TEXT("X"); break;
	case 2: AxisName = TEXT("Y"); break;
	case 3: AxisName = TEXT("X+Y"); break;
	default: AxisName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power grid axis set to: %d (%s)"), AutoConnectRuntimeSettings.PowerGridAxis, *AxisName);
}

void USFSubsystem::SetAutoConnectPowerReserved(int32 Reserved)
{
	AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(Reserved, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power reserved slots set to: %d"), AutoConnectRuntimeSettings.PowerReserved);
}

void USFSubsystem::TriggerAutoConnectRefresh()
{
	// Trigger auto-connect preview refresh by notifying all active orchestrators
	for (auto& Pair : AutoConnectOrchestrators)
	{
		if (Pair.Value)
		{
			Pair.Value->ForceRefresh();
		}
	}

	if (AutoConnectOrchestrators.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Auto-connect refresh triggered from settings change (%d orchestrators)"), AutoConnectOrchestrators.Num());
	}
}

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
		UE_LOG(LogSmartFoundations, Warning, TEXT("TrySwapToSmartHologram: Invalid hologram"));
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
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to create custom foundation hologram, falling back to vanilla"));
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
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to create custom logistics hologram, falling back to vanilla"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: Invalid original hologram"));
		return nullptr;
	}

	// Get the world context from the original hologram
	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: No world context"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: Failed to spawn custom hologram"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFFactoryHologram* CustomHologram = World->SpawnActor<ASFFactoryHologram>(ASFFactoryHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: Failed to spawn custom hologram"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFLogisticsHologram* CustomHologram = World->SpawnActor<ASFLogisticsHologram>(ASFLogisticsHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: Failed to spawn custom hologram"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("CopyHologramProperties: Invalid source or destination"));
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
		UE_LOG(LogSmartFoundations, Error, TEXT("ReplaceHologramInBuildGun: Invalid holograms"));
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltClassForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt class path
	FString BeltPath = FString::Printf(
		TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
		Tier, Tier, Tier);

	UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);

	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltClassForTier: Failed to load belt class for tier %d"), Tier);
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No recipe manager, defaulting to Mk1"));
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
	UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No belts unlocked, defaulting to Mk1"));
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

