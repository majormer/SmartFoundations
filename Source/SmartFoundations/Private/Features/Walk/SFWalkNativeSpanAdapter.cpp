// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkNativeSpanAdapter.h"
#include "SmartFoundations.h"
#include "Features/Walk/SFWalkCommitSpec.h"
#include "Features/Walk/SFWalkRailAdapter.h"
#include "Features/Walk/SFWalkVehiclePathAdapter.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"

bool FSFWalkNativeSpanAdapter::IsSpanType(ESFWalkConveyanceType Type)
{
    return Type == ESFWalkConveyanceType::Railroad || Type == ESFWalkConveyanceType::VehiclePath;
}

bool FSFWalkNativeSpanAdapter::IsSupported(AFGHologram* Hologram)
{
    return Cast<AFGRailroadTrackHologram>(Hologram) || Cast<AFGVehiclePathSegmentHologram>(Hologram);
}

FTransform FSFWalkNativeSpanAdapter::StartFrame(AFGHologram* Hologram)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { return FSFWalkRailAdapter::StartFrame(Rail); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { return FSFWalkVehiclePathAdapter::StartFrame(Path); }
    return FTransform::Identity;
}

bool FSFWalkNativeSpanAdapter::MatchesType(AFGHologram* Hologram, ESFWalkConveyanceType Type)
{
    return (Type == ESFWalkConveyanceType::Railroad && Cast<AFGRailroadTrackHologram>(Hologram))
        || (Type == ESFWalkConveyanceType::VehiclePath && Cast<AFGVehiclePathSegmentHologram>(Hologram));
}

FTransform FSFWalkNativeSpanAdapter::CrossSectionPose(const FTransform& Frame, int32 Lane, int32 Stack)
{
    return FSFWalkRailAdapter::CrossSectionPose(Frame, Lane, Stack);
}

bool FSFWalkNativeSpanAdapter::Route(AFGHologram* Hologram, const FTransform& From, const FTransform& To, AFGHologram* Seed)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { return FSFWalkRailAdapter::Route(Rail, From, To, Cast<AFGRailroadTrackHologram>(Seed)); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { return FSFWalkVehiclePathAdapter::Route(Path, From, To, Cast<AFGVehiclePathSegmentHologram>(Seed)); }
    return false;
}

bool FSFWalkNativeSpanAdapter::RefreshPreview(AFGHologram* Hologram, const FTransform& From, const FTransform& To)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { return FSFWalkRailAdapter::RefreshPreview(Rail, From, To); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { return FSFWalkVehiclePathAdapter::RefreshPreview(Path, From, To); }
    return false;
}

FString FSFWalkNativeSpanAdapter::Validate(AFGHologram* Hologram)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { return FSFWalkRailAdapter::Validate(Rail); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { return FSFWalkVehiclePathAdapter::Validate(Path); }
    return NSLOCTEXT("SmartFoundations", "Walk_UnsupportedNativeSpan", "Unsupported Walking native span").ToString();
}

float FSFWalkNativeSpanAdapter::MaximumAdvance(AFGHologram* Hologram)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { return FSFWalkRailAdapter::MaximumAdvance(Rail); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { return FSFWalkVehiclePathAdapter::MaximumAdvance(Path); }
    return 0.0f;
}

void FSFWalkNativeSpanAdapter::StageConstruction(AFGHologram* Hologram, AFGHologram* Previous)
{
    if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { FSFWalkRailAdapter::StageConstruction(Rail, Cast<AFGRailroadTrackHologram>(Previous)); }
    if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { FSFWalkVehiclePathAdapter::StageConstruction(Path, Cast<AFGVehiclePathSegmentHologram>(Previous)); }
}

void FSFWalkNativeSpanAdapter::ClearConstruction(const TArray<AFGHologram*>& Holograms)
{
    TArray<AFGRailroadTrackHologram*> Rails;
    TArray<AFGVehiclePathSegmentHologram*> Paths;
    for (auto* Hologram : Holograms)
    {
        if (auto* Rail = Cast<AFGRailroadTrackHologram>(Hologram)) { Rails.Add(Rail); }
        if (auto* Path = Cast<AFGVehiclePathSegmentHologram>(Hologram)) { Paths.Add(Path); }
    }
    FSFWalkRailAdapter::ClearConstruction(Rails);
    FSFWalkVehiclePathAdapter::ClearConstruction(Paths);
}

void FSFWalkNativeSpanAdapter::InstallConstructionHooks()
{
#if !WITH_EDITOR
    // The Editor SDK has stub bodies, not native construction implementations.
    // Runtime interception belongs only in the actual game/server targets.
    FSFWalkRailAdapter::InstallConstructionHook();
    FSFWalkVehiclePathAdapter::InstallConstructionHook();
#endif
}
