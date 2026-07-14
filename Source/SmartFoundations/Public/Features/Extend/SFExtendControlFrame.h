// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "HUD/SFHUDTypes.h"

struct FSFCloneTopology;

/** Signed, building-relative control frame consumed by every Extend placement path. */
struct SMARTFOUNDATIONS_API FSFExtendControlFrame
{
    float ChainSign = 1.0f;
    float RowsSign = 1.0f;

    static FSFExtendControlFrame FromState(const FSFCounterState& State);
};

/** Placement of one Extend cell relative to an arbitrary origin cell. */
struct SMARTFOUNDATIONS_API FSFExtendCellPlacement
{
    FVector WorldOffset = FVector::ZeroVector;
    FRotator RotationOffset = FRotator::ZeroRotator;
};

/** Include distributor width when a captured topology extends beyond the factory row footprint. */
SMARTFOUNDATIONS_API float CalculateExtendEffectiveRowHeight(
    const FVector& BuildingSize,
    const FSFCloneTopology* Topology);

/**
 * Canonical Extend placement used by live scaling, Restore replay, and post-build lookup.
 * ChainIndex/RowIndex and OriginChainIndex/OriginRowIndex are absolute cell coordinates.
 */
SMARTFOUNDATIONS_API FSFExtendCellPlacement CalculateExtendCellPlacement(
    const FRotator& BaseRotation,
    const FVector& BuildingSize,
    float EffectiveRowHeight,
    const FSFCounterState& State,
    int32 ChainIndex,
    int32 RowIndex,
    int32 OriginChainIndex = 0,
    int32 OriginRowIndex = 0);
