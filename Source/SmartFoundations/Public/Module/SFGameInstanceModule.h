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
	 * MP Slice 0 SAFETY GUARD. Hooks UFGBuildGunStateBuild::InternalExecuteDuBuildStepInput (the client fire
	 * handler, BEFORE vanilla serializes the construct). For an oversized client scaled grid (one that can't
	 * fit a single 64KB Server_ConstructHologram), it cancels the fire so nothing is serialized or sent - the
	 * grid stays live for the player to scale down. This prevents the orphaned-preview/dropped-RPC bug AND the
	 * server crash that hand-built chunk messages cause. It is NOT a large-grid MP feature (those constructs
	 * are not safely achievable today - see the method body and AGENTS.md). Engages only for NM_Client + Smart
	 * grids (tagged SF_GridChild); vanilla placements and blueprints are untouched.
	 */
	void RegisterClientGridChunkFireHook();

	/**
	 * MP spec-based scaling construction - CLASS-AGNOSTIC hook path (covers every scalable
	 * buildable, including vanilla Blueprint hologram wrappers like Holo_Foundation_C, without
	 * swapping the active hologram). Three hooks on vanilla virtual BODIES (they fire via the
	 * Super chain from any subclass):
	 *  - AFGHologram::SerializeConstructMessage: saving = capture spec + strip SF_GridChild
	 *    children -> original writes the O(1) message -> append spec -> restore children;
	 *    loading = original reads -> read spec into the hologram data registry.
	 *  - AFGHologram::Construct (before original runs): server expands the registry spec into
	 *    children post-validation (fresh holograms cannot pass vanilla placement validation -
	 *    live finding 2026-06-09).
	 *  - AFGHologram::GetCost: server scales the per-cell cost by the cell count while the
	 *    children do not exist yet (pre-Construct charge time).
	 * Gated by sf.MP.SpecConstruction + USFBuildableSizeRegistry.bSupportsScaling.
	 */
	void RegisterSpecConstructionHooks();

	/**
	 * [#368/#279] Hook AFGBuildGun::UnEquip to run Smart's holster cleanup. The legacy
	 * USFSubsystem::OnBuildGunUnequipped() (which clears the remembered recipe and, now, the vanilla
	 * clipboard recipe) was orphaned - never called from anywhere - so a recipe "stuck" across build
	 * sessions (it still showed in the HUD and re-applied after a holster). This wires it to the real
	 * unequip event (gated to the local player), and clears the local build-gun clipboard directly via
	 * the gun (the subsystem's Player->GetBuildGun() path can be null mid-unequip).
	 */
	void RegisterBuildGunUnequipHook();

	/**
	 * [#162/#429] Hook UFGBuildGunStateBuild::Scroll_Implementation to consume the wheel's rotation
	 * delta while Smart! owns the moment (modal window open, or Smart!-owned hologram lock incl.
	 * auto-hold - see USFSubsystem::ShouldSuppressBuildGunScroll). This chokepoint sits BELOW the
	 * input layer: every wheel-driven rotation funnels through it regardless of which mapping
	 * context delivered the scroll, covering both vanilla paths (the per-hologram
	 * AFGHologram::Scroll/ScrollRotate chain AND the build-state's player-relative re-push via
	 * mPlayerRelativeScrollRotation) - and, because AFGHologram::Scroll is then never called, it
	 * also starves InfiniteNudge's Scroll hook, which rotates any LOCKED hologram (Smart!'s own
	 * modifier lock was what activated it - the #162 conflict). Cancelling also prevents
	 * Server_Scroll from firing (MP-safe); gated to the local player's build gun. Replaces the
	 * old MC_BuildGunBuild context-removal machinery (#272), which had no callers - suppression
	 * de facto rode on the hologram lock alone (vanilla ignores wheel-rotate while locked).
	 * Approach proven in-game by the Air Build mod (AirBuildHologramHook.cpp).
	 */
	void RegisterBuildGunScrollSuppressionHook();

	/** Smart! Configuration blueprint - registered with SML for in-game menu access */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Smart! Configuration")
	TSubclassOf<class UModConfiguration> SmartConfigClass;

public:
	/**
	 * [#368] Sync the player's vanilla build-gun clipboard recipe. When a player picks a recipe via
	 * the U key / Smart Panel, Smart's chosen recipe must match the vanilla clipboard, because
	 * vanilla's PasteSettings re-applies mSampledClipboardSettings to the built manufacturer AFTER
	 * Smart's spec-construction SetRecipe (last-writer-wins) - so a stale sampled clipboard otherwise
	 * overrides the player's selection (live MP repro: sampled Copper Sheet overrode a U-selected
	 * Cable). This writes the recipe into the player's build-gun clipboard so vanilla pastes the SAME
	 * recipe Smart does. The field is NOT replicated, so each side sets its own copy: the client calls
	 * this locally and a per-player USFRCO::Server_SetClipboardRecipe sets the server's. Recipe==null
	 * CLEARS the clipboard (recipe-less vanilla placement; the holster path uses this). An existing
	 * manufacturer clipboard is modified in place (preserving any sampled overclock/Somersloop); a
	 * fresh clipboard is created neutral. Uses GetBuildGunStateFor(BGS_BUILD) so the clear works even
	 * after the gun has left build state on holster. Lives here because AccessTransformers grants
	 * USFGameInstanceModule friend access to UFGBuildGunStateBuild::mSampledClipboardSettings.
	 */
	static void SetBuildStateClipboardRecipe(class AFGCharacterPlayer* Player, TSubclassOf<class UFGRecipe> Recipe);
};
