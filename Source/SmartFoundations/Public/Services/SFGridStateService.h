#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HUD/SFHUDTypes.h" // for FSFCounterState
#include "SFGridStateService.generated.h"

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

    // Modal adjustment helpers (mouse wheel / value increase/decrease)
    void AdjustSpacing(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction)
    {
        constexpr int32 INCREMENT = 50;
        const int32 Delta = Direction * INCREMENT * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X: State.SpacingX += Delta; break;
        case ESFScaleAxis::Y: State.SpacingY += Delta; break;
        case ESFScaleAxis::Z: State.SpacingZ += Delta; break;
        default: break;
        }
    }

    void AdjustSteps(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction)
    {
        constexpr int32 INCREMENT = 50;
        const int32 Delta = Direction * INCREMENT * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X: State.StepsX += Delta; break;
        case ESFScaleAxis::Y: State.StepsY += Delta; break;
        default: break;
        }
    }

    void AdjustStagger(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction)
    {
        constexpr int32 INCREMENT = 50;
        const int32 Delta = Direction * INCREMENT * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::X:  State.StaggerX  += Delta; break;
        case ESFScaleAxis::Y:  State.StaggerY  += Delta; break;
        case ESFScaleAxis::ZX: State.StaggerZX += Delta; break;
        case ESFScaleAxis::ZY: State.StaggerZY += Delta; break;
        default: break;
        }
    }

    void AdjustRotation(FSFCounterState& State, ESFScaleAxis Axis, int32 AccumulatedSteps, int32 Direction)
    {
        // Rotation uses degrees, 5° per step for fine control
        constexpr float INCREMENT = 5.0f;
        const float Delta = Direction * INCREMENT * AccumulatedSteps;
        switch (Axis)
        {
        case ESFScaleAxis::Z: State.RotationZ += Delta; break;
        // Phase 2 will add X and Y axes
        default: break;
        }
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
        bool bRotationModeActive = false)
    {
        if (bSpacingModeActive)
        {
            AdjustSpacing(State, State.SpacingAxis, AccumulatedSteps, Direction);
            return EValueAdjustResult::CountersChanged;
        }
        if (bStepsModeActive)
        {
            AdjustSteps(State, State.StepsAxis, AccumulatedSteps, Direction);
            return EValueAdjustResult::CountersChanged;
        }
        if (bStaggerModeActive)
        {
            AdjustStagger(State, State.StaggerAxis, AccumulatedSteps, Direction);
            return EValueAdjustResult::CountersChanged;
        }
        if (bRotationModeActive)
        {
            AdjustRotation(State, State.RotationAxis, AccumulatedSteps, Direction);
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
