// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"
#include "Features/Upgrade/SFConveyorUpgradeOwner.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "Core/Construction/SFConveyorCohort.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "FGBlueprintProxy.h"
#include "UObject/StrongObjectPtr.h"

// SP/MP DIVERGENCE MAP — native factory replacements touching conveyors
// [MP-AUTH] Exact requester/payer and selected actors own one retained operation.
// [MP-SEAM] Native PreFactoryTick owns replacement, inventory transfer and resources.
// [MP-REPL] Results use the existing correlated native Upgrade result transport.
struct FSFNativeFactoryUpgradeWork
{
    struct FPort
    {
        FObjectKey Owner;
        FName Name;
        TOptional<FObjectKey> PeerOwner;
        FName PeerName;
        int32 Direction = INDEX_NONE;
    };
    FSFConstructionOperationKey Key;
    FGuid Generation;
    TWeakObjectPtr<USFConstructionOperationCoordinator> Coordinator;
    TWeakObjectPtr<AFGPlayerController> Requester;
    TWeakObjectPtr<AFGCharacterPlayer> Payer;
    TArray<TWeakObjectPtr<AFGBuildable>> Selected;
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Seeds;
    TArray<TWeakObjectPtr<AActor>> Outputs;
    TMap<FObjectKey, TWeakObjectPtr<AActor>> Actors;
    TMap<FObjectKey, TWeakObjectPtr<AActor>> Replacements;
    TArray<FPort> Graph;
    FSFConveyorCohort Before;
    FSFConveyorCohort::FCargoSnapshot Cargo;
    TMap<TSubclassOf<UFGItemDescriptor>, int64> MaterialsBefore;
    TMap<TSubclassOf<UFGItemDescriptor>, int32> Reserved;
    TWeakObjectPtr<AFGCrate> OverflowCrate;
    FSFConstructionOperationRecord* ExecutingRecord = nullptr;
    bool bExecuting = false;
    bool bInvoked = false;
    bool bObservedMaterialWrite = false;
    bool bCargoVerified = false;
    bool bResourcesVerified = false;
    bool bReportedUncertain = false;
    int32 ObservedPhases = 0;
};

namespace
{
thread_local const UWorld* NativeFactoryReplacementWorld = nullptr;
const FName FactoryEffect(TEXT("NativeFactoryReplacement"));
int64 FactoryMaterialCount(AFGCharacterPlayer* Player, TSubclassOf<UFGItemDescriptor> Class)
{
    auto* Inventory = IsValid(Player) ? Player->GetInventory() : nullptr;
    auto* Depot = IsValid(Player) ? AFGCentralStorageSubsystem::Get(Player->GetWorld()) : nullptr;
    return (Inventory ? int64(Inventory->GetNumItems(Class)) : 0)
        + (Depot ? int64(Depot->GetNumItemsFromCentralStorage(Class)) : 0);
}
AActor* ResolveFactoryActor(const FSFNativeFactoryUpgradeWork& Work, const FObjectKey& Key)
{
    if (const auto* Replacement = Work.Replacements.Find(Key)) return Replacement->Get();
    const auto* Original = Work.Actors.Find(Key);
    return Original ? Original->Get() : nullptr;
}
UFGFactoryConnectionComponent* FactoryPort(AActor* Actor, FName Name)
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return nullptr;
    UFGFactoryConnectionComponent* Found = nullptr;
    TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
    for (auto* Port : Ports) if (IsValid(Port) && Port->GetFName() == Name)
    { if (Found) return nullptr; Found = Port; }
    return Found;
}
bool VerifyFactoryGraph(const FSFNativeFactoryUpgradeWork& Work)
{
    auto* Coordinator = Work.Coordinator.Get();
    if (!Coordinator) return false;
    for (const auto& Entry : Work.Graph)
    {
        auto* Actor = ResolveFactoryActor(Work, Entry.Owner);
        auto* Port = FactoryPort(Actor, Entry.Name);
        auto* PeerActor = Entry.PeerOwner ? ResolveFactoryActor(Work, *Entry.PeerOwner) : nullptr;
        auto* Peer = Entry.PeerOwner ? FactoryPort(PeerActor, Entry.PeerName) : nullptr;
        if (!Port || !Actor->HasAuthority() || Actor->GetWorld() != Coordinator->GetWorld()
            || static_cast<int32>(Port->GetDirection()) != Entry.Direction
            || (Entry.PeerOwner && (!Peer || !PeerActor->HasAuthority() || PeerActor->GetWorld() != Actor->GetWorld()))
            || Port->GetConnection() != Peer || Port->IsConnected() != (Peer != nullptr)
            || (Peer && (Peer->GetConnection() != Port || !Peer->IsConnected()))) return false;
    }
    return true;
}
}

