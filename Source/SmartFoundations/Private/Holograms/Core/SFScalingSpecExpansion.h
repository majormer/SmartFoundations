// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - shared helpers for MP spec-based scaling construction.
//
// Used by the spec-capable parent holograms (ASFFactoryHologram for production buildings,
// ASFFoundationHologram for flat foundations). Keeps the capture + server-expansion logic in one
// place; the mChildren strip/restore stays in each class (protected member access).
// See docs/Features/Multiplayer/PLAN_MP_ScalingConstruction_Impl.md.

#pragma once

#include "CoreMinimal.h"
#include "Features/Scaling/SFScalingSpec.h"

class AFGHologram;
class UFGRecipe;

namespace SFScalingSpecExpansion
{
	/** True when the sf.MP.SpecConstruction console variable is enabled. */
	bool IsSpecConstructionEnabled();

	/**
	 * Client-side: capture the active Smart grid into a compact spec.
	 * Reads the authoritative FSFCounterState from the subsystem and ItemSize/AnchorOffset from the
	 * buildable size registry. Returns false (and leaves OutSpec invalid) for trivial 1x1x1 grids.
	 */
	bool CaptureScalingSpec(AFGHologram* Hologram, FSFScalingSpec& OutSpec);

	/**
	 * Client-side (#428): true unless this is a scalable water extractor with at least one grid cell
	 * NOT over valid water. The fire hook calls this BEFORE staging the spec / destroying the preview
	 * children and cancels the build if it returns false — matching single-player, where a disqualified
	 * water-pump grid child blocks the whole placement.
	 */
	bool AreAllWaterCellsValid(AFGHologram* Parent);

	/**
	 * Server-side: expand the spec into one child hologram per non-origin grid cell, parented to
	 * Parent via the vanilla AFGHologram::SpawnChildHologramFromRecipe path, positioned with
	 * FSFPositionCalculator (the same math the client preview uses). Returns the number spawned.
	 */
	int32 ExpandScalingSpecIntoChildren(AFGHologram* Parent, const FSFScalingSpec& Spec,
		TSubclassOf<UFGRecipe> Recipe);

	/**
	 * Client-side (#334): capture the auto-connect conduit wiring plan (belts, pipes, stackable
	 * runs, power wires) from the parent's tagged preview child holograms into the spec, BEFORE
	 * the fire hook strips/destroys those previews. Snapshots each preview's family kind, class,
	 * recipe, transform, routed spline (or wire endpoints) plus its exact vanilla preview cost
	 * into Spec.ConduitPlan / Spec.ConduitPlanCost.
	 */
	void CaptureConduitPlan(AFGHologram* Hologram, FSFScalingSpec& InOutSpec);

	/**
	 * Server-side (#334): replay the staged conduit plan as fresh tagged child holograms of the
	 * constructing parent (the same spawn recipes the client preview pipeline uses, minus the
	 * client-only visuals). MUST be called AFTER ExpandScalingSpecIntoChildren so the conduits sit
	 * after the grid cells in mChildren: the vanilla child-construct loop then builds the grid
	 * cells first and each family's existing Construct wiring path connects its conduit against
	 * BUILT actors. Wire endpoints are resolved by world location against the pre-construct
	 * hologram set. Returns the number of conduits spawned.
	 */
	int32 SpawnConduitPlanChildren(AFGHologram* Parent, const FSFScalingSpec& Spec);

	/**
	 * Server-side (#334): build the staged WIRE entries AFTER the grid has constructed. Wires are
	 * never built from holograms - even in SP the wire child holograms exist only for cost, and
	 * the persistent wire is direct-spawned post-build (unconnected wires self-destruct). Resolves
	 * each endpoint power connection by world location among the BUILT actors (parent +
	 * out_children, falling back to the world), dedupes against wires the power manager already
	 * spawned, then AFGBuildableWire + Connect (the proven OnPowerPoleBuilt primitive). Registers
	 * each wire into GroupProxy (may be null). Returns the number of wires built.
	 */
	int32 SpawnWirePlanPostConstruct(AActor* BuiltParent, const TArray<AActor*>& OutChildren,
		const FSFScalingSpec& Spec, class AFGBlueprintProxy* GroupProxy);
}
