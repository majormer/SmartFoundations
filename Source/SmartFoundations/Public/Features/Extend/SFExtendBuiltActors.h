// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

class AFGHologram;
class USFExtendService;

namespace SFExtendBuiltActors
{
    // Compatibility for native children that do not expose ConfigureActor registration.
    // Candidates must come from this Construct result, never a world/proximity search.
    AActor* Match(UClass* BuildClass, const FTransform& Transform, const TArray<AActor*>& Constructed);
    int32 RegisterChildren(USFExtendService* Service, AFGHologram* Parent, const TArray<AActor*>& Constructed);
}
