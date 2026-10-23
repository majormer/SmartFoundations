// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Components/SceneComponent.h"

namespace SFBlueprintVehicleNativeFrame
{
// Native candidate lookup converts the copied visual's relative location through
// its attachment frame. A constructed node's unattached root is not that visual.
inline bool Matches(const USceneComponent* Visual, const FTransform& Expected)
{
    if (!IsValid(Visual) || !IsValid(Visual->GetAttachParent())) return false;
    const auto& Actual = Visual->GetComponentTransform();
    return Actual.GetLocation().Equals(Expected.GetLocation(), .1)
        && Actual.GetRotation().Equals(Expected.GetRotation(), .0001)
        && Actual.GetScale3D().Equals(Expected.GetScale3D(), .0001);
}
}
