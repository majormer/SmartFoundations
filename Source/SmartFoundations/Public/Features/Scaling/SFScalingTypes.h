// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Scaling Feature Types

#pragma once

#include "CoreMinimal.h"
#include "SFScalingTypes.generated.h"

/**
 * Axis enumeration for scaling operations and stagger directions
 * Represents the primary axes and compound Z-based axes for stagger shift
 */
UENUM(BlueprintType)
enum class ESFScaleAxis : uint8
{
	X UMETA(DisplayName = "X Axis (Forward/Back)"),
	Y UMETA(DisplayName = "Y Axis (Left/Right)"),
	Z UMETA(DisplayName = "Z Axis (Up/Down)"),
	ZX UMETA(DisplayName = "ZX Axis (Vertical Shift Forward/Back)"),
	ZY UMETA(DisplayName = "ZY Axis (Vertical Shift Left/Right)")
};

/**
 * Bounds representation for validation
 * Defines an axis-aligned bounding box (AABB) for placement validation
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFScaleBounds
{
	GENERATED_BODY()

	/** Minimum point of the bounding box */
	UPROPERTY(BlueprintReadWrite, Category = "Smart! Scaling")
	FVector Min;

	/** Maximum point of the bounding box */
	UPROPERTY(BlueprintReadWrite, Category = "Smart! Scaling")
	FVector Max;

	FSFScaleBounds()
		: Min(FVector(-10000.0f, -10000.0f, -10000.0f))
		, Max(FVector(10000.0f, 10000.0f, 10000.0f))
	{}

	FSFScaleBounds(const FVector& InMin, const FVector& InMax)
		: Min(InMin)
		, Max(InMax)
	{}

	/** Check if a point is within this bounding box (inclusive) */
	bool Contains(const FVector& Point) const
	{
		return Point.X >= Min.X && Point.X <= Max.X &&
		       Point.Y >= Min.Y && Point.Y <= Max.Y &&
		       Point.Z >= Min.Z && Point.Z <= Max.Z;
	}

	/** Get the size of the bounding box */
	FVector GetSize() const
	{
		return Max - Min;
	}

	/** Get the center of the bounding box */
	FVector GetCenter() const
	{
		return (Min + Max) * 0.5f;
	}
};
