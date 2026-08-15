// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

enum class ESFRainOcclusionRemovalDecision : uint8
{
    Forward,
    DiscardInconsistentLookup
};

enum class ESFRainOcclusionRemovalPath : uint8
{
    BoxSprite,
    MeshShape
};

/**
 * Fail-closed observable contract for CL 502094 rain-occlusion removal.
 * The public structure maps each rain hash to an owner component and instance
 * index. Issue #514 reported a private bounds assertion at index 25 / array size
 * 24, but the failing array and source are unknown. Independently require a
 * complete, live lookup whose index is in range for the public owner's current
 * instance count before forwarding the opaque call.
 */
struct SFRainOcclusionRemovalPolicy
{
    [[nodiscard]] static constexpr ESFRainOcclusionRemovalDecision Decide(
        const bool bHasLookup,
        const bool bHasOwner,
        const int32 LookupIndex,
        const int32 OwnerInstanceCount)
    {
        return bHasLookup
            && bHasOwner
            && LookupIndex >= 0
            && OwnerInstanceCount >= 0
            && LookupIndex < OwnerInstanceCount
            ? ESFRainOcclusionRemovalDecision::Forward
            : ESFRainOcclusionRemovalDecision::DiscardInconsistentLookup;
    }

    [[nodiscard]] static constexpr ESFRainOcclusionRemovalDecision DecideForPath(
        const ESFRainOcclusionRemovalPath Path,
        const bool bHasLookup,
        const bool bHasOwner,
        const int32 LookupIndex,
        const int32 OwnerInstanceCount)
    {
        // Apply the subsystem's one public owner/index precondition at both
        // private sibling boundaries without assuming their opaque bodies are
        // identical. Keep the path explicit so dispatch cannot omit a sibling.
        (void)Path;
        return Decide(bHasLookup, bHasOwner, LookupIndex, OwnerInstanceCount);
    }
};
