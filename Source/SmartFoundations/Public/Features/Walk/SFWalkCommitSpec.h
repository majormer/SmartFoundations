// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Smart Walking (#356) Slice 3 multiplayer commit spec.

#pragma once

#include "CoreMinimal.h"
#include "ItemAmount.h"                 // FItemAmount (staged cost)
#include "Templates/SubclassOf.h"
#include "SFWalkCommitSpec.generated.h"

class UFGRecipe;

/** Which conveyance the walk is laying. Selected from the seed buildable at EnterWalk (belt pole vs pipeline
 *  support); travels in the commit spec so the server reconstructs the matching spanning element. */
UENUM()
enum class ESFWalkConveyanceType : uint8
{
	Belt = 0,
	Pipe = 1,
};

/**
 * One walk segment's AUTHORED parameters - the slim, wire-safe subset of FSFWalkSegment (no transient
 * hologram pointers). The server re-derives every world frame from these via the SAME forward kinematics
 * (USFWalkService::AccumulateFrame) the client previewed with, so only these few floats travel.
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFWalkCommitSegment
{
	GENERATED_BODY()

	UPROPERTY() float Advance = 0.0f;       // forward advance to this anchor, cm
	UPROPERTY() float TurnDegrees = 0.0f;   // yaw turn applied at the END of this segment, deg
	UPROPERTY() float Rise = 0.0f;          // vertical rise, cm
	UPROPERTY() float Shift = 0.0f;         // lateral sidestep, cm
	UPROPERTY() int32 NumLanes = 1;         // SIGNED bus lanes (|val| = count, sign = side)
	UPROPERTY() int32 NumStacks = 1;        // SIGNED bus stacks
};

/**
 * The staged Smart Walking commit a CLIENT ships to the server right before firing the build gun
 * (Slice 3; mirrors FSFExtendCommitSpec's intent->authority model). PARAMETERS ONLY: the server
 * re-derives every segment's world frame from OriginFrame + the per-segment deltas (AccumulateFrame),
 * spawns the pole + belt holograms server-side, AddChild's them to the seed at the construct seam,
 * and lets the vanilla cascade build them. The client's standalone PREVIEW holograms are never
 * shipped - they are discarded on commit. Overwrite semantics like the scaling/Extend specs (an
 * invalid commit staged on every fire is an explicit clear, so a stale commit can never leak).
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFWalkCommitSpec
{
	GENERATED_BODY()

	/** The seed (origin) world transform captured at EnterWalk - segment 0 advances forward from here. */
	UPROPERTY() FTransform OriginFrame = FTransform::Identity;

	/** The ordered Path: per-segment authored deltas (the source of truth the server re-derives from). */
	UPROPERTY() TArray<FSFWalkCommitSegment> Segments;

	/** Which spanning conveyance this run lays (belt vs pipe) - the server reconstructs the matching adapter. */
	UPROPERTY() ESFWalkConveyanceType ConveyanceType = ESFWalkConveyanceType::Belt;

	/** Belt routing mode (0=Default, 1=Curve, 2=Straight) the client has set - the server re-routes the
	 *  spanning belts with its OWN runtime default otherwise (mirrors FSFExtendCommitSpec::BeltRoutingMode). */
	UPROPERTY() int32 BeltRoutingMode = 0;

	/** Belt tier the client previewed (0 = Auto / highest unlocked). */
	UPROPERTY() int32 BeltTier = 0;

	/** Pipe routing mode (0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=H2V) - same install-on-server reason
	 *  as BeltRoutingMode. Only meaningful when ConveyanceType == Pipe. */
	UPROPERTY() int32 PipeRoutingMode = 0;

	/** Pipe tier the client previewed (0 = Auto). Only meaningful when ConveyanceType == Pipe. */
	UPROPERTY() int32 PipeTier = 0;

	/** Pipe indicator (the flow-indicator variant) the client has set. Only meaningful for pipes. */
	UPROPERTY() bool bPipeIndicator = true;

	/** Seed pole build class - the server only consumes a staged commit for a construct of the SAME
	 *  class (guards against any client/server fire mismatch or RPC race). */
	UPROPERTY() TSubclassOf<class AFGBuildable> BuildClass = nullptr;

	/** Exact preview cost (seed + every walk pole + belt, vanilla GetCost at fire) - the server's GetCost
	 *  hook overrides with this so the commit charges what was previewed, not just the lone seed. */
	UPROPERTY() TArray<FItemAmount> Cost;

	/** True once populated from a live walk session (an invalid spec is an explicit clear). */
	UPROPERTY() bool bValid = false;
};
