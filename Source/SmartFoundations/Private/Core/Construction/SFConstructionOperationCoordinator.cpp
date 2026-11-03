// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "SmartFoundations.h"
#include "Core/Construction/SFConstructionOperationRegistry.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

struct FSFConstructionOperationCoordinatorState
{
    struct FWork
    {
        TWeakObjectPtr<APlayerController> Requester;
        TArray<TWeakObjectPtr<AActor>> OriginalActors;
        TFunction<void(FSFConstructionOperationRecord&)> Mutate;
        TFunction<void(FSFConstructionOperationRecord&)> Observe;
        TFunction<void(ESFConstructionOperationState)> OnSettled;
        TMap<FName, TFunction<void(FSFConstructionOperationRecord&)>> Continuations;
        bool bDispatched = false;
    };
    explicit FSFConstructionOperationCoordinatorState(const FGuid& Generation) : Registry(Generation) {}
    FSFConstructionOperationRegistry Registry;
    TMap<FSFConstructionOperationKey, TUniquePtr<FWork>> Work;
    TArray<FSFConstructionOperationKey> Pending;
    TArray<TPair<FSFConstructionOperationKey, FName>> PendingContinuations;
    bool bStopping = false;
    bool bFinalizing = false;
    bool bWorkFinalized = false;
    bool bDispatching = false;
    bool bMutationPhase = false;
    bool bObservationPhase = false;
};

USFConstructionOperationCoordinator::USFConstructionOperationCoordinator() = default;
USFConstructionOperationCoordinator::~USFConstructionOperationCoordinator() = default;

bool USFConstructionOperationCoordinator::Initialize(UWorld* InWorld)
{
    if (!IsInGameThread() || State || !IsValid(InWorld)) return false;
    World = InWorld;
    State = MakeShared<FSFConstructionOperationCoordinatorState>(FGuid::NewGuid());
    return true;
}

UWorld* USFConstructionOperationCoordinator::GetWorld() const { return World.Get(); }

void USFConstructionOperationCoordinator::RefreshNativeBinding()
{
    if (IsInGameThread() && State && !State->bDispatching) TryBindNativeHandler();
}

bool USFConstructionOperationCoordinator::TryBindNativeHandler()
{
    UWorld* CurrentWorld = GetWorld();
    if (!IsInGameThread() || !State || State->bStopping || !IsValid(CurrentWorld)
        || CurrentWorld->bIsTearingDown || !CurrentWorld->HasBegunPlay()
        || CurrentWorld->GetNetMode() == NM_Client || !CurrentWorld->GetAuthGameMode()) return false;
    if (NativeSubsystem.IsValid()) return NativeSubsystem->GetWorld() == CurrentWorld;
    AFGBuildableSubsystem* Native = AFGBuildableSubsystem::Get(CurrentWorld);
    if (!IsValid(Native) || !Native->HasAuthority()) return false;
    NativeSubsystem = Native;
    Native->AddFactoryTickHandler(this);
    return true;
}

