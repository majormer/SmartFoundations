// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "FGSplineComponent.h"

namespace SFConveyorBeltGeometry
{
    // AFGBuildableConveyorBelt requires zero actor rotation. Keep the actor origin,
    // point order and world-space curve; change only the coordinate representation.
    inline bool BakeActorRotation(FTransform& Transform, TArray<FSplinePointData>& Points)
    {
        if (Points.Num() < 2 || !Transform.IsValid()
            || Transform.GetScale3D().GetAbsMin() <= SMALL_NUMBER
            || Transform.GetRotation().Equals(FQuat::Identity, 0.0))
        {
            return false;
        }

        for (const FSplinePointData& Point : Points)
        {
            if (Point.Location.ContainsNaN() || Point.ArriveTangent.ContainsNaN() || Point.LeaveTangent.ContainsNaN())
            {
                return false;
            }
        }

        const FTransform Canonical(FQuat::Identity, Transform.GetLocation(), Transform.GetScale3D());
        for (FSplinePointData& Point : Points)
        {
            // The origin is unchanged, so vector transforms also rebase positions
            // without subtracting large world coordinates. Tangents are derivatives:
            // rotate/scale them, but never translate or normalize them.
            Point.Location = Canonical.InverseTransformVector(Transform.TransformVector(Point.Location));
            Point.ArriveTangent = Canonical.InverseTransformVector(Transform.TransformVector(Point.ArriveTangent));
            Point.LeaveTangent = Canonical.InverseTransformVector(Transform.TransformVector(Point.LeaveTangent));
        }
        Transform = Canonical;
        return true;
    }
}
