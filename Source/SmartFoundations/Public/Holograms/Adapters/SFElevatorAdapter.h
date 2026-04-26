// Smart! Mod - Elevator Hologram Adapter
#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFElevatorAdapter - Adapter for personal elevator holograms
 * Note: Elevator placement is variable-height, multi-step (click-and-drag).
 * For now, scaling features are disabled to avoid conflicts with native behavior.
 */
class SMARTFOUNDATIONS_API FSFElevatorAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFElevatorAdapter(AFGHologram* InHologram)
		: FSFHologramAdapterBase(InHologram)
	{
	}

	virtual ~FSFElevatorAdapter() = default;

	// ISFHologramAdapter
	virtual FBoxSphereBounds GetBuildingBounds() const override
	{
		if (!HologramPtr.IsValid())
		{
			return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
		}
		const FBox WorldBox = HologramPtr->GetComponentsBoundingBox(/*bNonColliding=*/true);
		if (WorldBox.IsValid)
		{
			return FBoxSphereBounds(WorldBox);
		}
		// Conservative fallback box
		return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
	}

	virtual bool SupportsFeature(ESFFeature Feature) const override
	{
		// Elevators use native variable-height placement; disable Smart! scaling for now
		switch (Feature)
		{
		case ESFFeature::ScaleX:
		case ESFFeature::ScaleY:
		case ESFFeature::ScaleZ:
			return false;
		case ESFFeature::Arrows:
		case ESFFeature::Spacing:
			return false;
		default:
			return false;
		}
	}

	virtual FString GetAdapterTypeName() const override
	{
		return TEXT("Elevator");
	}
};
