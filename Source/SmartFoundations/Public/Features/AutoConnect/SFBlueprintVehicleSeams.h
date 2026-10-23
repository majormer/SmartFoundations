// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Features/AutoConnect/Net/SFBlueprintVehiclePlan.h"
class AFGHologram;
class AFGBlueprintHologram;
struct FSFScalingSpec;
struct FNetConstructionID;

/** Smart selects adjacent copies; the native adapter owns path legality and mutation. */
class SMARTFOUNDATIONS_API FSFBlueprintVehicleSeams
{
public:
    static void Update(AFGBlueprintHologram* Root);
    static void Reset(AFGHologram* Root);
    static void ResetAll();
    static void Capture(AFGHologram* Root, TArray<FSFBlueprintVehiclePlanEntry>& Plan);
    static bool IsBounded(const TArray<FSFBlueprintVehiclePlanEntry>& Plan);
    static bool Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec, FString* Failure);
    static bool IsPrepared(AFGHologram* Root);
    static void RecordCopy(AFGBlueprintHologram* Copy, AActor* Main, const TArray<AActor*>& Members);
    static void Construct(AFGBlueprintHologram* Root, FNetConstructionID ID, TArray<AActor*>& Bridges);
};
