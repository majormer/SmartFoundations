// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Spacing Module Implementation

#include "Features/Spacing/SFSpacingModule.h"
#include "SmartFoundations.h"

FVector FSFSpacingModule::CalculateAutoGap(
	ESFSpacingMode Mode,
	const FVector& HologramSize,
	float DefaultGap)
{
	FVector Gap = FVector::ZeroVector;

	switch (Mode)
	{
	case ESFSpacingMode::None:
		// No spacing
		break;

	case ESFSpacingMode::X:
		// Gap on X-axis only
		Gap.X = DefaultGap;
		break;

	case ESFSpacingMode::XY:
		// Gap on X and Y axes
		Gap.X = DefaultGap;
		Gap.Y = DefaultGap;
		break;

	case ESFSpacingMode::XYZ:
		// Gap on all axes
		Gap.X = DefaultGap;
		Gap.Y = DefaultGap;
		Gap.Z = DefaultGap;
		break;
	}

	return Gap;
}

FVector FSFSpacingModule::GetNextPlacementOffset(
	ESFSpacingMode Mode,
	const FVector& CurrentOffset,
	const FVector& HologramSize,
	float DefaultGap)
{
	// Calculate gap for this mode
	FVector Gap = CalculateAutoGap(Mode, HologramSize, DefaultGap);

	// Next offset = current position + hologram size + gap
	FVector NextOffset = CurrentOffset;

	if (Mode != ESFSpacingMode::None)
	{
		// Apply spacing on active axes
		// For each axis: move by (hologram size + gap)
		if (Gap.X > 0.0f)
		{
			NextOffset.X += HologramSize.X + Gap.X;
		}

		if (Gap.Y > 0.0f)
		{
			NextOffset.Y += HologramSize.Y + Gap.Y;
		}

		if (Gap.Z > 0.0f)
		{
			NextOffset.Z += HologramSize.Z + Gap.Z;
		}
	}

	return NextOffset;
}

ESFSpacingMode FSFSpacingModule::CycleSpacingMode(ESFSpacingMode CurrentMode)
{
	// Cycle: None → X → XY → XYZ → None
	switch (CurrentMode)
	{
	case ESFSpacingMode::None:
		return ESFSpacingMode::X;

	case ESFSpacingMode::X:
		return ESFSpacingMode::XY;

	case ESFSpacingMode::XY:
		return ESFSpacingMode::XYZ;

	case ESFSpacingMode::XYZ:
		return ESFSpacingMode::None;

	default:
		return ESFSpacingMode::None;
	}
}

FString FSFSpacingModule::GetSpacingModeName(ESFSpacingMode Mode)
{
	switch (Mode)
	{
	case ESFSpacingMode::None:
		return TEXT("No Spacing");

	case ESFSpacingMode::X:
		return TEXT("X-Axis Spacing");

	case ESFSpacingMode::XY:
		return TEXT("X+Y Spacing");

	case ESFSpacingMode::XYZ:
		return TEXT("X+Y+Z Spacing");

	default:
		return TEXT("Unknown");
	}
}

bool FSFSpacingModule::IsSpacingActive(ESFSpacingMode Mode)
{
	return Mode != ESFSpacingMode::None;
}
