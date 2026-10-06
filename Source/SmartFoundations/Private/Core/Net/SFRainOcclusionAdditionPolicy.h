// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

enum class ESFRainOcclusionAdditionDecision : uint8
{
    Forward,
    DiscardDuplicateAdd
};

/**
 * Fail-closed observable contract for CL 502094 rain-occlusion registration.
 * Issue #523 reports a bDEBUGIsAddedTwice assertion (FGRainOcclusionActor.cpp:502)
 * while zooping ramp walls: the same FRainHashKey reached the private add path
 * twice without an intervening removal. The proprietary body does not expose
 * which internal structure detects the duplicate or when it is populated, so the
 * decision is made against Smart's own synchronous mirror of forwarded
 * registrations (see SFGameInstanceModule_SpecHooks.cpp), which both private add
 * siblings and both private remove siblings maintain at call time. A duplicate
 * is discarded before vanilla can observe it; the shape already registered under
 * that hash continues to represent the occluder, so the cost of a wrong discard
 * is cosmetic (rain passing through one shape), never state corruption.
 */
struct SFRainOcclusionAdditionPolicy
{
    [[nodiscard]] static constexpr ESFRainOcclusionAdditionDecision Decide(
        const bool bHashAlreadyRegistered)
    {
        return bHashAlreadyRegistered
            ? ESFRainOcclusionAdditionDecision::DiscardDuplicateAdd
            : ESFRainOcclusionAdditionDecision::Forward;
    }
};
