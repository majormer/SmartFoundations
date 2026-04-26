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
 * - Vanilla Build Gun context management (disable during wheel input)
 * - Deferred input setup and rebinding
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
	// Vanilla Build Gun Context Management
	// ========================================

	/** Disable vanilla Build Gun mapping context to prevent wheel input conflicts */
	void DisableVanillaBuildGunContext();

	/** Re-enable vanilla Build Gun mapping context */
	void EnableVanillaBuildGunContext();

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
	
	/** Debug: base-game action to validate pipeline */
	void OnDebugPrimaryFire();

private:
	/** Owning subsystem - used to call back for feature logic */
	TWeakObjectPtr<USFSubsystem> OwnerSubsystem;

	/** Last detected player controller */
	TWeakObjectPtr<AFGPlayerController> LastController;

	/** Deferred rebind timer */
	FTimerHandle DeferredRebindTimer;

	/** Has input been set up for the current player */
	bool bInputSetupCompleted = false;

	/** Modifier key state for Z-axis wheel detection */
	bool bModifierScaleXActive = false;
	bool bModifierScaleYActive = false;

	/** Vanilla Build Gun context priority tracking (Issue #272)
	 * When we remove the vanilla context to prevent rotation, we must restore
	 * it at the SAME priority. Re-adding at priority 0 changes the priority
	 * stack, causing RMB (Hold) to be overridden by the build menu action. */
	int32 CachedVanillaContextPriority = -1;
	bool bVanillaContextRemoved = false;

	/** Feature mode state */
	bool bSpacingModeActive = false;
	bool bStepsModeActive = false;
	bool bStaggerModeActive = false;
	bool bRotationModeActive = false;

	/** Helper: Apply scaling from axis input */
	void ApplyAxisScaling(ESFScaleAxis Axis, int32 StepDelta, const TCHAR* DebugLabel);
};
