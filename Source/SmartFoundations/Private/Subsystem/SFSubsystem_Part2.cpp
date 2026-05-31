// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFSubsystem implementation (part 2). Split out of SFSubsystem.cpp (slice S, pure
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

void USFSubsystem::OnModifierScaleXReleased(const FInputActionValue& Value)
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleXReleased(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleXActive = InputHandler->IsModifierScaleXActive();
    }

    // Update arrow highlighting based on remaining modifiers
    if (InputHandler && InputHandler->IsModifierScaleYActive())
    {
        LastAxisInput = ELastAxisInput::Y;  // Y modifier still held = Y axis
    }
    else
    {
        LastAxisInput = ELastAxisInput::None;  // No modifiers = show all arrows
    }

    // Try to release lock (Task 52 - centralized)
    TryReleaseHologramLock();
}

void USFSubsystem::OnModifierScaleYPressed(const FInputActionValue& Value)
{
    // Scaled Extend (Issue #265): Allow Y modifier during extend mode for Scaled Extend.

    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleYPressed(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleYActive = InputHandler->IsModifierScaleYActive();
    }

    // Update arrow highlighting immediately based on modifier combination
    if (InputHandler && InputHandler->IsModifierScaleXActive() && InputHandler->IsModifierScaleYActive())
    {
        LastAxisInput = ELastAxisInput::Z;  // Both modifiers = Z axis
    }
    else if (bModifierScaleYActive)
    {
        LastAxisInput = ELastAxisInput::Y;  // Y modifier only = Y axis
    }

    // Try to acquire lock (Task 52 - centralized)
    TryAcquireHologramLock();
}

void USFSubsystem::OnModifierScaleYReleased(const FInputActionValue& Value)
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleYReleased(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleYActive = InputHandler->IsModifierScaleYActive();
    }

    // Update arrow highlighting based on remaining modifiers
    if (InputHandler && InputHandler->IsModifierScaleXActive())
    {
        LastAxisInput = ELastAxisInput::X;  // X modifier still held = X axis
    }
    else
    {
        LastAxisInput = ELastAxisInput::None;  // No modifiers = show all arrows
    }

    // Try to release lock (Task 52 - centralized)
    TryReleaseHologramLock();
}

// ========================================
// Hologram Lock Ownership Helpers (Task 52)
// ========================================

bool USFSubsystem::TryAcquireHologramLock()
{
    if (!ActiveHologram.IsValid())
    {
        return false;
    }

    // Only lock if not already locked by vanilla hold system
    if (!ActiveHologram->IsHologramLocked())
    {
        bLockedByModifier = true;
        ActiveHologram->LockHologramPosition(true);
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔒 Smart! acquired hologram lock"));
        return true;
    }
    else
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔒 Hologram already locked (vanilla hold owns it)"));
        return false;
    }
}

void USFSubsystem::TryReleaseHologramLock()
{
    // Only unlock if WE locked it AND no other Smart! modes are active
    // Issue #273: Don't release if auto-hold is active — that lock is managed by
    // ApplyAxisScaling (grid changes) and PollForActiveHologram (user override detection)
    if (bLockedByModifier && !bAutoHoldActive && !IsAnyModalFeatureActive() && ActiveHologram.IsValid())
    {
        bLockedByModifier = false;
        ActiveHologram->LockHologramPosition(false);
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔓 Smart! released hologram lock"));
    }
}

bool USFSubsystem::IsAnyModalFeatureActive() const
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        // Also check EXTEND mode which is managed by the ExtendService
        return InputHandler->IsAnyModalFeatureActive() || IsExtendModeActive();
    }

    // Fallback if module not initialized
    return bModifierScaleXActive || bModifierScaleYActive ||
           bSpacingModeActive || bStepsModeActive || bStaggerModeActive ||
           bRecipeModeActive || bAutoConnectSettingsModeActive || bRotationModeActive ||
           IsExtendModeActive();
}

FVector USFSubsystem::GetFurthestTopHologramPosition() const
{
    // If no active hologram, return zero vector
    if (!ActiveHologram.IsValid())
    {
        return FVector::ZeroVector;
    }

    // Get current grid counters
    const FIntVector& GridCounters = GetGridCounters();

    // If grid is 1x1x1 (no extension), return parent position
    if (GridCounters.X == 1 && GridCounters.Y == 1 && GridCounters.Z == 1)
    {
        return ActiveHologram->GetActorLocation();
    }

    // Calculate furthest grid position based on direction
    // The "furthest top" is at the corner furthest from the parent (X,Y)
    // and at the TOP Z level (highest)
    int32 FurthestX, FurthestY, TopZ;

    // Issue #275: Scaled Extend axis mapping
    // Extend clones are placed along the building's Y axis (via RightVector).
    // GridCounters.X = clone count (always positive, from FMath::Abs).
    // GridCounters.Y = row count (perpendicular to extend, along building's X axis).
    // CalculateChildPosition maps X param → building X, Y param → building Y.
    // So for Scaled Extend we must swap: clone count → FurthestY, row count → FurthestX.
    const bool bScaledExtend = ExtendService && ExtendService->IsScaledExtendActive();

    if (bScaledExtend)
    {
        ESFExtendDirection ExtendDir = ExtendService->GetExtendDirection();
        int32 CloneCount = FMath::Abs(GridCounters.X);

        // Clones extend along building's Y axis (RightVector)
        FurthestY = CloneCount - 1;
        if (ExtendDir == ESFExtendDirection::Left)
        {
            FurthestY = -FurthestY;  // Left = negative Y
        }

        // Rows extend along building's X axis (perpendicular to extend)
        FurthestX = GridCounters.Y > 0 ? GridCounters.Y - 1 : 0;

        // Scaled Extend doesn't use Z scaling
        TopZ = 0;
    }
    else
    {
        // Normal grid mode: counters directly map to axes
        // X direction
        if (GridCounters.X > 0)
        {
            FurthestX = GridCounters.X - 1;
        }
        else if (GridCounters.X < 0)
        {
            FurthestX = -(FMath::Abs(GridCounters.X) - 1);
        }
        else
        {
            FurthestX = 0;
        }

        // Y direction
        if (GridCounters.Y > 0)
        {
            FurthestY = GridCounters.Y - 1;
        }
        else if (GridCounters.Y < 0)
        {
            FurthestY = -(FMath::Abs(GridCounters.Y) - 1);
        }
        else
        {
            FurthestY = 0;
        }

        // Z direction
        if (GridCounters.Z > 0)
        {
            TopZ = GridCounters.Z - 1;
        }
        else if (GridCounters.Z < 0)
        {
            TopZ = -(FMath::Abs(GridCounters.Z) - 1);
        }
        else
        {
            TopZ = 0;
        }
    }

    // Get parent transform and item size
    FVector ParentLocation = ActiveHologram->GetActorLocation();
    FRotator ParentRotation = ActiveHologram->GetActorRotation();
    FVector ItemSize = GetCachedBuildingSize();

    // Calculate world position using PositionCalculator
    if (FSFPositionCalculator* PosCalc = GetPositionCalculator())
    {
        return PosCalc->CalculateChildPosition(
            FurthestX,
            FurthestY,
            TopZ,
            ParentLocation,
            ParentRotation,
            ItemSize,
            GetCounterState(),
            0, // Grid index not needed for position calculation
            FVector::ZeroVector // No anchor offset for position query
        );
    }

    // Fallback: return parent position if PositionCalculator is unavailable
    return ActiveHologram->GetActorLocation();
}