void USFUpgradeExecutionService::StartOwnedFactoryUpgrade()
{
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    auto* Requester = CurrentParams.PlayerController.Get();
    auto* Payer = IsValid(Requester) ? Cast<AFGCharacterPlayer>(Requester->GetPawn()) : nullptr;
    if (!Coordinator || !IsValid(Payer) || !Requester->HasAuthority())
    {
        WorkingResult.ErrorMessage = TEXT("Native factory replacement has no authoritative operation context; no changes applied.");
        WorkingResult.bCompleted = false; LastResult = WorkingResult;
        SendUpgradeResult(Requester, LastResult);
        return;
    }
    auto Work = MakeShared<FSFNativeFactoryUpgradeWork>();
    Work->Key = {FObjectKey(Requester), static_cast<uint64>(CurrentParams.RequestSequence)};
    Work->Generation = Coordinator->GetWorldGeneration(); Work->Coordinator = Coordinator;
    Work->Requester = Requester; Work->Payer = Payer;
    FSFConstructionOperationRequest Request;
    Request.Requester = Requester; Request.Sequence = Work->Key.Sequence;
    Request.RequestId = CurrentParams.RequestId; Request.WorldGeneration = Work->Generation;
    Request.IntentDigest = CurrentParams.RequestId.ToString();
    auto Add = [&](AActor* Actor)
    {
        if (!IsValid(Actor)) return;
        Request.ExpectedCohort.Add(FObjectKey(Actor)); Request.AffectedActors.AddUnique(Actor);
    };
    Add(Requester); Add(Payer);
    for (AFGBuildable* Target : PendingUpgrades) { Work->Selected.Add(Target); Add(Target); }
    NativeFactoryUpgrade = Work;
    if (!ConveyorOwner) ConveyorOwner = MakeShared<FSFConveyorUpgradeOwner>();
    ConveyorOwner->Remember(Work->Key, CurrentParams);
    bUpgradeInProgress = true; WorkingResult.bPending = true;
    TWeakObjectPtr<USFUpgradeExecutionService> WeakThis(this);
    const auto Admission = Coordinator->Submit(Request,
        [WeakThis, Work](FSFConstructionOperationRecord& Record)
        { if (auto* Service = WeakThis.Get()) Service->RunOwnedFactoryUpgrade(Record); },
        [WeakThis, Work](FSFConstructionOperationRecord& Record)
        { if (auto* Service = WeakThis.Get()) Service->ObserveOwnedFactoryUpgrade(Record); },
        [WeakThis, Work](ESFConstructionOperationState State)
        { if (auto* Service = WeakThis.Get()) Service->FinishOwnedFactoryUpgrade(static_cast<uint8>(State)); });
    if (Admission != ESFConstructionAdmission::Accepted)
    {
        WorkingResult.ErrorMessage = TEXT("Native factory replacement could not acquire its operation owner; no changes applied.");
        FinishOwnedFactoryUpgrade(static_cast<uint8>(ESFConstructionOperationState::CancelledBeforeEffects));
    }
    else SendUpgradeResult(Requester, WorkingResult);
}

