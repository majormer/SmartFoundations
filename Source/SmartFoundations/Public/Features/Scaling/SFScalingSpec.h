// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Multiplayer scaling construction spec

#pragma once

#include "CoreMinimal.h"
#include "HUD/SFHUDTypes.h"   // FSFCounterState (grid + transform counters)
#include "Components/SplineComponent.h"   // FSplinePointData (CSS engine addition)
#include "ItemAmount.h"                    // FItemAmount (belt plan cost)
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
/**
 * One auto-connect belt of the staged wiring plan (#334). Captured CLIENT-side at fire time from the
 * live belt preview hologram - the only party that ever holds the complete, real plan (server-side
 * re-derivation and aim-time-preview reuse both failed live; see PLAN_MP_AutoConnect_334.md). The
 * server replays each entry as a fresh ASFConveyorBeltHologram child appended AFTER the grid cells,
 * so the vanilla child-construct loop builds it and the SF_BeltAutoConnectChild Construct path wires
 * it geometrically (nearest free connector within 50cm, BUILT actors only) - no connector names or
 * replicated component refs need to cross the wire.
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFBeltPlanEntry
{
	GENERATED_BODY()

	/** Belt buildable class (tier) of the preview hologram. */
	UPROPERTY()
	TSubclassOf<class AFGBuildable> BeltClass = nullptr;

	/** Belt recipe (cost basis + dismantle refund identity). */
	UPROPERTY()
	TSubclassOf<class UFGRecipe> Recipe = nullptr;

	/** World transform of the belt hologram (spline points below are local to it). */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** Routed spline, local space - exactly what the client preview committed to. */
	UPROPERTY()
	TArray<FSplinePointData> SplinePoints;
};

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

	/** Build class of the uniform grid - the server only consumes a staged spec for a construct of
	 *  the SAME class (guards against any client/server fire mismatch or RPC race). */
	UPROPERTY()
	TSubclassOf<class AFGBuildable> BuildClass = nullptr;

	/** True once populated from a live grid; the server only expands when valid. */
	UPROPERTY()
	bool bValid = false;

	/** Auto-connect belt wiring plan (#334), captured from the client's live previews at fire time.
	 *  May be non-empty even for a 1-cell spec (a single distributor with belts). */
	UPROPERTY()
	TArray<FSFBeltPlanEntry> BeltPlan;

	/** Aggregated cost of the planned belts - the EXACT vanilla length-based preview costs, summed
	 *  client-side per item class. The server's GetCost hook appends this so the staged belts are
	 *  charged with the grid (server-built belts were previously free - known interim gap, closed). */
	UPROPERTY()
	TArray<FItemAmount> BeltPlanCost;

	/** Total cells described by the spec (product of |counters|, each treated as >=1). */
	int32 CellCount() const
	{
		const int32 X = FMath::Max(1, FMath::Abs(Counters.GridCounters.X));
		const int32 Y = FMath::Max(1, FMath::Abs(Counters.GridCounters.Y));
		const int32 Z = FMath::Max(1, FMath::Abs(Counters.GridCounters.Z));
		return X * Y * Z;
	}
};
