// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

class AFGBlueprintHologram;
class UFGFactoryConnectionComponent;
class UFGPipeConnectionComponent;

/**
 * [#168] Smart! Blueprints — seam auto-connect PAIR SEARCH.
 *
 * When a blueprint is scaled into a grid, the belts/pipes that terminate at the blueprint's
 * edges are wired copy-to-copy across the grid seams by Smart (vanilla's blueprint auto-connect
 * is interactive-only and cannot initiate from programmatic clones — proven live 2026-07-06).
 *
 * THE MODEL (locked with the maintainer): pairs are determined ONCE per blueprint, in the
 * blueprint's own untransformed space, and identified by connection-component INDEX — not by
 * position — so they survive every transform (spacing/stagger/rotation) by construction.
 * Transforms move endpoints, never pairs; per-evaluation VALIDITY is judged by the game
 * (#466 arbiters), so a pair whose current geometry is invalid goes dormant and returns when
 * geometry allows. Do NOT re-derive pairs by proximity per evaluation (distributor model —
 * cannot express dormant-pair-returns).
 *
 * INDEX IDENTITY (live-confirmed on 3 clones, 2026-07-06): AFGBlueprintHologram duplicates one
 * connection component per OPEN content connector (DuplicateConnectionComponent), in the
 * deterministic blueprint-content spawn order — identical enumeration order on every clone.
 * The dup names embed per-world instance ids ("Build_ConveyorBeltMk6_C_<id>_ConveyorAny1"),
 * which DIFFER between clones, so name SORTING is wrong; enumeration ORDER is the stable key.
 *
 * SEARCH SPACE: the PARENT hologram's dup connectors, transformed into HOLOGRAM-LOCAL space —
 * that frame IS the grid frame (grid X/Y = parent local X/Y) the resolved endpoints get
 * serviced in, and it is content-fixed. (The originals' blueprint-world frame is NOT usable
 * for geometry: live 2026-07-07 it read 180°-flipped vs the dup frame — the same content
 * convention mismatch as the clone content delta.) Originals still provide OPENNESS and belt
 * flow direction, which are frame-free. For the +X seam, open connectors on the +X face
 * pointing +X are matched against −X-face connectors pointing −X in the same local (Y,Z) lane;
 * same independently for Y and Z. Z pairs are computed from day one (axis-uniform, free) but
 * the v1 spawner services X/Y only (Z pipes = v1.5, Z belts need lift previews = v2).
 */

/** Blueprint-local seam axis. Grid X/Y/Z map onto these 1:1 (clones share the parent's rotation). */
enum class ESFSeamAxis : uint8
{
	X = 0,
	Y = 1,
	Z = 2,
};

/**
 * One seam connection between adjacent copies, expressed transform-invariantly.
 * FromIndex/ToIndex index the kind-filtered, enumeration-ordered duplicated-connector list
 * (belts and pipes have separate index spaces).
 */
struct FSFBlueprintSeamPair
{
	ESFSeamAxis Axis = ESFSeamAxis::X;

	/** false = belt (UFGFactoryConnectionComponent), true = pipe (UFGPipeConnectionComponent) */
	bool bIsPipe = false;

	/** Output-side connector index (belts: flow source; pipes: the +face end by convention) */
	int32 FromIndex = INDEX_NONE;

	/** Input-side connector index (belts: flow destination; pipes: the -face end) */
	int32 ToIndex = INDEX_NONE;

	/**
	 * true:  From sits on the +Axis face, so the conduit runs lower-cell -> upper-cell.
	 * false: reverse flow — From is the upper copy's -Axis-face connector (belt flowing
	 *        toward the lower copy). Pipes are undirected and always use true.
	 */
	bool bFromOnPositiveFace = true;

	/** Original (blueprint-world) connector names — order-divergence sanity net at resolve time */
	FName FromOriginalName;
	FName ToOriginalName;
};

/** Cached per-blueprint pair table. Keyed by mBlueprintDescName in the auto-connect service. */
struct FSFBlueprintSeamTable
{
	FName BlueprintName;
	bool bComputed = false;
	TArray<FSFBlueprintSeamPair> Pairs;

	/** Dup-connector counts at compute time — cheap staleness check (blueprint re-saved mid-session) */
	int32 BeltConnectorCount = 0;
	int32 PipeConnectorCount = 0;

	int32 NumPairsForAxis(ESFSeamAxis Axis) const
	{
		int32 Count = 0;
		for (const FSFBlueprintSeamPair& Pair : Pairs)
		{
			if (Pair.Axis == Axis)
			{
				Count++;
			}
		}
		return Count;
	}
};

/**
 * Stateless pair-search + index-resolution helpers. All AFGBlueprintHologram protected/private
 * access (mDuplicateConnectionToOriginalMap) is confined here (AccessTransformers Friend).
 */
class SMARTFOUNDATIONS_API FSFBlueprintSeamService
{
public:
	/** How far behind the outermost same-facing connector a connector may sit and still count
	 *  as "on the face". Excludes staggered interior stubs; ports intended to seam sit AT the
	 *  content edge (live TestBP: exactly on it). */
	static constexpr float FACE_TOLERANCE = 300.0f;

	/** Max lateral (lane) offset between two face connectors to count as intended seam mates.
	 *  Flush-tiled mates align exactly; the slack tolerates off-grid content. */
	static constexpr float LANE_MATCH_TOLERANCE = 300.0f;

	/** Min |normal·axis| for a connector to count as facing a seam axis (cos ~45.6°; a unit
	 *  normal can exceed this for at most one axis, so faces never double-claim a connector). */
	static constexpr float FACING_AXIS_MIN_DOT = 0.7f;

	/**
	 * Compute the seam pair table for a blueprint (all three axes, belts + pipes) from the
	 * ORIGINAL connectors' blueprint-world geometry. [#168] Logs the full table at Log level —
	 * the FR1 validation artifact.
	 * @return true if the table computed (even if empty — a blueprint with no open edge ports).
	 */
	static bool BuildSeamTable(AFGBlueprintHologram* Blueprint, FSFBlueprintSeamTable& OutTable);

	/** Duplicated belt connectors in enumeration (content) order — the belt index space. */
	static void GetDuplicatedBeltConnectors(AFGBlueprintHologram* Blueprint, TArray<UFGFactoryConnectionComponent*>& OutConnectors);

	/** Duplicated fluid-pipe connectors in enumeration (content) order — the pipe index space. */
	static void GetDuplicatedPipeConnectors(AFGBlueprintHologram* Blueprint, TArray<UFGPipeConnectionComponent*>& OutConnectors);

	/**
	 * Resolve a table index to the live duplicated connector on a clone. Sanity-checks the dup
	 * name suffix against the recorded original connector name; a mismatch means the clone's
	 * enumeration order diverged from the parent's (model violation) — logs [#168] and returns
	 * null so the pair is skipped instead of wiring the wrong ports.
	 */
	static UFGFactoryConnectionComponent* ResolveBeltConnector(AFGBlueprintHologram* Clone, int32 Index, const FName& ExpectedOriginalName);
	static UFGPipeConnectionComponent* ResolvePipeConnector(AFGBlueprintHologram* Clone, int32 Index, const FName& ExpectedOriginalName);

private:
	struct FSeamConnector;
	static void PairAxis(TArray<FSeamConnector>& Connectors, ESFSeamAxis Axis, bool bPipes, TArray<FSFBlueprintSeamPair>& OutPairs);
	static bool DupNameMatchesOriginal(const UObject* DupComponent, const FName& ExpectedOriginalName);
};
