// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
#include "Features/AutoConnect/Net/SFBlueprintRailPlan.h"
class AFGHologram;
class AFGBlueprintHologram;
struct FSFScalingSpec;
struct FNetConstructionID;

/** Native rail managers own routing and connections; Smart owns adjacency and exact replay. */
class SMARTFOUNDATIONS_API FSFBlueprintRailSeams
{
public:
    static void InstallNativeHooks();
    static void Update(AFGBlueprintHologram* Root);
    static void Reset(AFGHologram* Root);
    static void ResetAll();
    static void Capture(AFGHologram* Root, TArray<FSFBlueprintRailPlanEntry>& Plan, TArray<FItemAmount>& Cost);
    static bool IsBounded(const TArray<FSFBlueprintRailPlanEntry>& Plan);
    static bool SamePortIdentity(const FSFBlueprintRailPort& A, const FSFBlueprintRailPort& B);
    static bool Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec, FString* Failure = nullptr);
    static bool IsPrepared(AFGHologram* Root);
    static bool GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost);
    static FString DescribeNativeProgress(AFGBlueprintHologram* Root);
    static AActor* ConstructCopy(AFGBlueprintHologram* Copy, TFunctionRef<AActor*()> NativeConstruct);
    static void Construct(AFGBlueprintHologram* Root, const TArray<AActor*>& Copies, FNetConstructionID ID, TArray<AActor*>& Seams);

private:
    struct FSession;
    static TMap<TWeakObjectPtr<AFGBlueprintHologram>, TSharedPtr<FSession>>& Sessions();
    static FSFBlueprintRailPort CapturePort(class UFGRailroadTrackConnectionComponent* Port, const TMap<AFGBlueprintHologram*, FIntVector>& Cells);
    static void Discard(FSession& Session);
    static bool RestoreBindings(FSession& Session, const TArray<FSFBlueprintRailPlanEntry>& Plan, FString* Failure);
    static void ObserveNativeRemap(class FGBlueprintOpenConnectionManagerBase* Manager, class AFGBuildable* Built, int32 Index);
    static void ConstructAtNativeBoundary(class FGBlueprintOpenConnectionManagerBase* Manager, TArray<class AFGBuildable*>& Built, FNetConstructionID ID);
};
