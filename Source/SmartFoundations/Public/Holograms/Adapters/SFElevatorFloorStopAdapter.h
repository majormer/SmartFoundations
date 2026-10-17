// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"
#include "Hologram/FGHologram.h"
#include "Buildables/FGBuildableElevator.h"

/** Native single-stop placement; owned assembly stops use the elevator's construction contract. */
class SMARTFOUNDATIONS_API FSFElevatorFloorStopAdapter : public FSFHologramAdapterBase
{
public:
    explicit FSFElevatorFloorStopAdapter(AFGHologram* InHologram) : FSFHologramAdapterBase(InHologram) {}
    virtual FBoxSphereBounds GetBuildingBounds() const override
    {
        const float HalfHeight = AFGBuildableElevator::TOTAL_FLOOR_HEIGHT * 0.5f;
        return FBoxSphereBounds(FBox(FVector(-400, -400, -HalfHeight), FVector(400, 400, HalfHeight)));
    }
    virtual bool SupportsFeature(ESFFeature) const override
    {
        return false;
    }
    virtual FString GetAdapterTypeName() const override { return TEXT("ElevatorFloorStop"); }
};
