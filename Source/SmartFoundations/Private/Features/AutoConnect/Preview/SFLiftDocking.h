// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

namespace SFLiftDocking
{
    // Native attachment arms consume connector clearance; their markers do not coincide.
    inline bool Matches(const FVector& Mouth, const FVector& MouthNormal, float MouthClearance,
        const FVector& Peer, const FVector& PeerNormal, float PeerClearance)
    {
        const FVector Normal = PeerNormal.GetSafeNormal();
        return !Normal.IsNearlyZero() && FVector::DotProduct(MouthNormal.GetSafeNormal(), Normal) < -0.999f
            && Mouth.Equals(Peer + Normal * (PeerClearance + MouthClearance), 0.1f);
    }
}
