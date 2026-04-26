// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - Resource Extractor Adapter (Water Pumps, Miners, Oil Extractors)

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * Adapter for resource extractors that must be placed on specific resource nodes
 * Includes: Water Pumps, Miners, Oil Extractors, Geothermal Generators
 * 
 * These buildings CANNOT be scaled because:
 * - They must be on specific resource nodes (validation fails for children)
 * - Multiple extractors on one node doesn't make sense
 * - Child holograms fail "minimum depth" and "resource node" checks
 * 
 * Features DISABLED: Scaling, Spacing
 * Features MAY WORK: Levitation/Steps (doesn't offset horizontally)
 */
class SMARTFOUNDATIONS_API FSFResourceExtractorAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFResourceExtractorAdapter(AFGHologram* InHologram);
	virtual ~FSFResourceExtractorAdapter() = default;

	// FSFHologramAdapterBase interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;

private:
	/**
	 * Calculate extractor size from database or clearance box
	 */
	FVector CalculateExtractorSize() const;
};
