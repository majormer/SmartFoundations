// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFSubsystem implementation (part 3). Split out of SFSubsystem.cpp (slice S, pure
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

void USFSubsystem::PollForActiveHologram()
{
	// Get the player controller
	AFGPlayerController* PC = LastController.IsValid() ? LastController.Get() : nullptr;
	if (!PC)
	{
		// Try to find player controller if we don't have one cached
		UWorld* World = GetWorld();
		if (World && World->GetFirstPlayerController())
		{
			PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
			if (PC)
			{
				LastController = PC;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("PollForActiveHologram: Found player controller"));
			}
		}

		if (!PC)
		{
			return; // Still no controller - early return
		}
	}

	// Get the player character
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Player character unavailable"));

		// Clear hologram if character is not available
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}
		return;
	}

	// Get the build gun
	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Build gun unavailable"));
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}
		return;
	}

	// Subscribe to recipe sampling delegate for recipe copying (only once!)
	if (BuildGun && IsValid(BuildGun) && !bHasSubscribedToRecipeSampled)
	{
		BuildGun->mOnRecipeSampled.AddDynamic(this, &USFSubsystem::OnBuildGunRecipeSampled);
		bHasSubscribedToRecipeSampled = true;
		UE_LOG(LogSmartFoundations, Log, TEXT("RECIPE COPYING: Subscribed to build gun's OnRecipeSampled delegate"));
	}
	else if (!bHasSubscribedToRecipeSampled)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("RECIPE COPYING: BuildGun is invalid, cannot subscribe to recipe sampling delegate"));
	}

	// Check if we're in build state
	if (!BuildGun->IsInState(EBuildGunState::BGS_BUILD))
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Build gun left build state"));

		// Clear hologram if not in build state
		if (ActiveHologram.IsValid())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Clearing hologram: Build gun not in build state"));
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}

		// CRITICAL: Final cleanup for EXTEND when build gun leaves build mode
		// This catches any orphaned preview holograms that weren't cleaned up normally
		if (ExtendService)
		{
			ExtendService->CheckAndPerformFinalCleanup();
		}
		return;
	}

	// Get the build state
	UFGBuildGunState* CurrentState = BuildGun->GetCurrentState();
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(CurrentState);
	if (!BuildState)
	{
		return;
	}

	UClass* CurrentRecipe = BuildState->GetActiveRecipe();
	bool bAbortedRestoreForRecipeClear = false;
	if (!CurrentRecipe)
	{
		bAbortedRestoreForRecipeClear = AbortRestoreSession(TEXT("Build gun active recipe cleared"));
	}

	// Log recipe changes (identify what we're trying to build)
	{
		if (CurrentRecipe != LastLoggedRecipeClass)
		{
			LastLoggedRecipeClass = CurrentRecipe;
			AFGHologram* H = BuildState->GetHologram();
			UClass* InnerBuildClass = H ? H->GetBuildClass() : nullptr;
			const FText FriendlyName = CurrentRecipe ? UFGItemDescriptor::GetItemName(CurrentRecipe) : FText::GetEmpty();
			const FBox HoloBox = H ? H->GetComponentsBoundingBox(/*bNonColliding=*/true) : FBox(ForceInit);
			const FString HoloSizeStr = (H && HoloBox.IsValid) ? HoloBox.GetSize().ToString() : FString(TEXT("<n/a>"));
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BUILD RECIPE CHANGED: Recipe=%s Name=\"%s\" BuildClass=%s Hologram=%s HoloSize=%s"),
				*GetNameSafe(CurrentRecipe), *FriendlyName.ToString(), *GetNameSafe(InnerBuildClass), H ? *H->GetName() : TEXT("<none>"), *HoloSizeStr);
		}
	}

	// Get the hologram from the build state
	AFGHologram* CurrentHologram = BuildState->GetHologram();
	if (bAbortedRestoreForRecipeClear)
	{
		ResetCounters();
	}

	// Update registration if hologram changed
	if (CurrentHologram != ActiveHologram.Get())
	{
		// Unregister old hologram if exists
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}

		// Register new hologram if exists
		if (CurrentHologram)
		{
			RegisterActiveHologram(CurrentHologram);
		}
	}

	// Auto-Hold user override detection (Issue #273)
	// If we set the auto-hold lock but the hologram is now unlocked, the user pressed Hold
	// to release it manually. Set override flag so we don't re-lock until next grid change.
	if (bAutoHoldActive && !bAutoHoldUserOverrode && ActiveHologram.IsValid())
	{
		if (!ActiveHologram->IsHologramLocked())
		{
			bAutoHoldActive = false;
			bLockedByModifier = false;
			bAutoHoldUserOverrode = true;
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔓 AUTO-HOLD: User released lock manually — suppressing re-lock until next grid change"));
		}
	}

	// Detect and propagate transform changes via transform service (Phase 3 extraction)
	if (CurrentHologram && GridTransformService)
	{
		GridTransformService->DetectAndPropagateTransformChange(CurrentHologram);
	}

	// Update lift height on HUD for conveyor lift holograms
	if (CurrentHologram && HudService)
	{
		if (AFGConveyorLiftHologram* LiftHologram = Cast<AFGConveyorLiftHologram>(CurrentHologram))
		{
			// Access mTopTransform via reflection (it's protected in base class)
			float LiftHeight = 0.0f;
			if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
			{
				const FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(LiftHologram);
				if (TopTransformPtr)
				{
					LiftHeight = TopTransformPtr->GetTranslation().Z;
				}
			}

			float WorldHeight = LiftHologram->GetActorLocation().Z + LiftHeight;

			// Update HUD (HudService handles change detection internally)
			HudService->UpdateLiftHeight(LiftHeight, WorldHeight);
		}
		else
		{
			// Clear lift height when not placing a lift
			HudService->ClearLiftHeight();
		}
	}
}

