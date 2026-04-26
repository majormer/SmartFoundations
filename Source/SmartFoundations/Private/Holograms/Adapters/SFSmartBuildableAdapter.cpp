// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Buildable Hologram Adapter Implementation

#include "SFSmartBuildableAdapter.h"
#include "SmartFoundations.h"

FSFSmartBuildableAdapter::FSFSmartBuildableAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
	, SmartBuildableHologram(Cast<ASFBuildableHologram>(InHologram))
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("FSFSmartBuildableAdapter created for: %s (ASFBuildableHologram: %s)"), 
		InHologram ? *InHologram->GetName() : TEXT("NULL"),
		SmartBuildableHologram ? TEXT("Yes") : TEXT("No"));
}

FBoxSphereBounds FSFSmartBuildableAdapter::GetBuildingBounds() const
{
	if (SmartBuildableHologram)
	{
		// Delegate to custom hologram's Smart method
		return SmartBuildableHologram->GetSmartBuildingBounds();
	}
	
	// Fallback: Get bounds from vanilla hologram
	if (HologramPtr.IsValid())
	{
		FVector Origin;
		FVector BoxExtent;
		HologramPtr->GetActorBounds(false, Origin, BoxExtent);
		return FBoxSphereBounds(Origin, BoxExtent, BoxExtent.Size());
	}
	
	return FBoxSphereBounds();
}

bool FSFSmartBuildableAdapter::SupportsFeature(ESFFeature Feature) const
{
	if (SmartBuildableHologram)
	{
		// Delegate to custom hologram's Smart method
		return SmartBuildableHologram->SupportsSmartFeature(Feature);
	}
	
	return false;
}

void FSFSmartBuildableAdapter::ApplyTransformOffset(const FVector& Offset)
{
	if (SmartBuildableHologram)
	{
		// Delegate to custom hologram's Smart method
		SmartBuildableHologram->ApplySmartTransformOffset(Offset);
	}
}

FString FSFSmartBuildableAdapter::GetAdapterTypeName() const
{
	return TEXT("SmartBuildable");
}

int32 FSFSmartBuildableAdapter::GetPlacementGroupIndex() const
{
	if (SmartBuildableHologram)
	{
		return SmartBuildableHologram->GetPlacementGroupIndex();
	}
	return -1;
}

int32 FSFSmartBuildableAdapter::GetPlacementChildIndex() const
{
	if (SmartBuildableHologram)
	{
		return SmartBuildableHologram->GetPlacementChildIndex();
	}
	return -1;
}