// Note: ToggleExtendMode() removed - EXTEND is now AUTOMATIC
// Activates when pointing at a compatible building of the same type

void USFSubsystem::OnSpacingModeChanged(const FInputActionValue& Value)
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnSpacingModeChanged(Value);
        // Sync state from module
        bSpacingModeActive = InputHandler->IsSpacingModeActive();
    }
    else
    {
        bSpacingModeActive = Value.Get<bool>();
    }
    if (bSpacingModeActive)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("\U0001F527 Spacing Mode: ACTIVE (Semicolon held) - Current axis: %s"),
            *UEnum::GetValueAsString(CounterState.SpacingAxis));
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Counters: X=%d cm, Y=%d cm, Z=%d cm"),
            CounterState.SpacingX, CounterState.SpacingY, CounterState.SpacingZ);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("\U0001F527 Spacing Mode: Inactive (Semicolon released)"));

        // Try to release lock (Task 52 - centralized)
        TryReleaseHologramLock();
    }
}

void USFSubsystem::OnSpacingCycleAxis()
{
    // Legacy handler - redirects to generic context-aware handler
    // Kept for backward compatibility with existing blueprint bindings
    OnCycleAxis();
}

void USFSubsystem::OnStepsModeChanged(const FInputActionValue& Value)
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnStepsModeChanged(Value);
        // Sync state from module
        bStepsModeActive = InputHandler->IsStepsModeActive();
    }
    else
    {
        bStepsModeActive = Value.Get<bool>();
    }
    if (bStepsModeActive)
    {
        // X-axis: Columns (constant X) step up based on X position
        // Y-axis: Rows (constant Y) step up based on Y position
        const TCHAR* AxisName = (CounterState.StepsAxis == ESFScaleAxis::X) ? TEXT("X (columns)") : TEXT("Y (rows)");
        const int32 CurrentCounter = (CounterState.StepsAxis == ESFScaleAxis::X) ? CounterState.StepsX : CounterState.StepsY;

        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 Steps Mode: ACTIVE (I held) - Axis: %s"), AxisName);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current counter: %d units (Num0 to toggle X/Y)"), CurrentCounter);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 Steps Mode: Inactive (I released)"));

        // Try to release lock (Task 52 - centralized)
        TryReleaseHologramLock();
    }
}

void USFSubsystem::OnCycleAxis()
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnCycleAxis();
    }

    // Context-aware mode swapper - dispatches based on active mode
    // Allows Num0 to work intelligently across all modal features

    if (bAutoConnectSettingsModeActive)
    {
        // Auto-Connect Settings mode: Cycle to next setting
        CycleAutoConnectSetting();
    }
    else if (bSpacingModeActive)
    {
        // Spacing mode: Cycle spacing axis X → Y → Z via service
        if (GridStateService)
        {
            GridStateService->CycleSpacingAxis(CounterState);
        }

        const TCHAR* AxisName = TEXT("");
        switch (CounterState.SpacingAxis)
        {
        case ESFScaleAxis::X: AxisName = TEXT("X"); break;
        case ESFScaleAxis::Y: AxisName = TEXT("Y"); break;
        case ESFScaleAxis::Z: AxisName = TEXT("Z"); break;
        default: break;
        }
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Spacing Axis Cycled: Now adjusting %s-axis"), AxisName);
        UpdateCounterState(CounterState);
    }
    else if (bStepsModeActive)
    {
        // Steps mode: Toggle between X and Y only via service
        if (GridStateService)
        {
            GridStateService->ToggleStepsAxis(CounterState);
        }

        const TCHAR* AxisName = (CounterState.StepsAxis == ESFScaleAxis::X) ? TEXT("X") : TEXT("Y");
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Steps Axis Toggled: Now adjusting %s-axis (columns/rows)"), AxisName);
        UpdateCounterState(CounterState);
    }
    else if (bStaggerModeActive)
    {
        // Stagger mode: Cycle X → Y → ZX → ZY via service
        if (GridStateService)
        {
            GridStateService->CycleStaggerAxis(CounterState);
        }

		const TCHAR* AxisName = TEXT("Unknown");
		const TCHAR* AxisDescription = TEXT("");

		switch (CounterState.StaggerAxis)
		{
		case ESFScaleAxis::X:
			AxisName = TEXT("X");
			AxisDescription = TEXT("horizontal diagonal - sideways offset");
			break;
		case ESFScaleAxis::Y:
			AxisName = TEXT("Y");
			AxisDescription = TEXT("horizontal diagonal - forward/back offset");
			break;
		case ESFScaleAxis::ZX:
			AxisName = TEXT("ZX");
			AxisDescription = TEXT("vertical lean - forward/back");
			break;
		case ESFScaleAxis::ZY:
			AxisName = TEXT("ZY");
			AxisDescription = TEXT("vertical lean - sideways");
			break;
		default:
			break;
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Stagger Axis Toggled: Now adjusting %s-axis (%s)"), AxisName, AxisDescription);
		UpdateCounterState(CounterState);
	}
	else if (bRecipeModeActive)
	{
		// Recipe mode: Clear manual selection completely
		if (ActiveRecipeSource == ERecipeSource::ManuallySelected)
		{
			// Clear manual selection
			ActiveRecipeSource = ERecipeSource::None;
			ActiveRecipe = nullptr;
			CurrentRecipeIndex = 0;

			// Apply recipe clear to parent hologram
			ApplyRecipeToParentHologram();

			// Trigger debounced regeneration to propagate clear to children
			if (ActiveHologram.IsValid() && ActiveHologram->GetHologramChildren().Num() > 0)
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
					World->GetTimerManager().SetTimer(
						RecipeRegenerationTimer,
						this,
						&USFSubsystem::RegenerateChildHologramGrid,
						0.2f,  // 200ms debounce
						false
					);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe cleared - scheduled child regeneration in 200ms"));
				}
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe selection cleared - no recipe will be applied to parent or children"));
		}
		else
		{
			// No manual selection to clear, just clear everything
			ClearAllRecipes();

			// Apply clear to hologram registry and trigger regeneration
			ApplyRecipeToParentHologram();
			if (ActiveHologram.IsValid() && ActiveHologram->GetHologramChildren().Num() > 0)
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
					World->GetTimerManager().SetTimer(
						RecipeRegenerationTimer,
						this,
						&USFSubsystem::RegenerateChildHologramGrid,
						0.2f,  // 200ms debounce
						false
					);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ All recipes cleared - scheduled child regeneration in 200ms"));
				}
			}
		}

		UpdateCounterDisplay();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe selection cleared via Num0"));
	}
	else
	{
		// No mode active - check for double-tap to toggle Smart disable (Issue #198)
		// Only works when no modifier keys are held
		if (!bModifierScaleXActive && !bModifierScaleYActive)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			const double TimeSinceLastTap = CurrentTime - LastCycleAxisTapTime;

			if (TimeSinceLastTap < DoubleTapWindow && LastCycleAxisTapTime > 0.0)
			{
				// Double-tap detected - toggle the disable flags
				bDisableSmartForNextAction = !bDisableSmartForNextAction;
				bExtendDisabledForSession = !bExtendDisabledForSession;  // Issue #257: toggle Extend too

				if (bDisableSmartForNextAction)
				{
					SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("⏸️ Smart! DISABLED for session (Auto-Connect + Extend) (double-tap detected)"));

					// Issue #198: Immediately clear all auto-connect previews when disabling
					if (ActiveHologram.IsValid() && AutoConnectService)
					{
						USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
						if (Orchestrator)
						{
							Orchestrator->ForceRefresh();
						}
					}

					// Issue #257: Clear active Extend state when disabling
					if (ExtendService)
					{
						ExtendService->ClearExtendState();
					}
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("▶️ Smart! RE-ENABLED (Auto-Connect + Extend) (double-tap detected)"));

					// Re-enable: trigger refresh to recreate previews
					if (ActiveHologram.IsValid() && AutoConnectService)
					{
						USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
						if (Orchestrator)
						{
							Orchestrator->ForceRefresh();
						}
					}
				}

				// Reset timing to prevent triple-tap from toggling again
				LastCycleAxisTapTime = 0.0;

				// Update HUD to show the change
				UpdateCounterDisplay();
			}
			else
			{
				// First tap - record time for potential double-tap
				LastCycleAxisTapTime = CurrentTime;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Cycle axis tap recorded (no mode active) - waiting for potential double-tap"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Cycle axis ignored - modifier keys held"));
		}
	}
}