ESFConstructionAdmission USFConstructionOperationCoordinator::Submit(const FSFConstructionOperationRequest& Request,
    TFunction<void(FSFConstructionOperationRecord&)> Mutate, TFunction<void(FSFConstructionOperationRecord&)> Observe,
    TFunction<void(ESFConstructionOperationState)> OnSettled)
{
    UWorld* CurrentWorld = GetWorld();
    if (!IsInGameThread() || !State || State->bStopping || !IsValid(CurrentWorld)
        || CurrentWorld->bIsTearingDown || !CurrentWorld->HasBegunPlay()) return ESFConstructionAdmission::Unavailable;
    // [MP-AUTH] Requester identity is supplied by the authoritative caller, never trusted from an RPC payload.
    if (!IsValid(Request.Requester) || Request.Requester->GetWorld() != CurrentWorld || !Request.Requester->HasAuthority()
        || Request.Requester->IsActorBeingDestroyed()
        || !Request.RequestId.IsValid() || Request.Sequence == 0 || Request.IntentDigest.IsEmpty()
        || Request.WorldGeneration != GetWorldGeneration() || Request.ExpectedCohort.IsEmpty()
        || !Mutate || !Observe || !OnSettled) return ESFConstructionAdmission::Invalid;
    FSFConstructionOperationRegistry::FAdmission Admission;
    Admission.Key = {FObjectKey(Request.Requester), Request.Sequence, Request.Stream};
    Admission.RequestId = Request.RequestId;
    Admission.IntentDigest = Request.IntentDigest;
    Admission.Cohort = Request.ExpectedCohort;
    if (const auto Replay = State->Registry.ClassifyReplay(Admission)) return Replay.GetValue();
    TSet<FObjectKey> ActualCohort;
    for (const TWeakObjectPtr<AActor>& WeakActor : Request.AffectedActors)
    {
        AActor* Actor = WeakActor.Get();
        if (!IsValid(Actor) || Actor->GetWorld() != CurrentWorld || !Actor->HasAuthority() || Actor->IsActorBeingDestroyed())
            return ESFConstructionAdmission::Invalid;
        ActualCohort.Add(FObjectKey(Actor));
    }
    if (ActualCohort.Num() != Admission.Cohort.Num()) return ESFConstructionAdmission::Invalid;
    for (const FObjectKey& Actor : ActualCohort)
        if (!Admission.Cohort.Contains(Actor)) return ESFConstructionAdmission::Invalid;
    if (!NativeSubsystem.IsValid() || NativeSubsystem->GetWorld() != CurrentWorld || !NativeSubsystem->HasAuthority())
        return ESFConstructionAdmission::Unavailable;
    const ESFConstructionAdmission Result = State->Registry.Admit(Admission);
    if (Result != ESFConstructionAdmission::Accepted) return Result;
    auto Work = MakeUnique<FSFConstructionOperationCoordinatorState::FWork>();
    Work->Requester = Request.Requester;
    Work->OriginalActors = Request.AffectedActors;
    Work->Mutate = MoveTemp(Mutate);
    Work->Observe = MoveTemp(Observe);
    Work->OnSettled = MoveTemp(OnSettled);
    State->Work.Add(Admission.Key, MoveTemp(Work));
    State->Pending.Add(Admission.Key);
    return Result;
}

bool USFConstructionOperationCoordinator::IsExpectedCallback(AFGBuildableSubsystem* Subsystem) const
{
    return IsInGameThread() && State && !State->bStopping && IsValid(GetWorld())
        && !GetWorld()->bIsTearingDown && GetWorld()->HasBegunPlay() && IsValid(Subsystem)
        && NativeSubsystem.Get() == Subsystem && Subsystem->GetWorld() == GetWorld() && Subsystem->HasAuthority();
}

