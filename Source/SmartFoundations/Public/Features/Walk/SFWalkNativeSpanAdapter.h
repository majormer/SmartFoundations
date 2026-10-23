// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AFGHologram;
enum class ESFWalkConveyanceType : uint8;

/** Walking-only dispatch for native spans that do not use support-anchor grids. */
class FSFWalkNativeSpanAdapter
{
public:
    static bool IsSpanType(ESFWalkConveyanceType Type);
    static bool IsSupported(AFGHologram* Hologram);
    static bool MatchesType(AFGHologram* Hologram, ESFWalkConveyanceType Type);
    static FTransform StartFrame(AFGHologram* Hologram);
    static FTransform CrossSectionPose(const FTransform& Frame, int32 Lane, int32 Stack);
    static bool Route(AFGHologram* Hologram, const FTransform& From, const FTransform& To, AFGHologram* Seed);
    static bool RefreshPreview(AFGHologram* Hologram, const FTransform& From, const FTransform& To);
    static FString Validate(AFGHologram* Hologram);
    static float MaximumAdvance(AFGHologram* Hologram);
    static void StageConstruction(AFGHologram* Hologram, AFGHologram* Previous);
    static void ClearConstruction(const TArray<AFGHologram*>& Holograms);
    static void InstallConstructionHooks();
};
