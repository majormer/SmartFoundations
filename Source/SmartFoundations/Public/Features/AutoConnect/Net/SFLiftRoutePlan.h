// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGSplineHologram.h"
#include "ItemAmount.h"
#include "SFLiftRoutePlan.generated.h"
class AFGBuildable;
class AFGHologram;
class UFGRecipe;
USTRUCT()
struct SMARTFOUNDATIONS_API FSFLiftRouteEndpoint
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AFGBuildable> ExistingActor;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() FTransform OwnerPose;
    UPROPERTY() FTransform PortPose;
    UPROPERTY() FName PortName;
    UPROPERTY() int32 Direction = -1;
    UPROPERTY() int32 RouteIndex = INDEX_NONE;
    UPROPERTY() bool bExisting = false;
};
USTRUCT()
struct SMARTFOUNDATIONS_API FSFLiftRoutePlanEntry
{
    GENERATED_BODY()
    UPROPERTY() bool bLift = false;
    UPROPERTY() int32 Tier = 1;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() TSubclassOf<UFGRecipe> Recipe;
    UPROPERTY() FTransform Pose;
    UPROPERTY() FTransform Top;
    UPROPERTY() TArray<FSplinePointData> Spline;
    UPROPERTY() FSFLiftRouteEndpoint From;
    UPROPERTY() FSFLiftRouteEndpoint To;
    UPROPERTY() TArray<FItemAmount> Cost;
};
namespace SFLiftRoutePlan
{
    void Capture(AFGHologram* Parent, TArray<FSFLiftRoutePlanEntry>& Entries);
    bool IsBounded(const TArray<FSFLiftRoutePlanEntry>& Entries);
    bool Prepare(AFGHologram* Parent, const TArray<FSFLiftRoutePlanEntry>& Entries);
    bool ValidateTree(AFGHologram* Parent);
    bool GetPreparedCost(const AFGHologram* Parent, TArray<FItemAmount>& OutCost);
}
