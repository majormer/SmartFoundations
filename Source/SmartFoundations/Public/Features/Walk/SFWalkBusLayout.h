// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

/** Stable original coordinates inside a signed, expandable cross-section. Existing cells
 * never move when the user grows either side; only cells outside the original rectangle
 * need new origin supports. No actors, ownership or resource state is stored here. */
namespace SFWalkBusLayout
{
    inline bool IsValid(int32 Lanes, int32 Stacks, int32 OriginalLanes, int32 OriginalStacks)
    {
        return OriginalLanes >= 1 && OriginalLanes <= 256 && OriginalStacks >= 1 && OriginalStacks <= 256
            && Lanes >= -256 && Lanes <= 256 && Stacks >= -256 && Stacks <= 256
            && FMath::Abs(Lanes) >= OriginalLanes && FMath::Abs(Stacks) >= OriginalStacks
            && (Lanes > 0 || -Lanes > OriginalLanes) && (Stacks > 0 || -Stacks > OriginalStacks)
            && static_cast<int64>(FMath::Abs(Lanes)) * FMath::Abs(Stacks) <= 4096;
    }

    inline int32 Coordinate(int32 Row, int32 Counter, int32 OriginalCount)
    {
        return Row - (Counter < 0 ? -Counter - OriginalCount : 0);
    }

    inline int32 ExistingIndex(int32 Cell, int32 Lanes, int32 Stacks, int32 OriginalLanes, int32 OriginalStacks)
    {
        if (!IsValid(Lanes, Stacks, OriginalLanes, OriginalStacks) || Cell < 0
            || Cell >= FMath::Abs(Lanes) * FMath::Abs(Stacks)) { return INDEX_NONE; }
        const int32 Lane = Coordinate(Cell / FMath::Abs(Stacks), Lanes, OriginalLanes);
        const int32 Stack = Coordinate(Cell % FMath::Abs(Stacks), Stacks, OriginalStacks);
        return Lane >= 0 && Lane < OriginalLanes && Stack >= 0 && Stack < OriginalStacks
            ? Lane * OriginalStacks + Stack : INDEX_NONE;
    }

    // Preserve Walking's ... -3,-2,1,2,3 ... wheel convention around the original width.
    inline int32 Step(int32 Counter, int32 Delta, int32 OriginalCount)
    {
        if (!Delta) { return Counter; }
        const int64 Unit = Counter > 0 ? Counter - OriginalCount + 1 : Counter + OriginalCount - 1;
        int64 Next = Unit + static_cast<int64>(Delta);
        if (Next == 0 || Next == -1) { Next = Delta > 0 ? 1 : -2; }
        const int64 Expanded = Next > 0 ? Next + OriginalCount - 1 : Next - OriginalCount + 1;
        return static_cast<int32>(FMath::Clamp<int64>(Expanded, -256, 256));
    }
}
