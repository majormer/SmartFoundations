// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Resources/FGPowerShardDescriptor.h"

enum class ESFFactorySettingsApplyDecision : uint8
{
    Abort,
    Retry,
    Apply,
    GiveUp
};

/** Pure readiness/value policy shared by every factory-settings construction path. */
namespace FSFFactorySettingsApplyPolicy
{
    constexpr ESFFactorySettingsApplyDecision Decide(
        const bool bFactoryValid,
        const bool bActorHasBegunPlay,
        const bool bNeedsInventoryTransfer,
        const bool bPotentialInventoryReady,
        const bool bPlayerInventoryReady,
        const int32 AttemptNumber,
        const int32 MaxAttempts)
    {
        if (!bFactoryValid)
        {
            return ESFFactorySettingsApplyDecision::Abort;
        }
        const bool bReady = bActorHasBegunPlay
            && (!bNeedsInventoryTransfer || (bPotentialInventoryReady && bPlayerInventoryReady));
        if (bReady)
        {
            return ESFFactorySettingsApplyDecision::Apply;
        }
        return AttemptNumber >= MaxAttempts
            ? ESFFactorySettingsApplyDecision::GiveUp
            : ESFFactorySettingsApplyDecision::Retry;
    }

    inline bool TryResolvePendingValue(const float RequestedValue, const float CurrentMaximum, float& OutPendingValue)
    {
        if (!FMath::IsFinite(RequestedValue) || !FMath::IsFinite(CurrentMaximum) || CurrentMaximum <= 1.0f)
        {
            return false;
        }
        OutPendingValue = FMath::Min(RequestedValue, CurrentMaximum);
        return FMath::IsFinite(OutPendingValue) && OutPendingValue > 1.0f;
    }

    constexpr bool IsRecipeApplicationAllowed(const bool bHasRecipe, const bool bIsCompatible)
    {
        return bHasRecipe && bIsCompatible;
    }

    constexpr bool IsShardTypeAllowed(const EPowerShardType ExpectedType, const EPowerShardType ActualType)
    {
        return ExpectedType != EPowerShardType::PST_None && ExpectedType == ActualType;
    }

    constexpr int32 GetUnreturnedItemCount(const int32 StackCount, const int32 AddedCount)
    {
        return FMath::Max(0, StackCount - FMath::Max(0, AddedCount));
    }

    /** Native slot filling assumes the caller has already bounded the target by supply. */
    constexpr int32 GetAffordableShardTarget(const int32 RequestedCount,
        const int32 ExistingCount, const int32 AvailableCount)
    {
        const int32 Requested = FMath::Max(0, RequestedCount);
        const int32 Existing = FMath::Clamp(ExistingCount, 0, Requested);
        // Subtract before adding so even a large inventory cannot overflow the budget.
        return Existing + FMath::Min(Requested - Existing, FMath::Max(0, AvailableCount));
    }
}
