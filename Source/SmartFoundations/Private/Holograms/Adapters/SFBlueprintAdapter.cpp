// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Blueprint Hologram Adapter Implementation (#168)

#include "Holograms/Adapters/SFBlueprintAdapter.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Hologram/FGBlueprintHologram.h"

FSFBlueprintAdapter::FSFBlueprintAdapter(AFGBlueprintHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
{
}

FBoxSphereBounds FSFBlueprintAdapter::GetBuildingBounds() const
{
	if (AFGBlueprintHologram* Blueprint = Cast<AFGBlueprintHologram>(HologramPtr.Get()))
	{
		// mLocalBounds is the composite footprint vanilla computes for the loaded blueprint
		// (friend access via AccessTransformers). Valid once the blueprint world is staged -
		// for the HELD hologram that has already happened by the time Smart registers it.
		const FBox LocalBounds = Blueprint->mLocalBounds;
		if (LocalBounds.IsValid && LocalBounds.GetExtent().GetMax() > 1.0)
		{
			SF_LOG_ADAPTER(Normal, TEXT("FSFBlueprintAdapter: bounds size=%s (from mLocalBounds)"),
				*(LocalBounds.GetExtent() * 2.0).ToString());
			return FBoxSphereBounds(LocalBounds);
		}

		UE_LOG(LogSmartHologram, Log,
			TEXT("[#168] FSFBlueprintAdapter: mLocalBounds not valid yet on %s - falling back to 8m cube"),
			*Blueprint->GetName());
	}

	// Fallback: one foundation footprint so scaling stays usable rather than degenerate
	return CreateCenteredBounds(FVector(800.0f, 800.0f, 800.0f));
}

bool FSFBlueprintAdapter::SupportsFeature(ESFFeature Feature) const
{
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

FString FSFBlueprintAdapter::GetAdapterTypeName() const
{
	return TEXT("Blueprint");
}
