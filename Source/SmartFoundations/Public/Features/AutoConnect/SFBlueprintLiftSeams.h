// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Features/AutoConnect/Net/SFBlueprintLiftPlan.h"
struct FSFScalingSpec;
struct FNetConstructionID;
class AFGBlueprintHologram;
class AFGHologram;
class USFSubsystem;
class USFAutoConnectService;

/** Whole-copy placement for lift-tail repetition; no content actors are moved. */
class SMARTFOUNDATIONS_API FSFBlueprintLiftSeams
{
public:
    static void UpdatePreview(AFGBlueprintHologram* Root, USFSubsystem* Subsystem, USFAutoConnectService* Service);
    static void ResetPreview(AFGHologram* Root);
    static void ResetAllPreviews();
    static void Capture(AFGHologram* Root, FSFScalingSpec& Spec);
    static bool IsBounded(const TArray<FSFBlueprintLiftPlanEntry>& Plan);
    static bool SamePort(const FSFBlueprintLiftPort& A, const FSFBlueprintLiftPort& B);
    static bool Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec);
    static bool IsPrepared(AFGHologram* Root);
    static void Construct(AFGBlueprintHologram* Root, const TArray<AActor*>& Copies, FNetConstructionID ID, TArray<AActor*>& Seams);
    /** True when spacing changed and the caller must wait for the ordinary grid reposition. */
    static bool Align(AFGBlueprintHologram* Root, USFSubsystem* Subsystem, USFAutoConnectService* Service);
    /** Scoped exception to the visual-only grid child's placement guard. */
    static bool IsNativeValidation(const AFGHologram* Hologram);
    static FString Describe(AFGBlueprintHologram* Root);
    /** Native placement only; restores temporary child/placement state on success or rejection. */
    static bool ValidateCopy(AFGBlueprintHologram* Copy);
private:
    static void RetireProbe(AFGBlueprintHologram* Root, AFGHologram* Probe);
};