// ========================================
// Multi-Step Hologram Property Sync (Issue #200)
// ========================================
// Syncs properties like fixture angle and build step from parent to children.
// Runs in Tick to detect changes from vanilla ScrollRotate during step 2.

void USFSubsystem::SyncMultiStepHologramProperties()
{
	AFGHologram* Parent = ActiveHologram.Get();
	if (!Parent) return;

	// === Floodlight sync (Issue #200) ===
	AFGFloodlightHologram* FloodlightParent = Cast<AFGFloodlightHologram>(Parent);
	if (FloodlightParent)
	{
		// Read mFixtureAngle via reflection (private member)
		int32 ParentAngle = 0;
		FIntProperty* AngleProp = FindFProperty<FIntProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mFixtureAngle"));
		if (AngleProp)
		{
			ParentAngle = AngleProp->GetPropertyValue_InContainer(FloodlightParent);
		}

		// Read mBuildStep via reflection (private member, CustomSerialization enum)
		uint8 ParentBuildStep = 0;
		FProperty* StepProp = FindFProperty<FProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mBuildStep"));
		if (StepProp)
		{
			StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(FloodlightParent));
		}

		if (ParentAngle != CachedParentFixtureAngle || ParentBuildStep != CachedParentBuildStep)
		{
			CachedParentFixtureAngle = ParentAngle;
			CachedParentBuildStep = ParentBuildStep;

			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			int32 SyncedCount = 0;
			for (const auto& ChildPtr : SpawnedChildren)
			{
				if (!ChildPtr.IsValid()) continue;
				AFGFloodlightHologram* FloodlightChild = Cast<AFGFloodlightHologram>(ChildPtr.Get());
				if (!FloodlightChild) continue;

				if (AngleProp) AngleProp->SetPropertyValue_InContainer(FloodlightChild, ParentAngle);
				if (StepProp) StepProp->CopyCompleteValue(StepProp->ContainerPtrToValuePtr<void>(FloodlightChild), &ParentBuildStep);
				if (UFunction* RepFunc = FloodlightChild->FindFunction(TEXT("OnRep_FixtureAngle")))
				{
					FloodlightChild->ProcessEvent(RepFunc, nullptr);
				}
				SyncedCount++;
			}
			if (SyncedCount > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Synced floodlight properties to %d children: Angle=%d, BuildStep=%d"),
					SyncedCount, ParentAngle, ParentBuildStep);
			}
		}
		return;
	}

	// === Standalone sign/billboard sync (Issue #192) ===
	AFGStandaloneSignHologram* SignParent = Cast<AFGStandaloneSignHologram>(Parent);
	if (SignParent)
	{
		// Read mBuildStep via reflection (protected, CustomSerialization enum)
		uint8 ParentBuildStep = 0;
		FProperty* StepProp = FindFProperty<FProperty>(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep"));
		if (StepProp)
		{
			StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(SignParent));
		}

		if (ParentBuildStep != CachedParentBuildStep)
		{
			CachedParentBuildStep = ParentBuildStep;

			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			int32 SyncedCount = 0;
			for (const auto& ChildPtr : SpawnedChildren)
			{
				if (!ChildPtr.IsValid()) continue;
				ASFStandaloneSignChildHologram* SignChild = Cast<ASFStandaloneSignChildHologram>(ChildPtr.Get());
				if (!SignChild) continue;

				// Use the public setter on our child class (no reflection needed)
				SignChild->SetBuildStep(static_cast<ESignHologramBuildStep>(ParentBuildStep));
				SyncedCount++;
			}
			if (SyncedCount > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Synced sign build step to %d children: BuildStep=%d"),
					SyncedCount, ParentBuildStep);
			}
		}
		return;
	}
}

// Enhanced scaling application with child hologram generation (POC)
void USFSubsystem::ApplyScalingToHologram(const FVector& ScalingDelta)
{
	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("SCALING FAILED: No active hologram"));
		return;
	}

	AFGHologram* Hologram = ActiveHologram.Get();

	// Store old transform for logging
	const FTransform OldTransform = Hologram->GetTransform();
	const FVector OldLocation = OldTransform.GetLocation();

	// Update our scaling offset (diagnostic only). Do NOT move the parent here.
	CurrentScalingOffset += ScalingDelta;

	// Let the Build Gun own the parent transform; only regenerate Smart! children
	const FVector NewLocation = OldLocation; // unchanged parent location for logging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SCALING APPLIED: Delta=%s | Old=%s | New=%s | TotalOffset=%s"),
		*ScalingDelta.ToString(), *OldLocation.ToString(), *NewLocation.ToString(), *CurrentScalingOffset.ToString());

	// POC: Regenerate child hologram grid based on counters
	RegenerateChildHologramGrid();
}

// Full child hologram grid management with positioning (delegated)
void USFSubsystem::RegenerateChildHologramGrid()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->RegenerateChildHologramGrid();
    }
}

void USFSubsystem::QueueChildForDestroy(AFGHologram* Child)
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->QueueChildForDestroy(Child);
    }
}

// Destroy any queued children now (runs on next tick)
void USFSubsystem::FlushPendingDestroy()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->FlushPendingDestroy();
    }
}

bool USFSubsystem::CanSafelyDestroyChildren() const
{
    if (USFGridSpawnerService* Spawner = const_cast<USFSubsystem*>(this)->GetGridSpawnerService())
    {
        return Spawner->CanSafelyDestroyChildren();
    }
    return true;
}

void USFSubsystem::ForceDestroyPendingChildren()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->ForceDestroyPendingChildren();
    }
}

// Update positions of all child holograms in the grid
void USFSubsystem::UpdateChildPositions()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->UpdateChildPositions();
    }
}

void USFSubsystem::UpdateChildrenForCurrentTransform()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->UpdateChildrenForCurrentTransform();
    }
}

