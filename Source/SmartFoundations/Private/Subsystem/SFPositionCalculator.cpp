#include "Subsystem/SFPositionCalculator.h"
#include "SmartFoundations.h"
#include "FGHologram.h"

FSFPositionCalculator::FSFPositionCalculator()
{
}

FSFPositionCalculator::~FSFPositionCalculator()
{
}

FVector FSFPositionCalculator::CalculateChildPosition(
	int32 X, int32 Y, int32 Z,
	const FVector& ParentLocation,
	const FRotator& ParentRotation,
	const FVector& ItemSize,
	const FSFCounterState& CounterState,
	int32 GridIndex,
	const FVector& AnchorOffset
) const
{
	// Original Smart! logic from SFHologramHelper.cpp lines 1682-1771
	// Extracted from SFSubsystem::CalculateChildPosition (Phase 0 - Task #61.6)
	// Refactored to use helper methods (Module 2 completion - Phase 1)
	
	// NATIVE NUDGE COORDINATION (PRD Requirement):
	// Smart! offsets are automatically applied RELATIVE to native nudge offsets.
	// ParentLocation already includes any native nudge (GetActorLocation() = base + nudge),
	// so all Smart! calculations (spacing, steps, stagger) are relative to the nudged position.
	// This means if the player nudges the parent up 100cm, all children move up 100cm + their Smart! offsets.
	
	// Check if rotation is active - changes how we calculate position
	FRotator ChildRotationOffset = FRotator::ZeroRotator;
	const bool bRotationActive = !FMath::IsNearlyZero(CounterState.RotationZ);
	
	FVector NewLocation;
	
	if (bRotationActive)
	{
		// ROTATION MODE: Arc/radial placement
		// Rotation replaces linear X spacing with arc positioning
		FVector ArcOffset = CalculateRotationOffset(X, Y, Z, CounterState, ItemSize, ChildRotationOffset);
		
		// Start from parent location
		NewLocation = ParentLocation;
		
		// In linear mode, grid-local offsets (OffsetX, OffsetY) are mapped into
		// world space using the DegreeX / DegreeY transformation below. To keep
		// behavior consistent, we treat the arc offset as a replacement for the
		// linear (OffsetX, OffsetY) pair and run it through the same mapping.
		float DegreeX = FMath::DegreesToRadians(180.0f - ParentRotation.Yaw - 90.0f);
		float DegreeY = FMath::DegreesToRadians(180.0f - ParentRotation.Yaw);
		float SinX = FMath::Sin(DegreeX);
		float CosX = FMath::Cos(DegreeX);
		float SinY = FMath::Sin(DegreeY);
		float CosY = FMath::Cos(DegreeY);
		
		// Combine arc offset and stagger in grid-local space
		FVector StaggerOffset = CalculateStaggerOffset(X, Y, Z, CounterState);
		float OffsetX = ArcOffset.X + StaggerOffset.X;
		float OffsetY = ArcOffset.Y + StaggerOffset.Y;
		
		// Apply rotation to offsets (same mapping as linear mode)
		NewLocation.X = NewLocation.X - OffsetX * SinX + OffsetY * SinY;
		NewLocation.Y = NewLocation.Y - OffsetX * CosX + OffsetY * CosY;
		
		// Z offset from vertical spacing and steps
		NewLocation.Z += Z * ItemSize.Z + CounterState.SpacingZ * Z;
		
		// Apply Steps progressive VERTICAL offset - creates corkscrew/helix
		float StepsOffset = CalculateStepsOffset(X, Y, CounterState);
		NewLocation.Z += StepsOffset;
	}
	else
	{
		// LINEAR MODE: Original Smart! logic
		// Calculate base offsets in local space (building dimensions + spacing)
		FVector SpacingOffset = CalculateSpacingOffset(X, Y, Z, CounterState);
		float OffsetX = ItemSize.X * X + SpacingOffset.X;
		float OffsetY = ItemSize.Y * Y + SpacingOffset.Y;
		
		// Convert rotation to radians for trigonometry
		// Original Smart! formula: 180.0 - Yaw - 90.0 for X, 180.0 - Yaw for Y
		float DegreeX = FMath::DegreesToRadians(180.0f - ParentRotation.Yaw - 90.0f);
		float DegreeY = FMath::DegreesToRadians(180.0f - ParentRotation.Yaw);
		
		float SinX = FMath::Sin(DegreeX);
		float CosX = FMath::Cos(DegreeX);
		float SinY = FMath::Sin(DegreeY);
		float CosY = FMath::Cos(DegreeY);
		
		// Calculate Stagger offset BEFORE rotation (local space)
		FVector StaggerOffset = CalculateStaggerOffset(X, Y, Z, CounterState);
		
		// Add stagger to base offsets (in local space)
		OffsetX += StaggerOffset.X;
		OffsetY += StaggerOffset.Y;
		
		// Apply rotation to offsets (original Smart! formula for "normal" buildings)
		// This now includes stagger offset, rotated relative to hologram orientation
		NewLocation = ParentLocation;
		NewLocation.X = NewLocation.X - OffsetX * SinX + OffsetY * SinY;
		NewLocation.Y = NewLocation.Y - OffsetX * CosX + OffsetY * CosY;
		NewLocation.Z = NewLocation.Z + Z * ItemSize.Z + SpacingOffset.Z;
		
		// Apply Steps progressive VERTICAL offset - creates staircase effect
		float StepsOffset = CalculateStepsOffset(X, Y, CounterState);
		NewLocation.Z += StepsOffset;
	}
	
	// ATTACHMENT TYPE FIX: Apply anchor offset compensation
	// For attachment types (conveyor/pipeline/hypertube), parent pivot is elevated above visual bottom
	// AnchorOffset.Z (typically negative) lowers children to align with parent's visual bottom
	NewLocation += AnchorOffset;
	
	return NewLocation;
}

