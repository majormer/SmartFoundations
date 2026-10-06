// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AFGHologram;

/** Protects Smart grid transforms at the native wall-outlet placement boundary. */
class FSFWallOutletPlacement
{
public:
    static void RegisterHooks();
    static void PostPlacement(TConstArrayView<AFGHologram*> Children, TFunctionRef<void()> NativePlacement);
};
