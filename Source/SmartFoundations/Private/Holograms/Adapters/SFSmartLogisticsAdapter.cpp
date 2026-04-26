// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Logistics Hologram Adapter Implementation

#include "SFSmartLogisticsAdapter.h"
#include "SmartFoundations.h"

FSFSmartLogisticsAdapter::FSFSmartLogisticsAdapter(ASFLogisticsHologram* InHologram)
	: FSFSmartBuildableAdapter(InHologram)
	, SmartLogisticsHologram(InHologram)
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("FSFSmartLogisticsAdapter created for: %s"), 
		InHologram ? *InHologram->GetName() : TEXT("NULL"));
}

bool FSFSmartLogisticsAdapter::SupportsAutoConnect() const
{
	// All logistics holograms support Auto-Connect
	return true;
}

void FSFSmartLogisticsAdapter::SpawnAutoConnectChildren()
{
	if (SmartLogisticsHologram)
	{
		// Delegate to custom hologram method (Phase 1.3 stub for now)
		// SmartLogisticsHologram->SpawnLogisticsChildren();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("SpawnAutoConnectChildren called (Phase 1.3 stub)"));
	}
}

void FSFSmartLogisticsAdapter::ConnectToNearbyBuildings()
{
	if (SmartLogisticsHologram)
	{
		// Delegate to custom hologram method (Phase 1.3 stub for now)
		// SmartLogisticsHologram->ConnectToNearbyBuildings();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ConnectToNearbyBuildings called (Phase 1.3 stub)"));
	}
}

void FSFSmartLogisticsAdapter::ApplyAutoSpacing()
{
	if (SmartLogisticsHologram)
	{
		// Delegate to custom hologram method (Phase 1.3 stub for now)
		// SmartLogisticsHologram->ApplyAutoSpacing();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyAutoSpacing called (Phase 1.3 stub)"));
	}
}

FString FSFSmartLogisticsAdapter::GetAdapterTypeName() const
{
	return TEXT("SmartLogistics");
}
