// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Buildable Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"
#include "Holograms/Core/SFBuildableHologram.h"

/**
 * Adapter for Smart custom buildable holograms (ASFBuildableHologram)
 * 
 * This adapter bridges the gap between the Smart subsystem and custom hologram classes.
 * It delegates all operations to the custom hologram's native Smart methods.
 * 
 * Benefits:
 * - Direct delegation to hologram methods (no adapter logic duplication)
 * - Clean separation between subsystem interface and hologram implementation
 * - Easy to extend for new features
 */
class SMARTFOUNDATIONS_API FSFSmartBuildableAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFSmartBuildableAdapter(AFGHologram* InHologram);
	virtual ~FSFSmartBuildableAdapter() = default;

	// ISFHologramAdapter interface - delegates to custom hologram
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual void ApplyTransformOffset(const FVector& Offset) override;
	virtual FString GetAdapterTypeName() const override;

	// Metadata access
	int32 GetPlacementGroupIndex() const;
	int32 GetPlacementChildIndex() const;

protected:
	/** Typed pointer to Smart custom hologram */
	ASFBuildableHologram* SmartBuildableHologram = nullptr;
};
