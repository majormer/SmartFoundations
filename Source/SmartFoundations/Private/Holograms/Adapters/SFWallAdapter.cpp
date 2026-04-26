// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Wall Hologram Adapter Implementation

#include "SFWallAdapter.h"
#include "SmartFoundations.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFWallAdapter::FSFWallAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFWallAdapter::GetBuildingBounds() const
{
	FVector WallSize = CalculateWallSizeForGrid();

	FBox BoundingBox(
		-WallSize * 0.5f,
		WallSize * 0.5f
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFWallAdapter::SupportsFeature(ESFFeature Feature) const
{
	switch (Feature)
	{
	case ESFFeature::ScaleX:
	case ESFFeature::ScaleY:
		return true; // Walls support X and Y scaling

	case ESFFeature::ScaleZ:
		return false; // Walls typically don't support Z-axis scaling

	case ESFFeature::Arrows:
	case ESFFeature::Spacing:
		return true;

	default:
		return false;
	}
}

FString FSFWallAdapter::GetAdapterTypeName() const
{
	return TEXT("Wall");
}

FVector FSFWallAdapter::CalculateWallSizeForGrid() const
{
	if (!HologramPtr.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFWallAdapter::CalculateWallSizeForGrid - Invalid hologram"));
		// Default wall: 8m wide, 4m tall, use width for Y-axis spacing
		return FVector(800.0f, 800.0f, 400.0f);
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFWallAdapter: No BuildClass"));
		return FVector(800.0f, 800.0f, 400.0f);
	}

	// Get physical dimensions from registry or CDO queries
	FVector PhysicalSize;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, PhysicalSize, Source);

	// For walls: X=width, Y=thickness (tiny!), Z=height
	// Grid spacing problem: Y-axis scaling uses thickness instead of width
	// Solution: Return Y=X so Y-axis spacing uses wall width
	FVector GridSize(
		PhysicalSize.X,  // X: Use actual width for X-axis spacing
		PhysicalSize.X,  // Y: Use WIDTH (not thickness) for Y-axis spacing ← KEY FIX
		PhysicalSize.Z   // Z: Use actual height for Z-axis spacing
	);

	UE_LOG(LogSmartFoundations, Log, TEXT("FSFWallAdapter: Physical=%s Grid=%s Source=%s Class=%s"),
		*PhysicalSize.ToString(), *GridSize.ToString(), *Source, *BuildClass->GetName());

	return GridSize;
}
