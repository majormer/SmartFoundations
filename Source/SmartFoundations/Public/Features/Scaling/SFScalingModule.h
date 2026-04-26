// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Scaling Module (Pure Math)

#pragma once

#include "CoreMinimal.h"
#include "SFScalingTypes.h"

/**
 * FSFScalingModule - Pure, stateless scaling math module
 * 
 * This module provides all scaling calculations used by Smart! features.
 * It contains NO hologram-specific code and stores NO state.
 * All methods are pure functions that take inputs and return results.
 * 
 * Thread-safe by design (stateless, deterministic).
 * Used by ALL hologram types with zero duplication.
 */
class SMARTFOUNDATIONS_API FSFScalingModule
{
public:
	/**
	 * Calculate a new offset by applying steps along the given axis
	 * 
	 * @param CurrentOffset - The current offset vector
	 * @param Axis - Which axis to modify (X, Y, or Z)
	 * @param Steps - Number of steps to apply (can be negative)
	 * @param StepSize - Size of each step in cm (default: 50cm)
	 * @return New offset with the axis modified by (Steps * StepSize)
	 */
	static FVector CalculateOffset(
		const FVector& CurrentOffset,
		ESFScaleAxis Axis,
		int32 Steps,
		float StepSize = 50.0f
	);

	/**
	 * Calculate offset for a single axis value
	 * 
	 * @param Current - Current value along the axis
	 * @param Steps - Number of steps to apply
	 * @param StepSize - Size of each step in cm
	 * @return New value along the axis
	 */
	static float CalculateAxisOffset(
		float Current,
		int32 Steps,
		float StepSize = 50.0f
	);

	/**
	 * Snap a vector to the nearest grid point
	 * 
	 * @param Offset - The offset vector to snap
	 * @param SnapSize - Grid size in cm (must be > 0)
	 * @return Snapped offset with each component rounded to nearest multiple of SnapSize
	 */
	static FVector ApplySnapping(
		const FVector& Offset,
		float SnapSize = 50.0f
	);

	/**
	 * Snap a single value to the nearest grid point
	 * 
	 * @param Value - The value to snap
	 * @param SnapSize - Grid size (must be > 0)
	 * @return Value rounded to nearest multiple of SnapSize
	 */
	static float SnapValue(
		float Value,
		float SnapSize = 50.0f
	);

	/**
	 * Validate that an offset lies within bounds
	 * 
	 * @param Offset - The offset to validate
	 * @param Bounds - The bounds to check against
	 * @return true if offset is within bounds (inclusive), false otherwise
	 */
	static bool ValidateOffset(
		const FVector& Offset,
		const FSFScaleBounds& Bounds
	);

	/**
	 * Clamp an offset to the nearest point inside bounds
	 * 
	 * @param Offset - The offset to clamp
	 * @param Bounds - The bounds to clamp to
	 * @return Clamped offset (closest point inside bounds)
	 */
	static FVector ClampOffset(
		const FVector& Offset,
		const FSFScaleBounds& Bounds
	);

	/**
	 * Check if a vector is numerically safe (no NaN, no Inf, within reasonable range)
	 * 
	 * @param Offset - The vector to check
	 * @param MaxMagnitude - Maximum allowed magnitude (default: 1,000,000 cm = 10km)
	 * @return true if the vector is safe to use
	 */
	static bool IsWithinSafeRange(
		const FVector& Offset,
		float MaxMagnitude = 1000000.0f
	);

	/**
	 * Get the axis unit vector for a given axis enum
	 * 
	 * @param Axis - The axis to get the vector for
	 * @return Unit vector along the specified axis (e.g., X=ForwardVector)
	 */
	static FVector GetAxisVector(ESFScaleAxis Axis);

	/**
	 * Convert axis enum to axis index (0=X, 1=Y, 2=Z)
	 * 
	 * @param Axis - The axis enum
	 * @return Index (0, 1, or 2)
	 */
	static int32 GetAxisIndex(ESFScaleAxis Axis);
};