void USFSubsystem::OnStaggerModeChanged(const FInputActionValue& Value)
{
	// Block stagger mode while EXTEND is active
	if (IsExtendModeActive())
	{
		return;
	}

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnStaggerModeChanged(Value);
		// Sync state from module
		bStaggerModeActive = InputHandler->IsStaggerModeActive();
	}
	else
	{
		bStaggerModeActive = Value.Get<bool>();
	}
	if (bStaggerModeActive)
	{
		// Stagger only uses X and Y axes (lateral grid offset)
		const TCHAR* AxisName = (CounterState.StaggerAxis == ESFScaleAxis::X) ? TEXT("X (sideways curve)") : TEXT("Y (forward curve)");
		const int32 CurrentCounter = (CounterState.StaggerAxis == ESFScaleAxis::X) ? CounterState.StaggerX : CounterState.StaggerY;

		UE_LOG(LogSmartFoundations, Warning, TEXT(" Stagger Mode: ACTIVE (Y held) - Axis: %s"), AxisName);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current counter: %d units (Num0 to toggle X/Y)"), CurrentCounter);

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		// Try to release lock (Task 52 - centralized)
		TryReleaseHologramLock();
	}
}

void USFSubsystem::OnRotationModeChanged(const FInputActionValue& Value)
{
	// Scaled Extend (Issue #265): Allow rotation mode during extend.

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnRotationModeChanged(Value);
		// Sync state from module
		bRotationModeActive = InputHandler->IsRotationModeActive();
	}
	else
	{
		bRotationModeActive = Value.Get<bool>();
	}
	if (bRotationModeActive)
	{
		// Rotation currently only uses Z axis (horizontal arc)
		// Phase 2 will add X and Y axes for vertical arches
		UE_LOG(LogSmartFoundations, Warning, TEXT(" Rotation Mode: ACTIVE (Comma held) - Axis: Z (horizontal arc)"));
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current rotation: %.1f° (Num0 to cycle axis when Phase 2 adds X/Y)"), CounterState.RotationZ);

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		// Try to release lock (Task 52 - centralized)
		TryReleaseHologramLock();
	}
}

void USFSubsystem::OnToggleArrows()
{
	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnToggleArrows();
	}

#if SMART_ARROWS_ENABLED
	// Toggle runtime visibility flag (doesn't change config)
	bArrowsRuntimeVisible = !bArrowsRuntimeVisible;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Arrows TOGGLE - Visible: %s (Config default: %s)"),
		bArrowsRuntimeVisible ? TEXT("ON") : TEXT("OFF"),
		CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"));

	// Force immediate arrow update to handle visibility change
	// This ensures arrows appear/disappear instantly without waiting for hologram movement
	if (ActiveHologram.IsValid() && CurrentAdapter && CurrentAdapter->IsValid() && ArrowModule)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FTransform CurrentTransform = CurrentAdapter->GetBaseTransform();

			if (bArrowsRuntimeVisible)
			{
				// Force redraw when enabling - show arrows immediately
				ArrowModule->UpdateArrows(World, CurrentTransform, LastAxisInput, true);
				UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrows: Forced redraw on visibility enable"));
			}
			else
			{
				// Force clear when disabling - hide arrows immediately by calling with bVisible=false
				ArrowModule->UpdateArrows(World, CurrentTransform, LastAxisInput, false);
				UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrows: Forced clear on visibility disable"));
			}

			// Update cache to prevent immediate redraw in next Tick
			LastHologramTransform = CurrentTransform;
			LastKnownAxisInput = LastAxisInput;
			LastChildCount = 0;  // Reset child count when toggling visibility
		}
	}
#else
	UE_LOG(LogSmartFoundations, Warning, TEXT("Arrows feature is currently disabled (SMART_ARROWS_ENABLED=0)"));
#endif

	// DEBUG: Comprehensive manifold scan within 50m radius
	// Captures chain actors, connections, splines, and all properties for research
	if (RadarPulseService)
	{
		const float ScanRadius = 5000.0f;   // 50m = 5000cm
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📡 [Arrow Toggle Debug] Comprehensive scan within 50m..."));

		// 1. HOLOGRAM SCAN - Inspect all holograms in range (captures mesh paths!)
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📡 Scanning for holograms..."));
		RadarPulseService->InspectAllHologramsInRadius(ScanRadius);

		// 2. Chain actor scan (chain boundaries, members, connections)
		RadarPulseService->ScanChainActors(ScanRadius);

		// 3. Full radar pulse snapshot (all buildables with properties)
		// Filter to logistics and factory categories for manifold research
		TArray<FString> ManifoldCategories;
		ManifoldCategories.Add(TEXT("Belt"));
		ManifoldCategories.Add(TEXT("Lift"));
		ManifoldCategories.Add(TEXT("Distributor"));
		ManifoldCategories.Add(TEXT("Factory"));
		ManifoldCategories.Add(TEXT("Pipe"));
		ManifoldCategories.Add(TEXT("Junction"));
		ManifoldCategories.Add(TEXT("StackablePipePole"));
		ManifoldCategories.Add(TEXT("StackableBeltPole"));

		FSFRadarPulseSnapshot Snapshot = RadarPulseService->CaptureSnapshotFiltered(
			ScanRadius,
			TEXT("MANIFOLD_RESEARCH"),
			ManifoldCategories
		);

		// Log with verbose details and enumerate all categories
		RadarPulseService->LogSnapshotFiltered(Snapshot, true, ManifoldCategories);
	}
}