void USFSubsystem::OnChildHologramDestroyed(AActor* DestroyedActor)
{
	// Phase 3: Delegate to HologramHelperService (Task #61.6)
	if (HologramHelper)
	{
		// Create callback for transform updates
		auto UpdateCallback = [this]()
		{
			this->UpdateChildrenForCurrentTransform();
		};

		HologramHelper->OnChildHologramDestroyed(DestroyedActor, UpdateCallback);
	}
}

void USFSubsystem::OnParentHologramDestroyed(AActor* DestroyedActor)
{
	// Phase 3: Delegate to HologramHelperService (Task #61.6)
	if (HologramHelper)
	{
		HologramHelper->OnParentHologramDestroyed(DestroyedActor);
	}

	// Check if this was our active hologram for local state cleanup
	AFGHologram* DestroyedHologram = Cast<AFGHologram>(DestroyedActor);
	if (DestroyedHologram && ActiveHologram.Get() == DestroyedHologram)
	{
		// ========================================
		// STACKABLE PIPE SUPPORT: Positions already cached in ProcessStackablePipelineSupports
		// ========================================
		// NOTE: We do NOT cache positions here because:
		// 1. Build actors spawn BEFORE OnParentHologramDestroyed is called
		// 2. Child holograms are already destroyed by this point
		// Positions are cached in ProcessStackablePipelineSupports where children are still valid
		if (AutoConnectService && AutoConnectService->IsStackablePipelineSupportHologram(DestroyedHologram))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE SUPPORT: OnParentHologramDestroyed - cache already set (Expected=%d, Pending=%d)"),
				StackablePipeSupportExpectedCount, bStackablePipeBuildPending);
		}

		// Clean up local SFSubsystem state
		CurrentScalingOffset = FVector::ZeroVector;

		// Clear transform cache via transform service (Phase 3 extraction)
		if (GridTransformService)
		{
			GridTransformService->ClearCache();
		}

		CurrentAdapter.Reset();

		// Reset all counters (fixes persistence bug - Task 51)
		ResetCounters();
	}
}

void USFSubsystem::OnBuildGunEquipped()
{
	if (!LastController.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Cannot log contexts: No player controller"));
		return;
	}

	AFGPlayerController* PC = LastController.Get();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Cannot log contexts: No Enhanced Input subsystem"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📋 ACTIVE INPUT CONTEXTS [BuildGunEquipped]"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	// Get all active mapping contexts using the correct API
	const TArray<FEnhancedActionKeyMapping>& CurrentMappings = InputSubsystem->GetAllPlayerMappableActionKeyMappings();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Total player mappable key mappings: %d"), CurrentMappings.Num());

	// Log Smart! specific context status
	UFGInputMappingContext* SmartContext = USFInputRegistry::GetSmartInputMappingContext();
	if (SmartContext)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Smart! Context exists: %s"), *SmartContext->GetName());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Parent: %s"), SmartContext->mParentContext.IsValid() ? *SmartContext->mParentContext->GetName() : TEXT("None"));
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Display Name: %s"), *SmartContext->mDisplayName.ToString());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Priority: %.1f"), SmartContext->mMenuPriority);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Smart! Context NOT LOADED"));
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔫 BUILD GUN EQUIPPED - Smart! input context should now be active"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Numpad keys should trigger Smart! actions when hologram is present"));

	// Clean up any stale recipe inheritance state for safety
	// This ensures fresh state when equipping build gun after save loads
	CleanupStateForWorldTransition();

	// Start periodic cleanup of dead hologram entries
	StartPeriodicCleanup();

	// Note: Recipe delegate subscription now happens in MonitorActiveHologram() on first access

	// Log active contexts immediately after equip
	LogActiveInputContexts(TEXT("BuildGunEquipped"));

	// Start periodic monitoring during build mode
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ContextMonitorTimer,
			[this]()
			{
				LogActiveInputContexts(TEXT("PeriodicMonitor"));
			},
			5.0f,  // Log every 5 seconds
			true   // Repeat
		);
	}
}

void USFSubsystem::OnBuildGunUnequipped()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" BUILD GUN UNEQUIPPED - Smart! build context deactivated"));

	AbortRestoreSession(TEXT("Build gun unequipped"));

	// Reset recipe sampling subscription flag
	bHasSubscribedToRecipeSampled = false;

	// Log contexts at unequip
	LogActiveInputContexts(TEXT("BuildGunUnequipped"));

	// Stop periodic monitoring
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContextMonitorTimer);
	}

	// Stop periodic cleanup of dead hologram entries
	StopPeriodicCleanup();

	// Clear any active hologram and reset counters
	if (ActiveHologram.IsValid())
	{
		UnregisterActiveHologram(ActiveHologram.Get());
	}

	// Force counter display to clear (in case hologram was already unregistered)
	ResetCounters();

	// Issue #198: Reset one-shot Smart disable flag when build gun is unequipped
	ResetSmartDisableFlag();

	// Issue #208/#209: User requested that sampled recipes and items (Shards/Somersloops)
	// should NOT persist across build gun sessions to avoid accidentally applying them
	// to unrelated builds later. We clear them on unequip.
	ClearStoredProductionRecipe();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: Cleared stored recipe and items on build gun unequip"));

	// Now it is safe to destroy any pending children we deferred while building
	ForceDestroyPendingChildren();
}

void USFSubsystem::OnBuildGunStateChanged(const FString& NewStateName, const FString& OldStateName)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 BUILD GUN STATE CHANGE: %s -> %s"), *OldStateName, *NewStateName);
	LastBuildGunState = NewStateName;

	// Log contexts at every state change
	LogActiveInputContexts(FString::Printf(TEXT("StateChange_%s"), *NewStateName));
}

