// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Unsupported Hologram Adapter (Defensive Stub)

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFUnsupportedAdapter - Defensive adapter for hologram types that don't support Smart! features
 * 
 * Used for:
 * - Vehicles (FGWheeledVehicleHologram, FGDroneHologram, etc.)
 * - Trains (FGTrainHologram)
 * - Special buildings (FGSpaceElevatorHologram)
 * - Any unknown/future hologram types
 * 
 * All Smart! features are disabled to prevent broken behavior like:
 * - Grid spawning for non-buildable items
 * - Cost calculation bugs
 * - Construction failures
 * 
 * The subsystem still logs recipe information for these holograms,
 * allowing us to collect data for future implementation.
 */
class SMARTFOUNDATIONS_API FSFUnsupportedAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFUnsupportedAdapter(AFGHologram* InHologram, const FString& InTypeName)
		: FSFHologramAdapterBase(InHologram)
		, TypeName(InTypeName)
	{
		// Recipe/hologram info already logged by subsystem
		// This adapter just disables features
	}

	virtual ~FSFUnsupportedAdapter() = default;

	// ISFHologramAdapter interface
	virtual FBoxSphereBounds GetBuildingBounds() const override
	{
		if (!HologramPtr.IsValid())
		{
			return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
		}
		
		// Use actual hologram bounds (subsystem already logged this)
		const FBox WorldBox = HologramPtr->GetComponentsBoundingBox(/*bNonColliding=*/true);
		if (WorldBox.IsValid)
		{
			return FBoxSphereBounds(WorldBox);
		}
		
		// Fallback
		return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
	}

	virtual bool SupportsFeature(ESFFeature Feature) const override
	{
		// Explicitly disable ALL Smart! features for unsupported types
		return false;
	}

	virtual FString GetAdapterTypeName() const override
	{
		return FString::Printf(TEXT("Unsupported(%s)"), *TypeName);
	}

private:
	FString TypeName;
};