void USFSubsystem::OnToggleSettingsForm()
{
	UE_LOG(LogSmartFoundations, Warning, TEXT("!!! K KEY PRESSED !!! (OnToggleSettingsForm)"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Settings Form Toggle (Phase 0 validation)"));

	// Route to Upgrade Panel if holding an upgrade-capable hologram (belt/lift/pipe/pump/power pole/wall outlet)
	if (IsUpgradeCapableContext())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Routing to Upgrade Panel (upgrade-capable context)"));
		OnToggleUpgradePanel();
		return;
	}

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: No valid PlayerController available"));
		return;
	}

	// Toggle behavior: if widget is open, close it (with cancel/revert)
	if (SettingsFormWidget.IsValid() && SettingsFormWidget->IsInViewport())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Closing via toggle key (canceling unapplied changes)"));

		// Use CancelAndClose to revert unapplied changes before closing
		SettingsFormWidget->CancelAndClose();
		SettingsFormWidget.Reset();
		return;
	}

	// Load the Blueprint Widget class
	FString WidgetPath = TEXT("/SmartFoundations/SmartFoundations/UI/Smart_SettingsForm_Widget.Smart_SettingsForm_Widget_C");
	FSoftClassPath WidgetClassPath(WidgetPath);
	UClass* WidgetClass = WidgetClassPath.TryLoadClass<UUserWidget>();

	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance and add to viewport
	USmartSettingsFormWidget* WidgetInstance = CreateWidget<USmartSettingsFormWidget>(PC, WidgetClass);

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: Failed to create widget instance"));
		return;
	}

	// Store reference for toggle behavior
	SettingsFormWidget = WidgetInstance;

	// Suppress HUD while settings form is open
	if (HudService)
	{
		HudService->SetHUDSuppressed(true);
	}

	// Populate form with current counter state values
	WidgetInstance->PopulateFromCounterState(this);

	// Set visibility and add to viewport
	WidgetInstance->SetVisibility(ESlateVisibility::Visible);
	WidgetInstance->AddToViewport(100);

	// Enable mouse cursor and UI-only input mode (blocks game input to prevent click-through)
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(WidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}

bool USFSubsystem::IsUpgradeCapableContext() const
{
    AFGHologram* Hologram = ActiveHologram.Get();

    // Fallback: If the 100ms poll hasn't fired yet or cache is stale, check the Build Gun directly
    if (!Hologram)
    {
        AFGPlayerController* PC = GetLastController();
        if (PC)
        {
            if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetCharacter()))
            {
                if (AFGBuildGun* BuildGun = Character->GetBuildGun())
                {
                    if (BuildGun->IsInState(EBuildGunState::BGS_BUILD))
                    {
                        if (UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState()))
                        {
                            Hologram = BuildState->GetHologram();
                        }
                    }
                }
            }
        }
    }

    if (!Hologram)
    {
        return false;
    }

    FString ClassName = Hologram->GetClass()->GetName();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("IsUpgradeCapableContext: Checking hologram class: %s"), *ClassName);

    // Belt holograms (conveyor belts) - e.g. Holo_ConveyorBelt_C
    if (ClassName.Contains(TEXT("ConveyorBelt")) && !ClassName.Contains(TEXT("Lift")))
    {
        return true;
    }

    // Lift holograms (conveyor lifts) - e.g. Holo_ConveyorLift_C
    if (ClassName.Contains(TEXT("ConveyorLift")) || ClassName.Contains(TEXT("Lift")))
    {
        return true;
    }

    // Pipeline holograms (pipes)
    if (ClassName.Contains(TEXT("Pipeline")) && !ClassName.Contains(TEXT("Junction")) && !ClassName.Contains(TEXT("Pump")) && !ClassName.Contains(TEXT("Support")))
    {
        return true;
    }

    // Power line/wire holograms - use for power network upgrades instead of power poles
    // This avoids conflict with power pole (and wall outlet) scaling feature
    if (ClassName.Contains(TEXT("Wire")) || ClassName.Contains(TEXT("PowerLine")))
    {
        return true;
    }

    // Note: Pipeline Pumps, Power Poles, and Wall Outlets intentionally return false here
    // so that the Smart! Panel opens instead, allowing players to use grid scaling.

    return false;
}

void USFSubsystem::OnToggleUpgradePanel()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Upgrade Panel Toggle"));

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: No valid PlayerController available"));
		return;
	}

	// Toggle behavior: if widget is open, close it
	if (UpgradePanelWidget.IsValid() && UpgradePanelWidget->IsInViewport())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Closing via toggle key"));
		UpgradePanelWidget->RemoveFromParent();
		UpgradePanelWidget.Reset();

		// Restore game input mode
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		// Re-enable HUD
		if (HudService)
		{
			HudService->SetHUDSuppressed(false);
		}
		return;
	}

	// Load the Upgrade Panel Blueprint Widget class
	FString WidgetPath = TEXT("/SmartFoundations/SmartFoundations/UI/Smart_UpgradePanel_Widget.Smart_UpgradePanel_Widget_C");
	FSoftClassPath WidgetClassPath(WidgetPath);
	UClass* WidgetClass = WidgetClassPath.TryLoadClass<UUserWidget>();

	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance - try our C++ class first, fall back to UUserWidget if Blueprint not reparented
	USmartUpgradePanel* SmartPanelInstance = CreateWidget<USmartUpgradePanel>(PC, WidgetClass);
	UUserWidget* WidgetInstance = SmartPanelInstance;

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: CreateWidget<USmartUpgradePanel> failed, trying UUserWidget fallback"));
		// Blueprint may not be reparented yet - fall back to base UUserWidget
		WidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	}

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Failed to create widget instance - Blueprint may need to be reparented to UserWidget in Unreal Editor"));
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Open Smart_UpgradePanel_Widget in UE Editor, go to Class Settings, and set Parent Class to UserWidget or SmartUpgradePanel"));
		return;
	}

	// Store reference for toggle behavior
	UpgradePanelWidget = WidgetInstance;

	// If using fallback UUserWidget, manually bind the close button
	if (!SmartPanelInstance)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Using fallback mode - manually binding close button"));
		if (UButton* CloseButton = Cast<UButton>(WidgetInstance->GetWidgetFromName(TEXT("CloseButton"))))
		{
			CloseButton->OnClicked.AddDynamic(this, &USFSubsystem::OnUpgradePanelCloseClicked);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: CloseButton bound successfully"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: CloseButton not found in widget"));
		}
	}

	// Suppress HUD while upgrade panel is open
	if (HudService)
	{
		HudService->SetHUDSuppressed(true);
	}

	// Set visibility and add to viewport
	WidgetInstance->SetVisibility(ESlateVisibility::Visible);
	WidgetInstance->AddToViewport(100);

	// Enable mouse cursor and UI-only input mode (blocks game input to prevent click-through)
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(WidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	// Set widget to be focusable so it can receive keyboard input
	WidgetInstance->SetIsFocusable(true);
	WidgetInstance->SetKeyboardFocus();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Opened (ESC or Close button to close)"));
}