void USFUpgradeExecutionService::RunOwnedFactoryUpgrade(FSFConstructionOperationRecord& Record)
{
    auto Work = NativeFactoryUpgrade;
    auto* Coordinator = Work ? Work->Coordinator.Get() : nullptr;
    if (!Coordinator || !Coordinator->IsInMutationPhase()) return;
    auto Reject = [&](const FString& Reason)
    {
        WorkingResult.ErrorMessage = Reason;
        if (Work->bInvoked) { WorkingResult.bUncertain = true; Record.RequireReconciliation(Work->Generation); }
        else { Record.RequestCancellation(Work->Generation); Record.SettleCancellation(Work->Generation); }
    };
    if (Record.IsCancellationRequested() || !IsExecutingFactoryUpgrade(Work->Payer.Get()))
    { Reject(TEXT("Native factory requester, payer or world expired before replacement.")); return; }
    TArray<TStrongObjectPtr<AActor>> Pins;
    TArray<AActor*> GraphActors;
    for (const auto& Weak : Work->Selected)
    {
        auto* Target = Weak.Get();
        if (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->GetWorld() != Coordinator->GetWorld())
        { Reject(TEXT("Selected factory expired before replacement.")); return; }
        GraphActors.AddUnique(Target);
        auto* Extractor = Cast<AFGBuildableResourceExtractor>(Target);
        auto* Resource = Extractor ? Cast<AActor>(Extractor->GetExtractableResource().GetObject()) : nullptr;
        if (!IsValid(Resource) || !Coordinator->AdoptActor(Work->Key, Resource))
        { Reject(TEXT("Native factory resource ownership conflicts with another operation.")); return; }
        Pins.Emplace(Resource);
        for (AActor* Context : {static_cast<AActor*>(Target->GetBlueprintDesigner()), static_cast<AActor*>(Target->GetBlueprintProxy())})
            if (Context && !Coordinator->AdoptActor(Work->Key, Context))
            { Reject(TEXT("Factory Designer or dismantle group belongs to another operation.")); return; }
        TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Target);
        for (auto* Port : Ports)
        {
            auto* Peer = IsValid(Port) ? Port->GetConnection() : nullptr;
            if (!IsValid(Port) || Port->IsConnected() != (Peer != nullptr)
                || (Peer && (!IsValid(Peer) || !IsValid(Peer->GetOwner()) || !Peer->IsConnected() || Peer->GetConnection() != Port)))
            { Reject(TEXT("Selected factory has an incoherent existing edge.")); return; }
            if (Peer)
            {
                GraphActors.AddUnique(Peer->GetOwner());
                if (auto* Belt = Cast<AFGBuildableConveyorBase>(Peer->GetOwner())) Work->Seeds.AddUnique(Belt);
            }
        }
    }
    FString Reason;
    if (!Work->Seeds.IsEmpty())
    {
        if (FSFConveyorCohortValidator::Capture(*Coordinator, Work->Seeds, Work->Before, Reason) != ESFConveyorCohortStatus::CoherentStructure)
        { Reject(Reason); return; }
        for (const auto& Actor : Work->Before.AffectedActors) GraphActors.AddUnique(Actor.Get());
        if (FSFConveyorCohortValidator::SnapshotCargo(*Coordinator, Work->Before, Work->Cargo, Reason) != ESFConveyorCohortStatus::CoherentStructure)
        { Reject(Reason); return; }
    }
    // Freeze/lease every boundary owner before the first native constructor, inventory
    // debit or callback. No raw native worker container is retained by this operation.
    for (auto* Actor : GraphActors)
    {
        if (!IsValid(Actor) || !Coordinator->AdoptActor(Work->Key, Actor))
        { Reject(TEXT("Factory conveyor cohort conflicts with another operation.")); return; }
        Pins.Emplace(Actor); Work->Actors.Add(FObjectKey(Actor), Actor);
        TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
        for (auto* Port : Ports)
        {
            auto* Peer = IsValid(Port) ? Port->GetConnection() : nullptr;
            if (!IsValid(Port) || Port->IsConnected() != (Peer != nullptr)
                || (Peer && (!IsValid(Peer) || !IsValid(Peer->GetOwner()) || !Peer->IsConnected() || Peer->GetConnection() != Port)))
            { Reject(TEXT("Affected factory graph is not reciprocal.")); return; }
            FSFNativeFactoryUpgradeWork::FPort Entry;
            Entry.Owner = FObjectKey(Actor); Entry.Name = Port->GetFName(); Entry.Direction = static_cast<int32>(Port->GetDirection());
            if (Peer)
            {
                if (!Coordinator->AdoptActor(Work->Key, Peer->GetOwner()))
                { Reject(TEXT("Factory boundary peer conflicts with another operation.")); return; }
                Work->Actors.Add(FObjectKey(Peer->GetOwner()), Peer->GetOwner());
                Entry.PeerOwner = FObjectKey(Peer->GetOwner()); Entry.PeerName = Peer->GetFName();
            }
            Work->Graph.Add(MoveTemp(Entry));
        }
    }
    if (!Record.ClaimEffect(Work->Generation, FactoryEffect)) return;
    Work->bInvoked = true; Work->bExecuting = true; Work->ExecutingRecord = &Record;
    TGuardValue<const UWorld*> NativeContext(NativeFactoryReplacementWorld, Coordinator->GetWorld());
    CaptureExpectedConnectionManifests();
    ExecuteUpgradeTargets();
    Work->bExecuting = false; Work->ExecutingRecord = nullptr;
    TSet<FObjectKey> Retired;
    for (const auto& Pair : OldToNewBuildableMap)
    {
        const FObjectKey Original(Pair.Key);
        Work->Replacements.Add(Original, Pair.Value); Retired.Add(Original);
    }
    if (WorkingResult.bUncertain || !VerifyFactoryGraph(*Work))
    { Reject(TEXT("Native factory replacement left unresolved actor, inventory or conveyor topology effects.")); return; }
    if (!Work->Seeds.IsEmpty())
    {
        FSFConveyorCohort After; FSFConveyorCohort::FCargoSnapshot Cargo;
        const auto Status = FSFConveyorCohortValidator::Capture(*Coordinator, Work->Seeds, After, Reason);
        if ((Status != ESFConveyorCohortStatus::CoherentStructure && Status != ESFConveyorCohortStatus::NativePending)
            || FSFConveyorCohortValidator::SnapshotCargo(*Coordinator, After, Cargo, Reason) != ESFConveyorCohortStatus::CoherentStructure
            || Cargo.Num() != Work->Cargo.Num()) { Reject(TEXT("Factory replacement changed affected conveyor cargo owners.")); return; }
        for (const auto& Pair : Work->Cargo)
        {
            const auto* Actual = Cargo.Find(Pair.Key);
            if (!Actual || Actual->Num() != Pair.Value.Num()) { Reject(TEXT("Factory replacement changed conveyor cargo.")); return; }
            for (int32 Index = 0; Index < Actual->Num(); ++Index)
                if (!Pair.Value[Index].SameValue((*Actual)[Index]) || !FMath::IsNearlyEqual(Pair.Value[Index].Offset, (*Actual)[Index].Offset, .01f))
                { Reject(TEXT("Factory replacement changed typed conveyor cargo or offsets.")); return; }
        }
        if (!FSFConveyorCohortValidator::VerifyBoundaryInventories(*Coordinator, Work->Before, Reason, &Retired))
        { Reject(Reason); return; }
    }
    Work->bCargoVerified = true;
    Work->bResourcesVerified = SettleFactoryResources();
    if (!Work->bResourcesVerified) { Reject(TEXT("Native factory material settlement did not conserve the observed ledger.")); return; }
    bool Applied = !Work->Outputs.IsEmpty() || Work->bObservedMaterialWrite;
    for (const auto& Pair : BatchRefundedTotals) Applied |= Pair.Value > 0;
    Record.ObserveEffect(Work->Generation, FactoryEffect, Applied ? ESFConstructionEffectOutcome::Applied : ESFConstructionEffectOutcome::NoEffect);
}

