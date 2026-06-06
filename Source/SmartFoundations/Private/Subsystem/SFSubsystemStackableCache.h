// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Internal shared re-entry lock for grid placement (Issue #220).
 *
 * bProcessingGridPlacement was a file-scope static in SFSubsystem.cpp; promoted to a C++17 inline
 * var here so it stays a single shared instance across the split SFSubsystem_*.cpp (slice S).
 *
 * This header formerly also held the stackable-belt build cache (FStackableBeltBuildData,
 * GCachedStackableBeltData, bGStackableBeltDataCached). That cache was removed once stacked belts
 * moved to the STACK-CHAIN construct handler in ASFConveyorBeltHologram and its producer/consumer
 * (CacheStackableBeltPreviewsForBuild / the OnActorSpawned SpawnActor builder) were deleted as dead.
 * See THESIS §6.9–§6.13.
 */

#pragma once

#include "CoreMinimal.h"

/** Re-entry lock for grid placement (single shared instance across the split SFSubsystem_*.cpp). */
inline bool bProcessingGridPlacement = false;
