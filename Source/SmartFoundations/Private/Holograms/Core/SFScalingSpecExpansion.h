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
struct FNetConstructionID;

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
	 * [#418-MP] Should this spec skip the inline expansion and build DEFERRED (time-sliced across
	 * server frames)? True for large conduit-free, designer-free grids. Inline expansion +
	 * construction of a ~45K-cell grid blocked one server frame for ~57s (live incident
	 * 2026-07-03) - past the client's 30s net timeout, so every client was dropped mid-build.
	 * Conduit-plan specs (#334) and designer-resident grids stay inline: their correctness
	 * depends on the vanilla child-construct ordering at the seam, and both are small in practice.
	 */
	bool ShouldDeferSpecExpansion(AFGHologram* Parent, const FSFScalingSpec& Spec);

	/**
	 * [#418-MP] Begin the deferred expansion: capture everything BY VALUE at the Construct seam
	 * (the parent hologram dies when its Construct returns), then a world-timer loop constructs
	 * cells under a per-frame time budget so the server tick keeps servicing the net driver.
	 * Cells are spawned standalone (no parent to attach to), multi-step properties carried by a
	 * hidden template hologram, and each cell is Construct()ed + destroyed in place - the same
	 * per-cell semantics as the inline path, spread over frames.
	 */
	void BeginDeferredSpecExpansion(AFGHologram* Parent, const FSFScalingSpec& Spec,
		TSubclassOf<UFGRecipe> Recipe, const FNetConstructionID& ConstructionID);

	/**
	 * Client-side (#334): capture the auto-connect conduit wiring plan (belts, pipes, stackable
	 * runs, power wires) from the parent's tagged preview child holograms into the spec, BEFORE
	 * the fire hook strips/destroys those previews. Snapshots each preview's family kind, class,
	 * recipe, transform, routed spline (or wire endpoints) plus its exact vanilla preview cost
	 * into Spec.ConduitPlan / Spec.ConduitPlanCost.
	 */
	void CaptureConduitPlan(AFGHologram* Hologram, FSFScalingSpec& InOutSpec);

	/**
	 * Issue #487: estimate the extra cable cost for scale-time building-to-building power
	 * daisy-chains. Uses the live preview grid (parent + SF_GridChild holograms) and the same
	 * local-X row policy used by construction. Returns empty when disabled, locked, unsupported,
	 * or not a multi-cell X grid.
	 */
	TArray<struct FItemAmount> GetScaleDaisyChainPowerCost(AFGHologram* Parent);

	/** [#487] Return the valid world-space connector spans used by both preview and cost. */
	void GetScaleDaisyChainPowerSpans(AFGHologram* Parent, TArray<TPair<FVector, FVector>>& OutSpans);

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

	/**
	 * Issue #487: after a scaled factory/generator grid exists, wire adjacent copies
	 * building-to-building along local X, one independent chain per Y/Z row. Uses unlock-aware
	 * connector capacity and the same designer-aware direct wire spawn as other Smart power paths.
	 */
	int32 SpawnScaleDaisyChainPowerPostConstruct(AActor* BuiltParent, const TArray<AActor*>& OutChildren,
		const FSFCounterState& Counters, bool bEnabled, class AFGBlueprintProxy* GroupProxy = nullptr);

	/**
	 * [#168-MP] Measure a staged blueprint hologram's CONTENT ANCHOR: the first blueprint-world
	 * buildable's visual-root offset relative to the hologram actor (hologram-local). The staging
	 * convention VARIES - by blueprint AND by staging context (client interactive flow vs client
	 * preview child vs server construct-message parent vs server spec cell; live 2026-07-07:
	 * FluidGrid's server world sat one delta off the client plan, TestBP's sat exactly on it) -
	 * so alignment must be MEASURED per hologram, never inferred from a carried constant.
	 * Returns ZeroVector when unstaged/empty.
	 */
	FVector MeasureBlueprintContentAnchor(class AFGBlueprintHologram* Blueprint);
}
