// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "SFPowerWireEndpoint.generated.h"

class AFGBuildable;
class AActor;
class UFGCircuitConnectionComponent;

/** Exact existing actor, or a new buildable's class/pivot; always a named component, never nearest-face fallback. */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFPowerWireEndpoint
{
    GENERATED_BODY()

    UPROPERTY()
    bool bExisting = false;

    UPROPERTY()
    TObjectPtr<AFGBuildable> ExistingActor = nullptr;

    UPROPERTY()
    TSubclassOf<AFGBuildable> BuildClass;

    UPROPERTY()
    FVector OwnerLocation = FVector::ZeroVector;

    UPROPERTY()
    FName ComponentName;

    static FSFPowerWireEndpoint Capture(UFGCircuitConnectionComponent* Connection);
    UFGCircuitConnectionComponent* Resolve(AActor* BuiltParent, const TArray<AActor*>& BuiltChildren) const;
};