void USFSubsystem::OnUpgradePanelCloseClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Close button clicked (fallback handler)"));

	if (!UpgradePanelWidget.IsValid())
	{
		return;
	}

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	// Remove widget from viewport
	UpgradePanelWidget->RemoveFromParent();
	UpgradePanelWidget.Reset();

	// Restore game input mode
	if (PC)
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	// Re-enable HUD
	if (HudService)
	{
		HudService->SetHUDSuppressed(false);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Closed via close button"));
}

// Hologram management with enhanced logging
void USFSubsystem::RegisterActiveHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM REGISTRATION FAILED: Null hologram pointer"));
		return;
	}

	bool bRestoredExtendActiveAtRegister = IsRestoredExtendModeActive();

	// Lazy initialization on first hologram registration (Task #58)
	if (!bSubsystemInitialized)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Subsystem: Lazy initialization on first hologram registration"));

			// Load configuration (deferred from Initialize() to ensure ConfigManager is available)
			if (!bConfigLoaded)
			{
				LoadConfiguration();
			}

			// Initialize HUD widgets
			InitializeWidgets();

#if SMART_ARROWS_ENABLED
			// Initialize StaticMeshComponent-based arrow visualization (Task 17)
			// Always initialize so Num1 toggle works, but respect config for initial visibility
			if (ArrowModule && ArrowModule->Initialize(World, this, this))
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("✅ Arrow visualization initialized (StaticMeshComponent) - Config: bShowArrows=%s, Runtime: %s"),
					CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"),
					bArrowsRuntimeVisible ? TEXT("true") : TEXT("false"));

				// Start arrow drawing timer (every frame = ~16ms at 60fps)
				World->GetTimerManager().SetTimer(
					ArrowTickTimer,
					this,
					&USFSubsystem::TickArrows,
					0.016f,  // ~60fps
					true     // Repeat
				);
				UE_LOG(LogSmartFoundations, Log, TEXT("Started arrow tick timer"));
			}
			else
			{
				UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to initialize arrow visualization"));
			}
#else
			UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrow visualization disabled (SMART_ARROWS_ENABLED=0)"));
