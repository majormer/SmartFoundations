// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Multiplayer Extend commit spec ([EXTEND-MP] slice 2)

#pragma once

#include "CoreMinimal.h"
#include "Features/Extend/SFExtendCloneTopology.h"   // FSFCloneTopology (value-only, wire-safe)
#include "HUD/SFHUDTypes.h"                           // FSFCounterState (Restore commit panel state)
#include "ItemAmount.h"                               // FItemAmount (staged cost)
#include "Templates/SubclassOf.h"
#include "SFExtendCommitSpec.generated.h"

class UFGRecipe;

/**
 * The staged Extend commit a CLIENT ships to the server right before firing the build gun
 * ([EXTEND-MP] slice 2; see PLAN_MP_ExtendConstruction_Strategy.md). Mirrors the scaling spec's
 * intent->authority model, and like it the spec carries ONLY PARAMETERS: the server re-derives
 * the clone topology ITSELF (authoritative graph walk -> CaptureFromTopology -> FromSource at
 * ParentOffset) and reconstructs the children pre-scope(). The clone topology MUST be built
 * server-side: capturing it on the client poisons every segment's connections, because
 * CaptureBeltChain/CapturePipeChain read GetConnection() - null on clients by design - so the
 * wiring manifest comes out empty (live root cause 2026-06-10: every MP Extend built unwired).
 */
/** One Scaled Extend clone set's PARAMETERS ([EXTEND-MP]). The server does not need the clone
 *  topologies themselves: it re-walks the source's graph authoritatively and re-runs the SAME
 *  spawn pipeline the SP preview uses (SpawnScaledExtendPreviews) with these parameters - that
 *  regenerates each clone's factory child, infrastructure topology, rigid-body rotation fix-ups,
 *  and lane segments exactly as the client previewed them. */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFExtendCommitScaledClone
{
	GENERATED_BODY()

	UPROPERTY()
	FVector WorldOffset = FVector::ZeroVector;     // offset from the SOURCE building

	UPROPERTY()
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY()
	int32 GridX = 0;

	UPROPERTY()
	int32 GridY = 0;

	UPROPERTY()
	bool bIsSeed = false;
};

USTRUCT()
struct SMARTFOUNDATIONS_API FSFExtendCommitSpec
{
	GENERATED_BODY()

	/** Final fire-position offset of the parent clone from the SOURCE building. The server runs
	 *  FromSource(serverWalkedTopology, ParentOffset) to derive the clone topology itself. */
	UPROPERTY()
	FVector ParentOffset = FVector::ZeroVector;

	/** [#382] Parent clone's yaw rotation offset from the SOURCE building. FromSource() positions the
	 *  parent's belts but does NOT rotate them, and the server's counter state is a default mirror
	 *  (RotationZ=0), so without this the parent's belts stay at source orientation on the build even
	 *  though the factory rotates. The server applies this to the derived clone topology, mirroring
	 *  the SP preview's parent-rotation block. (Children carry their own per-clone RotationOffset.) */
	UPROPERTY()
	FRotator ParentRotation = FRotator::ZeroRotator;

	/** [#380] Belt routing mode (0=Default, 1=Curve, 2=Straight) the client has set. The server
	 *  re-derives lane belts with its OWN runtime settings (default 0), so without this MP lane
	 *  belts always route Default even with Curve selected. The server applies it before routing. */
	UPROPERTY()
	int32 BeltRoutingMode = 0;

	/** [#383] Pipe routing mode (0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=H2V) the client
	 *  has set. Same reason as BeltRoutingMode: the server re-derives lane pipes with its own default
	 *  pipe mode, so without this MP pipe lanes ignore the client's selection. Applied before routing. */
	UPROPERTY()
	int32 PipeRoutingMode = 0;

	/** [#382] Smart Panel counter state (rotation, grid, spacing, steps) the EXTEND re-derivation
	 *  reads. The server's own counter state is a default mirror (RotationZ=0), so cross-clone math
	 *  that reads it directly - e.g. the first child's manifold lane to the PREVIOUS (parent)
	 *  distributor uses CounterState.RotationZ for PrevCloneRotation - targets the parent's
	 *  un-rotated position on the build. The server installs this (UpdateCounterState) before
	 *  re-deriving. (Restore ships the analogous RestoreCounterState.) */
	UPROPERTY()
	FSFCounterState CounterState;

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

	/** Scaled Extend ([EXTEND-MP]): the per-clone parameters of every additional clone set.
	 *  Empty for a normal (single-clone) Extend. */
	UPROPERTY()
	TArray<FSFExtendCommitScaledClone> ScaledClones;

	/** RESTORE commit ([EXTEND-MP] Restore slice). A Smart Restore replay has NO source building
	 *  to walk: the topology comes from the saved preset, a value-only template captured at save
	 *  time. The client ships that compact TEMPLATE (not the expanded preview) plus the Smart
	 *  Panel counter state and the stored production recipe; the server installs them and re-runs
	 *  the SAME replay pipeline the SP preview uses (ReplayRestoreCloneTopology: expansion at the
	 *  validated parent's final position + rr_X_Y factory placement + infra spawn + pre-wiring).
	 *  The connection data is preset-sourced, not GetConnection()-derived at fire time, so the
	 *  client-capture poisoning that forbids shipping the EXTEND topology does not apply here. */
	UPROPERTY()
	bool bIsRestore = false;

	UPROPERTY()
	FSFCloneTopology RestoreTemplate;

	/** Smart Panel state the replay expansion + restored-factory placement math read
	 *  (grid counters, spacing, steps, rotation). The server's own counter state is a default
	 *  mirror that never saw the client's panel. */
	UPROPERTY()
	FSFCounterState RestoreCounterState;

	/** The preset's production recipe for the restored factories (asset class - package-map
	 *  safe). Null when the preset carried none. */
	UPROPERTY()
	TSubclassOf<UFGRecipe> RestoreProductionRecipe = nullptr;

	/** True once populated from a live Extend session. */
	UPROPERTY()
	bool bValid = false;
};
