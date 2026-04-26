// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Logistics Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "SFSmartBuildableAdapter.h"
#include "Holograms/Core/ASFLogisticsHologram.h"

/**
 * Adapter for Smart logistics holograms (ASFLogisticsHologram)
 * 
 * Extends base buildable adapter with Auto-Connect functionality.
 * Enables automatic connection of distributors to nearby buildings.
 */
class SMARTFOUNDATIONS_API FSFSmartLogisticsAdapter : public FSFSmartBuildableAdapter
{
public:
	explicit FSFSmartLogisticsAdapter(ASFLogisticsHologram* InHologram);
	virtual ~FSFSmartLogisticsAdapter() = default;

	// Auto-Connect interface (Phase 6 implementation)
	virtual bool SupportsAutoConnect() const;
	virtual void SpawnAutoConnectChildren();
	virtual void ConnectToNearbyBuildings();
	virtual void ApplyAutoSpacing();

	virtual FString GetAdapterTypeName() const override;

protected:
	/** Typed pointer to Smart logistics hologram */
	ASFLogisticsHologram* SmartLogisticsHologram = nullptr;
};
