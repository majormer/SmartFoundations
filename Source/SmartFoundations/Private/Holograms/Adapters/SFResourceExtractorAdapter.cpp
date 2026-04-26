// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - Resource Extractor Adapter Implementation

#include "SFResourceExtractorAdapter.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFResourceExtractorAdapter::FSFResourceExtractorAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFResourceExtractorAdapter::GetBuildingBounds() const
{
	FVector ExtractorSize = CalculateExtractorSize();

	// Create bounds centered at origin
	FBox BoundingBox(
		FVector(-ExtractorSize.X * 0.5f, -ExtractorSize.Y * 0.5f, 0.0f),
		FVector(ExtractorSize.X * 0.5f, ExtractorSize.Y * 0.5f, ExtractorSize.Z)
	);

	return FBoxSphereBounds(BoundingBox);
}

bool FSFResourceExtractorAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Resource extractors CANNOT be scaled - they must be on specific resource nodes
	// Child holograms fail validation ("Resource is not deep enough", etc.)
	switch (Feature)
	{
		case ESFFeature::ScaleX:
		case ESFFeature::ScaleY:
		case ESFFeature::ScaleZ:
		case ESFFeature::Spacing:
		case ESFFeature::Arrows:
			return false;  // All Smart! features disabled for resource extractors

		default:
			return false;
	}
}

FString FSFResourceExtractorAdapter::GetAdapterTypeName() const
{
	return TEXT("ResourceExtractor");
}

FVector FSFResourceExtractorAdapter::CalculateExtractorSize() const
{
	if (!HologramPtr.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFResourceExtractorAdapter::CalculateExtractorSize - Invalid hologram"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFResourceExtractorAdapter: No BuildClass"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	// Use smart fallback: Registry → ClearanceBox → MeshBounds → Default
	FVector Size;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, Size, Source);

	SF_LOG_ADAPTER(Normal, TEXT("FSFResourceExtractorAdapter: Size=%s Source=%s Class=%s (scaling disabled)"),
		*Size.ToString(), *Source, *BuildClass->GetName());

	return Size;
}