void USFSubsystem::LogActiveInputContexts(const FString& CallerContext)
{
	if (!LastController.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[%s] Cannot log contexts: No player controller"), *CallerContext);
		return;
	}

	AFGPlayerController* PC = LastController.Get();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("[%s] Cannot log contexts: No Enhanced Input subsystem"), *CallerContext);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📋 ACTIVE INPUT CONTEXTS [%s]"), *CallerContext);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	// Get all active mapping contexts using the correct API
	const TArray<FEnhancedActionKeyMapping>& CurrentMappings = InputSubsystem->GetAllPlayerMappableActionKeyMappings();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Total player mappable key mappings: %d"), CurrentMappings.Num());

	// Log Smart! specific context status
	UFGInputMappingContext* SmartContext = USFInputRegistry::GetSmartInputMappingContext();
	if (SmartContext)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Smart! Context exists: %s"), *SmartContext->GetName());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Parent: %s"), SmartContext->mParentContext.IsValid() ? *SmartContext->mParentContext->GetName() : TEXT("None"));
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Display Name: %s"), *SmartContext->mDisplayName.ToString());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Priority: %.1f"), SmartContext->mMenuPriority);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Smart! Context NOT LOADED"));
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
}

TSharedPtr<ISFHologramAdapter> USFSubsystem::CreateHologramAdapter(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CreateHologramAdapter: Null hologram"));
		return nullptr;
	}

	const FString HologramTypeName = Hologram->GetClass()->GetName();

	// Log inheritance chain for ALL holograms to diagnose misclassification issues
	TArray<FString> InheritanceChain;
	UClass* CurrentClass = Hologram->GetClass();
	while (CurrentClass)
	{
		InheritanceChain.Add(CurrentClass->GetName());
		CurrentClass = CurrentClass->GetSuperClass();

		if (CurrentClass && CurrentClass->GetName() == TEXT("Object"))
		{
			InheritanceChain.Add(TEXT("UObject"));
			break;
		}
	}

	FString InheritanceStr;
	for (int32 i = 0; i < InheritanceChain.Num(); i++)
	{
		InheritanceStr += InheritanceChain[i];
		if (i < InheritanceChain.Num() - 1)
		{
			InheritanceStr += TEXT(" -> ");
		}
	}

	// Also log the BuildClass to help identify resource extractors
	UClass* BuildClass = Hologram->GetBuildClass();
	FString BuildClassName = BuildClass ? BuildClass->GetName() : TEXT("NONE");

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 Hologram inheritance: %s"), *InheritanceStr);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 BuildClass: %s"), *BuildClassName);

	// ========================================
	// PHASE 4: RUNTIME HOLOGRAM SWAPPING (Smart Custom Holograms)
	// ========================================
	// NOTE: Parent hologram swapping removed - only swap child holograms
	// Parent holograms are already valid and don't need recipe copying
	// Child holograms are swapped in SpawnChildHologram function

	// ========================================
	// CRITICAL: Detect vanilla blueprint holograms FIRST (Issue #166)
	// Blueprint placement must not be scaled - it would break the blueprint system
	// ========================================
	if (Cast<AFGBlueprintHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected Blueprint hologram (%s) - Smart! features disabled (blueprint placement)"),
			*Hologram->GetName());
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Blueprint"));
	}

	// Check for custom logistics holograms first (most specific)
	if (ASFLogisticsHologram* LogisticsHolo = Cast<ASFLogisticsHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Smart Logistics adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFSmartLogisticsAdapter>(LogisticsHolo);
	}

	// Check for custom factory holograms
	if (ASFFactoryHologram* FactoryHolo = Cast<ASFFactoryHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Factory adapter for custom factory hologram %s"), *Hologram->GetName());
		return MakeShared<FSFFactoryAdapter>(FactoryHolo);
	}

	// Check for custom foundation holograms
	if (ASFFoundationHologram* FoundationHolo = Cast<ASFFoundationHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Foundation adapter for custom foundation hologram %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Foundation"));
	}

	// Check for custom buildable base class (catch-all for other custom holograms)
	if (ASFBuildableHologram* BuildableHolo = Cast<ASFBuildableHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Smart Buildable adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFSmartBuildableAdapter>(BuildableHolo);
	}

	// ========================================
	// VANILLA HOLOGRAMS (Fallback for mod compatibility)
	// ========================================

	// CRITICAL: Check resource extractors BEFORE foundations!
	// Some miners use Holo_Snappable_C which inherits from AFGFoundationHologram for grid snapping
	// but are actually resource extractors that must be on resource nodes
	if (Cast<AFGWaterPumpHologram>(Hologram))
	{
		// Water extractors CAN be scaled - they check water depth per-position, not resource nodes
		// Children validate independently via CheckMinimumDepth()
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		FString InnerBuildClassName = InnerBuildClass ? InnerBuildClass->GetName() : TEXT("NONE");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating WaterExtractor adapter for %s (scaling enabled - validates per-child) BuildClass=%s"),
			*Hologram->GetName(), *InnerBuildClassName);
		return MakeShared<FSFWaterExtractorAdapter>(Hologram);
	}
	else if (Cast<AFGResourceExtractorHologram>(Hologram))
	{
		// Other resource extractors (miners, oil extractors) CANNOT be scaled
		// They must be placed on specific resource nodes - children fail validation
		UE_LOG(LogSmartFoundations, Warning, TEXT("Creating ResourceExtractor adapter for %s (scaling disabled - must be on resource node)"), *Hologram->GetName());
		return MakeShared<FSFResourceExtractorAdapter>(Hologram);
	}
	else if (Cast<AFGFoundationHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Foundation adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Foundation"));
	}
	else if (Cast<AFGStackableStorageHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Storage adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Storage"));
	}
	else if (Cast<AFGPipelineJunctionHologram>(Hologram))
	{
		// CRITICAL: Check PipelineJunction BEFORE ConveyorAttachment and Factory!
		// AFGPipelineJunctionHologram inherits from AFGFactoryHologram
		// Uses registry AnchorOffset for elevated pivot compensation
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating PipelineJunction adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("PipelineJunction"));
	}
	else if (Cast<AFGConveyorAttachmentHologram>(Hologram))
	{
		// CRITICAL: Check ConveyorAttachment BEFORE Factory!
		// AFGConveyorAttachmentHologram inherits from AFGFactoryHologram
		// Uses registry AnchorOffset for center-pivot compensation + unique floor validation
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating ConveyorAttachment adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("ConveyorAttachment"));
	}
	else if (Cast<AFGPipeHyperAttachmentHologram>(Hologram))
	{
		// CRITICAL: Check PipeHyperAttachment BEFORE Factory!
		// AFGPipeHyperAttachmentHologram inherits from AFGPipeAttachmentHologram -> AFGFactoryHologram
		// Uses registry AnchorOffset for elevated pivot compensation (7/8 height ratio)
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating HypertubeAttachment adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("HypertubeAttachment"));
	}
	else if (Cast<AFGPassthroughHologram>(Hologram))
	{
		// Issue #187: Check Passthrough BEFORE Factory!
		// AFGPassthroughHologram inherits from AFGFactoryHologram
		// Floor holes for conveyors and pipes - children snap to foundations independently
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Passthrough adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFPassthroughAdapter>(Hologram);
	}
	else if (Cast<AFGFactoryHologram>(Hologram))
	{
		// Check registry first - some factory holograms have scaling disabled
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		if (InnerBuildClass && USFBuildableSizeRegistry::HasProfile(InnerBuildClass))
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(InnerBuildClass);
			if (!Profile.bSupportsScaling)
			{
				// Registry disables scaling for this buildable
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("Detected registry buildable with scaling disabled (%s) - Smart! features disabled"),
					*InnerBuildClass->GetName());
				return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("RegistryDisabled"));
			}
		}
		// Registry allows scaling or no profile - use Factory adapter
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Factory adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFFactoryAdapter>(Hologram);
	}

	// ========================================
	// PARTIAL SUPPORT (Stub Adapters - Features Disabled)
	// ========================================

	else if (Cast<AFGWallHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Wall adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFWallAdapter>(Hologram);
	}
	else if (Cast<AFGPillarHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Pillar adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFPillarAdapter>(Hologram);
	}
	else if (Cast<AFGElevatorHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Elevator adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFElevatorAdapter>(Hologram);
	}
	else if (Cast<AFGStairHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Ramp adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFRampAdapter>(Hologram);
	}
	else if (Cast<AFGJumpPadHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating JumpPad adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFJumpPadAdapter>(Hologram);
	}

	// ========================================
	// EXPLICITLY UNSUPPORTED TYPES
	// ========================================

	else if (Cast<AFGWheeledVehicleHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected vehicle hologram (%s) - Smart! features disabled (vehicles not supported)"),
			*HologramTypeName);
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Vehicle"));
	}
	else if (Cast<AFGSpaceElevatorHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected Space Elevator hologram (%s) - Smart! features disabled (unique building)"),
			*HologramTypeName);
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("SpaceElevator"));
	}

	// ========================================
	// REGISTRY-BASED ADAPTER SELECTION
	// ========================================

	// Check if this buildable has a registry profile with scaling enabled
	// This allows the registry to drive adapter selection for special buildables
	else if (Cast<AFGBuildableHologram>(Hologram))
	{
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		if (InnerBuildClass && USFBuildableSizeRegistry::HasProfile(InnerBuildClass))
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(InnerBuildClass);
			if (Profile.bSupportsScaling)
			{
				// Registry says this buildable supports scaling - use Factory adapter
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Factory adapter for %s (registry-enabled: %s)"),
					*Hologram->GetName(), *InnerBuildClass->GetName());
				return MakeShared<FSFFactoryAdapter>(Hologram);
			}
			else
			{
				// Registry says scaling is disabled (extractors, unique buildings, etc.)
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("Detected registry buildable with scaling disabled (%s) - Smart! features disabled"),
					*InnerBuildClass->GetName());
				return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("RegistryDisabled"));
			}
		}
		// If no registry profile, fall through to unknown handling
	}

	// ========================================
	// UNKNOWN TYPES (Default to Unsupported)
	// ========================================

	// No registry profile and no specific adapter - unsupported
	{
		// Log full inheritance chain to help identify modded buildables
		TArray<FString> InnerInheritanceChain;
		UClass* CurrentClassForChain = Hologram->GetClass();
		while (CurrentClassForChain)
		{
			InnerInheritanceChain.Add(CurrentClassForChain->GetName());
			CurrentClassForChain = CurrentClassForChain->GetSuperClass();

			// Stop at UObject to avoid cluttering the log
			if (CurrentClassForChain && CurrentClassForChain->GetName() == TEXT("Object"))
			{
				InnerInheritanceChain.Add(TEXT("UObject"));
				break;
			}
		}

		// Build inheritance string (e.g., "Holo_X -> AFGBuildableHologram -> AActor -> UObject")
		FString InnerInheritanceStr;
		for (int32 i = 0; i < InnerInheritanceChain.Num(); i++)
		{
			InnerInheritanceStr += InnerInheritanceChain[i];
			if (i < InnerInheritanceChain.Num() - 1)
			{
				InnerInheritanceStr += TEXT(" -> ");
			}
		}

		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected unknown hologram type (%s) - Smart! features disabled. Inheritance: %s"),
			*HologramTypeName, *InnerInheritanceStr);

		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Unknown"));
	}
}

