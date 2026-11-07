// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class UStaticMeshComponent;

namespace SFHypertubeSupportHologramState
{
	bool IsSelectedPole(AFGHologram* Hologram);
	bool GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes);
	bool IsFinalStep(AFGHologram* Hologram);
	bool IsLocalProxyGridConstruction(AFGHologram* Parent);
	void Sync(AFGHologram* Parent, AFGHologram* Child);
    FVector GridDimensions(const FVector& SelectedMeshDimensions, const FVector& ProfileDimensions,
        const FRotator& Rotation);
	bool TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions);
}