void USFUpgradeExecutionService::ObserveOwnedFactoryUpgrade(FSFConstructionOperationRecord& Record)
{
    auto Work = NativeFactoryUpgrade;
    auto* Coordinator = Work ? Work->Coordinator.Get() : nullptr;
    if (!Coordinator || !Coordinator->IsInObservationPhase()) return;
    auto Uncertain = [&](const FString& Reason)
    {
        WorkingResult.bUncertain = true; WorkingResult.ErrorMessage = Reason;
        Record.RequireReconciliation(Work->Generation);
    };
    if (!WorkingResult.bUncertain && Work->bInvoked)
    {
        FString Reason; FSFConveyorCohort Current;
        auto Status = Work->Seeds.IsEmpty() ? ESFConveyorCohortStatus::CoherentStructure
            : FSFConveyorCohortValidator::Capture(*Coordinator, Work->Seeds, Current, Reason);
        if (Status == ESFConveyorCohortStatus::NativePending && ++Work->ObservedPhases <= 120) return;
        if (Status != ESFConveyorCohortStatus::CoherentStructure || !VerifyFactoryGraph(*Work)
            || !Work->bCargoVerified || !Work->bResourcesVerified)
            Uncertain(TEXT("Native factory replacement did not settle its affected conveyor topology/membership."));
        else
        {
            for (const auto& Actor : Current.AffectedActors)
                if (!Coordinator->AdoptActor(Work->Key, Actor.Get()))
                { Uncertain(TEXT("Settled factory conveyor cohort conflicts with another operation.")); break; }
            if (!WorkingResult.bUncertain)
            {
                for (auto Obligation : {ESFConstructionObligation::Topology, ESFConstructionObligation::NativeMembership,
                    ESFConstructionObligation::Cargo, ESFConstructionObligation::DeferredWork, ESFConstructionObligation::Resources})
                    Record.VerifyObligation(Work->Generation, Obligation);
                if (Record.IsCancellationRequested()) Record.SettleCancellation(Work->Generation); else Record.Complete(Work->Generation);
            }
        }
    }
    if (WorkingResult.bUncertain && !Work->bReportedUncertain)
    {
        Work->bReportedUncertain = true; WorkingResult.bCompleted = false; WorkingResult.bPending = false;
        LastResult = WorkingResult; ConveyorOwner->Settle(Work->Key, LastResult);
        OnUpgradeExecutionCompleted.Broadcast(LastResult); SendUpgradeResult(Work->Requester.Get(), LastResult);
        UE_LOG(LogSmartUpgrade, Warning, TEXT("Native factory Upgrade %s requires reconciliation: %s; leases, output identities and material receipts retained."),
            *LastResult.RequestId.ToString(), *LastResult.ErrorMessage);
    }
}

