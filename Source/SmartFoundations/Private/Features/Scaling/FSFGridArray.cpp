#include "Features/Scaling/FSFGridArray.h"
#include "SmartFoundations.h"

// Define FVector3i constants
const FVector3i FVector3i::ZeroVector = FVector3i(0, 0, 0);
const FVector3i FVector3i::OneVector = FVector3i(1, 1, 1);

namespace FSFGridArray
{

// ========================================
// Counter Logic (Zero-Skip Semantics)
// ========================================

void IncrementCounter(int32& Counter)
{
	// Zero-skip: -1 → 1 (skip 0 from negative side)
	if (Counter == -1)
	{
		Counter = 1;
	}
	else
	{
		Counter++;
	}
}

void DecrementCounter(int32& Counter)
{
	// Zero-skip: 1 → -1 (skip 0 to negative side)
	if (Counter == 1)
	{
		Counter = -1;
	}
	else
	{
		Counter--;
	}
}

int32 GetDirection(int32 Counter)
{
	// Positive or zero = +1 direction
	// Negative = -1 direction
	return Counter >= 0 ? 1 : -1;
}

int32 GetAbsoluteCount(int32 Counter)
{
	return FMath::Abs(Counter);
}

// ========================================
// Grid Generation
// ========================================

TArray<FTransform> CalculateGridPositions(
	const FVector3i& Counters,
	const FVector& HologramSize,
	const FVector& Spacing,
	const FTransform& ParentTransform
)
{
	// Calculate total items
	const int32 TotalItems = CalculateTotalItems(Counters);
	
	// Early out for empty grids
	if (TotalItems == 0)
	{
		return TArray<FTransform>();
	}

	// Allocate result array
	TArray<FTransform> Transforms;
	Transforms.Reserve(TotalItems);

	// Get absolute counts and directions per axis
	const FVector3i AbsCounts = Counters.Abs();
	const FVector3i Directions(
		GetDirection(Counters.X),
		GetDirection(Counters.Y),
		GetDirection(Counters.Z)
	);

	// Get parent rotation for offset rotation
	const FRotator ParentRotation = ParentTransform.Rotator();

	// Loop order: Z, X, Y (documented requirement for child index mapping)
	for (int32 Z = 0; Z < AbsCounts.Z; ++Z)
	{
		for (int32 X = 0; X < AbsCounts.X; ++X)
		{
			for (int32 Y = 0; Y < AbsCounts.Y; ++Y)
			{
				// Calculate local offset (before rotation)
				const FVector LocalOffset = CalculateChildOffset(
					X, Y, Z,
					HologramSize,
					Spacing,
					Directions
				);

				// Rotate offset by hologram rotation
				const FVector RotatedOffset = ApplyHologramRotation(LocalOffset, ParentRotation);

				// Transform to world space
				FTransform ChildTransform = ParentTransform;
				ChildTransform.AddToTranslation(RotatedOffset);

				Transforms.Add(ChildTransform);
			}
		}
	}

	// Verify we generated the expected number of transforms
	check(Transforms.Num() == TotalItems);

	return Transforms;
}

// ========================================
// Helper Utilities
// ========================================

int32 CalculateTotalItems(const FVector3i& Counters)
{
	// Get absolute counts
	const int32 AbsX = FMath::Abs(Counters.X);
	const int32 AbsY = FMath::Abs(Counters.Y);
	const int32 AbsZ = FMath::Abs(Counters.Z);

	// Early out for zero counts
	if (AbsX == 0 || AbsY == 0 || AbsZ == 0)
	{
		return 0;
	}

	// Check for overflow potential
	// INT32_MAX is ~2.1 billion, so we check if intermediate products would overflow
	
	// First multiplication: X * Y
	if (AbsX > 0 && AbsY > INT32_MAX / AbsX)
	{
		UE_LOG(LogSmartFoundations, Warning, 
			TEXT("[FSFGridArray] CalculateTotalItems: Overflow detected (X*Y), clamping to INT32_MAX"));
		return INT32_MAX;
	}
	const int32 XY = AbsX * AbsY;

	// Second multiplication: (X * Y) * Z
	if (XY > 0 && AbsZ > INT32_MAX / XY)
	{
		UE_LOG(LogSmartFoundations, Warning, 
			TEXT("[FSFGridArray] CalculateTotalItems: Overflow detected (XY*Z), clamping to INT32_MAX"));
		return INT32_MAX;
	}

	return XY * AbsZ;
}

FVector CalculateChildOffset(
	int32 XIndex,
	int32 YIndex,
	int32 ZIndex,
	const FVector& Size,
	const FVector& Spacing,
	const FVector3i& Directions
)
{
	// Calculate offset per axis: Index * (Size + Spacing) * Direction
	const float OffsetX = XIndex * (Size.X + Spacing.X) * Directions.X;
	const float OffsetY = YIndex * (Size.Y + Spacing.Y) * Directions.Y;
	const float OffsetZ = ZIndex * (Size.Z + Spacing.Z) * Directions.Z;

	return FVector(OffsetX, OffsetY, OffsetZ);
}

FVector ApplyHologramRotation(
	const FVector& LocalOffset,
	const FRotator& HologramRotation
)
{
	// Use Unreal's standard rotation: Rotator.RotateVector()
	const FVector RotatedOffset = HologramRotation.RotateVector(LocalOffset);

	// Verify no NaNs/Infs (safety check for finite inputs)
	checkSlow(RotatedOffset.ContainsNaN() == false);

	return RotatedOffset;
}

} // namespace FSFGridArray
