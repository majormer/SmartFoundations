// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "GameFramework/PlayerController.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "FGFactoryConnectionComponent.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "Core/Construction/SFConveyorConnectionMutation.h"

namespace
{
const FName LinkEffect(TEXT("ExactMaterializedConveyorLinks"));
struct FLinkWork
{
    TWeakObjectPtr<USFConstructionOperationCoordinator> Owner;
    FSFConstructionOperationKey Key;
    FGuid Id;
    TArray<TPair<TWeakObjectPtr<UFGFactoryConnectionComponent>, TWeakObjectPtr<UFGFactoryConnectionComponent>>> Links;
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Seeds;
    TArray<TPair<TWeakObjectPtr<UFGFactoryConnectionComponent>, TWeakObjectPtr<UFGFactoryConnectionComponent>>> Expected;
    int32 PendingPhases = 0;
    int32 ObservedPhases = 0;
    bool bApplied = false;
    bool bStopped = false;
    TOptional<FObjectKey> Designer;
};

void StopLinks(const TSharedPtr<FLinkWork>& Work, FSFConstructionOperationRecord& Record, bool bStarted, const FString& Why)
{
    Work->bStopped = true;
    const FGuid Generation = Work->Owner->GetWorldGeneration();
    Record.ObserveEffect(Generation, LinkEffect, bStarted ? ESFConstructionEffectOutcome::Uncertain : ESFConstructionEffectOutcome::NoEffect);
    Record.RequestCancellation(Generation);
    if (bStarted) Record.RequireReconciliation(Generation);
    Record.SettleCancellation(Generation);
    UE_LOG(LogSmartFoundations, Warning, TEXT("Conveyor links %s unresolved: %s; topology effects started=%d. Native parent materialization/payment is separate."),
        *Work->Id.ToString(), *Why, bStarted);
}

void ApplyLinks(const TSharedPtr<FLinkWork>& Work, FSFConstructionOperationRecord& Record)
{
    auto* Owner = Work->Owner.Get();
    if (!Owner || !Owner->IsInMutationPhase() || Work->bStopped || Work->bApplied) return;
    if (Record.IsCancellationRequested()) { StopLinks(Work, Record, false, TEXT("cancelled_before_links")); return; }
    TArray<FSFConveyorConnectionPair> Edges;
    for (const auto& Pair : Work->Links)
    {
        if (!Pair.Key.IsValid() || !Pair.Value.IsValid()) { StopLinks(Work, Record, false, TEXT("endpoint_expired")); return; }
        Edges.Add({Pair.Key.Get(), Pair.Value.Get()});
    }
    FSFConveyorCohort Before;
    FString Reason;
    const auto Status = FSFConveyorCohortValidator::Capture(*Owner, Work->Seeds, Before, Reason, true);
    for (const auto& Actor : Before.AffectedActors)
        if (!Owner->AdoptActor(Work->Key, Actor.Get())) { StopLinks(Work, Record, false, TEXT("cohort_conflict")); return; }
    if (Status == ESFConveyorCohortStatus::NativePending && ++Work->PendingPhases <= 120)
    {
        if (!Owner->QueueContinuation(Work->Key, LinkEffect, [Work](FSFConstructionOperationRecord& Next) { ApplyLinks(Work, Next); }))
            StopLinks(Work, Record, false, TEXT("native_wait_not_owned"));
        return;
    }
    if (Status != ESFConveyorCohortStatus::CoherentStructure) { StopLinks(Work, Record, false, Reason); return; }
    FSFConveyorCohort After;
    FSFConveyorCohort::FCargoSnapshot Cargo;
    bool bStarted = false;
    if (!FSFConveyorConnectionMutation::Apply(*Owner, Work->Key, Work->Seeds, Edges, After, Cargo, bStarted, Reason))
    { StopLinks(Work, Record, bStarted, Reason); return; }
    for (const auto& Actor : After.AffectedActors)
    {
        if (!Actor.IsValid() || !Owner->AdoptActor(Work->Key, Actor.Get())) { StopLinks(Work, Record, bStarted, TEXT("result_cohort_conflict")); return; }
        TArray<UFGFactoryConnectionComponent*> Ports;
        Actor->GetComponents(Ports);
        for (auto* Port : Ports)
        {
            auto* Peer = IsValid(Port) ? Port->GetConnection() : nullptr;
            if (!IsValid(Port) || Port->IsConnected() != (Peer != nullptr)
                || (Peer && (!IsValid(Peer) || Peer->GetConnection() != Port)))
            { StopLinks(Work, Record, bStarted, TEXT("nonreciprocal_result")); return; }
            Work->Expected.Emplace(Port, Peer);
        }
    }
    Work->bApplied = true;
    Work->Designer = After.Designer;
    Record.ObserveEffect(Owner->GetWorldGeneration(), LinkEffect, bStarted ? ESFConstructionEffectOutcome::Applied : ESFConstructionEffectOutcome::NoEffect);
}
}

