// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

namespace SFBlueprintSpacingDefaults
{
    inline int32 ToCentimeters(float Meters)
    {
        // Config files can bypass the menu limits. Clamp before multiplication/conversion.
        return FMath::IsFinite(Meters) ? FMath::RoundToInt(FMath::Clamp(Meters, 0.0f, 100.0f) * 100.0f) : 100;
    }
}
