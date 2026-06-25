// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - hypertube auto-connect span construction (#405).

#pragma once

#include "CoreMinimal.h"

class AFGHologram;
class USFSubsystem;

/**
 * Hypertube auto-connect — span construction (slice Features/HypertubeAutoConnect/, #405).
 *
 * STRUCTURE (locked): the stackable state-machine (gather hypertube supports -> pair adjacent ->
 * track previews -> cleanup) lives on USFAutoConnectService (mirrors ProcessStackablePipelineSupports).
 * This slice owns ONLY the hypertube-SPECIFIC span construction between one adjacent support pair —
 * the analog of the stackable-pipe UpdateOrCreatePipeForPolePair, but routed by the GAME router
 * (ASFPipelineHologram::ApplyPipeBuildModeRouting) exactly like the Smart Walking pipe adapter
 * (USFWalkPipeConveyance::LinkOrUpdate), NOT by a hand-rolled spline.
 *
 * SCOPE: spawn/route the tube between two scaled supports, set its recipe (commit cost), and AddChild it
 * into the parent hologram — pipe-parity, so vanilla owns its lifecycle (cascade-destroys on cancel) AND
 * builds it on the run's commit fire (#405). An over-length pair is simply SKIPPED (returns null, no span)
 * — the same policy as the existing stackable auto-connect, NOT the walk's red+reason.
 *
 * The hypertube connector is UFGPipeConnectionComponentHyper, a subclass of UFGPipeConnectionComponentBase,
 * so GetComponents<UFGPipeConnectionComponentBase>() catches it with no retype.
 */
namespace SFHypertube
{
	/**
	 * Build (when ExistingSpan is null) or re-route (when ExistingSpan is a valid ASFPipelineHologram)
	 * the hypertube tube spanning FromSupport -> ToSupport.
	 *
	 * @param Sub            Smart subsystem (provides the unlock-gated hypertube build class + routing mode).
	 * @param FromSupport    Source stackable hypertube support hologram (entry end).
	 * @param ToSupport      Destination stackable hypertube support hologram (exit end).
	 * @param ExistingSpan   Previously-built span for this pair to re-route in place, or null to create one.
	 * @param ParentForChild Parent hologram to AddChild a newly-created span into (vanilla owns its lifecycle —
	 *                       cascade-destroys on cancel, builds on commit), mirroring the stackable-pipe AC.
	 * @return The span hologram (created or re-routed), or null when the pair is over MAX_HYPERTUBE_LENGTH,
	 *         a connector/world/class is missing, or the spawn failed.
	 */
	SMARTFOUNDATIONS_API AFGHologram* BuildOrUpdateSpan(
		USFSubsystem* Sub,
		AFGHologram* FromSupport,
		AFGHologram* ToSupport,
		AFGHologram* ExistingSpan,
		AFGHologram* ParentForChild);
}
