// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "FGPowerConnectionComponent.h"
#include "GameFramework/Actor.h"

namespace SFPowerBuildingTarget
{
inline bool IsEligible(bool bHidden, bool bHasCircuit, int32 FreeConnections)
{
    return !bHidden && !bHasCircuit && FreeConnections > 0;
}

/** Match preview and construction: an unconnected, externally wireable native power port. */
inline UFGPowerConnectionComponent* Find(AActor* Building)
{
    if (!IsValid(Building)) return nullptr;
    TInlineComponentArray<UFGPowerConnectionComponent*> Ports(Building);
    for (UFGPowerConnectionComponent* Port : Ports)
        if (IsValid(Port) && IsEligible(Port->IsHidden(), Port->IsConnected() != 0, Port->GetNumFreeConnections())) return Port;
    return nullptr;
}

inline bool IsWithinRange(const FVector& SourcePort, const FVector& TargetPort, double Range)
{
    return Range > 0.0 && FVector::DistSquared(SourcePort, TargetPort) <= FMath::Square(FMath::Min(Range, 10000.0));
}
}
