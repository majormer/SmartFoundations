// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "Engine/EngineBaseTypes.h"

namespace SFVisualSplineMeshPolicy
{
    constexpr bool ShouldGenerate(const ENetMode NetMode)
    {
        return NetMode != NM_DedicatedServer;
    }
}