void USFUpgradeExecutionService::FinishOwnedFactoryUpgrade(uint8 RawState)
{
    auto Work = NativeFactoryUpgrade;
    if (!Work) return;
    const auto State = static_cast<ESFConstructionOperationState>(RawState);
    WorkingResult.bPending = false;
    WorkingResult.bUncertain |= State == ESFConstructionOperationState::Uncertain;
    WorkingResult.bCompleted = State == ESFConstructionOperationState::Completed && !WorkingResult.bUncertain;
    if (!WorkingResult.bCompleted && WorkingResult.ErrorMessage.IsEmpty()) WorkingResult.ErrorMessage = TEXT("Native factory Upgrade cancelled; observed effects remain distinct from successful completion.");
    LastResult = WorkingResult; ConveyorOwner->Settle(Work->Key, LastResult);
    bUpgradeInProgress = false;
    // Resource delivery and graph verification happened in the native mutation
    // owner. Never call legacy CompleteUpgrade here: it contains repair/delivery effects.
    if (!WorkingResult.bUncertain)
    {
        PendingUpgrades.Reset(); OldToNewBuildableMap.Reset(); ExpectedConnectionEdges.Reset();
        CurrentUpgradeIndex = 0; NativeFactoryUpgrade.Reset();
    }
    OnUpgradeExecutionCompleted.Broadcast(LastResult); SendUpgradeResult(Work->Requester.Get(), LastResult);
}

