// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/Spacing/SFSpacingTypes.h"
#include "Features/Scaling/SFScalingTypes.h"
#include "SFHUDTypes.generated.h"

/**
 * Centralized counter state for Smart! HUD display.
 * Contains all counter values used by the HUD overlay and provides
 * a single Reset() method to fix counter persistence bugs.
 * 
 * This struct is the single source of truth for all HUD-displayable state.
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFCounterState
{
	GENERATED_BODY()

public:
	// === Grid Dimensions ===
	
	/** Grid array dimensions (X, Y, Z) - supports negative for directional arrays */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Grid")
	FIntVector GridCounters = FIntVector(1, 1, 1);
	
	/** Number of valid child holograms currently spawned */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Grid")
	int32 ValidChildCount = 0;

	// === Spacing Feature ===
	
	/** Spacing offset on X-axis in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Spacing")
	int32 SpacingX = 0;
	
	/** Spacing offset on Y-axis in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Spacing")
	int32 SpacingY = 0;
	
	/** Spacing offset on Z-axis in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Spacing")
	int32 SpacingZ = 0;
	
	/** Current spacing mode (None, X, Y, Z, XY, XZ, YZ, XYZ) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Spacing")
	ESFSpacingMode SpacingMode = ESFSpacingMode::None;
	
	/** Currently active axis for spacing adjustments */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Spacing")
	ESFScaleAxis SpacingAxis = ESFScaleAxis::X;

	// === Steps Feature ===
	
	/** Steps offset on X-axis (columns) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Steps")
	int32 StepsX = 0;
	
	/** Steps offset on Y-axis (rows) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Steps")
	int32 StepsY = 0;
	
	/** Currently active axis for steps adjustments */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Steps")
	ESFScaleAxis StepsAxis = ESFScaleAxis::X;

	// === Stagger Feature (Lateral Grid Offset) ===
	
	/** Stagger offset on X-axis (sideways curves) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Stagger")
	int32 StaggerX = 0;
	
	/** Stagger offset on Y-axis (forward/back curves) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Stagger")
	int32 StaggerY = 0;
	
	/** Stagger offset on ZX-axis (vertical layer lean in X direction) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Stagger")
	int32 StaggerZX = 0;
	
	/** Stagger offset on ZY-axis (vertical layer lean in Y direction) in UE units (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Stagger")
	int32 StaggerZY = 0;
	
	/** Currently active axis for stagger adjustments (ZX first for auto-connect grids) */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Stagger")
	ESFScaleAxis StaggerAxis = ESFScaleAxis::ZX;

	// === Rotation Feature (Radial/Arc placement) ===
	// DESIGN DECISION: Positive rotation = Clockwise (user expectation)
	// This is OPPOSITE to Unreal's Yaw convention (counter-clockwise positive)
	// The sign is flipped in CalculateRotationOffset() when applying to Unreal coordinates
	
	/** Rotation step on Z-axis (Yaw) in degrees - horizontal arc
	 * Positive = clockwise rotation when viewed from above
	 * Combined with SpacingX (arc length) to determine radius: R = Spacing / RotationStep(radians)
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Rotation")
	float RotationZ = 0.0f;
	
	/** Currently active axis for rotation adjustments */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Rotation")
	ESFScaleAxis RotationAxis = ESFScaleAxis::Z;

	// TODO: Add Levitation counters when Task 41 is implemented
	// UPROPERTY(BlueprintReadWrite, Category = "Smart|Levitation")
	// float LevitationRelative = 0.0f;
	// 
	// UPROPERTY(BlueprintReadWrite, Category = "Smart|Levitation")
	// float LevitationAbsolute = 0.0f;

	// === Methods ===
	
	/**
	 * Reset all counters to their default values.
	 * This is the ONLY method that should be called to reset counter state
	 * to fix the counter persistence bug.
	 */
	void Reset()
	{
		// Grid
		GridCounters = FIntVector(1, 1, 1);
		ValidChildCount = 0;
		
		// Spacing
		SpacingX = SpacingY = SpacingZ = 0;
		SpacingMode = ESFSpacingMode::None;
		SpacingAxis = ESFScaleAxis::X;
		
		// Steps
		StepsX = StepsY = 0;
		StepsAxis = ESFScaleAxis::X;
		
		// Stagger
		StaggerX = StaggerY = StaggerZX = StaggerZY = 0;
		StaggerAxis = ESFScaleAxis::ZX;  // Start with ZX for auto-connect grids
		
		// Rotation
		RotationZ = 0.0f;
		RotationAxis = ESFScaleAxis::Z;
		
		// Levitation (when implemented)
		// LevitationRelative = 0.0f;
		// LevitationAbsolute = 0.0f;
	}
	
	/**
	 * Check if counter state equals another state (useful for testing and coalescing)
	 */
	bool Equals(const FSFCounterState& Other) const
	{
		return GridCounters == Other.GridCounters
			&& ValidChildCount == Other.ValidChildCount
			&& SpacingX == Other.SpacingX
			&& SpacingY == Other.SpacingY
			&& SpacingZ == Other.SpacingZ
			&& SpacingMode == Other.SpacingMode
			&& SpacingAxis == Other.SpacingAxis
			&& StepsX == Other.StepsX
			&& StepsY == Other.StepsY
			&& StepsAxis == Other.StepsAxis
			&& StaggerX == Other.StaggerX
			&& StaggerY == Other.StaggerY
			&& StaggerZX == Other.StaggerZX
			&& StaggerZY == Other.StaggerZY
			&& StaggerAxis == Other.StaggerAxis
			&& FMath::IsNearlyEqual(RotationZ, Other.RotationZ)
			&& RotationAxis == Other.RotationAxis;
			// && LevitationRelative == Other.LevitationRelative
			// && LevitationAbsolute == Other.LevitationAbsolute;
	}
	
	/**
	 * Check if any non-default counters are set (used for HUD visibility)
	 */
	bool HasAnyNonDefaultValues() const
	{
		return (GridCounters.X != 1 || GridCounters.Y != 1 || GridCounters.Z != 1)
			|| SpacingX != 0 || SpacingY != 0 || SpacingZ != 0
			|| StepsX != 0 || StepsY != 0
			|| StaggerX != 0 || StaggerY != 0 || StaggerZX != 0 || StaggerZY != 0
			|| !FMath::IsNearlyZero(RotationZ);
			// || LevitationRelative != 0.0f;
	}
};
