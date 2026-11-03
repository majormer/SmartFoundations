// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
/** Replays native floor-hole placement and copies only its validated mounting context. */
class FSFNativeFloorHolePlacement
{
public:
    static bool Validate(AFGHologram* Root, AFGHologram* Child,
        const FTransform& Pose, float ExpectedThickness, FString& Reason);
};
