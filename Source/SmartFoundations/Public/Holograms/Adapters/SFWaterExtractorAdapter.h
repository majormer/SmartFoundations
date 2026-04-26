// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - Water Extractor Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * Adapter for Water Extractors - Special case resource extractor
 * 
 * Unlike miners/oil extractors, water extractors:
 * - Can be placed anywhere on water surfaces (no specific resource node)
 * - Check water depth at placement location (CheckMinimumDepth)
 * - CAN support scaling across flat water surfaces
 * 
 * Child holograms will validate independently via CheckMinimumDepth.
 * Scaling will work as long as all child positions have sufficient water depth.
 * 
 * Features ENABLED: Scaling, Spacing (validated per-child)
 * Features MAY WORK: Levitation/Steps
 */
class SMARTFOUNDATIONS_API FSFWaterExtractorAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFWaterExtractorAdapter(AFGHologram* InHologram);
	virtual ~FSFWaterExtractorAdapter() = default;

	// FSFHologramAdapterBase interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;

private:
	/**
	 * Calculate water extractor size from database or clearance box
	 */
	FVector CalculateWaterExtractorSize() const;
};
