// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "ItemAmount.h"
#include "SFBlueprintVehiclePlan.generated.h"
class AFGBuildable;

/** Exact content identity within one repeated Blueprint cell. */
USTRUCT()
struct FSFBlueprintVehicleMember
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() FTransform Original;
    UPROPERTY() FTransform World;
    // Segment geometry in world space, independent of native transform baking.
    UPROPERTY() TArray<FSplinePointData> Spline;
};

USTRUCT()
struct FSFBlueprintVehiclePlanEntry
{
    GENERATED_BODY()
    UPROPERTY() FSFBlueprintVehicleMember Tail;
    UPROPERTY() FSFBlueprintVehicleMember Head;
    UPROPERTY() FSFBlueprintVehicleMember Incoming;
    UPROPERTY() FSFBlueprintVehicleMember Outgoing;
    UPROPERTY() TArray<FSplinePointData> Bridge;
    UPROPERTY() TArray<FItemAmount> Cost;
};
