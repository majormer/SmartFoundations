// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Pillar Hologram Adapter Implementation

#include "SFPillarAdapter.h"
#include "SmartFoundations.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFPillarAdapter::FSFPillarAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFPillarAdapter::GetBuildingBounds() const
{
	// Bases and bodies have different native clearance dimensions. Known profiles
	// preserve those snapping dimensions; modded pillars use the registry fallback.
	FVector PillarSize = USFBuildableSizeRegistry::GetDefaultSize();
	if (AFGHologram* Hologram = HologramPtr.Get())
	{
		FString SizeSource;
		USFBuildableSizeRegistry::GetSizeWithFallback(Hologram->GetBuildClass(), PillarSize, SizeSource);
	}
	
	FBox BoundingBox(
		-PillarSize * 0.5f,
		PillarSize * 0.5f
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFPillarAdapter::SupportsFeature(ESFFeature Feature) const
{
	AFGHologram* Hologram = HologramPtr.Get();
	if (!Hologram || !Hologram->GetBuildClass()) return false;
	const bool bSupportsScaling = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass()).bSupportsScaling;
	switch (Feature)
	{
	case ESFFeature::ScaleX:
	case ESFFeature::ScaleY:
	case ESFFeature::ScaleZ:
	case ESFFeature::Arrows:
	case ESFFeature::Spacing:
		return bSupportsScaling;
	default:
		return false;
	}
}

FString FSFPillarAdapter::GetAdapterTypeName() const
{
	return TEXT("Pillar");
}
