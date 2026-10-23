// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
namespace SFBlueprintLiftValidation
{
// Game-thread probe state, consumed immediately by the one preparation summary.
// A failing probe never changes the copy's saved placement state or ownership.
inline FString& LastCopyFailure()
{
    static FString Reason;
    return Reason;
}
}
