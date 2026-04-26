// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Wall Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * Adapter for wall holograms
 * 
 * Walls have special geometry constraints:
 * - Physical dimensions: X=width (8m), Y=thickness (0.2m), Z=height (4m)
 * - Grid placement: When scaling Y-axis, walls should be spaced by WIDTH (X), not thickness (Y)
 * 
 * Solution: Return swapped dimensions (Y uses X value) so grid spacing works correctly
 */
class SMARTFOUNDATIONS_API FSFWallAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFWallAdapter(AFGHologram* InHologram);
	virtual ~FSFWallAdapter() = default;

	// FSFHologramAdapterBase interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;

private:
	/**
	 * Calculate wall size with corrected Y dimension for grid placement
	 */
	FVector CalculateWallSizeForGrid() const;
};