void FSFPositionCalculator::UpdateChildPositions(
	const TArray<TWeakObjectPtr<AFGHologram>>& Children,
	const FSFCounterState& CounterState,
	const FVector& ParentLocation,
	const FRotator& ParentRotation,
	const FVector& ItemSize
)
{
	// Extracted from SFSubsystem::UpdateChildPositions (Phase 0 - Task #61.6)
	// Core positioning logic - hologram validation/locking handled by HologramHelperService
	
	const FIntVector& GridCounters = CounterState.GridCounters;
	
	// Calculate grid dimensions and directions
	int32 XCount = FMath::Abs(GridCounters.X);
	int32 YCount = FMath::Abs(GridCounters.Y);
	int32 ZCount = FMath::Abs(GridCounters.Z);
	
	int32 XDir = GridCounters.X >= 0 ? 1 : -1;
	int32 YDir = GridCounters.Y >= 0 ? 1 : -1;
	int32 ZDir = GridCounters.Z >= 0 ? 1 : -1;
	
	// Iterate through grid and position each child
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 X = 0; X < XCount; ++X)
		{
			for (int32 Y = 0; Y < YCount; ++Y)
			{
				// Skip [0,0,0] - that's the parent
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue;
				}
				
				if (ChildIndex >= Children.Num())
				{
					break; // More grid positions than children
				}
				
				const TWeakObjectPtr<AFGHologram>& ChildPtr = Children[ChildIndex];
				if (ChildPtr.IsValid())
				{
					AFGHologram* Child = ChildPtr.Get();
					if (IsValid(Child))
					{
						// Calculate position using original Smart! logic
						FVector ChildPosition = CalculateChildPosition(
							X * XDir,
							Y * YDir,
							Z * ZDir,
							ParentLocation,
							ParentRotation,
							ItemSize,
							CounterState,
							ChildIndex
						);
						
						// Calculate child rotation - includes arc rotation if active
						FRotator ChildRotation = ParentRotation;
						if (!FMath::IsNearlyZero(CounterState.RotationZ))
						{
							// ROTATION MODE: Each child rotates progressively along the arc
							// Each building rotates to face tangent to the arc (same direction as arc curves)
							// The rotation matches the arc direction - positive RotationZ = clockwise arc = clockwise building rotation
							float ChildYawOffset = (X * XDir) * CounterState.RotationZ;
							ChildRotation.Yaw += ChildYawOffset;
							
							UE_LOG(LogSmartFoundations, Verbose, 
								TEXT("🔄 Child[%d] X=%d: ParentYaw=%.1f° + Offset=%.1f° = FinalYaw=%.1f°"),
								ChildIndex, X * XDir, ParentRotation.Yaw, ChildYawOffset, ChildRotation.Yaw);
						}
						
						// Set child transform
						Child->SetActorLocation(ChildPosition);
						Child->SetActorRotation(ChildRotation);
					}
				}
				
				ChildIndex++;
			}
		}
	}
}

