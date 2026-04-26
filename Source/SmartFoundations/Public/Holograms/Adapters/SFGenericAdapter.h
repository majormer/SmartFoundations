// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Generic Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFGenericAdapter - Generic adapter for holograms with standard behavior
 *
 * Replaces multiple identical adapter classes (Foundation, Storage, ConveyorAttachment,
 * HypertubeAttachment, PipelineJunction) that all share the same implementation:
 * - Support all Smart! features (ScaleX, ScaleY, ScaleZ, Arrows, Spacing)
 * - Use CreateCenteredBounds() for bounds calculation
 * - Delegate size calculation to USFBuildableSizeRegistry::GetSizeWithFallback()
 *
 * Only the type name differs for logging/debugging purposes.
 */
class SMARTFOUNDATIONS_API FSFGenericAdapter : public FSFHologramAdapterBase
{
public:
	/**
	 * Create a generic adapter with a specific type name
	 * @param InHologram - The hologram to adapt
	 * @param InTypeName - Type name for logging (e.g., "Foundation", "Storage")
	 */
	explicit FSFGenericAdapter(AFGHologram* InHologram, const FString& InTypeName);
	virtual ~FSFGenericAdapter() = default;

	// ISFHologramAdapter interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;

private:
	/** Type name for logging/debugging */
	FString TypeName;

	/** Calculate size using registry with CDO fallback */
	FVector CalculateSize() const;
};
