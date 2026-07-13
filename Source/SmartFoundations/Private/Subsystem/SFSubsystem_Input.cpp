// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - input-mode handlers (spacing/steps/stagger/rotation/toggles) + hologram-lock helpers.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"
#include "Features/Walk/SFWalkService.h"

// [#209] Re-tap window: a mode-key press this soon after its own release advances the transform
// target (stagger: flips the family) - the numpad-free "next axis" gesture. Hold semantics are
// untouched; a single stray tap just flickers the mode on/off as it always has.
static constexpr double SFModeKeyReTapSeconds = 0.3;


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

bool USFSubsystem::ShouldSuppressBuildGunScroll(const AFGHologram* BuildGunHologram) const
{
    // Only speak for the hologram Smart! is actively managing.
    if (!BuildGunHologram || ActiveHologram.Get() != BuildGunHologram)
    {
        return false;
    }

    // IsAnyModalFeatureActive covers the open modifier window even when the lock was NOT acquired
    // (vanilla hold already owned it when the modifier went down). bLockedByModifier/bAutoHoldActive
    // cover Smart!-owned locks outside a window (auto-hold after a grid change): InfiniteNudge treats
    // ANY locked hologram as rotatable/scalable per-child, which shears a Smart! grid apart - so while
    // the lock is ours, the wheel stays Smart!'s. A user-engaged lock falls through to vanilla/IN.
    return IsAnyModalFeatureActive() || bLockedByModifier || bAutoHoldActive;
}