#endif

			bSubsystemInitialized = true;
		}
	}

	if (bRestoredExtendActiveAtRegister && ExtendService && !ExtendService->IsHologramCompatibleWithRestoredCloneTopology(Hologram))
	{
		ExtendService->ClearRestoredCloneTopologySession(TEXT("RegisterActiveHologram build class mismatch"));
		if (RestoreService)
		{
			RestoreService->ClearActiveRestoreSession(TEXT("Active buildable changed away from restored Extend source"));
		}
		bRestoredExtendActiveAtRegister = false;

		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore][Extend] Dropped restored topology for incompatible active hologram: hologram=%s buildClass=%s"),
			*GetNameSafe(Hologram),
			*GetNameSafe(Hologram->GetBuildClass()));
	}

	// Reset auto-connect runtime settings when hologram changes
	ResetAutoConnectRuntimeSettings();

	// Issue #198: Reset one-shot Smart disable flag when hologram changes
	ResetSmartDisableFlag();

	// Reset current auto-connect setting based on hologram type
	// This ensures the HUD shows appropriate defaults for the new hologram type
	// NOTE: We determine the setting AFTER ActiveHologram is assigned so detection works
	// Each hologram type has its own "first" setting in its cycle
	if (AutoConnectService)
	{
		if (AutoConnectService->IsPowerPoleHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
		}
		else if (AutoConnectService->IsPipelineJunctionHologram(Hologram) ||
		         AutoConnectService->IsStackablePipelineSupportHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::Enabled; // Pipe context
		}
		else if (USFAutoConnectService::IsBeltSupportHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
		}
		else
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::Enabled; // Belt context (distributors)
		}
	}
	else
	{
		CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
	}

	// NOTE: Hologram swapping is DISABLED to fix the infinite re-registration loop
	// The build gun holds the original hologram reference, and swapping creates a mismatch:
	// - BuildState->GetHologram() returns vanilla hologram
	// - ActiveHologram is our swapped custom hologram
	// - PollForActiveHologram sees mismatch → unregister/re-register → infinite loop
	//
	// EXTEND and Scaling work with vanilla holograms - no swap needed.
	// If custom hologram behavior is needed in the future, use Blueprint class remapping at module load.
	ActiveHologram = Hologram;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegisterActiveHologram: %s"), *Hologram->GetName());

	// Issue #281: Notify hint bar service
	if (HintBarService)
	{
		HintBarService->OnHologramRegistered();
	}

	// NOTE: Pipe previews are now handled by the Auto-Connect Orchestrator (created below)
	// Don't call PipeAutoConnectManager directly here to avoid duplicate managers

	CurrentScalingOffset = FVector::ZeroVector;

	// Clear recipe cache when switching to new hologram type
	if (SortedFilteredRecipes.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Clearing recipe cache for new hologram type (had %d recipes)"),
			SortedFilteredRecipes.Num());
		SortedFilteredRecipes.Empty();
		CurrentRecipeIndex = 0;
	}

	// Clear active recipe when switching to new hologram type to prevent stale display
	if (ActiveRecipe != nullptr)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Clearing active recipe for new hologram type"));
		ActiveRecipe = nullptr;
		ActiveRecipeSource = ERecipeSource::None;
	}

	// Clear any pending recipe regeneration timer when switching building types
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
	}

	// Reset all counters for new hologram (centralized - Task 51).
	// Restored Extend presets are edited as a sticky staged graph; vanilla may recreate
	// the parent hologram while aiming, so preserve Smart transform state across that swap.
	if (!bRestoredExtendActiveAtRegister)
	{
		ResetCounters();
	}
	else
	{
		if (GridStateService)
		{
			GridStateService->UpdateCounterState(CounterState);
		}
		UpdateCounterDisplay();
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore][Extend] Preserved staged counters across hologram registration: grid=[%d,%d,%d] spacing=(%d,%d,%d) steps=(%d,%d) rotation=%.1f"),
			CounterState.GridCounters.X,
			CounterState.GridCounters.Y,
			CounterState.GridCounters.Z,
			CounterState.SpacingX,
			CounterState.SpacingY,
			CounterState.SpacingZ,
			CounterState.StepsX,
			CounterState.StepsY,
			CounterState.RotationZ);
	}

	// Reset HUD state to prevent stale display from previous hologram
	if (HudService)
	{
		HudService->ResetState();
	}
	if (bRestoredExtendActiveAtRegister)
	{
		UpdateCounterDisplay();
	}

	// Reset Zoop flag in HologramHelper (Issue #160)
	if (HologramHelper)
	{
		HologramHelper->ClearZoopState();
	}

	// Log counter state after reset to verify proper initialization
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" COUNTER STATE AFTER RESET:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Grid: [%d, %d, %d] = %d items | ValidChildren=%d"),
		CounterState.GridCounters.X, CounterState.GridCounters.Y, CounterState.GridCounters.Z,
		CounterState.GridCounters.X * CounterState.GridCounters.Y * CounterState.GridCounters.Z,
		CounterState.ValidChildCount);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Spacing: Mode=%d Axis=%d | X=%d Y=%d Z=%d (cm)"),
		(int32)CounterState.SpacingMode, (int32)CounterState.SpacingAxis,
		CounterState.SpacingX, CounterState.SpacingY, CounterState.SpacingZ);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Steps: Axis=%d | X=%d Y=%d (cm)"),
		(int32)CounterState.StepsAxis, CounterState.StepsX, CounterState.StepsY);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Stagger: Axis=%d | X=%d Y=%d ZX=%d ZY=%d (cm)"),
		(int32)CounterState.StaggerAxis, CounterState.StaggerX, CounterState.StaggerY,
		CounterState.StaggerZX, CounterState.StaggerZY);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Summary: GridCounters=[%d,%d,%d] SpacingMode=%d SpacingX=%d StepsX=%d StaggerX=%d"),
        CounterState.GridCounters.X, CounterState.GridCounters.Y, CounterState.GridCounters.Z,
        (int32)CounterState.SpacingMode, CounterState.SpacingX, CounterState.StepsX, CounterState.StaggerX);

	// Bind to hologram destruction to clean up when building is placed
	Hologram->OnDestroyed.AddUniqueDynamic(this, &USFSubsystem::OnParentHologramDestroyed);

	// Initialize transform cache via transform service (Phase 3 extraction)
	if (GridTransformService)
	{
		GridTransformService->DetectAndPropagateTransformChange(Hologram); // Initialize cache
	}

	// Create adapter to connect feature modules to this hologram
	CurrentAdapter = CreateHologramAdapter(Hologram);

	// Identify what we're trying to build (recipe/build class) for troubleshooting
	UClass* RecipeUClass = Hologram->GetRecipe();
	UClass* BuildUClass = Hologram->GetBuildClass();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BUILD SELECTION: Recipe=%s BuildClass=%s Hologram=%s"),
		*GetNameSafe(RecipeUClass), *GetNameSafe(BuildUClass), *Hologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("My log is running."));

	// Apply default ramp stepping if this is a ramp hologram (Task 76.3)
	// This sets a sensible default X-axis step based on the ramp's slope
	// Only applied once when ramp is first selected; user adjustments override this
	// NOTE: We check build class name instead of hologram type because ramps inherit from
	// AFGFoundationHologram and get detected as foundations in adapter creation
	if (BuildUClass && USFBuildableSizeRegistry::HasProfile(BuildUClass))
	{
		FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildUClass);
		float RampUnitHeight = USFBuildableSizeRegistry::GetRampUnitHeight(Profile);

		// Only apply default if this is a ramp (RampUnitHeight != 0) and StepsX is still unset (0)
		// Note: RampUnitHeight can be negative for descending structures (walkway ramps, catwalk stairs)
		if (RampUnitHeight != 0.0f && CounterState.StepsX == 0)
		{
			// Convert unit height to integer cm (rounding to nearest 100cm for clean values)
			int32 DefaultStepX = FMath::RoundToInt(RampUnitHeight / 100.0f) * 100;
			CounterState.StepsX = DefaultStepX;

			// Sync the updated state back to GridStateService if it exists
			if (GridStateService)
			{
				GridStateService->UpdateCounterState(CounterState);
			}

			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("🔧 RAMP DEFAULT STEPPING: Applied default StepsX=%dcm for %s (UnitHeight=%.1fcm)"),
				DefaultStepX, *Profile.BuildableClassName, RampUnitHeight);

			// Trigger HUD refresh to show the new stepping value
			UpdateCounterDisplay();
		}
	}

	// Apply default spacing for belt/pipe support poles (User Request, Issue #268)
	// Sets 54m (5400cm) spacing to facilitate bus building
	// Wall conveyor poles scale along Y, so default spacing goes on Y axis for them
	if (USFAutoConnectService::IsStackableSupportHologram(Hologram) || USFAutoConnectService::IsBeltSupportHologram(Hologram))
	{
		const bool bWallPole = USFAutoConnectService::IsWallConveyorPoleHologram(Hologram);
		int32& SpacingAxis = bWallPole ? CounterState.SpacingY : CounterState.SpacingX;

		if (SpacingAxis == 0)
		{
			SpacingAxis = 5400; // 54m (belts vanish at 55m due to belt length limit)

			// Sync the updated state back to GridStateService if it exists
			if (GridStateService)
			{
				GridStateService->UpdateCounterState(CounterState);
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 SUPPORT POLE: Applied default Spacing%s=5400cm for bus layout"),
				bWallPole ? TEXT("Y") : TEXT("X"));
			UpdateCounterDisplay();
		}
	}

	// Smart Auto-Connect: Check if this is a distributor hologram for auto-connect
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: Checking hologram type for auto-connect"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: AutoConnectService available: %s"), AutoConnectService ? TEXT("YES") : TEXT("NO"));

	if (AutoConnectService && AutoConnectService->IsDistributorHologram(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Distributor hologram detected - enabling auto-connect"));
		OnDistributorHologramUpdated(Hologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: Not a distributor hologram - auto-connect disabled"));
	}

	if (CurrentAdapter && CurrentAdapter->IsValid())
	{
		// Log hologram lock state for diagnostics (Task 38)
		const bool bIsLocked = Hologram->IsHologramLocked();
		const bool bCanLock = Hologram->CanLockHologram();
		const bool bCanNudge = Hologram->CanNudgeHologram();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HOLOGRAM REGISTERED: %s (%s adapter) - Ready for scaling | LOCK STATE: Locked=%s, CanLock=%s, CanNudge=%s"),
			*Hologram->GetName(), *CurrentAdapter->GetAdapterTypeName(),
			bIsLocked ? TEXT("YES") : TEXT("NO"),
			bCanLock ? TEXT("YES") : TEXT("NO"),
			bCanNudge ? TEXT("YES") : TEXT("NO"));

		// Debug: Check recipe cache initialization conditions
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ DEBUG: Cache init check - bHasStored=%d, Recipe=%s, CacheSize=%d"),
			bHasStoredProductionRecipe, StoredProductionRecipe ? TEXT("Valid") : TEXT("Null"), SortedFilteredRecipes.Num());

		// Initialize recipe cache now that hologram is registered
		// Cache should initialize for ANY factory building, not just when we have a stored recipe
		if (SortedFilteredRecipes.Num() == 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Initializing recipe cache after hologram registration"));
			SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
			if (SortedFilteredRecipes.Num() > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe cache populated: %d recipes available"), SortedFilteredRecipes.Num());

				// Restore active recipe from stored recipe if available and no manual selection is active
				if (bHasStoredProductionRecipe && StoredProductionRecipe && ActiveRecipeSource != ERecipeSource::ManuallySelected)
				{
					// Find the stored recipe in the new cache
					for (int32 i = 0; i < SortedFilteredRecipes.Num(); i++)
					{
						if (SortedFilteredRecipes[i] == StoredProductionRecipe)
						{
							ActiveRecipe = StoredProductionRecipe;
							CurrentRecipeIndex = i;
							ActiveRecipeSource = ERecipeSource::Copied;
							StoredRecipeDisplayName = GetRecipeDisplayName(ActiveRecipe);

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Restored active recipe from stored: %s [%d/%d]"),
								*StoredRecipeDisplayName, i + 1, SortedFilteredRecipes.Num());
							break;
						}
					}
				}

				UpdateCounterDisplay(); // Refresh display with populated cache
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ DEBUG: Cache already exists with %d recipes"), SortedFilteredRecipes.Num());
		}

		// ========================================
		// Recipe Copying System - Use existing stored recipe
		// ========================================

		// Recipe storage should only come from existing buildings via StoreProductionRecipeFromBuilding
		// Holograms only have build recipes, not production recipes
		// We preserve any existing stored production recipe for inheritance

		if (!Hologram->GetParentHologram()) // Only log for parent holograms
		{
			if (bHasStoredProductionRecipe)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: Using stored production recipe %s for child inheritance"),
					*StoredProductionRecipe->GetName());
			}
			else
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: No stored production recipe - middle-click an existing building to copy its recipe"));
			}
		}

		// Calculate and cache building size once at registration
		// This size is reused for all child positioning to avoid repeated adapter queries
		// Priority: Use validated registry dimensions if available, otherwise fall back to adapter bounds
		bool bBuildingSupportsScaling = true;  // Default to true for unknown buildings (safe fallback)

		if (USFBuildableSizeRegistry::HasProfile(BuildUClass))
		{
			// Use validated dimensions from registry
			AFGBuildableHologram* BuildableHologram = Cast<AFGBuildableHologram>(Hologram);
			if (BuildableHologram)
			{
				FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildUClass);
				CachedBuildingSize = USFBuildableSizeRegistry::GetSizeForHologram(BuildableHologram);
				CachedAnchorOffset = Profile.AnchorOffset;
				bBuildingSupportsScaling = Profile.bSupportsScaling;  // Issue #164: Check if building is scalable

				if (!CachedAnchorOffset.IsZero())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ CACHED BUILDING SIZE: %s | ANCHOR OFFSET: %s | Source: %s | Scalable: %s"),
						*CachedBuildingSize.ToString(), *CachedAnchorOffset.ToString(), *Profile.SourceFile,
						bBuildingSupportsScaling ? TEXT("YES") : TEXT("NO"));
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ CACHED BUILDING SIZE: %s | Source: %s | Scalable: %s"),
						*CachedBuildingSize.ToString(), *Profile.SourceFile,
						bBuildingSupportsScaling ? TEXT("YES") : TEXT("NO"));
				}
			}
			else
			{
				// Shouldn't happen - HasProfile returned true but hologram isn't buildable
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ Registry has profile but hologram is not AFGBuildableHologram - using adapter fallback"));
				const FBoxSphereBounds AdapterBounds = CurrentAdapter->GetBuildingBounds();
				CachedBuildingSize = AdapterBounds.BoxExtent * 2.0f;
				CachedAnchorOffset = FVector::ZeroVector;
			}
		}
		else
		{
			// Fallback: Use adapter bounds for unknown/modded buildables
			const FBoxSphereBounds AdapterBounds = CurrentAdapter->GetBuildingBounds();
			CachedBuildingSize = AdapterBounds.BoxExtent * 2.0f;
			CachedAnchorOffset = FVector::ZeroVector;

			// Validate and clamp to reasonable range
			const float MaxReasonableSize = 2000.0f; // 20 meters
			if (CachedBuildingSize.X > 1.f && CachedBuildingSize.Y > 1.f && CachedBuildingSize.Z > 1.f)
			{
				CachedBuildingSize.X = FMath::Min(CachedBuildingSize.X, MaxReasonableSize);
				CachedBuildingSize.Y = FMath::Min(CachedBuildingSize.Y, MaxReasonableSize);
				CachedBuildingSize.Z = FMath::Min(CachedBuildingSize.Z, MaxReasonableSize);
			}
			else
			{
				// Invalid size, use default
				CachedBuildingSize = FVector(800.0f, 800.0f, 100.0f);
			}

			UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ CACHED BUILDING SIZE: %s via %s fallback (unknown buildable - consider adding to registry)"),
				*CachedBuildingSize.ToString(), *CurrentAdapter->GetAdapterTypeName());
		}

		// Task 52 FIX: Reset lock ownership for new hologram
		// When building while holding modifiers, vanilla releases the lock on hologram transition.
		// We need to reset our ownership flag and re-acquire if modifiers are still held.
		bLockedByModifier = false;

		// If any modal features are still active (modifiers held during build), re-acquire lock
		if (IsAnyModalFeatureActive())
		{
			TryAcquireHologramLock();
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔒 Lock re-acquired for new hologram (modifiers still held)"));
		}

