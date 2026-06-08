// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "Configuration/ModConfiguration.h"
#include "SFGameInstanceModule.generated.h"

/**
 * Smart! Foundations Game Instance Module
 * Registers mod-wide initialization hooks
 */
UCLASS(Abstract)
class SMARTFOUNDATIONS_API USFGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	USFGameInstanceModule();

	/** Override lifecycle event to register module hooks */
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

protected:
	/** Register SML hook for blueprint construct to handle chain actor rebuilding (like AutoLink) */
	void RegisterBlueprintConstructHook();

	/**
	 * #341: Register SML hook on the conveyor-pole parent hologram's Construct (AFTER) to register a
	 * freshly-built stackable-pole belt run into one chain per series-run, in-frame and pre-tick (the
	 * timing Extend relies on). Doing this off a timer crashes Factory_Tick. THESIS Belts/ChainActors 6.16.
	 */
	/**
	 * #341: Register SML hook on the belt-support parent hologram's Construct (AFTER) for in-frame chain
	 * registration. Hooks AFGConveyorPoleHologram::Construct; because that resolves to the base
	 * AFGBuildableHologram::Construct, one hook covers ALL belt-support pole types - stackable, wall, and
	 * ceiling (verified live). THESIS Belts/ChainActors 6.16.
	 */
	void RegisterBeltSupportConstructHook();

	/**
	 * Multiplayer Slice 0 (Phase 1 - construct chunk guard). Hooks UFGBuildGunStateBuild::Server_ConstructHologram
	 * on the CLIENT (confirmed seam: the client calls this RPC directly; an earlier InternalConstructHologram hook
	 * never fired). A Smart scaled grid serializes the parent + all child holograms into one
	 * FConstructHologramMessage.SerializedHologramData blob; past an engine byte ceiling (~64KB, empirical ~135
	 * cells) the RPC fails to marshal ("Failed to serialize properties") and is dropped -> all-or-nothing failure
	 * with orphaned previews (no failure callback fires because the server never processed it). This guard reads
	 * the ACTUAL serialized byte size and cancels the send for an oversized Smart-grid construct, so nothing
	 * orphans (the preview stays live) and the player is told to build in smaller sections. Engages only for a
	 * network client (NM_Client) and only for Smart grids (children tagged SF_GridChild) - vanilla placements and
	 * blueprints are untouched. (Phase 2 will auto-chunk the placement instead of refusing it.)
	 */
	void RegisterClientConstructChunkGuardHook();

	/**
	 * MP Slice 0 chunking. Hooks UFGBuildGunStateBuild::InternalExecuteDuBuildStepInput (the client fire
	 * handler, BEFORE vanilla serializes the construct). For an oversized client scaled grid, slices the
	 * active hologram's child list down to a chunk that fits one 64KB Server_ConstructHologram before vanilla
	 * serializes it, so the construct succeeds instead of being dropped. Increment 1 builds a single chunk
	 * (proof of the shrink-then-vanilla-serialize mechanic); Increment 2 loops the remaining chunks. The
	 * Server_ConstructHologram guard remains as a backstop. Accesses mChildren via the USFGameInstanceModule
	 * friend AccessTransformer. Engages only for NM_Client + Smart grids; everything else is untouched.
	 */
	void RegisterClientGridChunkFireHook();

	/**
	 * Set the active hologram's child list to exactly the given holograms (replacing mChildren and the name
	 * lookup map). A static member so it inherits USFGameInstanceModule's friend access to AFGHologram's
	 * protected child members (see AccessTransformers.ini). Used by the MP chunk loop.
	 */
	static void SetActiveHologramChildren(class AFGHologram* Holo, const TArray<class AFGHologram*>& NewChildren);

	/** Smart! Configuration blueprint - registered with SML for in-game menu access */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Smart! Configuration")
	TSubclassOf<class UModConfiguration> SmartConfigClass;
};
