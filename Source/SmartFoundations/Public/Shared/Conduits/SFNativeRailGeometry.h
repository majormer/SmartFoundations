// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGRailroadTrackHologram;
/** Native rail geometry and actual curve-length gate shared by Walking and Blueprint seams. */
class SMARTFOUNDATIONS_API FSFNativeRailGeometry
{
public:
    static bool Route(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To, AFGRailroadTrackHologram* Seed);
    static bool RefreshPreview(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To);
    static FString Validate(AFGRailroadTrackHologram* Track);
};