bool FSFPositionCalculator::UpdateChildrenForCurrentTransform(
	const TArray<TWeakObjectPtr<AFGHologram>>& Children,
	const FSFCounterState& CounterState,
	const FTransform& CurrentTransform,
	FTransform& LastKnownTransform,
	const FVector& ItemSize
)
{
	// TODO: Extract from SFSubsystem::UpdateChildrenForCurrentTransform
	
	// Only update if transform changed
	if (CurrentTransform.Equals(LastKnownTransform))
	{
		return false;
	}
	
	UpdateChildPositions(
		Children,
		CounterState,
		CurrentTransform.GetLocation(),
		CurrentTransform.GetRotation().Rotator(),
		ItemSize
	);
	
	LastKnownTransform = CurrentTransform;
	return true;
}

FVector FSFPositionCalculator::CalculateSpacingOffset(
	int32 X, int32 Y, int32 Z,
	const FSFCounterState& CounterState
) const
{
	// Original Smart! spacing logic - applies to grid offsets in local space
	// Spacing is added per grid position, accumulating as you go further from origin
	// This is already integrated into base offsets in CalculateChildPosition()
	
	FVector SpacingOffset = FVector::ZeroVector;
	
	// Apply spacing counters (multiplied by grid index for accumulation)
	SpacingOffset.X = CounterState.SpacingX * X;
	SpacingOffset.Y = CounterState.SpacingY * Y;
	SpacingOffset.Z = CounterState.SpacingZ * Z;
	
	return SpacingOffset;
}

float FSFPositionCalculator::CalculateStepsOffset(
	int32 X, int32 Y,
	const FSFCounterState& CounterState
) const
{
	// Original Smart! Steps logic - progressive VERTICAL offset creates staircase effect
	// Building-specific layer stacking (NOT arbitrary vertical offset/Levitation)
	// Both X and Y steps can be active simultaneously
	
	float TotalStepsOffset = 0.0f;
	
	if (CounterState.StepsX != 0)
	{
		// X-axis Steps: Columns (constant X) rise/fall based on X position
		// Sign-aware: positive X ascends, negative X descends (for negative scale)
		TotalStepsOffset += CounterState.StepsX * X;
	}
	
	if (CounterState.StepsY != 0)
	{
		// Y-axis Steps: Rows (constant Y) rise/fall based on Y position
		// Sign-aware: positive Y ascends, negative Y descends (for negative scale)
		TotalStepsOffset += CounterState.StepsY * Y;
	}
	
	return TotalStepsOffset;
}

