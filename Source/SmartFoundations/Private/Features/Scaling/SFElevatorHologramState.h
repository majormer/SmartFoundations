// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class AFGBuildable;
class UStaticMeshComponent;
struct FItemAmount;

namespace SFElevatorHologramState
{
bool IsHandled(const AFGHologram* Hologram);
AFGBuildable* GetSnappedElevator(const AFGHologram* Hologram);
bool GetSelectedMeshes(AFGHologram* Parent, TArray<UStaticMeshComponent*>& OutMeshes);
bool TryGetDimensions(AFGHologram* Parent, FVector& OutDimensions);
bool GetStopCost(AFGHologram* Parent, TArray<FItemAmount>& OutCost);
void Sync(AFGHologram* Parent, AFGHologram* Child);
bool IsLocalProxyGridConstruction(AFGHologram* Parent);
AFGHologram* SpawnConstructionCell(AFGHologram* Seed, AFGHologram* OwnerParent,
    FName ChildName, const FTransform& Pose);
}
