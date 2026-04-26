// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - Water Extractor Adapter Implementation

#include "SFWaterExtractorAdapter.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFWaterExtractorAdapter::FSFWaterExtractorAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFWaterExtractorAdapter::GetBuildingBounds() const
{
	FVector ExtractorSize = CalculateWaterExtractorSize();

	// Create bounds centered at origin
	FBox BoundingBox(
		FVector(-ExtractorSize.X * 0.5f, -ExtractorSize.Y * 0.5f, 0.0f),
		FVector(ExtractorSize.X * 0.5f, ExtractorSize.Y * 0.5f, ExtractorSize.Z)
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFWaterExtractorAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Issue #197: Water extractor scaling ENABLED via ASFWaterPumpChildHologram
	//
	// Children use Smart!'s own water volume validation (EncompassesPoint) instead of
	// vanilla CheckMinimumDepth() which fails for children positioned via SetActorLocation.
	// Children over land are blocked with UFGCDNeedsWaterVolume disqualifier.
	switch (Feature)
	{
		case ESFFeature::ScaleX:
		case ESFFeature::ScaleY:
		case ESFFeature::Spacing:
		case ESFFeature::Arrows:
			return true;  // ENABLED - ASFWaterPumpChildHologram bypasses validation

		case ESFFeature::ScaleZ:
			return false;  // Vertical scaling disabled - water is 2D surface

		default:
			return false;
	}
}

FString FSFWaterExtractorAdapter::GetAdapterTypeName() const
{
	return TEXT("WaterExtractor");
}

FVector FSFWaterExtractorAdapter::CalculateWaterExtractorSize() const
{
	if (!HologramPtr.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFWaterExtractorAdapter::CalculateWaterExtractorSize - Invalid hologram"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFWaterExtractorAdapter: No BuildClass"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	// Use smart fallback: Registry → ClearanceBox → MeshBounds → Default
	FVector Size;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, Size, Source);

	SF_LOG_ADAPTER(Normal, TEXT("FSFWaterExtractorAdapter: Size=%s Source=%s Class=%s (scaling enabled - validates per-child)"),
		*Size.ToString(), *Source, *BuildClass->GetName());

	return Size;
}