#if SMART_ARROWS_ENABLED
		// Issue #164: Only attach arrows to scalable buildings
		// Non-scalable buildings (belts, pipes, extractors) should not show scaling arrows
		if (bBuildingSupportsScaling)
		{
			// Attach arrows to hologram
			if (ArrowModule && ArrowModule->IsInitialized())
			{
				USceneComponent* RootComponent = Hologram->GetRootComponent();

				if (RootComponent && ArrowModule->AttachToHologram(RootComponent))
				{
					// Reset arrow state for new hologram - should show all 3 arrows initially
					LastAxisInput = ELastAxisInput::None;
					ArrowModule->SetHighlightedAxis(ELastAxisInput::None);

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ ARROWS ATTACHED TO HOLOGRAM - State reset to show all 3 arrows"));
				}
				else
				{
					// AttachToHologram returns false when assets are still loading (async)
					// The deferred attachment system will complete the attachment when assets are ready
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏳ DEFERRED ARROW ATTACHMENT - Waiting for assets to load (arrows will appear shortly)"));
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, Error, TEXT("❌ ARROW MODULE NOT READY"));
			}
		}
		else
		{
			// Building does not support scaling - skip arrow attachment
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏭️ ARROWS SKIPPED - Building does not support scaling (Issue #164)"));
		}
#endif
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM REGISTERED: %s - Adapter creation failed"), *Hologram->GetName());
	}

	// Initial auto-connect check for distributor holograms (fixes missing previews on first selection)
	if (Hologram && AutoConnectService && AutoConnectService->IsDistributorHologram(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT: Initial check for newly selected distributor hologram: %s"), *Hologram->GetName());
		OnDistributorHologramUpdated(Hologram);

		// Issue #269: Post-registration refresh with FORCE RECREATE on next tick
		// At 1x1x1 (no children), OnGridChanged never fires so the forced initial evaluation
		// is the only opportunity for a clean slate. OnDistributorsMoved (non-forced) was insufficient.
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AFGHologram> WeakHolo = Hologram;
			FTimerDelegate D;
			D.BindLambda([this, WeakHolo]()
			{
				if (!WeakHolo.IsValid()) return;
				if (USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(WeakHolo.Get()))
				{
					Orchestrator->OnGridChanged();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Post-registration forced refresh (next tick) for %s"), *WeakHolo->GetName());
				}
			});
			World->GetTimerManager().SetTimerForNextTick(D);
		}
	}
	ActiveHologram = Hologram;

	if (bRestoredExtendActiveAtRegister && ExtendService)
	{
		TSharedPtr<FSFCloneTopology> RestoredTemplate = ExtendService->GetLastCloneTopology();
		if (RestoredTemplate.IsValid() && RestoredTemplate->ChildHolograms.Num() > 0)
		{
			ExtendService->ReplayRestoreCloneTopology(Hologram, *RestoredTemplate);
			SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore][Extend] Reattached staged topology to new active hologram: parent=%s templateChildren=%d"),
				*GetNameSafe(Hologram),
				RestoredTemplate->ChildHolograms.Num());
		}
	}

	// Issue #208/#209: Notify recipe service of build class change
	// OnNewBuildSession will bump session ID only if the build class differs from the shard source
	if (RecipeManagementService)
	{
		RecipeManagementService->OnNewBuildSession(Hologram->GetBuildClass());
	}

	// Notify external systems (like SmartCamera)
	if (OnHologramCreated.IsBound())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎥 Broadcasting OnHologramCreated to external systems"));
		OnHologramCreated.Broadcast(Hologram);
	}
}

