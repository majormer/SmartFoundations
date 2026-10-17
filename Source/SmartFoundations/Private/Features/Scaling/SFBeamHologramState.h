// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;

namespace SFBeamHologramState
{
	bool IsLocalProxyGridConstruction(AFGHologram* Parent);
	/** Shared by live previews, inline authority cells and retained deferred templates. */
	void Sync(AFGHologram* Parent, AFGHologram* Child);
	bool TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions);
}
