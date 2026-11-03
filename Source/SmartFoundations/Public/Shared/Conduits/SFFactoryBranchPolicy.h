// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

/** Pairing policy for generated factory branches, not a limit on native spline shapes. */
namespace SFFactoryBranchPolicy
{
    inline FVector Direction(FVector Value, bool bHorizontal)
    {
        if (bHorizontal) Value.Z = 0;
        return Value.GetSafeNormal();
    }

    /** An unused side facing away from the destination is not a failed candidate. */
    inline bool IsForwardCandidate(const FVector& PortNormal, const FVector& ToDestination, bool bHorizontal)
    {
        return FVector::DotProduct(Direction(PortNormal, bHorizontal),
            Direction(ToDestination, bHorizontal)) > KINDA_SMALL_NUMBER;
    }

    inline bool FacesOpposite(const FVector& FirstNormal, const FVector& SecondNormal,
        float MaxFacingAngleDegrees, bool bHorizontal)
    {
        const FVector A = Direction(FirstNormal, bHorizontal);
        const FVector B = Direction(SecondNormal, bHorizontal);
        return !A.IsNearlyZero() && !B.IsNearlyZero()
            && FVector::DotProduct(A, -B) >= FMath::Cos(FMath::DegreesToRadians(MaxFacingAngleDegrees));
    }

    inline bool OpposingBanks(const FVector& DistributorPosition,
        const FVector& FirstPosition, const FVector& FirstNormal,
        const FVector& SecondPosition, const FVector& SecondNormal, bool bHorizontal)
    {
        const FVector A = Direction(FirstPosition - DistributorPosition, bHorizontal);
        const FVector B = Direction(SecondPosition - DistributorPosition, bHorizontal);
        const FVector NA = Direction(FirstNormal, bHorizontal);
        const FVector NB = Direction(SecondNormal, bHorizontal);
        // Both destinations face back toward the distributor and lie behind the
        // opposite destination's facing plane. Height alone cannot split belt banks.
        return !A.IsNearlyZero() && !B.IsNearlyZero() && !NA.IsNearlyZero() && !NB.IsNearlyZero()
            && FVector::DotProduct(A, NA) < -KINDA_SMALL_NUMBER
            && FVector::DotProduct(B, NB) < -KINDA_SMALL_NUMBER
            && FVector::DotProduct(A, NB) > KINDA_SMALL_NUMBER
            && FVector::DotProduct(B, NA) > KINDA_SMALL_NUMBER
            && FVector::DotProduct(NA, NB) < -KINDA_SMALL_NUMBER;
    }
}
