// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "SFConveyorPlanEndpoint.generated.h"
class AFGBuildable;
class AFGHologram;
class UFGFactoryConnectionComponent;

/** A selected endpoint in one construction request, not permission to choose a world neighbour. */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFConveyorPlanEndpoint
{
    GENERATED_BODY()
    UPROPERTY() bool bPresent = false;
    UPROPERTY() bool bExisting = false;
    UPROPERTY() bool bBlueprintMember = false;
    UPROPERTY() TObjectPtr<AFGBuildable> ExistingActor = nullptr;
    UPROPERTY() TSubclassOf<AFGBuildable> BuildClass;
    UPROPERTY() FTransform OwnerPose = FTransform::Identity;
    UPROPERTY() FTransform PortPose = FTransform::Identity;
    UPROPERTY() FName PortName;
    UPROPERTY() int32 Direction = INDEX_NONE;
    UPROPERTY() float Clearance = 0;
    /** Blueprint-copy role plus the exact authored member/port behind its disposable duplicate. */
    UPROPERTY() FIntVector CopyCell = FIntVector::ZeroValue;
    UPROPERTY() FTransform OriginalOwnerPose = FTransform::Identity;

    static FSFConveyorPlanEndpoint Capture(UFGFactoryConnectionComponent* Port, AFGHologram* RequestRoot, bool bRetainPlacementAnchor = false);
    /** Positioning-only native support; never eligible for an item-transfer edge. */
    bool IsPlacementAnchor() const;
    bool IsBounded() const;
    /** Resolve only an existing actor identity or this authority request's preview tree. */
    UFGFactoryConnectionComponent* Resolve(AFGHologram* RequestRoot) const;
    void AddPreviewWorldOffset(const FVector& Shift);
};
