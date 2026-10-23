// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "ItemAmount.h"
#include "SFBlueprintLiftPlan.generated.h"
class AFGBuildable;
class UFGRecipe;

/** An exact authored mouth and the priced pose of its complete Blueprint copy. */
USTRUCT()
struct FSFBlueprintLiftPort
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY() int32 Index = INDEX_NONE;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() FTransform OriginalOwner;
    UPROPERTY() FTransform OwnerPose;
    UPROPERTY() FTransform PortLocalPose;
    UPROPERTY() FName Name;
    UPROPERTY() int32 Direction = -1;
};

USTRUCT()
struct FSFBlueprintLiftPlanEntry
{
    GENERATED_BODY()
    UPROPERTY() FSFBlueprintLiftPort From;
    UPROPERTY() FSFBlueprintLiftPort To;
    UPROPERTY() int32 Tier = 0;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() TSubclassOf<UFGRecipe> Recipe;
    UPROPERTY() FTransform Pose;
    UPROPERTY() TArray<FSplinePointData> Spline;
    UPROPERTY() TArray<FItemAmount> Cost;
};
