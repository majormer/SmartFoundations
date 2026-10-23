// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

/** Geometry only. The caller must resolve native, family-eligible, exposed connectors first.
 * Identity is an index into that caller's stable endpoint snapshot, never component enumeration order.
 * Flow is carried per lane; it is not inferred from a shared bus direction or changed by sorting.
 */
struct FSFWalkEndpointSample
{
    int32 Identity = INDEX_NONE;
    FVector Position = FVector::ZeroVector;
    FVector Outward = FVector::ForwardVector;
    bool bEligible = true;
    bool bOccupied = false;
    bool bReverseFlow = false;
};

enum class ESFWalkEndpointGridResult : uint8
{
    Ready,
    InvalidSelection,
    InvalidFrame,
    TooManyEndpoints,
    AmbiguousIdentity,
    AmbiguousGeometry,
    IncompleteRectangle
};

struct FSFWalkEndpointGrid
{
    ESFWalkEndpointGridResult Result = ESFWalkEndpointGridResult::InvalidSelection;
    /** Lower lateral/vertical corner in the supplied connector frame. */
    FVector Origin = FVector::ZeroVector;
    int32 Lanes = 0;
    int32 Stacks = 0;
    float LaneSpacing = 0.0f;
    float StackSpacing = 0.0f;
    int32 SelectedCell = INDEX_NONE;
    /** Lane-major, then stack; references the original samples by Identity. */
    TArray<int32> Identities;
    TArray<bool> ReverseFlow;
};

/** No world scans, spawning, resources, ownership changes or Smart group dependency.
 * A sparse/irregular or coincident cross-section is refused instead of choosing a convenient subset.
 * Ready describes only an unambiguous geometry snapshot, not permission to construct.
 */
namespace SFWalkEndpointGrid
{
    SMARTFOUNDATIONS_API FSFWalkEndpointGrid Resolve(
        const TArray<FSFWalkEndpointSample>& Samples, int32 SelectedIdentity,
        const FVector& Lateral, const FVector& Vertical, float ToleranceCm = 1.0f);
}