FVector FSFPositionCalculator::CalculateStaggerOffset(
	int32 X, int32 Y, int32 Z,
	const FSFCounterState& CounterState
) const
{
	// Stagger - progressive perpendicular offset creates diagonal/lean patterns
	// X-axis stagger: offsets Y position progressively based on X grid position
	// Y-axis stagger: offsets X position progressively based on Y grid position
	// ZX-axis stagger: offsets X position progressively based on Z grid position (forward/back lean)
	// ZY-axis stagger: offsets Y position progressively based on Z grid position (sideways lean)
	// Direct 1:1 ratio - 50cm counter = 50cm progressive offset per grid step
	// All stagger axes can be active simultaneously (creates compound patterns)
	
	FVector StaggerOffset = FVector::ZeroVector;
	
	if (CounterState.StaggerX != 0)
	{
		// X-axis Stagger: Progressive sideways offset on Y axis
		// For a line of foundations along X, each position gets progressively offset in Y
		// Uses SIGNED X (not Abs) - allows offset in both directions
		// Direct 1:1 ratio - 50cm counter = 50cm progressive offset per grid position
		// Example: X=0→Y=0cm, X=1→Y=50cm, X=2→Y=100cm (diagonal staircase)
		StaggerOffset.Y += CounterState.StaggerX * X;
	}
	
	if (CounterState.StaggerY != 0)
	{
		// Y-axis Stagger: Progressive forward/back offset on X axis
		// For a line of foundations along Y, each position gets progressively offset in X
		// Uses SIGNED Y (not Abs) - allows offset in both directions
		// Direct 1:1 ratio - 50cm counter = 50cm progressive offset per grid position
		// Example: Y=0→X=0cm, Y=1→X=50cm, Y=2→X=100cm (diagonal staircase)
		StaggerOffset.X += CounterState.StaggerY * Y;
	}
	
	if (CounterState.StaggerZX != 0)
	{
		// ZX-axis Stagger: Progressive X-direction lean based on vertical position
		// For vertical structures (multi-layer grids), each Z layer leans in X direction
		// Uses SIGNED Z (not Abs) - allows lean in both directions
		// Direct 1:1 ratio - 50cm counter = 50cm progressive X offset per vertical layer
		// Example: Z=0→X=0cm, Z=1→X=50cm, Z=2→X=100cm (forward/back lean)
		StaggerOffset.X += CounterState.StaggerZX * Z;
	}
	
	if (CounterState.StaggerZY != 0)
	{
		// ZY-axis Stagger: Progressive Y-direction lean based on vertical position
		// For vertical structures (multi-layer grids), each Z layer leans in Y direction
		// Uses SIGNED Z (not Abs) - allows lean in both directions
		// Direct 1:1 ratio - 50cm counter = 50cm progressive Y offset per vertical layer
		// Example: Z=0→Y=0cm, Z=1→Y=50cm, Z=2→Y=100cm (sideways lean)
		StaggerOffset.Y += CounterState.StaggerZY * Z;
	}
	
	// Note: Both StaggerZX and StaggerZY can be active simultaneously
	// This creates diagonal/compound lean angles for complex creative builds
	
	return StaggerOffset;
}

