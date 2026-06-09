// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Multiplayer scaling construction spec

#pragma once

#include "CoreMinimal.h"
#include "HUD/SFHUDTypes.h"   // FSFCounterState (grid + transform counters)
#include "SFScalingSpec.generated.h"

/**
 * Compact, fixed-size description of a Smart! scaling grid, sufficient to RECONSTRUCT the entire
 * grid server-side. This is the O(1) "intent" payload of the multiplayer construction model
 * (docs/Features/Multiplayer/DESIGN_MP_ConstructionModel.md): instead of serializing N child
 * holograms across the wire (the 64KB ceiling), the client ships this spec on the parent hologram
 * and the server expands it into N buildables via FSFPositionCalculator + the normal child-construct
 * path. Scaling grids are UNIFORM, so the recipe / build class / base transform come from the parent
 * hologram itself (vanilla-serialized); this spec only needs the grid + transform counters and the
 * per-cell sizing the position calculator requires.
 *
 * Lives in its own header (not SFScalingTypes.h) because it depends on FSFCounterState (SFHUDTypes.h),
 * and SFHUDTypes.h already includes SFScalingTypes.h for the scale-axis enums — putting the spec in
 * SFScalingTypes.h would form an include cycle.
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFScalingSpec
{
	GENERATED_BODY()

	/** Grid dimensions + spacing/steps/stagger/rotation. Drives FSFPositionCalculator per cell. */
	UPROPERTY()
	FSFCounterState Counters;

	/** Cell size used by the position calculator (from the buildable size registry at capture time). */
	UPROPERTY()
	FVector ItemSize = FVector::ZeroVector;

	/** Attachment anchor compensation (from the size registry at capture time). */
	UPROPERTY()
	FVector AnchorOffset = FVector::ZeroVector;

	/** True once populated from a live grid; the server only expands when valid. */
	UPROPERTY()
	bool bValid = false;

	/** Total cells described by the spec (product of |counters|, each treated as >=1). */
	int32 CellCount() const
	{
		const int32 X = FMath::Max(1, FMath::Abs(Counters.GridCounters.X));
		const int32 Y = FMath::Max(1, FMath::Abs(Counters.GridCounters.Y));
		const int32 Z = FMath::Max(1, FMath::Abs(Counters.GridCounters.Z));
		return X * Y * Z;
	}
};
