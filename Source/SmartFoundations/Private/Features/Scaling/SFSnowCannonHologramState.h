// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class UStaticMeshComponent;

namespace SFSnowCannonHologramState
{
bool IsHandled(const AFGHologram* Hologram);
bool TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions);
bool GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes);
void Sync(AFGHologram* Parent, AFGHologram* Child);
bool IsLocalProxyGridConstruction(AFGHologram* Parent);
AFGHologram* SpawnConstructionCell(AFGHologram* Seed, AFGHologram* OwnerParent,
    FName ChildName, const FTransform& Pose);
}