bool USFSubsystem::IsAnyModalFeatureActive() const
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        // Also check EXTEND mode (ExtendService) and Smart Walking (#356), both top-level modes
        return InputHandler->IsAnyModalFeatureActive() || IsExtendModeActive() || bWalkModeActive;
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

    // Smart Walking (#356): the camera latches the path FRONTIER — the active (last) segment's exit frame —
    // NOT a furthest-by-axis grid value. A turning/looping path breaks the "furthest cell" assumption, so the
    // frontier (the same exit frame the segment table's compass column reports) is the correct focus target.
    if (bWalkModeActive && WalkService && WalkService->IsActive() && WalkService->GetSegmentCount() > 0)
    {
        return WalkService->GetHeadFrame().GetLocation();
    }

    // Get current grid counters
    const FIntVector& CurrentGridCounters = GetGridCounters();

    // If grid is 1x1x1 (no extension), return parent position
    if (CurrentGridCounters.X == 1 && CurrentGridCounters.Y == 1 && CurrentGridCounters.Z == 1)
    {
        return ActiveHologram->GetActorLocation();
    }

    // [#373] Smart Restore of an Extend pattern is its OWN focus path - it has no live extend target, so
    // IsScaledExtendActive() is false and the generic grid path below would frame the MIRROR of the run
    // (the reported "camera follows the opposite of the scaled direction"). Read the furthest restored
    // clone from the same placement math the restore uses, so the focus matches the actual clones.
    if (ExtendService && ExtendService->IsRestoredCloneTopologyActive())
    {
        return ExtendService->GetFurthestRestoredCloneWorldPosition(ActiveHologram->GetActorLocation());
    }

    // Calculate furthest grid position based on direction
    // The "furthest top" is at the corner furthest from the parent (X,Y)
    // and at the TOP Z level (highest)
    int32 FurthestX, FurthestY, TopZ;

    // [#373] Scaled Extend uses a SEPARATE focus computation from normal grid scaling (the "two camera
    // functions"). Normal grid is symmetric: the cell sign follows the grid counter sign, so the generic
    // CalculateChildPosition path below works. Extend is NOT: once you pick a side, the run grows by the
    // ABSOLUTE scroll magnitude in that one direction (XDirectionSign = Right:+1 / Left:-1), and the real
    // clones are placed along the SOURCE's local X (forward) with spacing/steps/arc/rows by
    // SFExtendScaledService - not along the Y axis the old #275 mapping assumed. Re-deriving that placement
    // here would drift from it, so instead read the furthest ACTUAL clone the extend service already
    // computed (source location + its world offset). This frames the leading factory building whichever
    // way the run extends, on both SP and a client.
    const bool bScaledExtend = ExtendService && ExtendService->IsScaledExtendActive();

    if (bScaledExtend)
    {
        return ExtendService->GetFurthestScaledCloneWorldPosition(ActiveHologram->GetActorLocation());
    }
    else
    {
        // Normal grid mode: counters directly map to axes
        // X direction
        if (CurrentGridCounters.X > 0)
        {
            FurthestX = CurrentGridCounters.X - 1;
        }
        else if (CurrentGridCounters.X < 0)
        {
            FurthestX = -(FMath::Abs(CurrentGridCounters.X) - 1);
        }
        else
        {
            FurthestX = 0;
        }

        // Y direction
        if (CurrentGridCounters.Y > 0)
        {
            FurthestY = CurrentGridCounters.Y - 1;
        }
        else if (CurrentGridCounters.Y < 0)
        {
            FurthestY = -(FMath::Abs(CurrentGridCounters.Y) - 1);
        }
        else
        {
            FurthestY = 0;
        }

        // Z direction
        if (CurrentGridCounters.Z > 0)
        {
            TopZ = CurrentGridCounters.Z - 1;
        }
        else if (CurrentGridCounters.Z < 0)
        {
            TopZ = -(FMath::Abs(CurrentGridCounters.Z) - 1);
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
        // [#209] Re-tap (quick release -> re-press) advances the spacing target (classic: next
        // axis; Player Relative: next slot) - same operation as Num0, no numpad needed.
        if (FPlatformTime::Seconds() - LastSpacingModeReleaseSeconds < SFModeKeyReTapSeconds)
        {
            OnCycleAxis();
        }

        UE_LOG(LogSmartFoundations, Verbose, TEXT("\U0001F527 Spacing Mode: ACTIVE (Semicolon held) - Current axis: %s"),
            *UEnum::GetValueAsString(CounterState.SpacingAxis));
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Counters: X=%d cm, Y=%d cm, Z=%d cm"),
            CounterState.SpacingX, CounterState.SpacingY, CounterState.SpacingZ);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        LastSpacingModeReleaseSeconds = FPlatformTime::Seconds();
        UE_LOG(LogSmartFoundations, Verbose, TEXT("\U0001F527 Spacing Mode: Inactive (Semicolon released)"));

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
        // [#209] Re-tap advances the steps target (classic: X/Y toggle; PR: fwd/side slot).
        if (FPlatformTime::Seconds() - LastStepsModeReleaseSeconds < SFModeKeyReTapSeconds)
        {
            OnCycleAxis();
        }

        // X-axis: Columns (constant X) step up based on X position
        // Y-axis: Rows (constant Y) step up based on Y position
        const TCHAR* AxisName = (CounterState.StepsAxis == ESFScaleAxis::X) ? TEXT("X (columns)") : TEXT("Y (rows)");
        const int32 CurrentCounter = (CounterState.StepsAxis == ESFScaleAxis::X) ? CounterState.StepsX : CounterState.StepsY;

        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 Steps Mode: ACTIVE (I held) - Axis: %s"), AxisName);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current counter: %d units (Num0 to toggle X/Y)"), CurrentCounter);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        LastStepsModeReleaseSeconds = FPlatformTime::Seconds();
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 Steps Mode: Inactive (I released)"));

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
    // [#209] Player Relative: Num0 (and re-tap) advance the active transform's TARGET SLOT -
    // fwd -> side (-> vert for spacing) - the numpad-free selector. STAGGER is the one exception,
    // aligned with classic: Num0 toggles the FAMILY (Z on/off) and the drift direction rides
    // re-tap/Num6/4 - the family is the bigger mental switch and gets the dedicated key.
    else if ((bSpacingModeActive || bStepsModeActive || bStaggerModeActive || bRotationModeActive)
             && IsPlayerRelativeEnabled()
             && !(WalkService && WalkService->IsActive()))  // walk ignores PR target slots - classic below
    {
        if (bStaggerModeActive)
        {
            ToggleStaggerFamilyProjection();
        }
        else
        {
            AdvancePlayerRelativeSlot();
        }
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
        // [#209] Stagger navigates as FAMILY x AXIS. Classic: Num0 toggles the FAMILY (Z on/off -
        // ZX<->X / ZY<->Y, keeping the direction) and a stagger-key re-tap toggles the direction
        // within it; Num9/3 jump straight to Stack/Flat. Replaces the old ZX -> ZY -> X -> Y 4-way
        // cycle - same four modes, stored state unchanged. [feel-tested swap, 2026-07-09]
        if (GridStateService)
        {
            GridStateService->SetStaggerFamily(CounterState,
                !USFGridStateService::IsStaggerStackFamily(CounterState.StaggerAxis));
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
	else if (bRotationModeActive)
	{
		// Rotation mode: toggle the PROGRESSION axis X <-> Y via service.
		// Rotation is always yaw (buildings stay upright); this only chooses whether the
		// yaw builds up along X-clones (run curves) or Y-rows (rows fan out).
		if (GridStateService)
		{
			GridStateService->CycleRotationAxis(CounterState);
		}

		const TCHAR* AxisDescription = (CounterState.RotationAxis == ESFScaleAxis::Y)
			? TEXT("Y (rows fan out)")
			: TEXT("X (curve along the run)");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Rotation Progression Axis Toggled: Now %s"), AxisDescription);
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
		// [#209] Stagger's two selectors each get a gesture, IDENTICAL in both control modes
		// (classic feel-tested, PR aligned): re-tap = DIRECTION within the family (classic
		// ZX<->ZY / X<->Y, PR fwd<->side drift), Num0 = FAMILY (Z on/off). All four modes are
		// reachable with no numpad.
		if (FPlatformTime::Seconds() - LastStaggerModeReleaseSeconds < SFModeKeyReTapSeconds)
		{
			if (IsPlayerRelativeEnabled() && !(WalkService && WalkService->IsActive()))
			{
				AdvancePlayerRelativeSlot();
			}
			else if (GridStateService)
			{
				GridStateService->ToggleStaggerAxisInFamily(CounterState);
				UpdateCounterState(CounterState);
			}
		}

		UE_LOG(LogSmartFoundations, Verbose, TEXT(" Stagger Mode: ACTIVE (Y held) - Axis: %s"),
			*UEnum::GetValueAsString(CounterState.StaggerAxis));
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   (Num0/re-tap: axis in family | Num9/3: Stack/Flat family)"));

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		LastStaggerModeReleaseSeconds = FPlatformTime::Seconds();
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
		// [#209] Re-tap advances the rotation progression target (classic: X/Y; PR: fwd/side).
		if (FPlatformTime::Seconds() - LastRotationModeReleaseSeconds < SFModeKeyReTapSeconds)
		{
			OnCycleAxis();
		}

		// Rotation is ALWAYS yaw (horizontal arc, upright). Num0 toggles the PROGRESSION axis:
		// X (the run curves as it extends) vs Y (the rows fan out).
		const TCHAR* ProgressDesc = (CounterState.RotationAxis == ESFScaleAxis::Y)
			? TEXT("Y (rows fan out)")
			: TEXT("X (curve along the run)");
		UE_LOG(LogSmartFoundations, Verbose, TEXT(" Rotation Mode: ACTIVE (Comma held) - Progression: %s"), ProgressDesc);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current rotation: %.1f° (Num0 to toggle progression axis X/Y)"), CounterState.RotationZ);

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		LastRotationModeReleaseSeconds = FPlatformTime::Seconds();
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
				int32 ArrowDirectionSign = 0;
				const ELastAxisInput ArrowAxis = GetEffectiveArrowAxisInput(&ArrowDirectionSign);
				ArrowModule->UpdateArrows(World, CurrentTransform, ArrowAxis, true, ArrowDirectionSign);
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
			LastKnownAxisInput = GetEffectiveArrowAxisInput(&LastKnownArrowDirectionSign);
			LastChildCount = 0;  // Reset child count when toggling visibility
		}
	}
#else
	UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrows feature is currently disabled (SMART_ARROWS_ENABLED=0)"));
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
	UE_LOG(LogSmartFoundations, Verbose, TEXT("!!! K KEY PRESSED !!! (OnToggleSettingsForm)"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Settings Form Toggle (Phase 0 validation)"));

	// Smart Walking (#356): while a walk is engaged, K toggles the WALK panel in lieu of the Smart Panel (hide to steer
	// with a clean screen / restore to review the segment path). The walk seed is a belt/pipe — itself upgrade-capable —
	// so this MUST run before the upgrade-panel routing below, or K would open the Upgrade Panel mid-walk instead.
	if (WalkService && WalkService->IsActive())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: walk active → toggling Walk panel in lieu of Smart Panel"));
		ToggleWalkPanel();
		return;
	}

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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: No valid PlayerController available"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance and add to viewport
	USmartSettingsFormWidget* WidgetInstance = CreateWidget<USmartSettingsFormWidget>(PC, WidgetClass);

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Failed to create widget instance"));
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
    // [#390] Exclude "Stackable": the Stackable Pipeline Support's hologram is named
    // Holo_PipelineStackable_C — it contains "Pipeline" but NOT "Support", so without this
    // exclusion it was misclassified as upgrade-capable and opened the Upgrade panel on K
    // instead of the Smart! Panel. It's a scaling target (like the stackable conveyor pole,
    // which similarly isn't caught by the ConveyorBelt check above), not a pipe to upgrade.
    if (ClassName.Contains(TEXT("Pipeline")) && !ClassName.Contains(TEXT("Junction")) && !ClassName.Contains(TEXT("Pump")) && !ClassName.Contains(TEXT("Support")) && !ClassName.Contains(TEXT("Stackable")))
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: No valid PlayerController available"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance - try our C++ class first, fall back to UUserWidget if Blueprint not reparented
	USmartUpgradePanel* SmartPanelInstance = CreateWidget<USmartUpgradePanel>(PC, WidgetClass);
	UUserWidget* WidgetInstance = SmartPanelInstance;

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: CreateWidget<USmartUpgradePanel> failed, trying UUserWidget fallback"));
		// Blueprint may not be reparented yet - fall back to base UUserWidget
		WidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	}

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: Failed to create widget instance - Blueprint may need to be reparented to UserWidget in Unreal Editor"));
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: Open Smart_UpgradePanel_Widget in UE Editor, go to Class Settings, and set Parent Class to UserWidget or SmartUpgradePanel"));
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
			UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Panel: CloseButton not found in widget"));
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
