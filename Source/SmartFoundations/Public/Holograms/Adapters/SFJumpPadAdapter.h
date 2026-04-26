// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Jump Pad Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * FSFJumpPadAdapter - Adapter for Jump Pad holograms
 * Maps to AFGJumpPadHologram and AFGJumpPadLauncherHologram
 * 
 * Jump Pads have special placement logic (variable angle, trajectory visualization)
 * Features disabled until we determine appropriate support strategy
 */
class SMARTFOUNDATIONS_API FSFJumpPadAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFJumpPadAdapter(AFGHologram* InHologram)
		: FSFHologramAdapterBase(InHologram)
	{
	}

	virtual ~FSFJumpPadAdapter() = default;

	virtual FBoxSphereBounds GetBuildingBounds() const override
	{
		if (!HologramPtr.IsValid())
		{
			return FBoxSphereBounds(FBox(FVector(-50), FVector(50)));
		}
		
		const FBox WorldBox = HologramPtr->GetComponentsBoundingBox(/*bNonColliding=*/true);
		if (WorldBox.IsValid)
		{
			return FBoxSphereBounds(WorldBox);
		}
		
		// Conservative fallback (jump pad is roughly 4m x 4m x 1m)
		FVector JumpPadSize(400.0f, 400.0f, 100.0f);
		return FBoxSphereBounds(FBox(-JumpPadSize * 0.5f, JumpPadSize * 0.5f));
	}

	virtual bool SupportsFeature(ESFFeature Feature) const override
	{
		// Jump pads use custom placement with angle adjustment
		// Disable Smart! features to avoid conflicts
		return false;
	}

	virtual FString GetAdapterTypeName() const override
	{
		return TEXT("JumpPad");
	}
};