bool USFUpgradeExecutionService::CancelOwnedFactoryUpgrade()
{
    if (!NativeFactoryUpgrade) return false;
    if (auto* Coordinator = NativeFactoryUpgrade->Coordinator.Get()) Coordinator->RequestCancellation(NativeFactoryUpgrade->Key);
    return true;
}

bool USFUpgradeExecutionService::IsExecutingFactoryUpgrade(AFGCharacterPlayer* Player) const
{
    auto Work = NativeFactoryUpgrade;
    auto* Coordinator = Work ? Work->Coordinator.Get() : nullptr;
    return Work && Coordinator && Coordinator->IsInMutationPhase() && Coordinator->GetWorldGeneration() == Work->Generation
        && IsValid(Player) && Player == Work->Payer.Get() && Work->Requester.IsValid()
        && Work->Requester->GetPawn() == Player && Player->GetWorld() == Coordinator->GetWorld();
}

bool USFUpgradeExecutionService::AdoptFactoryOutputs(AActor* Main, const TArray<AActor*>& Children)
{
    auto Work = NativeFactoryUpgrade;
    if (!Work || !Work->bExecuting || !IsExecutingFactoryUpgrade(Work->Payer.Get())) return false;
    bool Valid = IsValid(Main);
    auto Add = [&](AActor* Actor)
    {
        if (!Actor) return;
        Work->Outputs.AddUnique(Actor);
        Valid &= Work->Coordinator->AdoptActor(Work->Key, Actor);
    };
    Add(Main); for (auto* Child : Children) Add(Child);
    return Valid;
}

bool USFUpgradeExecutionService::ReserveFactoryCharge(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* Player)
{
    auto Work = NativeFactoryUpgrade;
    if (!Work || !Work->bExecuting || !Work->ExecutingRecord || Work->ExecutingRecord->IsCancellationRequested()
        || !IsExecutingFactoryUpgrade(Player) || !Work->Reserved.IsEmpty()) return false;
    for (const auto& Pair : Ledger.NetCharge) if (!Work->MaterialsBefore.Contains(Pair.Key)) Work->MaterialsBefore.Add(Pair.Key, FactoryMaterialCount(Player, Pair.Key));
    for (const auto& Pair : Ledger.NetRefund) if (!Work->MaterialsBefore.Contains(Pair.Key)) Work->MaterialsBefore.Add(Pair.Key, FactoryMaterialCount(Player, Pair.Key));
    if (Ledger.bFreeBuild) return true;
    auto* Depot = AFGCentralStorageSubsystem::Get(Player->GetWorld());
    auto* PlayerState = Player->GetPlayerState<AFGPlayerState>();
    const bool InventoryFirst = PlayerState ? PlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;
    for (const auto& Pair : Ledger.NetCharge)
    {
        const int64 Before = FactoryMaterialCount(Player, Pair.Key);
        UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(Player->GetInventory(), Depot, InventoryFirst, Pair.Key, Pair.Value);
        const int64 Removed = Before - FactoryMaterialCount(Player, Pair.Key);
        if (Removed < 0 || Removed > Pair.Value) { WorkingResult.bUncertain = true; return false; }
        if (Removed) { Work->bObservedMaterialWrite = true; Work->Reserved.Add(Pair.Key, static_cast<int32>(Removed)); BatchChargedTotals.FindOrAdd(Pair.Key) += static_cast<int32>(Removed); }
        if (!IsExecutingFactoryUpgrade(Player) || Work->ExecutingRecord->IsCancellationRequested() || Removed != Pair.Value) return false;
    }
    return true;
}

