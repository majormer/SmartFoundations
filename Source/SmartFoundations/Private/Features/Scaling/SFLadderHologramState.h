// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;

namespace SFLadderHologramState
{
	/** Copy the native selected height without replacing the authored Smart cell transform. */
	void Sync(AFGHologram* Parent, AFGHologram* Child);
	bool TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions);
	bool IsFinalStep(AFGHologram* Hologram);
	bool IsLocalProxyGridConstruction(AFGHologram* Parent);
}
