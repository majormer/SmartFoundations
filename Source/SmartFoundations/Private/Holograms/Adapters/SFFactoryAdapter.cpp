// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Factory/Production Building Hologram Adapter Implementation

#include "SFFactoryAdapter.h"
#include "SmartFoundations.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFFactoryAdapter::FSFFactoryAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFFactoryAdapter::GetBuildingBounds() const
{
	FVector FactorySize = CalculateFactorySize();

	// Create bounds centered at origin
	FBox BoundingBox(
		FVector(-FactorySize.X * 0.5f, -FactorySize.Y * 0.5f, 0.0f),
		FVector(FactorySize.X * 0.5f, FactorySize.Y * 0.5f, FactorySize.Z)
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFFactoryAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Factory buildings support all Smart! features (matches original Smart behavior)
	// Note: Z-axis scaling may cause visual overlaps until custom spacing is implemented
	switch (Feature)
	{
	case ESFFeature::ScaleX:
	case ESFFeature::ScaleY:
	case ESFFeature::ScaleZ:
	case ESFFeature::Arrows:
	case ESFFeature::Spacing:
		return true;
	default:
		return false;
	}
}

FString FSFFactoryAdapter::GetAdapterTypeName() const
{
	return TEXT("Factory");
}

FVector FSFFactoryAdapter::CalculateFactorySize() const
{
	if (!HologramPtr.IsValid())
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("FSFFactoryAdapter::CalculateFactorySize - Invalid hologram"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("FSFFactoryAdapter: No BuildClass"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	// Use smart fallback: Registry → ClearanceBox → MeshBounds → Default
	FVector Size;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, Size, Source);


	return Size;
}
