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
	 * Multiplayer Slice 0 (Phase 1 - construct chunk guard). Hooks UFGBuildGunStateBuild::InternalConstructHologram
	 * on the CLIENT. A Smart scaled grid commits as a single reliable Server_ConstructHologram RPC carrying the
	 * parent + all child holograms serialized into one blob; past an engine byte ceiling (empirical ~135 cells)
	 * that RPC is dropped at the net layer -> all-or-nothing failure with orphaned previews (no failure callback
	 * fires because the server never processed it). This guard cancels the oversized construct BEFORE it is sent,
	 * so nothing orphans (the preview stays as the live active hologram) and the player is told to build in
	 * smaller sections. Engages only for a network client (NM_Client) above the safe cell count; single-player,
	 * listen-server host, dedicated-server authority, and small grids take the untouched vanilla path.
	 * (Phase 2 will auto-chunk the placement instead of refusing it.)
	 */
	void RegisterClientConstructChunkGuardHook();

	/** Smart! Configuration blueprint - registered with SML for in-game menu access */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Smart! Configuration")
	TSubclassOf<class UModConfiguration> SmartConfigClass;
};
