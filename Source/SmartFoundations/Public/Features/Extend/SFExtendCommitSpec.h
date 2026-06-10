// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Multiplayer Extend commit spec ([EXTEND-MP] slice 2)

#pragma once

#include "CoreMinimal.h"
#include "Features/Extend/SFExtendCloneTopology.h"   // FSFCloneTopology (value-only, wire-safe)
#include "ItemAmount.h"                               // FItemAmount (staged cost)
#include "SFExtendCommitSpec.generated.h"

/**
 * The staged Extend commit a CLIENT ships to the server right before firing the build gun
 * ([EXTEND-MP] slice 2; see PLAN_MP_ExtendConstruction_Strategy.md). Mirrors the scaling spec's
 * intent->authority model: the client re-emits a FRESH clone topology at fire time (the preview's
 * stored topology has stale world transforms - children are re-positioned while aiming), destroys
 * its preview children, and fires a childless O(1) parent; the server's Construct hook consumes
 * this spec and reconstructs the clone children PRE-scope() via the same executor the client
 * preview uses (FSFCloneTopology::SpawnChildHolograms + WireChildHologramConnections).
 *
 * FSFCloneTopology is wire-safe by construction: a fully reflected, VALUE-ONLY schema (strings,
 * floats, transforms, spline points - zero object pointers).
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFExtendCommitSpec
{
	GENERATED_BODY()

	/** The clone description, emitted at FIRE time with the final parent offset. */
	UPROPERTY()
	FSFCloneTopology Clone;

	/** Exact preview cost (parent + all preview children, vanilla GetCost(true) at fire) -
	 *  the server's GetCost hook overrides with this so the commit charges what was previewed. */
	UPROPERTY()
	TArray<FItemAmount> Cost;

	/** Parent build class - the server only consumes a staged commit for a construct of the SAME
	 *  class (guards against any client/server fire mismatch or RPC race). */
	UPROPERTY()
	TSubclassOf<class AFGBuildable> BuildClass = nullptr;

	/** The building the player extended FROM (replicated actor, serializes via the package map).
	 *  The dedi no longer runs the Extend preview pipeline, so the session state the post-build
	 *  wiring anchors on (LastBuiltFromBuilding / CurrentExtendTarget - e.g. the #344 daisy-chain
	 *  head) must arrive with the commit. */
	UPROPERTY()
	TObjectPtr<class AFGBuildable> SourceBuilding = nullptr;

	/** True once populated from a live Extend session. */
	UPROPERTY()
	bool bValid = false;
};
