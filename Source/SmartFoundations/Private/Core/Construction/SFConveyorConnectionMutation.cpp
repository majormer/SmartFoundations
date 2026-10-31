// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFConveyorConnectionMutation.h"
#include "SmartFoundations.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/PlayerController.h"
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"

bool FSFConveyorConnectionMutation::Apply(USFConstructionOperationCoordinator& Coordinator,
    const FSFConstructionOperationKey& Key,
    const TArray<TWeakObjectPtr<AFGBuildableConveyorBase>>& Seeds,
    const TArray<FSFConveyorConnectionPair>& Edges, FSFConveyorCohort& After,
    FSFConveyorCohort::FCargoSnapshot& AfterCargo, bool& bEffectsStarted, FString& Reason)
{
    auto* Owner = &Coordinator;
    bEffectsStarted = false;
    auto Reject = [&](const TCHAR* Why) { Reason = Why; return false; };
    if (!Owner->IsInMutationPhase()) return Reject(TEXT("no_native_mutation_owner"));
    TMap<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*> Intended;
    TArray<TStrongObjectPtr<UFGFactoryConnectionComponent>> KeepPorts;
    for (const auto& Edge : Edges)
    {
        auto* A = Edge.A; auto* B = Edge.B;
        if (!IsValid(A) || !IsValid(B) || A == B || !IsValid(A->GetOwner()) || !IsValid(B->GetOwner())
            || A->GetOwner() == B->GetOwner() || A->GetWorld() != Owner->GetWorld() || B->GetWorld() != Owner->GetWorld()
            || A->GetOwner()->IsActorBeingDestroyed() || B->GetOwner()->IsActorBeingDestroyed()
            || !A->GetOwner()->HasAuthority() || !B->GetOwner()->HasAuthority()) return Reject(TEXT("invalid_exact_edge"));
        const auto ADirection = A->GetDirection();
        const auto BDirection = B->GetDirection();
        if (!((ADirection == EFactoryConnectionDirection::FCD_INPUT && BDirection == EFactoryConnectionDirection::FCD_OUTPUT)
            || (ADirection == EFactoryConnectionDirection::FCD_OUTPUT && BDirection == EFactoryConnectionDirection::FCD_INPUT)))
            return Reject(TEXT("unresolved_or_incompatible_native_flow"));
        if ((Intended.Contains(A) && Intended[A] != B) || (Intended.Contains(B) && Intended[B] != A)) return Reject(TEXT("conflicting_exact_edges"));
        const bool Exact = A->GetConnection() == B && B->GetConnection() == A && A->IsConnected() && B->IsConnected();
        if (!Exact && (A->IsConnected() || B->IsConnected() || A->GetConnection() || B->GetConnection() || !A->CanConnectTo(B))) return Reject(TEXT("occupied_or_incompatible_edge"));
        if (!Owner->AdoptActor(Key, A->GetOwner()) || !Owner->AdoptActor(Key, B->GetOwner())) return Reject(TEXT("endpoint_owner_conflict"));
        Intended.Add(A, B); Intended.Add(B, A);
        KeepPorts.Emplace(A); KeepPorts.Emplace(B);
    }
    FSFConveyorCohort Before;
    FSFConveyorCohort::FCargoSnapshot Cargo;
    const auto Status = FSFConveyorCohortValidator::Capture(*Owner, Seeds, Before, Reason, true);
    if (Status != ESFConveyorCohortStatus::CoherentStructure && Status != ESFConveyorCohortStatus::NativePending)
        return Reject(TEXT("affected_native_cohort_not_coherent"));
    for (const auto& Edge : Edges)
        for (auto* Actor : {Edge.A->GetOwner(), Edge.B->GetOwner()})
        {
            if (Before.Designer.IsSet())
            {
                if (!FSFConveyorCohortValidator::IsDesignerMember(Actor, Before.Designer.GetValue()))
                    return Reject(TEXT("edge_crosses_designer_ownership"));
            }
            else if (auto* Buildable = Cast<AFGBuildable>(Actor); Buildable && Buildable->GetBlueprintDesigner())
                return Reject(TEXT("world_edge_targets_designer_template"));
        }
    TSet<FObjectKey> OwnedMembers;
    for (const auto& Pair : Before.Members) OwnedMembers.Add(Pair.Key);
    for (const auto& Actor : Before.AffectedActors)
        if (!Owner->AdoptActor(Key, Actor.Get())) return Reject(TEXT("affected_cohort_conflict"));
    if (FSFConveyorCohortValidator::SnapshotCargo(*Owner, Before, Cargo, Reason) != ESFConveyorCohortStatus::CoherentStructure)
        return Reject(TEXT("affected_cargo_not_supported"));
    // Preserve every pre-existing reciprocal edge in the affected ownership
    // closure. Only the admitted exact pairs may change; a post-only snapshot
    // would silently accept damage already done during native detach/register.
    struct FPortBefore
    {
        TWeakObjectPtr<UFGFactoryConnectionComponent> Port;
        TWeakObjectPtr<UFGFactoryConnectionComponent> Peer;
    };
    TArray<FPortBefore> GraphBefore;
    TArray<TStrongObjectPtr<AActor>> KeepActors;
    TSet<UFGFactoryConnectionComponent*> Seen;
    auto CapturePorts = [&](AActor* Actor)
    {
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return false;
        KeepActors.Emplace(Actor);
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        for (auto* Port : Ports)
        {
            if (!IsValid(Port) || Seen.Contains(Port)) continue;
            auto* Peer = Port->GetConnection();
            if (Port->IsConnected() != (Peer != nullptr) || (Peer && (!IsValid(Peer) || Peer->GetConnection() != Port))) return false;
            if (Peer && Before.Designer.IsSet()
                && !FSFConveyorCohortValidator::IsDesignerMember(Peer->GetOwner(), Before.Designer.GetValue())) return false;
            Seen.Add(Port); KeepPorts.Emplace(Port); GraphBefore.Add({Port, Peer});
        }
        return true;
    };
    for (const auto& Actor : Before.AffectedActors)
        if (!CapturePorts(Actor.Get())) return Reject(TEXT("invalid_existing_graph"));
    for (const auto& Edge : Edges)
        if (!CapturePorts(Edge.A->GetOwner()) || !CapturePorts(Edge.B->GetOwner())) return Reject(TEXT("invalid_endpoint_graph"));
    TSet<int32> ChangedBuckets;
    for (const auto& Edge : Edges)
    {
        if (Edge.A->GetConnection() == Edge.B) continue;
        for (auto* EndpointActor : {Edge.A->GetOwner(), Edge.B->GetOwner()})
        {
            if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(EndpointActor))
            {
                const auto* Captured = Before.Members.Find(FObjectKey(Conveyor));
                if (!Captured) return Reject(TEXT("changed_endpoint_not_in_captured_cohort"));
                ChangedBuckets.Add(Captured->Bucket);
            }
        }
    }
    const bool bChanges = !ChangedBuckets.IsEmpty();
    auto* Native = AFGBuildableSubsystem::Get(Owner->GetWorld());
    TArray<AFGBuildableConveyorBase*> Members;
    for (const auto& Pair : Before.Members)
    {
        // Freeze complete changed-group identities before bucket rearrangement.
        // Other captured groups are conservation dependencies, not mutation targets.
        // Designer members never enter native factory registration. Their
        // local graph and cargo remain protected by the same transaction.
        if (Before.Designer.IsSet() || !ChangedBuckets.Contains(Pair.Value.Bucket)) continue;
        auto* Member = Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr());
        if (!Native || !FSFConveyorCohortValidator::CanRetire(*Owner, Member, OwnedMembers, Reason)) return Reject(TEXT("native_detach_preconditions"));
        if (!Member->ShouldRegisterToFactoryTickGroup()) return Reject(TEXT("registered_conveyor_native_tick_disabled"));
        Members.Add(Member);
    }
    TArray<TStrongObjectPtr<AFGBuildableConveyorBase>> KeepMembers;
    for (auto* Member : Members) KeepMembers.Emplace(Member);
    if (bChanges)
    {
        bEffectsStarted = true;
        // Preserve the native distinction between materialized chains and
        // local/shared/loop groups; RemoveConveyor owns each native detach.
        // Remove through the complete native transaction, re-resolving after
        // every bucket rearrangement. No saved group/chain is ever reinstalled.
        for (auto* Member : Members)
        {
            if (!FSFConveyorCohortValidator::CanRetire(*Owner, Member, OwnedMembers, Reason)) return Reject(TEXT("native_detach_changed"));
            // The outer native path invalidates mConveyorBucketGroups as well
            // as retiring membership. Direct RemoveConveyor leaves cached
            // worker pointers to freed groups (CL513885 server: 0x180694370).
            Native->RemoveFromTickGroup(Member);
            if (!IsValid(Member) || Member->IsActorBeingDestroyed() || Member->GetConveyorBucketID() != INDEX_NONE || Member->GetConveyorChainActor()) return Reject(TEXT("native_detach_incomplete"));
        }
        for (const auto& Edge : Edges)
        {
            if (!IsValid(Edge.A) || !IsValid(Edge.B) || !IsValid(Edge.A->GetOwner()) || !IsValid(Edge.B->GetOwner())
                || Edge.A->GetOwner()->IsActorBeingDestroyed() || Edge.B->GetOwner()->IsActorBeingDestroyed()) return Reject(TEXT("endpoint_retired_during_detach"));
            if (Edge.A->GetConnection() != Edge.B)
            {
                const auto ADirection = Edge.A->GetDirection();
                const auto BDirection = Edge.B->GetDirection();
                if (Edge.A->IsConnected() || Edge.B->IsConnected() || !Edge.A->CanConnectTo(Edge.B)
                    || !((ADirection == EFactoryConnectionDirection::FCD_INPUT && BDirection == EFactoryConnectionDirection::FCD_OUTPUT)
                        || (ADirection == EFactoryConnectionDirection::FCD_OUTPUT && BDirection == EFactoryConnectionDirection::FCD_INPUT)))
                    return Reject(TEXT("edge_changed_during_detach"));
                Edge.A->SetConnection(Edge.B);
            }
        }
        for (auto* Member : Members)
        {
            if (!IsValid(Member) || Member->IsActorBeingDestroyed() || Member->GetConveyorBucketID() != INDEX_NONE || Member->GetConveyorChainActor()) return Reject(TEXT("duplicate_native_registration"));
            Native->AddToTickGroup(Member);
        }
    }
    for (const auto& Entry : GraphBefore)
    {
        auto* Port = Entry.Port.Get();
        auto* Expected = Port && Intended.Contains(Port) ? Intended[Port] : Entry.Peer.Get();
        if (!Port || (!Intended.Contains(Port) && !Entry.Peer.IsExplicitlyNull() && !Expected)
            || Port->GetConnection() != Expected || Port->IsConnected() != (Expected != nullptr)
            || (Expected && (!IsValid(Expected) || Expected->GetConnection() != Port)))
            return Reject(TEXT("existing_or_intended_graph_changed"));
    }
    // Capture before factory workers can advance items. Compare full native item-state
    // owner/order/local-offset ledgers, including untouched cohort survivors.
    const auto AfterStatus = FSFConveyorCohortValidator::Capture(*Owner, Seeds, After, Reason, true);
    if (AfterStatus != ESFConveyorCohortStatus::CoherentStructure && AfterStatus != ESFConveyorCohortStatus::NativePending)
    {
        Reason = FString::Printf(TEXT("native_registration_capture_failed (status=%d): %s"), static_cast<int32>(AfterStatus), *Reason);
        return false;
    }
    if (Before.Designer != After.Designer) return Reject(TEXT("designer_owner_changed_during_wiring"));
    const auto CargoStatus = FSFConveyorCohortValidator::SnapshotCargo(*Owner, After, AfterCargo, Reason);
    if (CargoStatus != ESFConveyorCohortStatus::CoherentStructure)
    {
        Reason = FString::Printf(TEXT("native_registration_cargo_capture_failed (status=%d): %s"), static_cast<int32>(CargoStatus), *Reason);
        return false;
    }
    auto SameCargo = [](const auto& Expected, const auto& Actual)
    {
        if (Expected.Num() != Actual.Num()) return false;
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
            if (!Expected[Index].SameValue(Actual[Index]) || !FMath::IsNearlyEqual(Expected[Index].Offset, Actual[Index].Offset, 0.01f)) return false;
        return true;
    };
    if (Cargo.Num() != AfterCargo.Num()) return Reject(TEXT("cargo_owner_set_changed"));
    for (const auto& Pair : Cargo)
    {
        const auto* Actual = AfterCargo.Find(Pair.Key);
        if (!Actual || !SameCargo(Pair.Value, *Actual)) return Reject(TEXT("cargo_changed_during_wiring"));
    }
    if (!FSFConveyorCohortValidator::VerifyBoundaryInventories(*Owner, Before, Reason)) return false;
    return true;
}
