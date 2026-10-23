// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "SFBlueprintRailPlan.generated.h"
class AFGBuildable;

/** Content identity and priced preview pose, independent of staging-world actor names. */
USTRUCT()
struct FSFBlueprintRailPort
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() FTransform OriginalOwner;
    UPROPERTY() FTransform OwnerPose;
    UPROPERTY() FName ComponentName;
};

USTRUCT()
struct FSFBlueprintRailPlanEntry
{
    GENERATED_BODY()
    // A native open-ended junction can contain several source connections.
    UPROPERTY() TArray<FSFBlueprintRailPort> From;
    UPROPERTY() FSFBlueprintRailPort To;
    UPROPERTY() bool bDirect = false;
    UPROPERTY() FTransform BridgePose;
    UPROPERTY() TArray<FSplinePointData> Spline;
};
