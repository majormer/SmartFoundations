// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Scaling Module Implementation

#include "Features/Scaling/SFScalingModule.h"
#include "SmartFoundations.h"

FVector FSFScalingModule::CalculateOffset(
	const FVector& CurrentOffset,
	ESFScaleAxis Axis,
	int32 Steps,
	float StepSize)
{
	// Validate step size
	if (StepSize <= 0.0f)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFScalingModule::CalculateOffset - Invalid StepSize: %f (must be > 0)"), StepSize);
		return CurrentOffset;
	}

	// Calculate the delta for this axis
	float Delta = Steps * StepSize;

	// Apply to the appropriate axis
	FVector NewOffset = CurrentOffset;
	switch (Axis)
	{
	case ESFScaleAxis::X:
		NewOffset.X += Delta;
		break;
	case ESFScaleAxis::Y:
		NewOffset.Y += Delta;
		break;
	case ESFScaleAxis::Z:
		NewOffset.Z += Delta;
		break;
	}

	return NewOffset;
}

float FSFScalingModule::CalculateAxisOffset(
	float Current,
	int32 Steps,
	float StepSize)
{
	if (StepSize <= 0.0f)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFScalingModule::CalculateAxisOffset - Invalid StepSize: %f"), StepSize);
		return Current;
	}

	return Current + (Steps * StepSize);
}

FVector FSFScalingModule::ApplySnapping(
	const FVector& Offset,
	float SnapSize)
{
	// Validate snap size
	if (SnapSize <= 0.0f)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFScalingModule::ApplySnapping - Invalid SnapSize: %f (must be > 0), returning original offset"), SnapSize);
		return Offset;
	}

	// Snap each component independently
	return FVector(
		SnapValue(Offset.X, SnapSize),
		SnapValue(Offset.Y, SnapSize),
		SnapValue(Offset.Z, SnapSize)
	);
}

float FSFScalingModule::SnapValue(
	float Value,
	float SnapSize)
{
	if (SnapSize <= 0.0f)
	{
		return Value;
	}

	// Round to nearest multiple of SnapSize
	return FMath::RoundToFloat(Value / SnapSize) * SnapSize;
}

bool FSFScalingModule::ValidateOffset(
	const FVector& Offset,
	const FSFScaleBounds& Bounds)
{
	// Check numeric safety first
	if (!IsWithinSafeRange(Offset))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFScalingModule::ValidateOffset - Offset not within safe range: %s"), *Offset.ToString());
		return false;
	}

	// Use bounds' Contains method
	return Bounds.Contains(Offset);
}

FVector FSFScalingModule::ClampOffset(
	const FVector& Offset,
	const FSFScaleBounds& Bounds)
{
	return FVector(
		FMath::Clamp(Offset.X, Bounds.Min.X, Bounds.Max.X),
		FMath::Clamp(Offset.Y, Bounds.Min.Y, Bounds.Max.Y),
		FMath::Clamp(Offset.Z, Bounds.Min.Z, Bounds.Max.Z)
	);
}

bool FSFScalingModule::IsWithinSafeRange(
	const FVector& Offset,
	float MaxMagnitude)
{
	// Check for NaN or Inf
	if (!Offset.ContainsNaN() && FMath::IsFinite(Offset.X) && FMath::IsFinite(Offset.Y) && FMath::IsFinite(Offset.Z))
	{
		// Check magnitude
		float MagnitudeSq = Offset.SizeSquared();
		float MaxMagnitudeSq = MaxMagnitude * MaxMagnitude;
		
		return MagnitudeSq <= MaxMagnitudeSq;
	}

	return false;
}

FVector FSFScalingModule::GetAxisVector(ESFScaleAxis Axis)
{
	switch (Axis)
	{
	case ESFScaleAxis::X:
		return FVector::ForwardVector; // (1, 0, 0)
	case ESFScaleAxis::Y:
		return FVector::RightVector;   // (0, 1, 0)
	case ESFScaleAxis::Z:
		return FVector::UpVector;      // (0, 0, 1)
	default:
		return FVector::ZeroVector;
	}
}

int32 FSFScalingModule::GetAxisIndex(ESFScaleAxis Axis)
{
	switch (Axis)
	{
	case ESFScaleAxis::X:
		return 0;
	case ESFScaleAxis::Y:
		return 1;
	case ESFScaleAxis::Z:
		return 2;
	default:
		return 0;
	}
}
