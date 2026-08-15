// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "SFFactorySettingsSnapshot.generated.h"

class UFGRecipe;
class UFGPowerShardDescriptor;

/** Immutable value snapshot of recipe, Power Shard, and Somersloop intent for one construction commit. */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFFactorySettingsSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    bool bHasRecipe = false;

    UPROPERTY()
    TSubclassOf<UFGRecipe> Recipe = nullptr;

    UPROPERTY()
    bool bHasPotential = false;

    UPROPERTY()
    float Potential = 1.0f;

    UPROPERTY()
    TSubclassOf<UFGPowerShardDescriptor> OverclockShardClass = nullptr;

    UPROPERTY()
    int32 OverclockShardCount = 0;

    UPROPERTY()
    bool bHasProductionBoost = false;

    UPROPERTY()
    float ProductionBoost = 1.0f;

    UPROPERTY()
    TSubclassOf<UFGPowerShardDescriptor> ProductionBoostShardClass = nullptr;

    UPROPERTY()
    int32 ProductionBoostShardCount = 0;

    bool HasAnySettings() const
    {
        return bHasRecipe || bHasPotential || bHasProductionBoost;
    }

    bool NeedsInventoryTransfer() const
    {
        return bHasPotential || bHasProductionBoost;
    }
};
