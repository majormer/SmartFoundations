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
/** Which auto-connect family a staged conduit plan entry belongs to - selects the replay recipe
 *  (hologram class, tags, registry data) so the existing family-specific Construct wiring runs. */
UENUM()
enum class ESFConduitPlanKind : uint8
{
	Belt,            // distributor auto-connect belt (SF_BeltAutoConnectChild)
	Pipe,            // junction / floor-hole auto-connect pipe (SF_PipeAutoConnectChild)
	StackableBelt,   // stacked-pole belt (SF_StackableChild)
	StackablePipe,   // stacked-support pipe (SF_StackableChild)
	Wire             // power auto-connect wire (SF_PowerAutoConnectChild)
};

/**
 * One auto-connect conduit (belt / pipe / wire) of the staged wiring plan (#334). Captured
 * CLIENT-side at fire time from the live preview child holograms - the only party that ever holds
 * the complete, real plan (server-side re-derivation and aim-time-preview reuse both failed live;
 * see PLAN_MP_AutoConnect_334.md). The server replays each entry as a fresh tagged child hologram
 * appended AFTER the grid cells, so the vanilla child-construct loop builds it and the family's
 * existing post-build wiring path connects it geometrically against BUILT actors - no connector
 * names or replicated component refs need to cross the wire. Wires are the exception: their two
 * endpoint connection components are resolved by WORLD LOCATION against the pre-construct hologram
 * set (vanilla remaps hologram connections to built poles during construct, the SP mechanism).
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFConduitPlanEntry
{
	GENERATED_BODY()

	UPROPERTY()
	ESFConduitPlanKind Kind = ESFConduitPlanKind::Belt;

	/** Buildable class (tier) of the preview hologram. */
	UPROPERTY()
	TSubclassOf<class AFGBuildable> BuildClass = nullptr;

	/** Recipe (cost basis + dismantle refund identity). */
	UPROPERTY()
	TSubclassOf<class UFGRecipe> Recipe = nullptr;

	/** World transform of the conduit hologram (spline points below are local to it). */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** Routed spline, local space - exactly what the client preview committed to. Empty for Wire. */
	UPROPERTY()
	TArray<FSplinePointData> SplinePoints;

	/** Wire only: world locations of the two endpoint power connections at capture time. */
	UPROPERTY()
	FVector WireStart = FVector::ZeroVector;

	UPROPERTY()
	FVector WireEnd = FVector::ZeroVector;

	/** Pipe only: floor-hole (passthrough) pipe - replay leaves the junction-connector registry
	 *  field null so the floor-hole Construct branch (passthrough snap registration) runs. */
	UPROPERTY()
	bool bFloorHolePipe = false;

	/** Stackable kinds: position in the run (registry StackableBeltIndex / StackablePipeIndex). */
	UPROPERTY()
	int32 StackIndex = -1;
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

	/** Auto-connect conduit wiring plan (#334) - belts, pipes, stackable runs, power wires -
	 *  captured from the client's live previews at fire time. May be non-empty even for a 1-cell
	 *  spec (a single distributor/junction/pole with previews). */
	UPROPERTY()
	TArray<FSFConduitPlanEntry> ConduitPlan;

	/** Aggregated cost of the planned conduits - the EXACT vanilla preview costs (length-based for
	 *  belts/pipes, span-based for wires), summed client-side per item class. The server's GetCost
	 *  hook appends this so the staged conduits are charged with the grid. */
	UPROPERTY()
	TArray<FItemAmount> ConduitPlanCost;

	/** Total cells described by the spec (product of |counters|, each treated as >=1). */
	int32 CellCount() const
	{
		const int32 X = FMath::Max(1, FMath::Abs(Counters.GridCounters.X));
		const int32 Y = FMath::Max(1, FMath::Abs(Counters.GridCounters.Y));
		const int32 Z = FMath::Max(1, FMath::Abs(Counters.GridCounters.Z));
		return X * Y * Z;
	}
};