void USFConstructionOperationCoordinator::PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime)
{
    if (!IsExpectedCallback(Subsystem) || State->bDispatching
        || (State->Pending.IsEmpty() && State->PendingContinuations.IsEmpty())) return;
    TStrongObjectPtr<USFConstructionOperationCoordinator> KeepAlive(this);
    State->bDispatching = true;
    State->bMutationPhase = true;
    TArray<FSFConstructionOperationKey> Batch = MoveTemp(State->Pending);
    State->Pending.Reset();
    // Drain only work queued before this native phase. A not-ready continuation
    // can queue itself for the next phase without spinning in this invocation.
    auto ContinuationBatch = MoveTemp(State->PendingContinuations);
    State->PendingContinuations.Reset();
    for (const FSFConstructionOperationKey& Key : Batch)
    {
        if (State->bStopping) break;
        auto* Work = State->Work.Find(Key);
        FSFConstructionOperationRecord* Record = State->Registry.FindRecord(Key);
        if (!Work || !Record) continue;
        APlayerController* Requester = (*Work)->Requester.Get();
        bool bValid = IsValid(Requester) && Requester->GetWorld() == GetWorld()
            && Requester->HasAuthority() && !Requester->IsActorBeingDestroyed();
        for (const TWeakObjectPtr<AActor>& Actor : (*Work)->OriginalActors)
            bValid &= Actor.IsValid() && Actor->GetWorld() == GetWorld()
                && Actor->HasAuthority() && !Actor->IsActorBeingDestroyed();
        if (!bValid)
        {
            Record->RequestCancellation(GetWorldGeneration());
            Record->SettleCancellation(GetWorldGeneration());
            ReleaseSettled(Key);
            continue;
        }
        if (!Record->BeginMutation(GetWorldGeneration()))
        {
            Record->SettleCancellation(GetWorldGeneration());
            ReleaseSettled(Key);
            continue;
        }
        (*Work)->bDispatched = true;
        // Heap-owned entries survive a Submit performed by the callback. No array/map iterator is held across it.
        auto* StableWork = Work->Get();
        StableWork->Mutate(*Record);
        Record->SealMutation(GetWorldGeneration());
    }
    for (const auto& Entry : ContinuationBatch)
    {
        if (State->bStopping) break;
        auto* Work = State->Work.Find(Entry.Key);
        auto* Record = State->Registry.FindRecord(Entry.Key);
        if (!Work || !Record) continue;
        TFunction<void(FSFConstructionOperationRecord&)> Continue;
        if (!(*Work)->Continuations.RemoveAndCopyValue(Entry.Value, Continue)) continue;
        auto* Requester = (*Work)->Requester.Get();
        if (!IsValid(Requester) || Requester->IsActorBeingDestroyed() || Requester->GetWorld() != GetWorld()
            || !Requester->HasAuthority()) Record->RequestCancellation(GetWorldGeneration());
        if (Record->CanContinueEffect(GetWorldGeneration(), Entry.Value) && Continue) Continue(*Record);
    }
    State->bMutationPhase = false;
    State->bDispatching = false;
    if (State->bStopping) FinalizeStoppedWork();
}

void USFConstructionOperationCoordinator::PostFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime)
{
    if (!IsExpectedCallback(Subsystem) || State->bDispatching || State->Work.IsEmpty()) return;
    TStrongObjectPtr<USFConstructionOperationCoordinator> KeepAlive(this);
    State->bDispatching = true;
    State->bObservationPhase = true;
    TArray<FSFConstructionOperationKey> Keys;
    State->Work.GetKeys(Keys);
    for (const FSFConstructionOperationKey& Key : Keys)
    {
        if (State->bStopping) break;
        auto* Work = State->Work.Find(Key);
        FSFConstructionOperationRecord* Record = State->Registry.FindRecord(Key);
        if (!Work || !Record || !(*Work)->bDispatched) continue;
        auto* StableWork = Work->Get();
        StableWork->Observe(*Record);
        Record->SettleCancellation(GetWorldGeneration());
        ReleaseSettled(Key);
    }
    State->bObservationPhase = false;
    State->bDispatching = false;
    if (State->bStopping) FinalizeStoppedWork();
}

void USFConstructionOperationCoordinator::ReleaseSettled(const FSFConstructionOperationKey& Key)
{
    auto* Work = State->Work.Find(Key);
    FSFConstructionOperationRecord* Record = State->Registry.FindRecord(Key);
    if (!Work || !Record || !Record->IsSettled()) return;
    const ESFConstructionOperationState Outcome = Record->GetState();
    if (!State->Registry.ReleaseSettled(Key)) return;
    auto OnSettled = MoveTemp((*Work)->OnSettled);
    State->Work.Remove(Key);
    // Release and remove before notifying: reentrant submissions cannot execute the old callback twice.
    if (OnSettled) OnSettled(Outcome);
}

bool USFConstructionOperationCoordinator::AdoptActor(const FSFConstructionOperationKey& Key, AActor* Actor)
{
    return IsInGameThread() && State && !State->bStopping && IsValid(Actor) && Actor->HasAuthority()
        && Actor->GetWorld() == GetWorld() && !Actor->IsActorBeingDestroyed()
        && State->Registry.Adopt(Key, FObjectKey(Actor));
}

