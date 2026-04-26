// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Factory/Production Building Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * Adapter for factory/production buildings
 * Handles: Constructors, Assemblers, Manufacturers, Smelters, Refineries, etc.
 * 
 * These buildings use FGFactoryHologram and typically have clearance boxes.
 */
class SMARTFOUNDATIONS_API FSFFactoryAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFFactoryAdapter(AFGHologram* InHologram);
	virtual ~FSFFactoryAdapter() = default;

	// FSFHologramAdapterBase interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;

private:
	/**
	 * Calculate factory building size using:
	 * 1. Hardcoded database (if present)
	 * 2. Clearance box auto-discovery
	 * 3. Default fallback
	 */
	FVector CalculateFactorySize() const;
};
