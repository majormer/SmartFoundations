// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Spacing Module (Pure Math)

#pragma once

#include "CoreMinimal.h"
#include "SFSpacingTypes.h"

/**
 * FSFSpacingModule - Pure, stateless spacing calculation module
 * 
 * This module provides all spacing/gap calculations for consecutive placements.
 * It contains NO hologram-specific code and stores NO state.
 * All methods are pure functions that take inputs and return results.
 * 
 * Thread-safe by design (stateless, deterministic).
 * Used by ALL hologram types with zero duplication.
 */
class SMARTFOUNDATIONS_API FSFSpacingModule
{
public:
	/**
	 * Calculate automatic gap for spacing mode
	 * 
	 * @param Mode - Spacing mode (None, X, XY, XYZ)
	 * @param HologramSize - Size of the hologram being placed
	 * @param DefaultGap - Default gap size in cm (default: 50cm = 0.5m)
	 * @return Gap vector with spacing applied to appropriate axes
	 */
	static FVector CalculateAutoGap(
		ESFSpacingMode Mode,
		const FVector& HologramSize,
		float DefaultGap = 50.0f
	);

	/**
	 * Get next placement offset for consecutive placements
	 * 
	 * @param Mode - Spacing mode
	 * @param CurrentOffset - Current placement offset
	 * @param HologramSize - Size of the hologram
	 * @param DefaultGap - Gap size in cm
	 * @return Offset for next placement including gaps
	 */
	static FVector GetNextPlacementOffset(
		ESFSpacingMode Mode,
		const FVector& CurrentOffset,
		const FVector& HologramSize,
		float DefaultGap = 50.0f
	);

	/**
	 * Cycle to next spacing mode
	 * 
	 * @param CurrentMode - Current mode
	 * @return Next mode in the cycle (None → X → XY → XYZ → None)
	 */
	static ESFSpacingMode CycleSpacingMode(ESFSpacingMode CurrentMode);

	/**
	 * Get human-readable name for spacing mode
	 * 
	 * @param Mode - Spacing mode
	 * @return Display name (e.g., "X-Axis Spacing")
	 */
	static FString GetSpacingModeName(ESFSpacingMode Mode);

	/**
	 * Check if a spacing mode is active (not None)
	 * 
	 * @param Mode - Spacing mode to check
	 * @return true if spacing is enabled
	 */
	static bool IsSpacingActive(ESFSpacingMode Mode);
};
