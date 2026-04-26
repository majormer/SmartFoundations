#pragma once

#include "CoreMinimal.h"
#include "FSFGridArrayTypes.h"

/**
 * Smart! Grid Array Math Module
 * 
 * Pure, stateless functions for grid array placement calculations.
 * Provides:
 * - Zero-skip counter logic for array placement
 * - Grid position generation for child holograms
 * - Transform calculations with hologram-relative rotation
 * 
 * This module is hologram-agnostic and contains only pure math.
 * All functions are thread-safe by virtue of being stateless.
 * 
 * @see docs/Features/Scaling/ARRAY_PLACEMENT_ARCHITECTURE.md
 * @see Task #16 in tasks/tasks.json
 */
namespace FSFGridArray
{
	// ========================================
	// Counter Logic (Zero-Skip Semantics)
	// ========================================

	/**
	 * Increment counter with zero-skip semantics
	 * 
	 * Rules:
	 * - If Counter == -1, set to 1 (skip zero from negative)
	 * - Otherwise, increment by 1
	 * 
	 * Used by: Array placement controls to increase item count
	 * 
	 * @param Counter Counter to modify (passed by reference)
	 */
	SMARTFOUNDATIONS_API void IncrementCounter(int32& Counter);

	/**
	 * Decrement counter with zero-skip semantics
	 * 
	 * Rules:
	 * - If Counter == 1, set to -1 (skip zero to negative)
	 * - Otherwise, decrement by 1
	 * 
	 * Used by: Array placement controls to decrease item count
	 * 
	 * @param Counter Counter to modify (passed by reference)
	 */
	SMARTFOUNDATIONS_API void DecrementCounter(int32& Counter);

	/**
	 * Get direction from counter value
	 * 
	 * Returns:
	 * - +1 if Counter > 0
	 * - -1 if Counter < 0
	 * - +1 if Counter == 0 (default positive direction)
	 * 
	 * Used by: Grid offset calculations to determine placement direction
	 * 
	 * @param Counter Counter value to check
	 * @return Direction as +1 or -1
	 */
	SMARTFOUNDATIONS_API int32 GetDirection(int32 Counter);

	/**
	 * Get absolute count from counter value
	 * 
	 * Returns abs(Counter), representing actual number of items.
	 * Zero-skip semantics mean: Counter values -1, 0, 1 all represent 1 item.
	 * 
	 * @param Counter Counter value
	 * @return Absolute item count
	 */
	SMARTFOUNDATIONS_API int32 GetAbsoluteCount(int32 Counter);

	// ========================================
	// Grid Generation
	// ========================================

	/**
	 * Calculate world-space transforms for grid array children
	 * 
	 * Generates abs(Counters.X) × abs(Counters.Y) × abs(Counters.Z) transforms.
	 * 
	 * Loop order: for Z, for X, for Y (this order must be maintained for child index mapping)
	 * 
	 * Process:
	 * 1. Calculate local offset for each grid cell
	 * 2. Apply hologram rotation to offset
	 * 3. Transform to world space using ParentTransform
	 * 
	 * @param Counters Grid dimensions (X, Y, Z item counts)
	 * @param HologramSize Size of single item (used for spacing calculation)
	 * @param Spacing Additional spacing between items per axis
	 * @param ParentTransform World transform of parent hologram
	 * @return Array of world-space transforms for children (size = CalculateTotalItems(Counters))
	 */
	SMARTFOUNDATIONS_API TArray<FTransform> CalculateGridPositions(
		const FVector3i& Counters,
		const FVector& HologramSize,
		const FVector& Spacing,
		const FTransform& ParentTransform
	);

	// ========================================
	// Helper Utilities
	// ========================================

	/**
	 * Calculate total number of items in grid
	 * 
	 * Returns: abs(Counters.X) * abs(Counters.Y) * abs(Counters.Z)
	 * 
	 * Overflow safety: Clamps to INT32_MAX if multiplication would overflow.
	 * Returns 0 if any counter is 0.
	 * 
	 * @param Counters Grid dimensions
	 * @return Total item count (clamped to INT32_MAX on overflow)
	 */
	SMARTFOUNDATIONS_API int32 CalculateTotalItems(const FVector3i& Counters);

	/**
	 * Calculate local offset for child at grid indices
	 * 
	 * Returns offset relative to hologram origin (before rotation applied).
	 * 
	 * Formula per axis: (Index * (Size + Spacing)) * Direction
	 * 
	 * @param XIndex Grid X index (0-based)
	 * @param YIndex Grid Y index (0-based)
	 * @param ZIndex Grid Z index (0-based)
	 * @param Size Item size per axis (from hologram bounds)
	 * @param Spacing Additional spacing per axis
	 * @param Directions Direction per axis (+1 or -1)
	 * @return Local offset vector (before rotation)
	 */
	SMARTFOUNDATIONS_API FVector CalculateChildOffset(
		int32 XIndex,
		int32 YIndex,
		int32 ZIndex,
		const FVector& Size,
		const FVector& Spacing,
		const FVector3i& Directions
	);

	/**
	 * Apply hologram rotation to local offset
	 * 
	 * Rotates the local offset vector by the hologram's rotation.
	 * Uses Unreal's standard rotation: Rotator.RotateVector().
	 * 
	 * Guarantees: No NaNs/Infs for finite inputs.
	 * 
	 * @param LocalOffset Offset in hologram local space
	 * @param HologramRotation Hologram's world rotation
	 * @return Rotated offset vector
	 */
	SMARTFOUNDATIONS_API FVector ApplyHologramRotation(
		const FVector& LocalOffset,
		const FRotator& HologramRotation
	);

} // namespace FSFGridArray
