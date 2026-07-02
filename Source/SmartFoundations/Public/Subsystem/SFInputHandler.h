// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Features/Scaling/SFScalingTypes.h"

class AFGPlayerController;
class UInputAction;
class UFGEnhancedInputComponent;
class USFSubsystem;

/**
 * Smart! Input Handler - Manages Enhanced Input bindings and input state
 * 
 * Extracted from SFSubsystem.cpp (Phase 0 Refactoring - Task #61.6)
 * 
 * Responsibilities:
 * - Enhanced Input action binding and lifecycle management
 * - Input state tracking (modifiers, modal features)
 * - Deferred input setup and rebinding
 *
 * [#162/#429] Wheel-rotation suppression during modifier windows is NOT handled here anymore.
 * The old approach (remove the vanilla MC_BuildGunBuild mapping context, cache its priority,
 * restore on release - the #272 machinery) had been dead code for a while: rotation suppression
 * actually rode on the hologram lock (vanilla ignores wheel-rotate while locked). It is now the
 * UFGBuildGunStateBuild::Scroll_Implementation hook in SFGameInstanceModule, gated by
 * USFSubsystem::ShouldSuppressBuildGunScroll - which also suppresses other mods' scroll-driven
 * rotation (InfiniteNudge) that no input-context surgery could reach.
 * 
 * Dependencies:
 * - Calls back to USFSubsystem for actual feature logic
 * - Reads counter state from subsystem for validation
 */
class SMARTFOUNDATIONS_API FSFInputHandler
{
public:
	FSFInputHandler();
	~FSFInputHandler();

	/** Initialize input handler with owning subsystem */
	void Initialize(USFSubsystem* InOwnerSubsystem);

	/** Cleanup and unbind all input */
	void Shutdown();

	/** Setup Enhanced Input bindings for player controller */
	void SetupPlayerInput(AFGPlayerController* PlayerController);

	/** Check and setup input periodically (polling for player controller) */
	void CheckForPlayerController();

	/** Rebind shortly after SetupPlayerInput to catch post-initialization state */
	void RebindAfterDelay();

	/** [#358] Add/remove the Smart! mapping context with hologram lifecycle. The context is
	 * scoped to hologram-active so Smart!'s priority-100 bindings (e.g. Scale X on X) only
	 * shadow vanilla keys while the player is actually building - Satisfactory 1.2 bound the
	 * Customizer to X, and an always-on context consumed it everywhere. */
	void SetSmartContextActive(bool bActive);

	// ========================================
	// Input State Queries
	// ========================================

	bool IsModifierScaleXActive() const { return bModifierScaleXActive; }
	bool IsModifierScaleYActive() const { return bModifierScaleYActive; }
	bool IsSpacingModeActive() const { return bSpacingModeActive; }
	bool IsStepsModeActive() const { return bStepsModeActive; }
	bool IsStaggerModeActive() const { return bStaggerModeActive; }
	bool IsRotationModeActive() const { return bRotationModeActive; }

	/** Check if any Smart! modal features are currently active */
	bool IsAnyModalFeatureActive() const;

	/** Has input been set up successfully? */
	bool IsInputSetupCompleted() const { return bInputSetupCompleted; }

	// ========================================
	// Native Nudge Compatibility (PRD Requirement - Task #61.6)
	// ========================================

	/** Check if native vertical nudge is active (PageUp/PageDown) */
	bool IsNativeVerticalNudgeActive(class AFGHologram* Hologram) const;

	/** Check if native horizontal nudge is active (Arrow keys) */
	bool IsNativeHorizontalNudgeActive(class AFGHologram* Hologram) const;

	/** Get native nudge offset vector from hologram */
	FVector GetNativeNudgeOffset(class AFGHologram* Hologram) const;

	// ========================================
	// Enhanced Input Action Handlers
	// ========================================

	/** Grid Scaling - Axis1D handlers */
	void OnScaleXChanged(const FInputActionValue& Value);
	void OnScaleYChanged(const FInputActionValue& Value);
	void OnScaleZChanged(const FInputActionValue& Value);

	/** Grid Scaling - Modifier tracking */
	void OnModifierScaleXPressed(const FInputActionValue& Value);
	void OnModifierScaleXReleased(const FInputActionValue& Value);
	void OnModifierScaleYPressed(const FInputActionValue& Value);
	void OnModifierScaleYReleased(const FInputActionValue& Value);

	/** Spacing - Mode tracking + Axis adjustment */
	void OnSpacingModeChanged(const FInputActionValue& Value);
	void OnSpacingCycleAxis();

	/** Steps - Mode tracking + Axis adjustment */
	void OnStepsModeChanged(const FInputActionValue& Value);

	/** Generic Cycle Axis - Context-aware mode swapper */
	void OnCycleAxis();

	/** Stagger - Mode tracking + Axis adjustment */
	void OnStaggerModeChanged(const FInputActionValue& Value);

	/** Rotation - Mode tracking + Axis adjustment */
	void OnRotationModeChanged(const FInputActionValue& Value);

	/** Unified Value Adjustment - Context-aware handlers */
	void OnValueIncreased(const FInputActionValue& Value);
	void OnValueDecreased(const FInputActionValue& Value);

	/** Toggle Arrows */
	void OnToggleArrows();
	
private:
	/** Owning subsystem - used to call back for feature logic */
	TWeakObjectPtr<USFSubsystem> OwnerSubsystem;

	/** Last detected player controller */
	TWeakObjectPtr<AFGPlayerController> LastController;

	/** Deferred rebind timer */
	FTimerHandle DeferredRebindTimer;

	/** Has input been set up for the current player */
	bool bInputSetupCompleted = false;

	/** [#358] Whether the Smart! mapping context is currently in the Enhanced Input stack */
	bool bSmartContextActive = false;

	/** Modifier key state for Z-axis wheel detection */
	bool bModifierScaleXActive = false;
	bool bModifierScaleYActive = false;

	/** Feature mode state */
	bool bSpacingModeActive = false;
	bool bStepsModeActive = false;
	bool bStaggerModeActive = false;
	bool bRotationModeActive = false;

	/** Helper: Apply scaling from axis input */
	void ApplyAxisScaling(ESFScaleAxis Axis, int32 StepDelta, const TCHAR* DebugLabel);
};
