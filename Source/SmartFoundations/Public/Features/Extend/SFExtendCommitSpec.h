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

	/** True once populated from a live Extend session. */
	UPROPERTY()
	bool bValid = false;
};
