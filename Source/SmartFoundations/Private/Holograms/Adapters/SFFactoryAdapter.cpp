// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Factory/Production Building Hologram Adapter Implementation

#include "SFFactoryAdapter.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFFactoryAdapter::CalculateFactorySize - Invalid hologram"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFFactoryAdapter: No BuildClass"));
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	// DEBUG: Show both clearance box and mesh bounds
	FVector ClearanceSize;
	FVector MeshSize;
	bool bHasClearance = USFBuildableSizeRegistry::TryGetSizeFromClearanceBox(BuildClass, ClearanceSize);
	bool bHasMesh = USFBuildableSizeRegistry::TryGetSizeFromMeshBounds(BuildClass, MeshSize);

	if (bHasClearance && bHasMesh)
	{
		SF_LOG_ADAPTER(VeryVerbose, TEXT("FSFFactoryAdapter: CLEARANCE=%s MESH=%s"),
			*ClearanceSize.ToString(), *MeshSize.ToString());
	}
	else if (bHasClearance)
	{
		SF_LOG_ADAPTER(VeryVerbose, TEXT("FSFFactoryAdapter: CLEARANCE BOX reports: %s (no mesh bounds)"), *ClearanceSize.ToString());
	}
	else if (bHasMesh)
	{
		SF_LOG_ADAPTER(VeryVerbose, TEXT("FSFFactoryAdapter: MESH BOUNDS reports: %s (no clearance box)"), *MeshSize.ToString());
	}
	else
	{
		SF_LOG_ADAPTER(VeryVerbose, TEXT("FSFFactoryAdapter: No clearance box or mesh bounds found"));
	}

	// Use smart fallback: Registry → ClearanceBox → MeshBounds → Default
	FVector Size;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, Size, Source);

	SF_LOG_ADAPTER(Normal, TEXT("FSFFactoryAdapter: Size=%s Source=%s Class=%s"),
		*Size.ToString(), *Source, *BuildClass->GetName());

	return Size;
}
