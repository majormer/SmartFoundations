// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Blueprint Designer registration for directly-spawned power wires (#421).

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGCircuitConnectionComponent.h"
#include "FGDismantleInterface.h"

/**
 * [#421] Smart! spawns some power wires with a bare SpawnActor + AFGBuildableWire::Connect
 * (power auto-connect's deferred pole-to-pole/pole-to-building wires, the MP wire plan, and
 * Extend's post-build wiring) instead of the vanilla hologram-construct pipeline. That pipeline
 * is what registers a buildable with the Blueprint Designer containing it
 * (AFGBuildableBlueprintDesigner::OnBuildableConstructedInsideDesigner - "This way we don't need
 * to gather them to serialize"). A bare-spawned wire therefore never entered the designer's
 * contained list: the POLES saved into the blueprint carrying their SaveGame'd wire references,
 * but the WIRES themselves were missing from the blueprint. Placing such a blueprint produced
 * poles claiming phantom connections, per-tick "RebuildCircuit ... reachable components but only
 * N listed" circuit repair spam on dedicated servers, and dangling-wire-reference save corruption.
 *
 * Call RegisterSpawnedWire after every successful Connect() on a directly-spawned wire. Outside a
 * designer it is a no-op; inside, it stamps the wire with the designer (SaveGame'd
 * mBlueprintDesigner) and registers it in the contained list so it serializes into the blueprint
 * exactly like a hand-built wire.
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
	 * Register a directly-spawned, already-Connect()ed wire with the designer containing its
	 * endpoints. A wire may never span the designer wall: on an endpoint-designer mismatch the
	 * wire is dismantled (Dismantle, not Destroy - a bare Destroy leaves a dead entry in the
	 * connection's SaveGame'd wire list) and false is returned so the caller can skip its
	 * bookkeeping. Returns true for world wires (no designer, nothing to do) and for wires
	 * successfully registered with their designer.
	 */
	inline bool RegisterSpawnedWire(AFGBuildableWire* Wire)
	{
		if (!Wire)
		{
			return false;
		}

		AFGBuildableBlueprintDesigner* DesignerA = EndpointDesigner(Wire->GetConnection(0));
		AFGBuildableBlueprintDesigner* DesignerB = EndpointDesigner(Wire->GetConnection(1));

		if (DesignerA != DesignerB)
		{
			IFGDismantleInterface::Execute_Dismantle(Wire);
			return false;
		}
		if (!DesignerA)
		{
			return true; // world wire - vanilla behavior, nothing to register
		}

		Wire->SetInsideBlueprintDesigner(DesignerA);
		DesignerA->OnBuildableConstructedInsideDesigner(Wire);
		return true;
	}
}