bool USFConstructionOperationCoordinator::QueueContinuation(const FSFConstructionOperationKey& Key, FName EffectKey,
    TFunction<void(FSFConstructionOperationRecord&)> Continue)
{
    if (!IsInGameThread() || !State || State->bStopping || !Continue || EffectKey.IsNone()) return false;
    auto* Work = State->Work.Find(Key);
    auto* Record = State->Registry.FindRecord(Key);
    if (!Work || !(*Work)->bDispatched || !Record || !Record->CanContinueEffect(GetWorldGeneration(), EffectKey)
        || (*Work)->Continuations.Contains(EffectKey)) return false;
    (*Work)->Continuations.Add(EffectKey, MoveTemp(Continue));
    State->PendingContinuations.Emplace(Key, EffectKey);
    return true;
}

bool USFConstructionOperationCoordinator::RequestCancellation(const FSFConstructionOperationKey& Key)
{
    if (!IsInGameThread() || !State || State->bStopping) return false;
    FSFConstructionOperationRecord* Record = State->Registry.FindRecord(Key);
    return Record && Record->RequestCancellation(GetWorldGeneration());
}

TOptional<ESFConstructionOperationState> USFConstructionOperationCoordinator::GetOperationState(const FSFConstructionOperationKey& Key) const
{
    return IsInGameThread() && State ? State->Registry.GetState(Key) : TOptional<ESFConstructionOperationState>();
}

FGuid USFConstructionOperationCoordinator::GetWorldGeneration() const
{
    return IsInGameThread() && State ? State->Registry.GetWorldGeneration() : FGuid();
}

bool USFConstructionOperationCoordinator::IsInMutationPhase() const
{
    return IsInGameThread() && State && !State->bStopping && State->bMutationPhase;
}

bool USFConstructionOperationCoordinator::IsInObservationPhase() const
{
    return IsInGameThread() && State && !State->bStopping && State->bObservationPhase;
}

void USFConstructionOperationCoordinator::Shutdown()
{
    if (!IsInGameThread() || !State || State->bFinalizing) return;
    State->bStopping = true;
    if (State->bDispatching)
    {
        // Removing a native handler from inside its dispatch can invalidate the native iteration.
        if (!bShutdownQueued && World.IsValid())
        {
            bShutdownQueued = true;
            // The owning subsystem can release us before this timer runs. Keep the
            // handler alive until it can be removed outside native iteration.
            auto DeferredOwner = MakeShared<TStrongObjectPtr<USFConstructionOperationCoordinator>>(this);
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([DeferredOwner]()
            {
                auto* Coordinator = DeferredOwner->Get();
                Coordinator->bShutdownQueued = false;
                Coordinator->Shutdown();
            }));
        }
        return;
    }
    if (NativeSubsystem.IsValid()) NativeSubsystem->RemoveFactoryTickHandler(this);
    NativeSubsystem.Reset();
    FinalizeStoppedWork();
    State.Reset();
    World.Reset();
}

void USFConstructionOperationCoordinator::FinalizeStoppedWork()
{
    if (!State || !State->bStopping || State->bDispatching || State->bFinalizing || State->bWorkFinalized) return;
    State->bFinalizing = true;
    // Notify every retained feature owner exactly once before releasing this world.
    // Merely dropping Work left clients pending and silently discarded cancellation
    // results. A world-end Uncertain result explicitly retains unresolved effects.
    TArray<FSFConstructionOperationKey> Keys;
    State->Work.GetKeys(Keys);
    int32 UncertainCount = 0;
    for (const auto& Key : Keys)
    {
        auto* Record = State->Registry.FindRecord(Key);
        if (!Record || !Record->TerminateForWorldEnd(GetWorldGeneration())) continue;
        UncertainCount += Record->GetState() == ESFConstructionOperationState::Uncertain;
        ReleaseSettled(Key);
    }
    if (UncertainCount > 0)
        UE_LOG(LogSmartFoundations, Warning, TEXT("Construction world ended with %d uncertain operation(s); requesters were notified, no completion or rollback is inferred."), UncertainCount);
    State->bWorkFinalized = true;
    State->bFinalizing = false;
}
