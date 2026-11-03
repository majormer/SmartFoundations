// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "HUD/SFHUDTypes.h"

/** Count-independent cell identities for live Extend; not persisted Restore identities. */
namespace SFScaledExtendGrid
{
    /** Reject a repeated module pose without limiting arc angle or helical placement. */
    class FPlacementReservations
    {
    public:
        bool TryAdd(const FTransform& Placement)
        {
            const FVector Position = Placement.GetLocation();
            const FIntVector Cell(FMath::FloorToInt(Position.X / PositionToleranceCm),
                FMath::FloorToInt(Position.Y / PositionToleranceCm),
                FMath::FloorToInt(Position.Z / PositionToleranceCm));
            for (int32 X = -1; X <= 1; ++X)
                for (int32 Y = -1; Y <= 1; ++Y)
                    for (int32 Z = -1; Z <= 1; ++Z)
                        if (const auto* Candidates = Placements.Find(Cell + FIntVector(X, Y, Z)))
                            for (const FTransform& Existing : *Candidates)
                                if (Position.Equals(Existing.GetLocation(), PositionToleranceCm)
                                    && Placement.GetRotation().Equals(Existing.GetRotation(), 0.0001f))
                                    return false;
            Placements.FindOrAdd(Cell).Add(Placement);
            return true;
        }

    private:
        static constexpr double PositionToleranceCm = 0.1;
        TMap<FIntVector, TArray<FTransform>> Placements;
    };

    /** The held first clone defines forward; other rows/layers must not bias pipe-port selection. */
    inline FVector PrincipalAxis(const FVector& SourceLocation, const FVector& ParentLocation)
    {
        return (ParentLocation - SourceLocation).GetSafeNormal2D();
    }

    inline FString Prefix(int32 X, int32 Y, int32 Z)
    {
        return FString::Printf(TEXT("sc_%d_%d_%d_"), X, Y, Z);
    }

    template <typename Visitor>
    void ForEachAdditionalCell(const FSFCounterState& State, Visitor&& Visit)
    {
        const int32 XCount = FMath::Max(1, FMath::Abs(State.GridCounters.X));
        const int32 YCount = FMath::Max(1, FMath::Abs(State.GridCounters.Y));
        const int32 ZCount = FMath::Max(1, FMath::Abs(State.GridCounters.Z));
        for (int32 Z = 0; Z < ZCount; ++Z)
        {
            for (int32 Y = 0; Y < YCount; ++Y)
            {
                for (int32 X = 0; X <= XCount; ++X)
                {
                    // Source (0,0,0) already exists; (1,0,0) is the held parent.
                    if (Z == 0 && Y == 0 && X <= 1) continue;
                    Visit(FIntVector(X, Y, Z));
                }
            }
        }
    }
}
