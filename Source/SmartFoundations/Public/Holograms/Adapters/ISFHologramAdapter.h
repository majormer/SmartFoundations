// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Hologram Adapter Interface

#pragma once

#include "CoreMinimal.h"
#include "FGHologram.h"
#include "Features/Scaling/SFScalingTypes.h"

/**
 * Feature flags for hologram capabilities
 */
enum class ESFFeature : uint8
{
	ScaleX,
	ScaleY,
	ScaleZ,
	Arrows,
	Spacing
};

/**
 * ISFHologramAdapter - Minimal abstraction layer between feature modules and holograms
 * 
 * This interface provides a stable contract for feature modules to interact with
 * holograms without depending on specific hologram types.
 * 
 * Implementations are thin wrappers that translate between the adapter API
 * and hologram-specific behavior.
 */
class SMARTFOUNDATIONS_API ISFHologramAdapter
{
public:
	virtual ~ISFHologramAdapter() = default;

	/**
	 * Get the building bounds for this hologram
	 * Used by scaling module for validation
	 * 
	 * @return Bounding box of the hologram
	 */
	virtual FBoxSphereBounds GetBuildingBounds() const = 0;

	/**
	 * Get the base transform of the hologram
	 * 
	 * @return Current transform (location, rotation, scale)
	 */
	virtual FTransform GetBaseTransform() const = 0;

	/**
	 * Apply a transform offset to the hologram
	 * This modifies the hologram's visual representation temporarily
	 * 
	 * @param Offset - Offset to apply (in local space)
	 */
	virtual void ApplyTransformOffset(const FVector& Offset) = 0;

	/**
	 * Check if this hologram supports a specific feature
	 * 
	 * @param Feature - Feature to check
	 * @return true if supported, false otherwise
	 */
	virtual bool SupportsFeature(ESFFeature Feature) const = 0;

	/**
	 * Get the underlying hologram actor (weak pointer for safety)
	 * 
	 * @return Weak pointer to the hologram
	 */
	virtual TWeakObjectPtr<AFGHologram> GetHologram() const = 0;

	/**
	 * Check if the hologram is still valid
	 * 
	 * @return true if hologram exists and is valid
	 */
	virtual bool IsValid() const = 0;

	/**
	 * Get a human-readable name for debugging
	 * 
	 * @return Adapter type name (e.g., "Foundation", "Wall")
	 */
	virtual FString GetAdapterTypeName() const { return "Base Adapter"; };
};

/**
 * Base adapter implementation with common functionality
 * Concrete adapters can inherit from this to reduce boilerplate
 */
class SMARTFOUNDATIONS_API FSFHologramAdapterBase : public ISFHologramAdapter
{
public:
	explicit FSFHologramAdapterBase(AFGHologram* InHologram)
		: HologramPtr(InHologram)
	{
	}

	virtual ~FSFHologramAdapterBase() = default;

	// ISFHologramAdapter interface
	virtual TWeakObjectPtr<AFGHologram> GetHologram() const override
	{
		return HologramPtr;
	}

	virtual bool IsValid() const override
	{
		return HologramPtr.IsValid();
	}

	virtual FTransform GetBaseTransform() const override
	{
		if (HologramPtr.IsValid())
		{
			return HologramPtr->GetActorTransform();
		}
		return FTransform::Identity;
	}

	virtual void ApplyTransformOffset(const FVector& Offset) override
	{
		if (HologramPtr.IsValid())
		{
			FVector CurrentLocation = HologramPtr->GetActorLocation();
			HologramPtr->SetActorLocation(CurrentLocation + Offset);
		}
	}

protected:
	/** Weak pointer to the hologram (safe if hologram is destroyed) */
	TWeakObjectPtr<AFGHologram> HologramPtr;

	/**
	 * Helper: Create Z-centered bounds from a size vector
	 * Fallback for buildings without registry profiles (unknown/modded buildings)
	 * Creates bounding box centered at origin on all axes: (-Size/2, Size/2)
	 * 
	 * @param Size - Building dimensions (width, depth, height)
	 * @return Centered bounding box
	 */
	static FBoxSphereBounds CreateCenteredBounds(const FVector& Size)
	{
		// Z-centered bounds - fallback for unknown/modded buildings
		// Registry profiles with AnchorOffset override this for known buildings
		FBox BoundingBox(-Size * 0.5f, Size * 0.5f);
		return FBoxSphereBounds(BoundingBox);
	}
};