void USFSubsystem::UnregisterActiveHologram(AFGHologram* Hologram)
{
	if (Hologram && ActiveHologram.Get() == Hologram)
	{
		// Notify external systems before cleanup
		if (OnHologramDestroyed.IsBound())
		{
			OnHologramDestroyed.Broadcast(Hologram);
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Unregistering active hologram: %s"), *Hologram->GetName());

		// Issue #281: Remove Smart! hints from hint bar
		if (HintBarService)
		{
			HintBarService->OnHologramUnregistered();
		}

		// CRITICAL: Suppress child updates during mass destruction to prevent UObject exhaustion
		// When destroying 700+ children, each OnChildHologramDestroyed would trigger expensive updates
		bSuppressChildUpdates = true;

		// Clean up spawned children from HologramHelper (Phase 3 fix)
		if (HologramHelper)
		{
			TArray<TWeakObjectPtr<AFGHologram>> CurrentChildren = HologramHelper->GetSpawnedChildren();
			for (TWeakObjectPtr<AFGHologram>& Child : CurrentChildren)
			{
				if (Child.IsValid())
				{
					QueueChildForDestroy(Child.Get());
				}
			}
		}

		// Re-enable updates and clear mass destruction flag after manual cleanup complete
		bSuppressChildUpdates = false;
		bInMassDestruction = false;

		ActiveHologram.Reset();
		CurrentScalingOffset = FVector::ZeroVector;

		// Clear Smart Dismantle blueprint proxy for this build session (Issue #166)
		if (CurrentBuildProxy.IsValid())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🏗️ SMART DISMANTLE: Build session ended - clearing proxy %s"), *CurrentBuildProxy->GetName());
			CurrentBuildProxy.Reset();
			CurrentProxyOwner.Reset();
		}

		// Clear transform cache via transform service (Phase 3 extraction)
		if (GridTransformService)
		{
			GridTransformService->ClearCache();
		}

		// Reset multi-step hologram property cache (Issue #200)
		CachedParentFixtureAngle = INT_MIN;
		CachedParentBuildStep = 0;

		// Reset auto-hold state (Issue #273)
		bAutoHoldActive = false;
		bAutoHoldUserOverrode = false;

		// Clear adapter
		CurrentAdapter.Reset();

		// Clear auto-connect belt preview helpers when hologram is cancelled
		// This ensures belt holograms don't persist after build cancellation
		if (AutoConnectService)
		{
			AutoConnectService->ClearBeltPreviewHelpers();
			// Clear all pipe managers (previews are managed by auto-connect service now)
			AutoConnectService->ClearAllPipeManagers();
		}

		// Clean up EXTEND service children when hologram is cancelled
		// Always call cleanup - it's safe even if EXTEND wasn't active (just clears empty arrays)
		// This ensures stale child holograms are destroyed when switching away from EXTEND
		if (ExtendService)
		{
			ExtendService->CleanupExtension(Hologram);
		}

#if SMART_ARROWS_ENABLED
		// Detach arrows from hologram and reset state
		if (ArrowModule)
		{
			ArrowModule->DetachFromHologram();
			LastAxisInput = ELastAxisInput::None;
			ArrowModule->SetHighlightedAxis(ELastAxisInput::None);
		}
#endif

		// Reset all counters (fixes persistence bug - Task 51), except while a
		// restored Extend topology is staged for editing across transient aim gaps.
		if (!IsRestoredExtendModeActive())
		{
			ResetCounters();
		}
		else
		{
			UpdateCounterDisplay();
			SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore][Extend] Preserved staged counters across hologram unregister: grid=[%d,%d,%d]"),
				CounterState.GridCounters.X,
				CounterState.GridCounters.Y,
				CounterState.GridCounters.Z);
		}

		// CRITICAL FIX: Reset auto-connect runtime settings so next build session loads from global config
		// Without this, runtime settings persist between build sessions, ignoring global config changes
		AutoConnectRuntimeSettings.bInitialized = false;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Reset auto-connect runtime settings (will reload from config on next hologram)"));
	}
	else
	{
		if (Hologram)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM UNREGISTER FAILED: %s not currently active"), *Hologram->GetName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM UNREGISTER FAILED: Null hologram pointer"));
		}
	}
}