// ========================================
// RPC Handler Methods (Called by SFRCO)
// ========================================

void USFSubsystem::ApplyScalingFromRPC(AFGHologram* HologramActor, uint8 Axis, int32 Delta, int32 NewCounter)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Axis=%d, Delta=%d, Counter=%d, Hologram=%s"),
		Axis, Delta, NewCounter, *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[RPC] ApplyScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Validate axis
	if (Axis > 2)
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[RPC] ApplyScalingFromRPC: Invalid axis %d"), Axis);
		return;
	}

	// Calculate scaling delta from counter value
	// Counter represents cumulative steps; Delta is the increment direction
	const float StepSize = ScaleStepSize * Delta;
	FVector ScalingDelta = FVector::ZeroVector;

	switch (Axis)
	{
		case 0: // X axis
			ScalingDelta.X = StepSize;
			break;
		case 1: // Y axis
			ScalingDelta.Y = StepSize;
			break;
		case 2: // Z axis
			ScalingDelta.Z = StepSize;
			break;
	}

	// Apply the scaling
	ApplyScalingToHologram(ScalingDelta);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Applied delta %s, TotalOffset now %s"),
		*ScalingDelta.ToString(), *CurrentScalingOffset.ToString());

	// TODO Task #21: Replicate to other clients via multicast or property replication
}

