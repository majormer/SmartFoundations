// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Arrow Visualization Module (Redesigned for Task #17)

#pragma once

#include "CoreMinimal.h"
#include "FSFArrowTypes.h"

/**
 * FSFArrowModule - Dynamic visual axis indicators for hologram manipulation
 * 
 * REDESIGNED IMPLEMENTATION (Task #17):
 * - Uses DrawDebugDirectionalArrow for PIE and packaged game visibility
 * - Provides hologram-relative arrow positioning with proper rotation
 * - Supports modifier-key aware highlighting
 * - Tracks last-used axis for visual feedback
 * - Non-replicated, local-only visualization
 * 
 * ARCHITECTURE:
 * - Stateless rendering via DrawDebugDirectionalArrow
 * - Caller responsibility: call UpdateArrows() on each state change or tick while visible
 * - No component lifecycle management required
 * 
 * ORIENTATION (Hologram-Relative):
 * - Red (X): Hologram forward = HologramRotation.RotateVector(FVector(500, 0, 0))
 * - Green (Y): Hologram right = HologramRotation.RotateVector(FVector(0, 500, 0))
 * - Blue (Z): World up = FVector(0, 0, 500) (no rotation)
 * 
 * HIGHLIGHTING PRIORITY:
 * 1. Modifier keys (LeftShift=X, LeftCtrl=Y, Both=Z) override LastAxis
 * 2. If no modifiers, use LastAxis for highlighting
 * 3. ArrowCount=1 shows only highlighted axis, ArrowCount=3 shows all
 * 
 * @see docs/Features/Scaling/ARROW_SYSTEM_ANALYSIS.md
 * @see docs/Features/Scaling/AXIS_ORIENTATION.md
 * @see Task #17 in tasks/tasks.json
 * 
 * USAGE EXAMPLE:
 * ```cpp
 * FSFArrowModule ArrowModule;
 * 
 * // In hologram Tick() or on state change:
 * ArrowModule.UpdateArrows(
 *     GetWorld(),
 *     HologramTransform,
 *     CurrentLastAxis,
 *     bArrowsVisible
 * );
 * 
 * // On axis input (e.g., Numpad 8):
 * ArrowModule.SetHighlightedAxis(EAxis::X);
 * ```
 */
class SMARTFOUNDATIONS_API FSFArrowModule
{
public:
	/** Constructor */
	FSFArrowModule();

	/**
	 * Primary update method - computes and draws arrows
	 * 
	 * Call this:
	 * - On every tick while arrows should be visible (for persistent debug rendering)
	 * - On hologram transform changes
	 * - On input state changes (axis, modifiers, visibility)
	 * 
	 * @param World World context for drawing
	 * @param HologramTransform World transform of the hologram
	 * @param LastAxis Last axis that was manipulated (for highlighting)
	 * @param bVisible Whether arrows should be drawn
	 */
	void UpdateArrows(
		UWorld* World,
		const FTransform& HologramTransform,
		ELastAxisInput LastAxis,
		bool bVisible
	);

	/**
	 * Set highlighted axis programmatically
	 * 
	 * Updates internal LastAxis state and triggers visual update if currently visible.
	 * Next UpdateArrows() call will reflect this change.
	 * 
	 * @param Axis Axis to highlight (X, Y, Z, or None)
	 */
	void SetHighlightedAxis(ELastAxisInput Axis);

	/**
	 * Low-level draw call for arrows at specific location/rotation
	 * 
	 * Used internally by UpdateArrows, but exposed for testing/debugging.
	 * Directly calls DrawDebugDirectionalArrow with current configuration.
	 * 
	 * @param World World context for drawing
	 * @param Location Center point for arrow placement
	 * @param Rotation Base rotation for orientation calculations
	 */
	void DrawArrows(
		UWorld* World,
		const FVector& Location,
		const FRotator& Rotation
	);

	/**
	 * Update color scheme at runtime
	 * 
	 * Default: X=Red, Y=Green, Z=Blue
	 * 
	 * @param ColorScheme New color configuration
	 */
	void SetArrowColors(const FArrowColorScheme& ColorScheme);

	/**
	 * Update arrow configuration
	 * 
	 * Allows runtime adjustment of arrow size, offset, count mode, etc.
	 * 
	 * @param Config New configuration settings
	 */
	void SetArrowConfig(const FArrowConfig& Config);

	/**
	 * Update modifier key state
	 * 
	 * Call this when modifier keys change (e.g., from input component).
	 * Affects highlighting priority in next UpdateArrows() call.
	 * 
	 * @param bShift LeftShift key pressed
	 * @param bCtrl LeftCtrl key pressed
	 */
	void SetModifierKeys(bool bShift, bool bCtrl);

	/** Check if arrows are currently visible */
	bool AreArrowsVisible() const { return bCurrentlyVisible; }

	/** Get current highlighted axis (considering modifiers and LastAxis) */
	ELastAxisInput GetEffectiveHighlightedAxis() const;

	// ========================================
	// Legacy API Compatibility (Deprecated)
	// ========================================

	/** @deprecated Use UpdateArrows() instead */
	void SpawnArrows(UWorld* World, USceneComponent* ParentComponent, const FVector& BaseLocation, const FRotator& BaseRotation);

	/** @deprecated Use UpdateArrows() instead */
	void SetVisibility(bool bVisible);

	/** @deprecated Arrows always exist, use UpdateArrows() with bVisible=false */
	bool AreArrowsSpawned() const { return true; }

private:
	/** Current color scheme */
	FArrowColorScheme ColorScheme;

	/** Current configuration */
	FArrowConfig Config;

	/** Last axis input state (set by SetHighlightedAxis or UpdateArrows) */
	ELastAxisInput CurrentLastAxis;

	/** Modifier key states */
	bool bLeftShiftPressed;
	bool bLeftCtrlPressed;

	/** Current visibility state */
	bool bCurrentlyVisible;

	/** Last draw world (for logging/debugging) */
	TWeakObjectPtr<UWorld> LastWorld;

	/** Draw a single arrow using debug visualization */
	void DrawSingleArrow(
		UWorld* World,
		const FVector& Start,
		const FVector& End,
		const FColor& Color,
		float Thickness
	);

	/** Calculate effective axis to highlight based on modifiers and LastAxis */
	ELastAxisInput CalculateHighlightedAxis() const;

	/** Get thickness for an axis (base or highlighted) */
	float GetThicknessForAxis(ELastAxisInput Axis, ELastAxisInput HighlightedAxis) const;
};