ESFConstructionAdmission FSFConveyorConnectionMutation::Queue(USFConstructionOperationCoordinator& Owner,
    APlayerController* Requester, uint64 Sequence, const FGuid& RequestId, const TArray<FSFConveyorConnectionPair>& Edges)
{
    if (Edges.IsEmpty()) return ESFConstructionAdmission::Invalid;
    FSFConstructionOperationRequest Request;
    Request.Requester = Requester;
    Request.Sequence = Sequence;
    Request.Stream = ESFConstructionRequestStream::ConveyorLinks;
    Request.RequestId = RequestId;
    Request.WorldGeneration = Owner.GetWorldGeneration();
    auto Work = MakeShared<FLinkWork>();
    Work->Owner = &Owner; Work->Id = RequestId;
    Work->Key = {FObjectKey(Requester), Sequence, Request.Stream};
    TMap<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*> Intended;
    for (const auto& Edge : Edges)
    {
        if (!IsValid(Edge.A) || !IsValid(Edge.B) || Edge.A == Edge.B) return ESFConstructionAdmission::Invalid;
        if ((Intended.Contains(Edge.A) && Intended[Edge.A] != Edge.B) || (Intended.Contains(Edge.B) && Intended[Edge.B] != Edge.A))
            return ESFConstructionAdmission::Invalid;
        Intended.Add(Edge.A, Edge.B); Intended.Add(Edge.B, Edge.A);
        for (auto* Port : {Edge.A, Edge.B})
        {
            auto* Actor = Port->GetOwner();
            if (!IsValid(Actor) || Actor->GetWorld() != Owner.GetWorld() || Actor->IsActorBeingDestroyed()) return ESFConstructionAdmission::Invalid;
            Request.ExpectedCohort.Add(FObjectKey(Actor)); Request.AffectedActors.AddUnique(Actor);
            if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Actor)) Work->Seeds.AddUnique(Conveyor);
        }
        Work->Links.Emplace(Edge.A, Edge.B);
        // Object index/serial remain part of immutable identity even if a name is reused.
        Request.IntentDigest += FString::Printf(TEXT("%s:%u>%s:%u;"), *Edge.A->GetPathName(), GetTypeHash(FObjectKey(Edge.A)),
            *Edge.B->GetPathName(), GetTypeHash(FObjectKey(Edge.B)));
    }
    if (Work->Seeds.IsEmpty()) return ESFConstructionAdmission::Invalid;
    return Owner.Submit(Request,
        [Work](FSFConstructionOperationRecord& Record)
        {
            if (Record.ClaimEffect(Work->Owner->GetWorldGeneration(), LinkEffect)) ApplyLinks(Work, Record);
        },
        [Work](FSFConstructionOperationRecord& Record)
        {
            auto* Owner = Work->Owner.Get();
            if (!Owner || Work->bStopped) return;
            if (Record.IsCancellationRequested() && !Work->bApplied)
            {
                StopLinks(Work, Record, false, TEXT("cancelled_while_waiting_before_links"));
                return;
            }
            if (!Work->bApplied) return;
            FSFConveyorCohort Current; FString Reason;
            const auto Status = FSFConveyorCohortValidator::Capture(*Owner, Work->Seeds, Current, Reason, true);
            if (Status == ESFConveyorCohortStatus::NativePending && ++Work->ObservedPhases <= 120) return;
            if (Status != ESFConveyorCohortStatus::CoherentStructure) { StopLinks(Work, Record, true, Reason); return; }
            if (Current.Designer != Work->Designer) { StopLinks(Work, Record, true, TEXT("settled_designer_owner_changed")); return; }
            for (const auto& Pair : Work->Expected)
            {
                auto* Port = Pair.Key.Get(); auto* Peer = Pair.Value.Get();
                if (!Port || !IsValid(Port->GetOwner()) || Port->GetOwner()->IsActorBeingDestroyed()
                    || (!Pair.Value.IsExplicitlyNull() && !Peer)
                    || Port->GetConnection() != Peer || Port->IsConnected() != (Peer != nullptr)
                    || (Peer && Peer->GetConnection() != Port)) { StopLinks(Work, Record, true, TEXT("settled_edge_changed")); return; }
            }
            const auto Generation = Owner->GetWorldGeneration();
            Record.VerifyObligation(Generation, ESFConstructionObligation::Topology);
            Record.VerifyObligation(Generation, ESFConstructionObligation::NativeMembership);
            Record.VerifyObligation(Generation, ESFConstructionObligation::Cargo); // immediate mutation ledger compared by Apply
            Record.VerifyObligation(Generation, ESFConstructionObligation::DeferredWork);
            // This operation invokes only native detach/link/register: it neither constructs nor charges/refunds.
            // The native parent owns its separate material settlement; this receipt does not certify it.
            Record.VerifyObligation(Generation, ESFConstructionObligation::Resources);
            if (Record.IsCancellationRequested()) Record.SettleCancellation(Generation); else Record.Complete(Generation);
        },
        [Work](ESFConstructionOperationState State)
        {
            UE_LOG(LogSmartFoundations, Log, TEXT("Conveyor links %s settled state=%d edges=%d; native parent payment is separate."),
                *Work->Id.ToString(), static_cast<int32>(State), Work->Links.Num());
        });
}