void USFSubsystem::ResetScalingFromRPC(AFGHologram* HologramActor)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Hologram=%s"), *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[RPC] ResetScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Reset scaling offset
	CurrentScalingOffset = FVector::ZeroVector;

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Scaling offset cleared"));

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetSpacingModeFromRPC(ESFSpacingMode NewMode)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: NewMode=%d"), static_cast<uint8>(NewMode));

	// Update spacing mode in CounterState
	CounterState.SpacingMode = NewMode;

	FString ModeName = FSFSpacingModule::GetSpacingModeName(CounterState.SpacingMode);
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: Mode set to %s"), *ModeName);

	// Sync via centralized update
	UpdateCounterState(CounterState);

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetArrowVisibilityFromRPC(bool bVisible)
{
#if SMART_ARROWS_ENABLED
    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Visible=%d"), bVisible);

    // Update arrow runtime visibility state - Tick() will handle drawing
    bArrowsRuntimeVisible = bVisible;

    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Arrows visibility set to %s"),
        bVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
#else
    UE_LOG(LogSmartFoundations, Warning,
        TEXT("[RPC] SetArrowVisibilityFromRPC ignored - arrows feature disabled"));
#endif

    // TODO Task #21: Replicate to other clients
}

FString USFSubsystem::GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipeDisplayName(Recipe);
	}
	return TEXT("None");
}

UTexture2D* USFSubsystem::GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipePrimaryProductIcon(Recipe);
	}
	return nullptr;
}

FString USFSubsystem::GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithInputsOutputs(Recipe);
    }
    return TEXT("None");
}

void USFSubsystem::GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const
{
    if (RecipeManagementService)
    {
        RecipeManagementService->GetRecipeDisplayInfo(OutCurrentIndex, OutTotalRecipes);
    }
    else
    {
        OutCurrentIndex = 0;
        OutTotalRecipes = 0;
    }
}

TSubclassOf<UFGRecipe> USFSubsystem::GetActiveRecipe() const
{
    return RecipeManagementService ? RecipeManagementService->GetActiveRecipe() : nullptr;
}

FString USFSubsystem::GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithIngredient(Recipe);
	}
	return TEXT("None");
}

void USFSubsystem::UpdateCounterDisplay()
{
    // Build formatted strings (Phase B): route through HUD service
    TPair<FString, FString> DisplayLines = HudService
        ? HudService->BuildCounterDisplayLines()
        : TPair<FString, FString>(FString(), FString());
    const FString& FirstLine = DisplayLines.Key;
    const FString& SecondLine = DisplayLines.Value;

    // Log for debugging
    if (FirstLine.Len() > 0 || SecondLine.Len() > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Counter Display] Line1: '%s' Line2: '%s'"),
            *FirstLine, *SecondLine);
    }

    // Update HUD via service
    if (HudService)
    {
        HudService->UpdateWidgetDisplay(FirstLine, SecondLine);
    }
}

// ========================================
// Configuration System (Task 69)
// ========================================

void USFSubsystem::LoadConfiguration()
{
	// Try to load configuration from SML config system
	// Note: ConfigManager might not be available during early initialization
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: No world context, using defaults"));
		// Use defaults - config will be loaded later if needed
		bArrowsRuntimeVisible = true;
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: No game instance, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: ConfigManager not available yet, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	// ConfigManager is available, load config
	CachedConfig = FSmart_ConfigStruct::GetActiveConfig(this);

	// Initialize runtime arrow visibility from config (Issue #146)
	// This can be toggled during runtime with Num1, but starts with config value
	bArrowsRuntimeVisible = CachedConfig.bShowArrows;

	// Initialize arrow orbit and label toggles from config (Issue #179)
	if (ArrowModule)
	{
		ArrowModule->SetOrbitEnabled(CachedConfig.bShowArrowOrbit);
		ArrowModule->SetLabelsVisible(CachedConfig.bShowArrowLabels);
	}

	// Issue #257: Initialize Extend enabled state from config
	bExtendEnabledByConfig = CachedConfig.bExtendEnabled;

	bConfigLoaded = true;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Smart! Configuration loaded:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowHUD: %s"), CachedConfig.bShowHUD ? TEXT("true") : TEXT("false"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowArrows: %s (runtime: %s, orbit: %s, labels: %s)"),
		CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"),
		bArrowsRuntimeVisible ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowOrbit ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowLabels ? TEXT("true") : TEXT("false"));
}

void USFSubsystem::CleanupStateForWorldTransition()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Resetting recipe inheritance state"));

	// Clear stored recipe and cache
	StoredProductionRecipe = nullptr;
	StoredRecipeDisplayName = TEXT("");
	bHasStoredProductionRecipe = false;

	// Clear hologram registry to prevent stale entries
	USFHologramDataRegistry::ClearRegistry();

	// Update HUD to reflect cleared state
	UpdateCounterDisplay();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Recipe inheritance state reset complete"));
}

bool USFSubsystem::AbortRestoreSession(const TCHAR* Reason)
{
	const bool bHadRestoredTopology = ExtendService && ExtendService->IsRestoredCloneTopologyActive();
	const bool bHadRestoreSession = RestoreService && RestoreService->IsRestoreSessionActive();
	if (!bHadRestoredTopology && !bHadRestoreSession)
	{
		return false;
	}

	if (bHadRestoredTopology)
	{
		ExtendService->ClearRestoredCloneTopologySession(Reason);
	}

	if (bHadRestoreSession)
	{
		RestoreService->ClearActiveRestoreSession(Reason);
	}

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Aborted restore session: reason=%s topology=%d session=%d"),
		Reason ? Reason : TEXT("Unknown"),
		bHadRestoredTopology ? 1 : 0,
		bHadRestoreSession ? 1 : 0);

	UpdateCounterDisplay();
	return true;
}

