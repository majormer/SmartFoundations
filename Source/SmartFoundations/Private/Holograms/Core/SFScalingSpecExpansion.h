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
	 * Server-side: expand the spec into one child hologram per non-origin grid cell, parented to
	 * Parent via the vanilla AFGHologram::SpawnChildHologramFromRecipe path, positioned with
	 * FSFPositionCalculator (the same math the client preview uses). Returns the number spawned.
	 */
	int32 ExpandScalingSpecIntoChildren(AFGHologram* Parent, const FSFScalingSpec& Spec,
		TSubclassOf<UFGRecipe> Recipe);

	/**
	 * Client-side (#334): capture the auto-connect belt wiring plan from the live preview helpers
	 * into the spec, BEFORE the fire hook strips/destroys those previews. Walks the parent + every
	 * child hologram and snapshots each stored belt preview (class, recipe, transform, routed
	 * spline) plus its exact vanilla length-based cost into Spec.BeltPlan / Spec.BeltPlanCost.
	 */
	void CaptureBeltPlan(AFGHologram* Hologram, FSFScalingSpec& InOutSpec);

	/**
	 * Server-side (#334): replay the staged belt plan as fresh ASFConveyorBeltHologram children of
	 * the constructing parent. MUST be called AFTER ExpandScalingSpecIntoChildren so the belts sit
	 * after the grid cells in mChildren: the vanilla child-construct loop then builds the
	 * distributors first and each belt's SF_BeltAutoConnectChild Construct path wires it by
	 * geometric coincidence against BUILT actors only. Returns the number of belts spawned.
	 */
	int32 SpawnBeltPlanChildren(AFGHologram* Parent, const FSFScalingSpec& Spec);
}
