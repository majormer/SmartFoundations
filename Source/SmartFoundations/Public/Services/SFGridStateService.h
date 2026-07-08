// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HUD/SFHUDTypes.h" // for FSFCounterState
#include "SFGridStateService.generated.h"

/**
 * [#217] Per-transform scroll-wheel increments, resolved from config (quantized/floored/clamped)
 * and cached on the subsystem. Distance fields are centimeters (grid state is integer cm); rotation
 * is degrees. Shared 1:1 by the grid path and the Smart Walking path (Advance=Spacing, Rise=Steps,
 * Shift=Stagger, Turn=Rotation). Defaults mirror the previous hardcoded behavior.
 */
struct FSFScrollIncrements
{
    int32 SpacingCm = 50;
    int32 StepsCm = 50;
    int32 StaggerCm = 50;
    float RotationDeg = 5.0f;
};

UCLASS()
class SMARTFOUNDATIONS_API USFGridStateService : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UObject* InOuter) { /* reserved for future use */ }

    const FSFCounterState& GetCounterState() const { return CounterState; }

    void UpdateCounterState(const FSFCounterState& NewState)
    {
        CounterState = NewState;
    }

    void Reset()
    {
        CounterState.Reset();
    }

    // Axis cycle helpers used by subsystem forwarders
    void CycleSpacingAxis(FSFCounterState& State)
    {
        switch (State.SpacingAxis)
        {
        case ESFScaleAxis::X: State.SpacingAxis = ESFScaleAxis::Y; break;
        case ESFScaleAxis::Y: State.SpacingAxis = ESFScaleAxis::Z; break;
        case ESFScaleAxis::Z: State.SpacingAxis = ESFScaleAxis::X; break;
        default: break;
        }
    }

    void ToggleStepsAxis(FSFCounterState& State)
    {
        // Steps only uses X and Y
        State.StepsAxis = (State.StepsAxis == ESFScaleAxis::X) ? ESFScaleAxis::Y : ESFScaleAxis::X;
    }

    void CycleStaggerAxis(FSFCounterState& State)
    {
        // Cycle order: ZX → ZY → X → Y (ZX first for auto-connect distributor grids)
        switch (State.StaggerAxis)
        {
        case ESFScaleAxis::ZX: State.StaggerAxis = ESFScaleAxis::ZY; break;
        case ESFScaleAxis::ZY: State.StaggerAxis = ESFScaleAxis::X;  break;
        case ESFScaleAxis::X:  State.StaggerAxis = ESFScaleAxis::Y;  break;
        case ESFScaleAxis::Y:  default: State.StaggerAxis = ESFScaleAxis::ZX; break;
        }
    }

    void CycleRotationAxis(FSFCounterState& State)
    {
        // Rotation is ALWAYS yaw (around vertical Z); buildings stay upright. This only chooses
        // whether the yaw progresses along X-clones (run curves) or Y-rows (rows fan out).
        // Toggle X <-> Y only.
        State.RotationAxis = (State.RotationAxis == ESFScaleAxis::X) ? ESFScaleAxis::Y : ESFScaleAxis::X;
    }

    // Apply grid counter scaling with forbidden value skipping (0, -1)
    // Returns previous value for logging/comparison
    int32 ApplyAxisScaling(FSFCounterState& State, ESFScaleAxis Axis, int32 StepDelta)
    {
        int32* CounterPtr = nullptr;
        switch (Axis)
        {
        case ESFScaleAxis::X: CounterPtr = &State.GridCounters.X; break;
        case ESFScaleAxis::Y: CounterPtr = &State.GridCounters.Y; break;
        case ESFScaleAxis::Z: CounterPtr = &State.GridCounters.Z; break;
        default: return 0;
        }

        const int32 PreviousValue = *CounterPtr;  // Capture for return
        int32 NewValue = *CounterPtr + StepDelta;
        
        // Skip forbidden values: 0 and -1
        if (NewValue == 0)
        {
            NewValue = (StepDelta > 0) ? 1 : -2;
        }
        else if (NewValue == -1)
        {
            NewValue = (StepDelta > 0) ? 1 : -2;
        }
        
        *CounterPtr = NewValue;
        return PreviousValue;
    }

    // Modal adjustment helpers (mouse wheel / value increase/decrease).
    // [#217] The per-notch increment is passed in (config-driven) rather than hardcoded.
    void AdjustSpacing(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction, int32 IncrementCm)
    {
        const int32 Delta = Direction * IncrementCm * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X: State.SpacingX += Delta; break;
        case ESFScaleAxis::Y: State.SpacingY += Delta; break;
        case ESFScaleAxis::Z: State.SpacingZ += Delta; break;
        default: break;
        }
    }

    void AdjustSteps(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction, int32 IncrementCm)
    {
        const int32 Delta = Direction * IncrementCm * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X: State.StepsX += Delta; break;
        case ESFScaleAxis::Y: State.StepsY += Delta; break;
        default: break;
        }
    }

    void AdjustStagger(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction, int32 IncrementCm)
    {
        const int32 Delta = Direction * IncrementCm * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X:  State.StaggerX  += Delta; break;
        case ESFScaleAxis::Y:  State.StaggerY  += Delta; break;
        case ESFScaleAxis::ZX: State.StaggerZX += Delta; break;
        case ESFScaleAxis::ZY: State.StaggerZY += Delta; break;
        default: break;
        }
    }

    void AdjustRotation(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction, float IncrementDeg)
    {
        // The PROGRESSION axis (X-clones vs Y-rows) doesn't change the angle magnitude, but the Y
        // progression swaps the X/Y roles in the arc parametrization (a reflection) - so the SAME
        // stored RotationZ sign curls the OPPOSITE way on Y vs X. Negate the delta for Y so scroll-up
        // curls RIGHT consistently on both axes ("away = right"). [#209 feel-test, 2026-07-07]
        // (Side effect: the stored RotationZ sign is inverted for Y-rows, so the HUD shows a negative
        //  angle for a right curl there - cosmetic; the curl direction is what matters.)
        const int32 CurlDir = (Axis == ESFScaleAxis::Y) ? -Direction : Direction;
        const float Delta = CurlDir * IncrementDeg * AccumulatedSteps;
        State.RotationZ += Delta;
    }

    // Unified value adjustment routing
    enum class EValueAdjustResult : uint8
    {
        None = 0,
        CountersChanged = 1,   // spacing/steps/stagger adjusted
        RecipeChanged = 2       // caller should handle recipe cycling
    };

    EValueAdjustResult DispatchValueAdjust(
        FSFCounterState& State,
        int32 AccumulatedSteps,
        int32 Direction,
        bool bRecipeModeActive,
        bool bSpacingModeActive,
        bool bStepsModeActive,
        bool bStaggerModeActive,
        bool bRotationModeActive,
        const FSFScrollIncrements& Inc)
    {
        if (bSpacingModeActive)
        {
            AdjustSpacing(State, State.SpacingAxis, AccumulatedSteps, Direction, Inc.SpacingCm);
            return EValueAdjustResult::CountersChanged;
        }
        if (bStepsModeActive)
        {
            AdjustSteps(State, State.StepsAxis, AccumulatedSteps, Direction, Inc.StepsCm);
            return EValueAdjustResult::CountersChanged;
        }
        if (bStaggerModeActive)
        {
            AdjustStagger(State, State.StaggerAxis, AccumulatedSteps, Direction, Inc.StaggerCm);
            return EValueAdjustResult::CountersChanged;
        }
        if (bRotationModeActive)
        {
            AdjustRotation(State, State.RotationAxis, AccumulatedSteps, Direction, Inc.RotationDeg);
            return EValueAdjustResult::CountersChanged;
        }
        if (bRecipeModeActive)
        {
            return EValueAdjustResult::RecipeChanged; // caller cycles recipe
        }
        return EValueAdjustResult::None;
    }

private:
    FSFCounterState CounterState;
};