void USFSubsystem::InitializeHologramCleanup()
{
	if (UWorld* World = GetWorld())
	{
		// Bind to OnActorDestroyed delegate to automatically clean up hologram data
		// This ensures deterministic cleanup for any hologram destruction scenario
		World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &USFSubsystem::OnActorDestroyed));
		UE_LOG(LogSmartFoundations, Log, TEXT("✅ Initialized Hologram Cleanup: Bound to OnActorDestroyed delegate."));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT(" Failed to Initialize Hologram Cleanup: World is null."));
	}
}

void USFSubsystem::OnActorDestroyed(AActor* DestroyedActor)
{
	// Filter for holograms only (deterministic cleanup)
	if (AFGHologram* Hologram = Cast<AFGHologram>(DestroyedActor))
	{
		USFHologramDataRegistry::ClearData(Hologram);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("OnActorDestroyed: Auto-cleared hologram %s from registry"),
			*GetNameSafe(Hologram));

		// Clean up auto-connect previews when hologram is destroyed (covers Escape, right-click, etc.)
		if (ActiveHologram.IsValid() && ActiveHologram.Get() == Hologram)
		{
			if (AutoConnectService)
			{
				// Only clear the appropriate preview type based on hologram type
				if (AutoConnectService->IsDistributorHologram(Hologram))
				{
					AutoConnectService->ClearBeltPreviewHelpers();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared belt previews for distributor (OnActorDestroyed)"));
				}
				else if (AutoConnectService->IsPipelineJunctionHologram(Hologram))
				{
					// CRITICAL FIX: Only clear previews for THIS specific junction, not all junctions
					// This prevents child hologram destruction from clearing parent previews (fixes flickering)
					AutoConnectService->ClearPipePreviews(Hologram);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared pipe previews for junction %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
				else if (AutoConnectService->IsPowerPoleHologram(Hologram))
				{
					// Clear power line previews when power pole hologram is destroyed
					AutoConnectService->ClearAllPowerPreviews();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared power line previews for pole %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
			}

			// NOTE: Don't reset bProcessingGridPlacement here!
			// The hologram is destroyed both on successful build AND on cancel.
			// The belt building lambda will reset the flag after completion.
			// If user cancels, OnActorSpawned never runs, so flag is never set.
		}
	}
}

void USFSubsystem::StartPeriodicCleanup()
{
	// Start periodic cleanup timer during build mode
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PeriodicCleanupTimer,
			this,
			&USFSubsystem::OnPeriodicCleanup,
			10.0f,  // Every 10 seconds
			true    // Looping
		);

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StartPeriodicCleanup: Started periodic hologram registry cleanup"));
	}
}

void USFSubsystem::StopPeriodicCleanup()
{
	// Stop periodic cleanup timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicCleanupTimer);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StopPeriodicCleanup: Stopped periodic hologram registry cleanup"));
	}
}

void USFSubsystem::OnPeriodicCleanup()
{
	// Perform deterministic cleanup of dead entries
	USFHologramDataRegistry::CleanupDeadEntries();
}

// ========================================
// Build HUD Widget Management
// ========================================

void USFSubsystem::InitializeWidgets()
{
	if (HudService)
	{
		HudService->InitializeWidgets();
	}
}

void USFSubsystem::EnsureHUDBinding()
{
	if (HudService)
	{
		HudService->EnsureHUDBinding();
	}
}

void USFSubsystem::CleanupWidgets()
{
    if (HudService)
    {
        HudService->CleanupWidgets();
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Widgets cleaned up"));
}

void USFSubsystem::DisableVanillaBuildGunContext()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->DisableVanillaBuildGunContext();
	}
}

void USFSubsystem::EnableVanillaBuildGunContext()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->EnableVanillaBuildGunContext();
	}
}

// ========================================
// Recipe Copying System Implementation
// ========================================

void USFSubsystem::StoreProductionRecipeFromBuilding(AFGBuildable* SourceBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->StoreProductionRecipeFromBuilding(SourceBuilding);
	}
}

void USFSubsystem::ApplyStoredProductionRecipeToBuilding(AFGBuildable* TargetBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyStoredProductionRecipeToBuilding(TargetBuilding);
	}
}

bool USFSubsystem::IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass)
{
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsRecipeCompatibleWithHologram(Recipe, HologramBuildClass);
	}
	return false;
}

bool USFSubsystem::IsProductionBuilding(AFGBuildable* Building) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsProductionBuilding(Building);
	}
	return false;
}

void USFSubsystem::ClearStoredProductionRecipe()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearStoredProductionRecipe();
	}
}

void USFSubsystem::OnBuildGunRecipeSampled(TSubclassOf<UFGRecipe> SampledRecipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->OnBuildGunRecipeSampled(SampledRecipe);
	}
}

