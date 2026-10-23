// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AFGHologram;
class AFGRailroadTrackHologram;
class AFGBuildable;

/** Native rail geometry and construction contract; confined access to the rail family. */
class FSFWalkRailAdapter
{
public:
    static FTransform StartFrame(AFGRailroadTrackHologram* Track);
    static FTransform CrossSectionPose(const FTransform& Frame, int32 Lane, int32 Stack);
    static bool Route(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To, AFGRailroadTrackHologram* Seed);
    static bool RefreshPreview(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To);
    static FString Validate(AFGRailroadTrackHologram* Track);
    static float MaximumAdvance(AFGRailroadTrackHologram* Track);
    static void StageConstruction(AFGRailroadTrackHologram* Track, AFGRailroadTrackHologram* Previous);
    static void ClearConstruction(const TArray<AFGRailroadTrackHologram*>& Tracks);
    static void InstallConstructionHook();
private:
    static void ConfigureConnections(const AFGRailroadTrackHologram* Track, AFGBuildable* Buildable);
};
