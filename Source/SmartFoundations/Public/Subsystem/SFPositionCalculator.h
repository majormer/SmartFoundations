#pragma once

#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "Features/Scaling/SFScalingTypes.h"
#include "Features/Spacing/SFSpacingTypes.h"
#include "HUD/SFHUDTypes.h"

class AFGHologram;

/**
 * Smart! Position Calculator - Handles grid position and offset calculations
 * 
 * Extracted from SFSubsystem.cpp (Phase 0 Refactoring - Task #61.6)
 * 
 * Responsibilities:
 * - Calculate world positions for child holograms in grid arrays
 * - Apply spacing offsets (X, Y, Z, X+Y modes)
 * - Apply steps elevation (stair-step effect for rows/columns)
 * - Apply stagger curves (lateral grid offset effects)
 * - Update child hologram transforms based on parent movement
 * 
 * Dependencies:
 * - Reads counter state (grid dimensions, spacing, steps, stagger)
 * - Applies transforms to hologram actors
 */
class SMARTFOUNDATIONS_API FSFPositionCalculator
{
public:
	FSFPositionCalculator();
	~FSFPositionCalculator();

	/**
	 * Calculate position for a single child hologram in the grid
	 * 
	 * @param X Grid X index (direction-aware)
	 * @param Y Grid Y index (direction-aware)
	 * @param Z Grid Z index (direction-aware)
	 * @param ParentLocation Parent hologram location
	 * @param ParentRotation Parent hologram rotation
	 * @param ItemSize Size of one grid cell (building dimensions)
	 * @param CounterState Current counter values (spacing, steps, stagger)
	 * @param GridIndex Linear index in grid array (for logging)
	 * @param AnchorOffset Pivot offset compensation (Z for attachment types)
	 * @return Calculated world position for child hologram
	 */
	FVector CalculateChildPosition(
		int32 X, int32 Y, int32 Z,
		const FVector& ParentLocation,
		const FRotator& ParentRotation,
		const FVector& ItemSize,
		const FSFCounterState& CounterState,
		int32 GridIndex = 0,
		const FVector& AnchorOffset = FVector::ZeroVector
	) const;

	/**
	 * Update positions for all child holograms in grid
	 * 
	 * @param Children Array of child hologram weak pointers
	 * @param CounterState Current counter values
	 * @param ParentLocation Parent hologram location
	 * @param ParentRotation Parent hologram rotation
	 * @param ItemSize Building dimensions
	 */
	void UpdateChildPositions(
		const TArray<TWeakObjectPtr<AFGHologram>>& Children,
		const FSFCounterState& CounterState,
		const FVector& ParentLocation,
		const FRotator& ParentRotation,
		const FVector& ItemSize
	);

	/**
	 * Update child positions to match parent transform changes
	 * Optimized version that only updates if parent moved
	 * 
	 * @param Children Array of child holograms
	 * @param CounterState Counter values
	 * @param CurrentTransform Current parent transform
	 * @param LastKnownTransform Last cached transform (for change detection)
	 * @param ItemSize Building dimensions
	 * @return true if positions were updated, false if no change
	 */
	bool UpdateChildrenForCurrentTransform(
		const TArray<TWeakObjectPtr<AFGHologram>>& Children,
		const FSFCounterState& CounterState,
		const FTransform& CurrentTransform,
		FTransform& LastKnownTransform,
		const FVector& ItemSize
	);

	// ========================================
	// Native Nudge Coordination (PRD Requirement)
	// ========================================

	/**
	 * Check if native vertical nudge is active (PageUp/PageDown)
	 * Used for floor validation coordination
	 * 
	 * @param Hologram Hologram to check nudge state
	 * @return true if vertical nudge offset > 0.1f threshold
	 */
	bool IsNativeVerticalNudgeActive(AFGHologram* Hologram) const;

	/**
	 * Check if native horizontal nudge is active (Arrow keys)
	 * 
	 * @param Hologram Hologram to check nudge state
	 * @return true if horizontal nudge offset > 0.1f threshold
	 */
	bool IsNativeHorizontalNudgeActive(AFGHologram* Hologram) const;

	/**
	 * Get native nudge offset vector from hologram
	 * Smart! offsets are automatically applied RELATIVE to this offset
	 * because ParentLocation includes the nudge
	 * 
	 * @param Hologram Hologram to query
	 * @return Native nudge offset vector
	 */
	FVector GetNativeNudgeOffset(AFGHologram* Hologram) const;

private:
	/**
	 * Calculate spacing offset for a grid position
	 * 
	 * @param X Grid X coordinate
	 * @param Y Grid Y coordinate
	 * @param Z Grid Z coordinate
	 * @param CounterState Counter values
	 * @return Spacing offset vector in local space
	 */
	FVector CalculateSpacingOffset(
		int32 X, int32 Y, int32 Z,
		const FSFCounterState& CounterState
	) const;

	/**
	 * Calculate steps elevation offset for a grid position
	 * 
	 * @param X Grid X coordinate
	 * @param Y Grid Y coordinate
	 * @param CounterState Counter values
	 * @return Steps elevation offset (Z-axis)
	 */
	float CalculateStepsOffset(
		int32 X, int32 Y,
		const FSFCounterState& CounterState
	) const;

	/**
	 * Calculate stagger curve offset for a grid position (lateral grid offset + vertical shift)
	 * 
	 * @param X Grid X coordinate
	 * @param Y Grid Y coordinate
	 * @param Z Grid Z coordinate
	 * @param CounterState Counter values (includes StaggerX, StaggerY, StaggerZX, StaggerZY)
	 * @return Stagger curve offset vector (perpendicular axis offset + independent vertical shift)
	 */
	FVector CalculateStaggerOffset(
		int32 X, int32 Y, int32 Z,
		const FSFCounterState& CounterState
	) const;

	/**
	 * Calculate rotation offset for a grid position (radial/arc placement)
	 * 
	 * DESIGN DECISIONS:
	 * - Positive rotation = Clockwise (user expectation, opposite Unreal's CCW convention)
	 * - SpacingX = arc length (distance along curve between buildings)
	 * - If SpacingX is 0, uses ItemSize.X as default arc length (edge-to-edge placement)
	 * - Multi-row (Y > 1) = Concentric arcs (stadium seating style)
	 * 
	 * @param X Grid X coordinate (position along arc)
	 * @param Y Grid Y coordinate (concentric arc index)
	 * @param Z Grid Z coordinate (vertical layer)
	 * @param CounterState Counter values (includes RotationZ, SpacingX, SpacingY)
	 * @param ItemSize Building dimensions (used as fallback arc length if SpacingX is 0)
	 * @param OutRotation Output rotation to apply to the building
	 * @return Position offset for arc placement (replaces linear X spacing when active)
	 */
	FVector CalculateRotationOffset(
		int32 X, int32 Y, int32 Z,
		const FSFCounterState& CounterState,
		const FVector& ItemSize,
		FRotator& OutRotation
	) const;
};
