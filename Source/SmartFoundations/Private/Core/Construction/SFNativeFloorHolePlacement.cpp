// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFNativeFloorHolePlacement.h"
#include "SmartFoundations.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Equipment/FGBuildGunBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Hologram/FGPassthroughHologram.h"
#include "FGCharacterPlayer.h"
#include "FGConstructDisqualifier.h"
#include "Core/SFNativeHit.h"
#include "Misc/ScopeExit.h"

namespace
{
    constexpr double LiftHoleRiseLimit = 400.0;
    constexpr double LiftHoleLateralLimit = 100.0;
    constexpr double LiftHolePoseTolerance = 0.1;
}

bool FSFNativeFloorHolePlacement::ReplayLiftHoleNudge(AFGPassthroughHologram* Native, const FTransform& Pose, FString& Detail)
{
    auto Fail = [&Detail](const TCHAR* Why) { Detail = Why; return false; };
    const FQuat Frame = Native->GetActorQuat();
    if (!Frame.Equals(Pose.GetRotation(), 0.001) || !Native->CanLockHologram()) return Fail(TEXT("cannot_lock_at_requested_rotation"));
    const FVector Offset = Frame.UnrotateVector(Pose.GetLocation() - Native->GetActorLocation());
    if (Offset.ContainsNaN() || FMath::Abs(Offset.X) > LiftHoleLateralLimit + LiftHolePoseTolerance
        || FMath::Abs(Offset.Y) > LiftHoleLateralLimit + LiftHolePoseTolerance
        || Offset.Z < -LiftHolePoseTolerance || Offset.Z > LiftHoleRiseLimit + LiftHolePoseTolerance)
        return Fail(TEXT("outside_replay_range"));

    // The temporary cell has no player lock marker. Set the local native
    // lock context and let native nudge operations accept or reject each step.
    Native->mHologramLockLocation = Native->GetActorLocation();
    Native->mHologramIsLocked = true;
    if (!Native->CanNudgeHologram()) return Fail(TEXT("cannot_nudge"));
    // Use the ordinary full upward steps first, then the native half-step
    // control for the lateral correction. Each axis still uses native bounds.
    for (const int32 Axis : {2, 0, 1})
    {
        Native->SetSnapToGuideLines(Axis != 2);
        const double Step = Native->GetNudgeDistance();
        if (!FMath::IsFinite(Step) || Step <= 0 || FMath::Abs(Offset[Axis] / Step) > 16.0)
            return Fail(TEXT("invalid_step_distance"));
        const int32 Count = FMath::RoundToInt(Offset[Axis] / Step);
        if (!FMath::IsNearlyEqual(Count * Step, Offset[Axis], LiftHolePoseTolerance))
        {
            Detail = FString::Printf(TEXT("axis=%d offset=%.3f step=%.3f"), Axis, Offset[Axis], Step);
            return false;
        }
        FVector Direction = FVector::ZeroVector;
        Direction[Axis] = FMath::Sign(Count);
        for (int32 Index = 0; Index < FMath::Abs(Count); ++Index)
        {
            const auto Verdict = Native->NudgeTowardsWorldDirection(Frame.RotateVector(Direction));
            if (Verdict != ENudgeFailReason::NFR_Success)
            {
                Detail = FString::Printf(TEXT("axis=%d step=%d verdict=%d"), Axis, Index, static_cast<int32>(Verdict));
                return false;
            }
            Native->SetHologramNudgeLocation();
        }
    }
    return Native->GetActorTransform().Equals(Pose, LiftHolePoseTolerance) || Fail(TEXT("final_pose_mismatch"));
}

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
    TArray<AFGHologram*> PendingHolograms{Root};
    TSet<AFGHologram*> Seen;
    while (!PendingHolograms.IsEmpty())
    {
        auto* Holo = PendingHolograms.Pop(EAllowShrinking::No);
        if (!IsValid(Holo) || Seen.Contains(Holo)) continue;
        Seen.Add(Holo); Query.AddIgnoredActor(Holo); PendingHolograms.Append(Holo->GetHologramChildren());
    }
    const FVector Axis = Pose.GetUnitAxis(EAxis::Z);
    // A spaced grid can be off the foundation's default snap lattice, while
    // an Extend companion can also have been nudged up from a lower floor.
    const bool bReplayLiftNudge = Axis.Z > 0.999 && !Root->GetBlueprintDesigner()
        && Child->GetBuildClass()->GetName() == TEXT("Build_FoundationPassthrough_Lift_C");
    const bool bAllowLowerMount = bReplayLiftNudge && Root->GetBuildClass() != Child->GetBuildClass();
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
    if (Hits.IsEmpty())
    {
        // Adjacent foundation collision meshes can leave a sub-centimetre seam.
        // Native placement still snaps a nearby real hit to the requested hole.
        // Probe that surface without moving the requested pose or synthesizing a
        // hit; the native pose, mount, thickness and cost checks below still apply.
        const FVector X = Pose.GetUnitAxis(EAxis::X);
        const FVector Y = Pose.GetUnitAxis(EAxis::Y);
        for (const FVector Offset : {X, -X, Y, -Y, X + Y, X - Y, -X + Y, -X - Y})
        {
            TArray<FHitResult> NearbyHits;
            const FVector Center = Pose.GetLocation() + Offset;
            Root->GetWorld()->LineTraceMultiByChannel(NearbyHits, Center + Axis * Span,
                Center - Axis * Span, BuildChannel, Query);
            Hits.Append(NearbyHits);
            if (Hits.Num() > 64) return Fail(TEXT("floor_hole_hit_capture_exceeds_bound"));
        }
    }
    if (bAllowLowerMount)
    {
        // A floor hole can be snapped to a real floor and then nudged upward.
        // Only examine nearby lower mounts within this replay's height budget.
        const FVector X = Pose.GetUnitAxis(EAxis::X);
        const FVector Y = Pose.GetUnitAxis(EAxis::Y);
        // A seam may have a valid native snap point on either side of the
        // exact centre. Sample both sides within one native half-step.
        for (const FVector Offset : {FVector::ZeroVector, X, -X, Y, -Y, X + Y, X - Y, -X + Y, -X - Y,
            25.0 * X, -25.0 * X, 25.0 * Y, -25.0 * Y})
        {
            const FVector Center = Pose.GetLocation() + Offset;
            TArray<FHitResult> Below;
            Root->GetWorld()->LineTraceMultiByChannel(Below, Center - Axis * Span,
                Center - Axis * (LiftHoleRiseLimit + ExpectedThickness * 0.5), BuildChannel, Query);
            for (auto& Hit : Below)
            {
                if (!Hits.ContainsByPredicate([&Hit](const FHitResult& Existing) {
                    return Existing.GetActor() == Hit.GetActor() && Existing.Item == Hit.Item
                        && Existing.ImpactPoint.Equals(Hit.ImpactPoint, 1.5);
                })) Hits.Add(MoveTemp(Hit));
            }
            if (Hits.Num() > 64) return Fail(TEXT("floor_hole_hit_capture_exceeds_bound"));
        }
    }
    if (Hits.Num() > 64) return Fail(TEXT("floor_hole_hit_capture_exceeds_bound"));
    TArray<FString> Failures;
    for (auto Hit : Hits)
    {
        FString LastFailure;
        ON_SCOPE_EXIT {
            if (!LastFailure.IsEmpty() && Failures.Num() < 4)
                Failures.AddUnique(MoveTemp(LastFailure));
        };
        SFNativeHit::ResolveForPlacement(Hit);
        auto* Spawned = AFGHologram::SpawnHologramFromRecipe(Child->GetRecipe(), Root->GetOwner(), Pose.GetLocation(), Player);
        TStrongObjectPtr<AFGHologram> KeepAlive(Spawned);
        ON_SCOPE_EXIT { if (IsValid(Spawned)) Spawned->Destroy(); };
        auto* Native = Cast<AFGPassthroughHologram>(Spawned);
        if (!Native || Native->GetBuildClass() != Child->GetBuildClass() || !Native->IsValidHitResult(Hit)) continue;
        Native->Tags.AddUnique(TEXT("SF_NativeFloorHoleValidator"));
        Native->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
        Native->SetActorTransform(Pose);
        auto ReplayPlacement = [&]()
        {
            Native->ResetConstructDisqualifiers();
            Native->PreHologramPlacement(Hit, false);
            if (!Native->TrySnapToActor(Hit)) Native->UpdateHologramPlacement(Hit);
            Native->PostHologramPlacement(Hit, false);
        };
        ReplayPlacement();
        // Native snapping derives orientation from the foundation and scroll state,
        // overwriting SetActorTransform. Replay legal rotations before comparing the
        // captured pose; a fresh zero-scroll hologram is not the only valid placement.
        const int32 RotationStep = Spawned->GetRotationStep();
        const int32 RotationCount = RotationStep > 0 ? FMath::CeilToInt(360.0 / RotationStep) : 0;
        if (RotationCount > 0 && RotationCount <= 24)
        {
            for (int32 Turn = 1; Turn < RotationCount
                && !Native->GetActorQuat().Equals(Pose.GetRotation(), 0.1f); ++Turn)
            {
                Native->Scroll(1);
                ReplayPlacement();
            }
        }
        Native->SetActorHiddenInGame(false);
        const bool bNudged = bReplayLiftNudge && !Native->GetActorTransform().Equals(Pose, LiftHolePoseTolerance);
        FString NudgeFailure;
        if (bNudged && !bAllowLowerMount
            && !FMath::IsNearlyEqual(Pose.GetLocation().Z, Native->GetActorLocation().Z, LiftHolePoseTolerance))
        {
            LastFailure = TEXT("native_floor_hole_grid_requires_same_level_mount");
            continue;
        }
        if (bNudged && !ReplayLiftHoleNudge(Native, Pose, NudgeFailure))
        {
            LastFailure = FString::Printf(TEXT("native_floor_hole_nudge_not_reproducible: %s expected={%s} actual={%s} hit=%s"),
                *NudgeFailure, *Pose.ToHumanReadableString(), *Native->GetActorTransform().ToHumanReadableString(),
                *GetNameSafe(Hit.GetActor()));
            continue;
        }
        const FVector Nudge = Native->GetNudgeOffset();
        Native->ValidatePlacementAndCost(Player->GetInventory());
        if (bNudged)
        {
            // Passthrough validation can resnap to the supporting foundation.
            // Restore only the offset that native nudge accepted, then check
            // clearance again at the final pose without clearing any verdict.
            if (!Native->GetNudgeOffset().Equals(Nudge, LiftHolePoseTolerance))
            {
                LastFailure = TEXT("native_floor_hole_nudge_changed_during_validation");
                continue;
            }
            Native->SetHologramNudgeLocation();
            if (Native->GetActorTransform().Equals(Pose, LiftHolePoseTolerance))
                static_cast<AFGHologram*>(Native)->CheckClearance();
        }
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
        if (!Native->GetActorTransform().Equals(Pose, 0.1f))
        {
            LastFailure = FString::Printf(TEXT("native_floor_hole_pose_mismatch: expected={%s} actual={%s} hit=%s"),
                *Pose.ToHumanReadableString(), *Native->GetActorTransform().ToHumanReadableString(),
                *GetNameSafe(Hit.GetActor()));
            continue;
        }
        if (!FMath::IsNearlyEqual(Thickness->GetPropertyValue_InContainer(Native), ExpectedThickness, 0.01f))
        {
            LastFailure = FString::Printf(TEXT("native_floor_hole_thickness_mismatch: expected=%.3f actual=%.3f hit=%s"),
                ExpectedThickness, Thickness->GetPropertyValue_InContainer(Native), *GetNameSafe(Hit.GetActor()));
            continue;
        }
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
    Reason = FString::Printf(TEXT("%s; child=%s class=%s pose={%s} thickness=%.3f traceHits=%d"),
        Failures.IsEmpty() ? TEXT("native_floor_hole_no_eligible_surface") : *FString::Join(Failures, TEXT("; ")),
        *GetNameSafe(Child), *GetNameSafe(Child->GetBuildClass()),
        *Pose.ToHumanReadableString(), ExpectedThickness, Hits.Num());
    return false;
}