FVector FSFPositionCalculator::CalculateRotationOffset(
	int32 X, int32 Y, int32 Z,
	const FSFCounterState& CounterState,
	const FVector& ItemSize,
	FRotator& OutRotation
) const
{
	// Only apply rotation when non-zero
	if (FMath::IsNearlyZero(CounterState.RotationZ))
	{
		OutRotation = FRotator::ZeroRotator;
		return FVector::ZeroVector;
	}
	
	// DESIGN DECISION: Positive RotationZ = Clockwise (user expectation)
	// Internally we use the signed user step directly:
	// - The SIGN of RotationZ controls whether the arc curves LEFT or RIGHT
	// - Forward movement should always extend AWAY from the parent
	const float RotationStepDeg = CounterState.RotationZ;
	
	// Arc length per child along the curve (cumulative building width + spacing)
	const float ArcLength = ItemSize.X + static_cast<float>(CounterState.SpacingX);
	
	// Magnitude of per-step angle in radians (always positive for radius math)
	const float StepRadians = FMath::Abs(FMath::DegreesToRadians(RotationStepDeg));
	const float BaseRadius = (StepRadians > KINDA_SMALL_NUMBER)
		? ArcLength / StepRadians
		: 0.0f;
	
	// Multi-row behavior: parallel curved lanes (like a curved road)
	// Y rows maintain their relative positions (Y=1 always to the right of Y=0)
	// For +rotation (right curve): Y=1 is on the INSIDE (smaller radius)
	// For -rotation (left curve): Y=1 is on the OUTSIDE (larger radius)
	const float RowGap = ItemSize.Y + static_cast<float>(CounterState.SpacingY);
	const float SignRotation = (RotationStepDeg >= 0.0f) ? 1.0f : -1.0f;
	const float Radius = BaseRadius - SignRotation * Y * RowGap;
	
	// Angle for this X position (signed). We use the user step directly so that
	// changing the sign mirrors the arc left/right while preserving forward motion.
	const float AngleDegrees = static_cast<float>(X) * RotationStepDeg;
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	
	// DEBUG: Log core geometry values for troubleshooting
	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("RotationOffset: X=%d Y=%d, RotationZ=%.1f°, ArcLength=%.0fcm, BaseRadius=%.0fcm, Radius=%.0fcm, Angle=%.1f°"),
		X, Y, RotationStepDeg, ArcLength, BaseRadius, Radius, AngleDegrees);
	
	// Arc parametrization for PARALLEL CURVED LANES (like a curved road):
	// - X_local = forward (along grid X axis, away from parent)
	// - Y_local = right (along grid Y axis)
	//
	// For an arc that CONTINUES FORWARD while curving left/right:
	//   Forward  = R * sin(|θ|)
	//   Sideways = BaseRadius - R * cos(|θ|)  (with sign for direction)
	//
	// Combined with the radius adjustment above, this ensures:
	// - At θ=0: Row 0 is at origin, Row 1 is offset by +RowGap (to the RIGHT)
	// - X > 0: Arc goes forward, X < 0: Arc goes backward
	// - Rotation > 0: Curves right, Rotation < 0: Curves left
	// - Y rows maintain their relative positions regardless of X or Rotation sign
	
	// IMPORTANT: Separate the signs to handle all 4 combinations correctly:
	// - sign(X) determines forward vs backward
	// - sign(Rotation) determines right vs left curve
	// - magnitude uses |X * Rotation| for the arc geometry
	const float SignX = (X >= 0) ? 1.0f : -1.0f;
	const float AbsAngleRadians = FMath::Abs(AngleRadians);
	
	const float SinTheta = FMath::Sin(AbsAngleRadians);
	const float CosTheta = FMath::Cos(AbsAngleRadians);
	
	FVector ArcOffset;
	// Forward: SignX determines direction (positive X = forward, negative X = backward)
	ArcOffset.X = SignX * Radius * SinTheta;
	// Sideways: SignRotation determines curve direction (positive = right, negative = left)
	ArcOffset.Y = SignRotation * (BaseRadius - Radius * CosTheta);
	ArcOffset.Z = 0.0f;  // Z handled by Steps/SpacingZ
	
	// Set rotation for this building so it faces tangent to the arc.
	OutRotation = FRotator(0.0f, AngleDegrees, 0.0f);
	
	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("  -> Radius=%.0fcm, ArcOffset=(%.0f, %.0f, %.0f), Rotation=%.1f°"),
		Radius, ArcOffset.X, ArcOffset.Y, ArcOffset.Z, AngleDegrees);
	
	return ArcOffset;
}

// ========================================
// Native Nudge Coordination (PRD Requirement)
// ========================================

bool FSFPositionCalculator::IsNativeVerticalNudgeActive(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return false;
	}

	// Query native nudge system via Satisfactory API
	const FVector NudgeOffset = Hologram->GetHologramNudgeOffset();
	const float VerticalNudge = NudgeOffset.Z;

	// Use 0.1f threshold for floating point comparison (PRD specification)
	return FMath::Abs(VerticalNudge) > 0.1f;
}

bool FSFPositionCalculator::IsNativeHorizontalNudgeActive(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return false;
	}

	// Query native nudge system via Satisfactory API
	const FVector NudgeOffset = Hologram->GetHologramNudgeOffset();
	const FVector2D HorizontalNudge(NudgeOffset.X, NudgeOffset.Y);

	// Use 0.1f threshold for floating point comparison (PRD specification)
	return HorizontalNudge.Size() > 0.1f;
}

FVector FSFPositionCalculator::GetNativeNudgeOffset(AFGHologram* Hologram) const
{
	if (!Hologram || !IsValid(Hologram))
	{
		return FVector::ZeroVector;
	}

	// Query native nudge system via Satisfactory API
	// NOTE: Smart! offsets are automatically applied RELATIVE to this offset
	// because CalculateChildPosition() receives ParentLocation which already
	// includes the nudge (GetActorLocation() returns base + nudge)
	return Hologram->GetHologramNudgeOffset();
}
