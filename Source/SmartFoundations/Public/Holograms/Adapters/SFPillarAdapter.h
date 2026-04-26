// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Pillar Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFPillarAdapter - Adapter for pillar holograms
 * 
 * Pillars are vertical supports:
 * - Support all scaling axes
 * - Tall, thin profile
 */
class SMARTFOUNDATIONS_API FSFPillarAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFPillarAdapter(AFGHologram* InHologram);
	virtual ~FSFPillarAdapter() = default;

	// ISFHologramAdapter interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;
};
