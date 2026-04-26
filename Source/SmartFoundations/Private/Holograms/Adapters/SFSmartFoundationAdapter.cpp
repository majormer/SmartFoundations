// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Foundation Hologram Adapter Implementation

#include "SFSmartFoundationAdapter.h"
#include "SmartFoundations.h"

FSFSmartFoundationAdapter::FSFSmartFoundationAdapter(ASFFoundationHologram* InHologram)
	: FSFSmartBuildableAdapter(InHologram)
	, SmartFoundationHologram(InHologram)
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("FSFSmartFoundationAdapter created for: %s"), 
		InHologram ? *InHologram->GetName() : TEXT("NULL"));
}

bool FSFSmartFoundationAdapter::ValidateFoundationGrid() const
{
	if (SmartFoundationHologram)
	{
		// Delegate to custom hologram method (Phase 1.4 stub for now)
		// return SmartFoundationHologram->ValidateFoundationGrid();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ValidateFoundationGrid called (Phase 1.4 stub)"));
	}
	return true;
}

void FSFSmartFoundationAdapter::ApplyGridSnapping()
{
	if (SmartFoundationHologram)
	{
		// Delegate to custom hologram method (Phase 1.4 stub for now)
		// SmartFoundationHologram->ApplyGridSnapping();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyGridSnapping called (Phase 1.4 stub)"));
	}
}

FString FSFSmartFoundationAdapter::GetAdapterTypeName() const
{
	return TEXT("SmartFoundation");
}
