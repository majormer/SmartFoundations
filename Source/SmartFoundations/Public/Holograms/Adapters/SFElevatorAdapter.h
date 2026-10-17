// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

// Smart! Mod - Elevator Hologram Adapter
#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"
#include "UObject/UnrealType.h"

/**
 * FSFElevatorAdapter - Adapter for personal elevator holograms
 * Complete new shafts use selected assembly proxies; native shaft extension remains distinct.
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
		// The selected shaft/stop footprint is refreshed by the scaling service.
		// Adapter bounds are local; world coordinates must not become grid dimensions.
		return FBoxSphereBounds(FBox(FVector(-400, -400, -200), FVector(400, 400, 200)));
	}

	virtual bool SupportsFeature(ESFFeature Feature) const override
	{
		if (!HologramPtr.IsValid()) return false;
		const FObjectPropertyBase* Snapped = HologramPtr->GetClass()->FindPropertyByName(TEXT("mSnappedToElevator"))
			? FindFProperty<FObjectPropertyBase>(HologramPtr->GetClass(), TEXT("mSnappedToElevator")) : nullptr;
		if (!Snapped || Snapped->GetObjectPropertyValue_InContainer(HologramPtr.Get())) return false;
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

	virtual FString GetAdapterTypeName() const override
	{
		return TEXT("Elevator");
	}
};
