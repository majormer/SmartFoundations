// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * [#482] Tap-to-toggle (latching) transform modes - the PURE activation-policy layer.
 *
 * Smart's modal transform inputs are hold-to-activate, which controllers and Steam Input
 * radial/touch menus cannot express (they send momentary taps). When the optional
 * bToggleTransformModes setting is ON, taps latch a mode instead: first tap activates,
 * tapping the same mode deactivates, tapping a different latchable mode switches.
 *
 * This header holds ONLY value-level policy decisions - no engine state, no side effects -
 * so the state machine is unit-testable (SFLatchedTransformTests.cpp). USFSubsystem owns
 * applying decisions to the real mode booleans, hologram lock, HUD, and recipe service.
 * FSFCounterState remains the sole transform/grid value state; the latch records
 * activation policy only.
 */

#pragma once

#include "CoreMinimal.h"

/** Which transform modal is latched. Only these five participate; X/Y scale modifiers,
 *  Auto-Connect settings (U on logistics), Extend side selection, and the Smart Panel
 *  keep their existing behavior in both settings. */
enum class ESFLatchedTransformMode : uint8
{
	None,
	Spacing,
	Steps,
	Stagger,
	Rotation,
	Recipe,
};

/** What the subsystem should do with one raw modal input event under the latch policy. */
struct FSFLatchInputDecision
{
	/** True = the event is fully handled by the latch policy; the hold path must not run.
	 *  False = latch is not in effect; run the existing hold path byte-for-byte. */
	bool bConsume = false;

	/** The latched mode after this event (only meaningful when bConsume). */
	ESFLatchedTransformMode NewMode = ESFLatchedTransformMode::None;

	/** True when a stale latch exists while the setting is OFF - the subsystem must clear
	 *  it (mode booleans, lock, HUD) before running the hold path, so flipping the setting
	 *  off mid-latch can never leave a mode stuck. */
	bool bClearStaleLatch = false;
};

namespace SFLatchedTransform
{
	/** Pure latch transition: tapping TappedMode while CurrentMode is latched.
	 *  Same mode -> None (toggle off); anything else -> the tapped mode. */
	inline ESFLatchedTransformMode Transition(ESFLatchedTransformMode CurrentMode, ESFLatchedTransformMode TappedMode)
	{
		return CurrentMode == TappedMode ? ESFLatchedTransformMode::None : TappedMode;
	}

	/**
	 * Decide how one raw modal input event behaves.
	 * @param bSettingOn   bToggleTransformModes, read live at the event.
	 * @param bPressed     The event's boolean value (Started=true / Completed=false).
	 * @param CurrentMode  The currently latched mode.
	 * @param TappedMode   The latchable mode this input maps to.
	 *
	 * Policy:
	 *  - Setting OFF: never consume (hold path runs unchanged); flag a stale latch for clearing.
	 *  - Setting ON, release (Completed): consume and change nothing - a latch ignores releases,
	 *    and crucially never writes the hold path's release timestamps, so the re-tap-to-cycle
	 *    gesture stays exclusively a hold-mode behavior.
	 *  - Setting ON, press (Started): consume and apply Transition().
	 */
	inline FSFLatchInputDecision DecideInput(bool bSettingOn, bool bPressed,
		ESFLatchedTransformMode CurrentMode, ESFLatchedTransformMode TappedMode)
	{
		FSFLatchInputDecision Decision;
		if (!bSettingOn)
		{
			Decision.bConsume = false;
			Decision.NewMode = ESFLatchedTransformMode::None;
			Decision.bClearStaleLatch = (CurrentMode != ESFLatchedTransformMode::None);
			return Decision;
		}

		Decision.bConsume = true;
		Decision.NewMode = bPressed ? Transition(CurrentMode, TappedMode) : CurrentMode;
		Decision.bClearStaleLatch = false;
		return Decision;
	}

	/** Display name for logs/tests. */
	inline const TCHAR* ToString(ESFLatchedTransformMode Mode)
	{
		switch (Mode)
		{
		case ESFLatchedTransformMode::Spacing:  return TEXT("Spacing");
		case ESFLatchedTransformMode::Steps:    return TEXT("Steps");
		case ESFLatchedTransformMode::Stagger:  return TEXT("Stagger");
		case ESFLatchedTransformMode::Rotation: return TEXT("Rotation");
		case ESFLatchedTransformMode::Recipe:   return TEXT("Recipe");
		default:                                return TEXT("None");
		}
	}
}
