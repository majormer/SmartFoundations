// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendLiftAttachmentReceipt.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Hologram/FGPassthroughHologram.h"
#include "FGCharacterPlayer.h"
#include "Core/SFNativeHit.h"
#include "Misc/ScopeExit.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGFactoryConnectionComponent.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Features/Extend/SFExtendWiringPlan.h"
#include "Core/Construction/SFNativeFloorHolePlacement.h"

bool FSFExtendLiftAttachmentReceipt::ValidatePlacement(AFGHologram* Root, AFGHologram* Child,
    const FSFCloneHologram& Plan, FString& Reason)
{
    if (!IsValid(Child) || !Child->GetBuildClass() || Child->GetBuildClass()->GetName() != Plan.BuildClass)
    { Reason = TEXT("floor_hole_input_mismatch"); return false; }
    return FSFNativeFloorHolePlacement::Validate(Root, Child, Plan.Transform.ToFTransform(), Plan.Thickness, Reason);
}

bool FSFExtendLiftAttachmentReceipt::Verify(const FSFCloneTopology& Plan,
    const TMap<FString, FObjectKey>& Actors, const TSet<FObjectKey>& ActualOutputs,
    USFConstructionOperationCoordinator& Owner, const FSFConstructionOperationKey& Key, FString& Reason)
{
    auto Fail = [&](const TCHAR* Why) { Reason = Why; return false; };
    if (!Owner.IsInObservationPhase() || !FSFExtendWiringPlan::HasExactLiftHoleRoles(Plan))
        return Fail(TEXT("invalid_lift_attachment_plan_or_phase"));
    auto Resolve = [&](const FString& Role) -> AActor*
    {
        const auto* Identity = Actors.Find(Role);
        auto* Actor = Identity ? Cast<AActor>(Identity->ResolveObjectPtr()) : nullptr;
        return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->HasAuthority()
            && Actor->GetWorld() == Owner.GetWorld() && ActualOutputs.Contains(FObjectKey(Actor)) ? Actor : nullptr;
    };
    for (const auto& Child : Plan.ChildHolograms)
    {
        if (Child.bPreviewOnly || !Child.bConstructible) continue;
        if (Child.bHasLiftData)
        {
            auto* Lift = Cast<AFGBuildableConveyorLift>(Resolve(Child.HologramId));
            if (!Lift || Lift->GetClass()->GetName() != Child.BuildClass
                || !Lift->GetActorTransform().Equals(Child.Transform.ToFTransform(), 0.1f)
                || !Lift->GetTopTransform().Equals(Child.LiftData.TopTransform.ToFTransform(), 0.1f)
                || Lift->GetIsReversed() != Child.LiftData.bIsReversed
                || !Owner.AdoptActor(Key, Lift)) return Fail(TEXT("built_lift_geometry_or_flow_mismatch"));
            const auto Slots = Lift->GetSnappedPassthroughs();
            if (Slots.Num() != 2) return Fail(TEXT("built_lift_slot_shape_mismatch"));
            for (int32 Index = 0; Index < 2; ++Index)
            {
                const FString Role = Child.LiftData.PassthroughCloneIds.IsValidIndex(Index)
                    ? Child.LiftData.PassthroughCloneIds[Index] : FString();
                auto* Hole = Role.IsEmpty() ? nullptr : Cast<AFGBuildablePassthrough>(Resolve(Role));
                if ((!Role.IsEmpty() && !Hole) || Slots[Index] != Hole) return Fail(TEXT("built_lift_exact_hole_slot_mismatch"));
            }
            Lifts.Add(FObjectKey(Lift));
        }
        if (Child.Role != TEXT("passthrough") || Child.BuildClass != TEXT("Build_FoundationPassthrough_Lift_C")) continue;
        auto* Hole = Cast<AFGBuildablePassthrough>(Resolve(Child.HologramId));
        if (!Hole || Hole->GetClass()->GetName() != Child.BuildClass || !Child.bHasPassthroughLinks
            || !Hole->GetActorTransform().Equals(Child.Transform.ToFTransform(), 0.1f)
            || !FMath::IsNearlyEqual(Hole->GetSnappedBuildingThickness(), Child.Thickness, 0.01f)
            || !Owner.AdoptActor(Key, Hole)) return Fail(TEXT("built_floor_hole_geometry_mismatch"));
        for (const bool bTop : {false, true})
        {
            const auto& Face = bTop ? Child.PassthroughTop : Child.PassthroughBottom;
            UFGFactoryConnectionComponent* Expected = nullptr;
            if (!Face.Target.IsEmpty())
            {
                auto* Lift = Cast<AFGBuildableConveyorLift>(Resolve(Face.Target));
                if (!Lift) return Fail(TEXT("floor_hole_face_target_missing"));
                for (auto* Port : {Lift->GetConnection0(), Lift->GetConnection1()})
                    if (IsValid(Port) && Port->GetName() == Face.Connector)
                    { if (Expected) return Fail(TEXT("ambiguous_floor_hole_face_port")); Expected = Port; }
                if (!Expected) return Fail(TEXT("floor_hole_face_port_missing"));
                // A face pointer alone is insufficient: its owner must also name this hole.
                if (!Lift->GetSnappedPassthroughs().Contains(Hole)) return Fail(TEXT("floor_hole_reverse_reference_missing"));
            }
            auto* Actual = bTop ? Hole->GetTopSnappedConnection<UFGConnectionComponent>()
                : Hole->GetBottomSnappedConnection<UFGConnectionComponent>();
            if (Actual != Expected) return Fail(TEXT("floor_hole_exact_face_binding_mismatch"));
        }
        Holes.Add(FObjectKey(Hole));
    }
    return true;
}
