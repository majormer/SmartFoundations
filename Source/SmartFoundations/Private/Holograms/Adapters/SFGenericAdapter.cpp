// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Generic Hologram Adapter Implementation

#include "SFGenericAdapter.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Data/SFBuildableSizeRegistry.h"

FSFGenericAdapter::FSFGenericAdapter(AFGHologram* InHologram, const FString& InTypeName)
	: FSFHologramAdapterBase(InHologram)
	, TypeName(InTypeName)
{
}

FBoxSphereBounds FSFGenericAdapter::GetBuildingBounds() const
{
	return CreateCenteredBounds(CalculateSize());
}

bool FSFGenericAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Generic adapter supports all Smart! features
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

FString FSFGenericAdapter::GetAdapterTypeName() const
{
	return TypeName;
}

FVector FSFGenericAdapter::CalculateSize() const
{
	if (!HologramPtr.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFGenericAdapter[%s]::CalculateSize - Invalid hologram"), *TypeName);
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	UClass* BuildClass = HologramPtr->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFGenericAdapter[%s]: No BuildClass"), *TypeName);
		return USFBuildableSizeRegistry::GetDefaultSize();
	}

	// Use smart fallback: Registry -> ClearanceBox -> MeshBounds -> Default
	FVector Size;
	FString Source;
	USFBuildableSizeRegistry::GetSizeWithFallback(BuildClass, Size, Source);

	SF_LOG_ADAPTER(Normal, TEXT("FSFGenericAdapter[%s]: Size=%s Source=%s Class=%s"),
		*TypeName, *Size.ToString(), *Source, *BuildClass->GetName());

	return Size;
}
