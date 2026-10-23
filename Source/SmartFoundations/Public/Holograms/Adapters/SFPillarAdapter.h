// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Pillar Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFPillarAdapter - Adapter for pillar holograms
 * 
 * Pillar bases and bodies use their registered native clearance dimensions.
 * Scaling capability follows the same profile used by the controls and grid.
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
