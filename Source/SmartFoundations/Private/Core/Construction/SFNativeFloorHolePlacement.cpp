// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFNativeFloorHolePlacement.h"
#include "SmartFoundations.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Hologram/FGPassthroughHologram.h"
#include "FGCharacterPlayer.h"
#include "Core/SFNativeHit.h"
#include "Misc/ScopeExit.h"

// Native placement replay shared by captured conveyor companions and ordinary grids.
// No buildable, live face, native registration or material payment is changed.
bool FSFNativeFloorHolePlacement::Validate(AFGHologram* Root, AFGHologram* Child,
    const FTransform& Pose, float ExpectedThickness, FString& Reason)
{
    auto Fail = [&](const TCHAR* Why) { Reason = Why; return false; };
    auto* Player = IsValid(Root) ? Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator()) : nullptr;
    if (Pose.ContainsNaN() || !IsValid(Child) || !Player || !Player->GetInventory() || !Root->GetWorld()
        || !Root->HasAuthority() || !Child->GetRecipe() || !Child->GetBuildClass()
        || Child->GetWorld() != Root->GetWorld() || !Child->HasAuthority()
        || Child->GetBlueprintDesigner() != Root->GetBlueprintDesigner()
        || (Child->GetBuildClass()->GetName() != TEXT("Build_FoundationPassthrough_Lift_C")
            && Child->GetBuildClass()->GetName() != TEXT("Build_FoundationPassthrough_Pipe_C"))
        || !FMath::IsFinite(ExpectedThickness) || ExpectedThickness <= 0
        || !Child->GetActorTransform().Equals(Pose, 0.1f)) return Fail(TEXT("floor_hole_input_mismatch"));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartNativeFloorHole), false, Root);
    Query.AddIgnoredActor(Player);
    Query.AddIgnoredActor(Child);
    TArray<AFGHologram*> Pending{Root};
    TSet<AFGHologram*> Seen;
    while (!Pending.IsEmpty())
    {
        auto* Holo = Pending.Pop(EAllowShrinking::No);
        if (!IsValid(Holo) || Seen.Contains(Holo)) continue;
        Seen.Add(Holo); Query.AddIgnoredActor(Holo); Pending.Append(Holo->GetHologramChildren());
    }
    const FVector Axis = Pose.GetUnitAxis(EAxis::Z);
    TArray<FHitResult> Hits;
    // The recipe's native validator bounds legal thickness. Bound the query too;
    // malformed preset data must not turn a placement into an unbounded world probe.
    if (ExpectedThickness > 100000.0f) return Fail(TEXT("floor_hole_thickness_exceeds_capture_bound"));
    const double Span = ExpectedThickness * 0.5 + 200.0;
    ECollisionChannel BuildChannel = ECC_MAX;
    if (const auto* Profile = UCollisionProfile::Get())
        for (int32 Index = 0; Index < ECC_MAX; ++Index)
            if (Profile->ReturnChannelNameFromContainerIndex(Index) == FName(TEXT("BuildGun")))
            { BuildChannel = static_cast<ECollisionChannel>(Index); break; }
    if (BuildChannel == ECC_MAX) return Fail(TEXT("native_build_gun_trace_channel_unavailable"));
    // Replay the build gun's placement channel. Visibility traces need not hit
    // native buildable instances, especially on a dedicated authority. Resolve
    // its configured name because the native TC_BuildGun constant is not exported
    // by every supported target's import library.
    Root->GetWorld()->LineTraceMultiByChannel(Hits, Pose.GetLocation() + Axis * Span,
        Pose.GetLocation() - Axis * Span, BuildChannel, Query);
    if (Hits.Num() > 64) return Fail(TEXT("floor_hole_hit_capture_exceeds_bound"));
    FString LastFailure = TEXT("native_floor_hole_no_eligible_surface");
    for (auto Hit : Hits)
    {
        SFNativeHit::ResolveForPlacement(Hit);
        auto* Spawned = AFGHologram::SpawnHologramFromRecipe(Child->GetRecipe(), Root->GetOwner(), Pose.GetLocation(), Player);
        TStrongObjectPtr<AFGHologram> KeepAlive(Spawned);
        ON_SCOPE_EXIT { if (IsValid(Spawned)) Spawned->Destroy(); };
        auto* Native = Cast<AFGPassthroughHologram>(Spawned);
        if (!Native || Native->GetBuildClass() != Child->GetBuildClass() || !Native->IsValidHitResult(Hit)) continue;
        Native->Tags.AddUnique(TEXT("SF_NativeFloorHoleValidator"));
        Native->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
        Native->SetActorTransform(Pose);
        Native->ResetConstructDisqualifiers();
        Native->PreHologramPlacement(Hit, false);
        if (!Native->TrySnapToActor(Hit)) Native->UpdateHologramPlacement(Hit);
        Native->PostHologramPlacement(Hit, false);
        Native->SetActorHiddenInGame(false);
        Native->ValidatePlacementAndCost(Player->GetInventory());
        auto* Thickness = FindFProperty<FFloatProperty>(AFGPassthroughHologram::StaticClass(), TEXT("mSnappedBuildingThickness"));
        auto* Foundations = FindFProperty<FArrayProperty>(AFGPassthroughHologram::StaticClass(), TEXT("mSnappedFoundations"));
        auto* FoundationObject = Foundations ? CastField<FObjectPropertyBase>(Foundations->Inner) : nullptr;
        if (!Thickness || !FoundationObject) { LastFailure = TEXT("native_floor_hole_context_unavailable"); continue; }
        if (Native->IsUpgrade() || !Native->CanConstruct())
        {
            TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
            Native->GetConstructDisqualifiers(Disqualifiers);
            FString Names;
            for (const auto& Disqualifier : Disqualifiers)
            {
                if (!Names.IsEmpty()) Names += TEXT(",");
                Names += GetNameSafe(Disqualifier.Get());
            }
            LastFailure = FString::Printf(TEXT("native_floor_hole_placement_rejected: hit=%s upgrade=%d disqualifiers=%s"),
                *GetNameSafe(Hit.GetActor() ? Hit.GetActor()->GetClass() : nullptr), Native->IsUpgrade() ? 1 : 0, *Names);
            continue;
        }
        if (!Native->GetActorTransform().Equals(Pose, 0.1f)) { LastFailure = TEXT("native_floor_hole_pose_mismatch"); continue; }
        if (!FMath::IsNearlyEqual(Thickness->GetPropertyValue_InContainer(Native), ExpectedThickness, 0.01f))
        { LastFailure = TEXT("native_floor_hole_thickness_mismatch"); continue; }
        FScriptArrayHelper Mounted(Foundations, Foundations->ContainerPtrToValuePtr<void>(Native));
        if (Mounted.Num() == 0) { LastFailure = TEXT("native_floor_hole_missing_mount"); continue; }
        bool bLive = true;
        for (int32 Index = 0; Index < Mounted.Num(); ++Index)
        {
            auto* Foundation = Cast<AActor>(FoundationObject->GetObjectPropertyValue(Mounted.GetRawPtr(Index)));
            bLive &= IsValid(Foundation) && Foundation->GetWorld() == Root->GetWorld() && !Foundation->IsActorBeingDestroyed();
        }
        if (!bLive) { LastFailure = TEXT("native_floor_hole_stale_mount"); continue; }
        // Copy only validated placement context. No existing hole/lift, native
        // registration, cargo or payment is changed by this preview admission.
        Foundations->CopyCompleteValue(Foundations->ContainerPtrToValuePtr<void>(Child), Foundations->ContainerPtrToValuePtr<void>(Native));
        if (auto* Permission = FindFProperty<FBoolProperty>(AFGHologram::StaticClass(), TEXT("mCanBePlacedInBlueprintDesigner")))
            Permission->SetPropertyValue_InContainer(Child, Permission->GetPropertyValue_InContainer(Native));
        Child->SetConstructionInstigator(Player);
        TMap<TSubclassOf<UFGItemDescriptor>, int32> ExpectedCost, ActualCost;
        for (const auto& Entry : Native->GetCost(false)) ExpectedCost.FindOrAdd(Entry.ItemClass) += Entry.Amount;
        for (const auto& Entry : Child->GetCost(false)) ActualCost.FindOrAdd(Entry.ItemClass) += Entry.Amount;
        if (ExpectedCost.Num() != ActualCost.Num()) { LastFailure = TEXT("native_floor_hole_cost_mismatch"); continue; }
        bool bSameCost = true;
        for (const auto& Entry : ExpectedCost) bSameCost &= ActualCost.FindRef(Entry.Key) == Entry.Value;
        if (!bSameCost) { LastFailure = TEXT("native_floor_hole_cost_mismatch"); continue; }
        return true;
    }
    return Fail(*LastFailure);
}
