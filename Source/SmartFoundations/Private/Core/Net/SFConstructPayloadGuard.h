// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreTypes.h"

namespace SFConstructPayloadGuard
{
    inline constexpr int32 MaxSerializedBytes = 60000;

    constexpr bool ShouldReject(const int32 SerializedBytes)
    {
        return SerializedBytes > MaxSerializedBytes;
    }
}
