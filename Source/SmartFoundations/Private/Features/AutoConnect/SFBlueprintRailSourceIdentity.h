// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Components/SplineComponent.h"

namespace SFBlueprintRailSourceIdentity
{
// Native Blueprint construction can bake the owner's rotation into spline data.
// Compare serialized world geometry before BeginPlay, without depending on
// connection components which native construction may still be refreshing.
inline TArray<FSplinePointData> InWorld(const TArray<FSplinePointData>& Local, const FTransform& Owner)
{
    auto World = Local;
    for (auto& Point : World)
    {
        Point.Location = Owner.TransformPosition(Point.Location);
        Point.ArriveTangent = Owner.TransformVector(Point.ArriveTangent);
        Point.LeaveTangent = Owner.TransformVector(Point.LeaveTangent);
    }
    return World;
}
inline bool Matches(const FTransform& ExpectedOwner, const TArray<FSplinePointData>& ExpectedWorld,
    const FTransform& ActualOwner, const TArray<FSplinePointData>& ActualLocal)
{
    if (!ExpectedOwner.GetLocation().Equals(ActualOwner.GetLocation(), .1)
        || !ExpectedOwner.GetScale3D().Equals(ActualOwner.GetScale3D(), .0001)
        || ExpectedWorld.Num() < 2 || ExpectedWorld.Num() != ActualLocal.Num()) return false;
    for (int32 I = 0; I < ExpectedWorld.Num(); ++I)
        if (!ExpectedWorld[I].Location.Equals(ActualOwner.TransformPosition(ActualLocal[I].Location), .1)
            || !ExpectedWorld[I].ArriveTangent.Equals(ActualOwner.TransformVector(ActualLocal[I].ArriveTangent), .1)
            || !ExpectedWorld[I].LeaveTangent.Equals(ActualOwner.TransformVector(ActualLocal[I].LeaveTangent), .1)) return false;
    return true;
}
}