// Recipe Selector System Implementation
// ========================================
void USFSubsystem::OnRecipeModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();

	// Context-aware mode switching: U button behaves differently based on hologram type
	// - Distributors: Auto-Connect Settings Mode (navigate belt settings)
	// - Pipe Junctions: Auto-Connect Settings Mode (navigate pipe settings)
	// - Power Poles: Auto-Connect Settings Mode (navigate power settings)
	// - Factories: Recipe Mode (select recipes)

	bool bIsAutoConnectHologram = false;
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		bool bIsDistributor = AutoConnectService->IsDistributorHologram(ActiveHologram.Get());
		bool bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
		bool bIsStackablePipe = AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get());
		bool bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
		bool bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
		bool bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get());

		bIsAutoConnectHologram = bIsDistributor || bIsPipeJunction || bIsStackablePipe || bIsStackableBelt || bIsPowerPole || bIsPassthroughPipe;

		FString BuildClassName = ActiveHologram->GetBuildClass() ? ActiveHologram->GetBuildClass()->GetName() : TEXT("NULL");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnRecipeModeChanged: Hologram=%s, BuildClass=%s, Distributor=%d, PipeJunction=%d, StackablePipe=%d, BeltSupport=%d, PowerPole=%d, PassthroughPipe=%d, IsAutoConnect=%d"),
			*ActiveHologram->GetClass()->GetName(), *BuildClassName, bIsDistributor, bIsPipeJunction, bIsStackablePipe, bIsStackableBelt, bIsPowerPole, bIsPassthroughPipe, bIsAutoConnectHologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT(" OnRecipeModeChanged: ActiveHologram=%d, AutoConnectService=%d"),
			ActiveHologram.IsValid(), AutoConnectService != nullptr);
	}

	if (bIsAutoConnectHologram)
	{
		// Auto-Connect Settings Mode for distributors, pipe junctions, and power poles
		bAutoConnectSettingsModeActive = bPressed;
		bRecipeModeActive = false;  // Ensure recipe mode is off

		if (bAutoConnectSettingsModeActive)
		{
			// Initialize runtime settings from config only if not already initialized
			// This preserves user modifications when re-entering settings mode
			if (bConfigLoaded && !AutoConnectRuntimeSettings.bInitialized)
			{
				AutoConnectRuntimeSettings.InitFromConfig(CachedConfig);
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect runtime settings initialized from config"));
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect Settings Mode: Active (U held on auto-connect hologram)"));
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current Setting: %s"), *GetAutoConnectSettingDisplayString());

			// Try to acquire lock
			TryAcquireHologramLock();

			// Update HUD to show current active setting
			UpdateCounterDisplay();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚙️ Auto-Connect Settings Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();

			// Update HUD to refresh display (remove active indicator)
			UpdateCounterDisplay();
		}
	}
	else
	{
		// Recipe Mode for factory buildings
		bRecipeModeActive = bPressed;
		bAutoConnectSettingsModeActive = false;  // Ensure auto-connect settings mode is off

		if (bRecipeModeActive)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe Mode: Active (U held on factory)"));

			// Try to acquire lock
			TryAcquireHologramLock();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Recipe Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();
		}

		// Delegate to recipe management service (only for factories)
		if (RecipeManagementService)
		{
			RecipeManagementService->OnRecipeModeChanged(Value);
		}
	}
}

void USFSubsystem::AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->AddRecipeToUnlocked(Recipe);
	}
}

TArray<TSubclassOf<UFGRecipe>> USFSubsystem::GetFilteredRecipesForCurrentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetFilteredRecipesForCurrentHologram();
	}
	return TArray<TSubclassOf<UFGRecipe>>();
}


void USFSubsystem::SetActiveRecipeByIndex(int32 Index)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->SetActiveRecipeByIndex(Index);
	}
}

// ========================================
// Smart Restore Enhanced — New Subsystem Methods
// ========================================

void USFSubsystem::SetAutoConnectRuntimeSettingsFromPreset(const FSFRestoreAutoConnectState& PresetState)
{
	AutoConnectRuntimeSettings.bEnabled = PresetState.bBeltEnabled;
	AutoConnectRuntimeSettings.BeltTierMain = PresetState.BeltTierMain;
	AutoConnectRuntimeSettings.BeltTierToBuilding = PresetState.BeltTierToBuilding;
	AutoConnectRuntimeSettings.bChainDistributors = PresetState.bChainDistributors;
	AutoConnectRuntimeSettings.BeltRoutingMode = PresetState.BeltRoutingMode;
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = PresetState.bPipeEnabled;
	AutoConnectRuntimeSettings.PipeTierMain = PresetState.PipeTierMain;
	AutoConnectRuntimeSettings.PipeTierToBuilding = PresetState.PipeTierToBuilding;
	AutoConnectRuntimeSettings.bPipeIndicator = PresetState.bPipeIndicator;
	AutoConnectRuntimeSettings.PipeRoutingMode = PresetState.PipeRoutingMode;
	AutoConnectRuntimeSettings.bConnectPower = PresetState.bPowerEnabled;
	AutoConnectRuntimeSettings.PowerGridAxis = PresetState.PowerGridAxis;
	AutoConnectRuntimeSettings.PowerReserved = PresetState.PowerReserved;
	AutoConnectRuntimeSettings.bInitialized = true;

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Display,
		TEXT("[SmartRestore] Applied auto-connect settings from preset"));
}

bool USFSubsystem::SetBuildGunByRecipeName(const FString& RecipeClassName)
{
	if (RecipeClassName.IsEmpty())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Find the recipe by class name in available recipes
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No recipe manager"));
		return false;
	}

	TArray<TSubclassOf<UFGRecipe>> AllRecipes;
	RecipeManager->GetAllAvailableRecipes(AllRecipes);

	TSubclassOf<UFGRecipe> TargetRecipe = nullptr;
	for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
	{
		if (Recipe && Recipe->GetName() == RecipeClassName)
		{
			TargetRecipe = Recipe;
			break;
		}
	}

	if (!TargetRecipe)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: Recipe '%s' not found in available recipes"),
			*RecipeClassName);
		return false;
	}

	// Get the player's build gun and set the recipe
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		return false;
	}

	// The build gun is equipment — get it from the character
	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun found"));
		return false;
	}

	// Set the active recipe on the build gun's build state
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(
		BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun build state"));
		return false;
	}

	BuildState->SetActiveRecipe(TargetRecipe);

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Display,
		TEXT("[SmartRestore] Switched build gun to recipe '%s'"), *RecipeClassName);
	return true;
}

void USFSubsystem::ApplyRecipeToParentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipeToParentHologram();
	}
}