void USFUpgradeExecutionService::RollbackFactoryCharge(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* Player)
{
    if (!NativeFactoryUpgrade || WorkingResult.bUncertain) return;
    FSFUpgradeSettlementLedger Actual = Ledger;
    Actual.NetCharge = MoveTemp(NativeFactoryUpgrade->Reserved);
    NativeFactoryUpgrade->Reserved.Reset();
    RollbackUpgradeCharge(Actual, Player); // Only amounts actually removed, never the unfulfilled quote.
}

bool USFUpgradeExecutionService::SettleFactoryResources()
{
    auto Work = NativeFactoryUpgrade;
    auto* Player = Work ? Work->Payer.Get() : nullptr;
    if (!Work || !IsExecutingFactoryUpgrade(Player)) return false;
    for (const auto& Pair : BatchChargedTotals) if (!Work->MaterialsBefore.Contains(Pair.Key)) return false;
    for (const auto& Pair : BatchRefundedTotals) if (!Work->MaterialsBefore.Contains(Pair.Key)) return false;
    if (!OverflowItems.IsEmpty())
    {
        TArray<FInventoryStack> Stacks;
        for (const auto& Pair : OverflowItems) if (Pair.Key && Pair.Value > 0) Stacks.Add(FInventoryStack(Pair.Value, Pair.Key));
        FSpawnInventoryCrateParameters Params; Params.bAllowCrateMerging = false;
        auto* Crate = AFGItemPickup_Spawnable::SpawnInventoryCrate(Player->GetWorld(), Stacks, Player->GetActorLocation() + FVector(0, 0, 50), Params);
        Work->OverflowCrate = Crate; if (Crate) Work->Outputs.AddUnique(Crate);
        if (!IsValid(Crate) || !Crate->GetInventory() || !Work->Coordinator->AdoptActor(Work->Key, Crate)) return false;
        TArray<FSFInventoryStackSnapshot> Expected; TArray<FInventoryStack> Actual;
        if (!SFInventorySnapshots::Capture(Stacks, Expected)) return false;
        Crate->GetInventory()->GetInventoryStacks(Actual);
        if (!SFInventorySnapshots::SameContents(Expected, Actual)) return false;
    }
    for (const auto& Pair : Work->MaterialsBefore)
    {
        int64 Actual = FactoryMaterialCount(Player, Pair.Key);
        if (Work->OverflowCrate.IsValid()) Actual += Work->OverflowCrate->GetInventory()->GetNumItems(Pair.Key);
        if (Actual != Pair.Value - BatchChargedTotals.FindRef(Pair.Key) + BatchRefundedTotals.FindRef(Pair.Key)) return false;
    }
    OverflowItems.Reset(); Work->Reserved.Reset();
    return true;
}

void USFUpgradeExecutionService::CommitFactoryCharge()
{
    if (NativeFactoryUpgrade && NativeFactoryUpgrade->bExecuting) NativeFactoryUpgrade->Reserved.Reset();
}

bool USFUpgradeExecutionService::ShouldStopFactoryUpgrade() const
{
    auto Work = NativeFactoryUpgrade;
    return Work && (!Work->bExecuting || !Work->ExecutingRecord || Work->ExecutingRecord->IsCancellationRequested()
        || !IsExecutingFactoryUpgrade(Work->Payer.Get()));
}

bool USFUpgradeExecutionService::IsConstructingNativeFactoryReplacement(const UWorld* World)
{
    return World && IsInGameThread() && NativeFactoryReplacementWorld == World;
}
