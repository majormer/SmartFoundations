// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AFGVehiclePathSegmentHologram;

/** Native path routing, preset validation and pre-BeginPlay directed node ownership. */
class FSFWalkVehiclePathAdapter
{
public:
    static FTransform StartFrame(AFGVehiclePathSegmentHologram* Path);
    static bool Route(AFGVehiclePathSegmentHologram* Path, const FTransform& From, const FTransform& To, AFGVehiclePathSegmentHologram* Seed);
    static bool RefreshPreview(AFGVehiclePathSegmentHologram* Path, const FTransform& From, const FTransform& To);
    static FString Validate(AFGVehiclePathSegmentHologram* Path);
    static float MaximumAdvance(AFGVehiclePathSegmentHologram* Path);
    static void StageConstruction(AFGVehiclePathSegmentHologram* Path, AFGVehiclePathSegmentHologram* Previous);
    static void ClearConstruction(const TArray<AFGVehiclePathSegmentHologram*>& Paths);
    static void InstallConstructionHook();
};
