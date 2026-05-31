// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Internal shared cache for stackable belt build data (Issue #220).
 * Shared between CacheStackableBeltPreviewsForBuild() and OnActorSpawned(). Promoted from a
 * file-scope anonymous-namespace cache in SFSubsystem.cpp to C++17 inline variables here so the
 * cache stays a single shared instance now that USFSubsystem's implementation is split across
 * multiple .cpp (slice S). Behavior is identical to the pre-split file-static cache.
 */

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGSplineHologram.h"  // For FSplinePointData

class UFGFactoryConnectionComponent;

struct FStackableBeltBuildData
{
    TArray<FSplinePointData> SplineData;
    TWeakObjectPtr<UFGFactoryConnectionComponent> OutputConnector;
    TWeakObjectPtr<UFGFactoryConnectionComponent> InputConnector;
    int32 BeltTier = 0;
};

inline TArray<FStackableBeltBuildData> GCachedStackableBeltData;
inline bool bGStackableBeltDataCached = false;

/** Re-entry lock for grid placement (was a file-scope static in SFSubsystem.cpp; promoted to a
 *  shared inline var so it stays a single instance across the split SFSubsystem_*.cpp). */
inline bool bProcessingGridPlacement = false;
