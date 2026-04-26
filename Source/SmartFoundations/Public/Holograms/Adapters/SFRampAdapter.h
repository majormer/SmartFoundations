// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Ramp/Stair Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"
#include "Data/SFBuildableSizeRegistry.h"

/**
 * FSFRampAdapter - Adapter for ramp and stair holograms
 * Maps to AFGStairHologram
 *
 * TODO: Implement full ramp-specific sizing and feature support
 * Currently stubbed to disable features until properly implemented
 */
class SMARTFOUNDATIONS_API FSFRampAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFRampAdapter(AFGHologram* InHologram)
		: FSFHologramAdapterBase(InHologram)
	{
	}

	virtual ~FSFRampAdapter() = default;

	virtual FBoxSphereBounds GetBuildingBounds() const override
	{
		if (!HologramPtr.IsValid())
		{
			return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
		}

		// Get actual bounds from hologram
		const FBox WorldBox = HologramPtr->GetComponentsBoundingBox(/*bNonColliding=*/true);
		if (WorldBox.IsValid)
		{
			return FBoxSphereBounds(WorldBox);
		}

		// Use registry for ramp sizes, fallback to standard 8m x 8m x 2m rise
		FVector RampSize(800.0f, 800.0f, 200.0f);
		return FBoxSphereBounds(FBox(-RampSize * 0.5f, RampSize * 0.5f));
	}

	virtual bool SupportsFeature(ESFFeature Feature) const override
	{
		// TODO: Enable features once ramp-specific logic is implemented
		// Ramps have complex geometry (angled) that needs special handling
		switch (Feature)
		{
		case ESFFeature::ScaleX:
		case ESFFeature::ScaleY:
			return false; // Disabled until proper implementation
		case ESFFeature::ScaleZ:
			return false; // Z-scaling affects angle, needs special math
		case ESFFeature::Arrows:
		case ESFFeature::Spacing:
			return false; // Disabled for now
		default:
			return false;
		}
	}

	virtual FString GetAdapterTypeName() const override
	{
		return TEXT("Ramp");
	}
};
