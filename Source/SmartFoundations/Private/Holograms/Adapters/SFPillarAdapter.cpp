// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Pillar Hologram Adapter Implementation

#include "SFPillarAdapter.h"
#include "SmartFoundations.h"

FSFPillarAdapter::FSFPillarAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFPillarAdapter::GetBuildingBounds() const
{
	// Standard pillar: 2m x 2m x 4m tall
	// (200cm x 200cm x 400cm)
	FVector PillarSize(200.0f, 200.0f, 400.0f);
	
	FBox BoundingBox(
		-PillarSize * 0.5f,
		PillarSize * 0.5f
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFPillarAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Pillars support all features
	switch (Feature)
	{
	case ESFFeature::ScaleX:
	case ESFFeature::ScaleY:
	case ESFFeature::ScaleZ:
	case ESFFeature::Arrows:
	case ESFFeature::Spacing:
		return true;
	default:
		return false;
	}
}

FString FSFPillarAdapter::GetAdapterTypeName() const
{
	return TEXT("Pillar");
}
