// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Blueprint Designer containment for directly-spawned power wires (#421).

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGCircuitConnectionComponent.h"

/**
 * [#421] Smart! spawns some power wires directly (power auto-connect's deferred
 * pole-to-pole/pole-to-building wires, the MP wire plan, Extend's post-build wiring) instead of
 * going through the vanilla hologram-construct pipeline. That pipeline is what marks a buildable
 * as contained in the Blueprint Designer (AFGBuildable::mBlueprintDesigner, SaveGame'd), which
 * feeds blueprint serialization. A direct-spawned wire never got the mark: blueprints saved the
 * POLES with their SaveGame'd wire references while the WIRES themselves were missing. Placing
 * such a blueprint produced poles claiming phantom connections, per-tick "RebuildCircuit ...
 * reachable components but only N listed" repair spam on dedicated servers, and dangling-wire
 * save corruption.
 *
 * TIMING CONTRACT (learned from a live dedi crash 2026-07-01): vanilla check()s that
 * SetInsideBlueprintDesigner runs on the authority BEFORE BeginPlay is dispatched - the designer
 * mark must be in place when BeginPlay performs the containment bookkeeping, exactly like a
 * hologram-built buildable (stamped in ConfigureActor on the still-deferred actor). So the wire
 * MUST be spawned deferred, stamped, and only then FinishSpawning'd - a post-spawn fixup is a
 * fatal assert. SpawnWireForEndpoints wraps that sequence; use it for EVERY direct wire spawn.
 * No explicit contained-list registration is done here: vanilla completes containment from the
 * pre-BeginPlay mark (same path as hologram-built wires).
 */
namespace SFWireDesigner
{
	/** The Blueprint Designer containing a wire endpoint's owner, or null for a world buildable. */
	inline AFGBuildableBlueprintDesigner* EndpointDesigner(const UFGCircuitConnectionComponent* Conn)
	{
		const AFGBuildable* Owner = Conn ? Cast<AFGBuildable>(Conn->GetOwner()) : nullptr;
		return Owner ? Owner->GetBlueprintDesigner() : nullptr;
	}

	/**
	 * Spawn a wire for the given endpoints with correct designer containment:
	 * deferred spawn -> stamp the endpoints' designer (pre-BeginPlay, authority only) ->
	 * FinishSpawning. Outside a designer this is equivalent to the old direct spawn.
	 * Returns nullptr if the endpoints resolve to DIFFERENT designers (a wire may never span
	 * the designer wall) - callers treat that as a failed spawn. The wire is returned
	 * unconnected; callers Connect() it as before.
	 */
	inline AFGBuildableWire* SpawnWireForEndpoints(UWorld* World, UClass* WireClass, const FVector& Location,
		const UFGCircuitConnectionComponent* ConnA, const UFGCircuitConnectionComponent* ConnB)
	{
		if (!World || !WireClass)
		{
			return nullptr;
		}

		AFGBuildableBlueprintDesigner* DesignerA = EndpointDesigner(ConnA);
		AFGBuildableBlueprintDesigner* DesignerB = EndpointDesigner(ConnB);
		if (DesignerA != DesignerB)
		{
			return nullptr; // designer-wall-spanning wire - never valid
		}

		const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true; // the designer mark must land before BeginPlay

		AFGBuildableWire* Wire = World->SpawnActor<AFGBuildableWire>(WireClass, SpawnTransform, SpawnParams);
		if (!Wire)
		{
			return nullptr;
		}

		// Authority only (the vanilla check() demands it): the server owns containment. A pure
		// client-side spawn (preview-grade) skips the mark - matching pre-fix behavior there.
		if (DesignerA && World->GetNetMode() != NM_Client)
		{
			Wire->SetInsideBlueprintDesigner(DesignerA);
		}

		Wire->FinishSpawning(SpawnTransform);
		return Wire;
	}
}
