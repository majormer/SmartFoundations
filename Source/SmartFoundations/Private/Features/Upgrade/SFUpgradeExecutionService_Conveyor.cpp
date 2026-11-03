// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"
#include "Features/Upgrade/SFConveyorUpgradeOwner.h"
#include "Features/Upgrade/SFConveyorLiftUpgradeContract.h"
#include "Features/Upgrade/SFConveyorUpgradeCompanions.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "FGRecipeManager.h"
#include "FGConstructDisqualifier.h"
#include "FGBlueprintProxy.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"

namespace
{
    thread_local const UWorld* ConveyorReplacementWorld = nullptr;
    int64 ConveyorMaterialCount(AFGCharacterPlayer* Player, TSubclassOf<UFGItemDescriptor> Descriptor)
    {
        auto* Inventory = Player ? Player->GetInventory() : nullptr;
        auto* Storage = Player ? AFGCentralStorageSubsystem::Get(Player->GetWorld()) : nullptr;
        return (Inventory ? int64(Inventory->GetNumItems(Descriptor)) : 0)
            + (Storage ? int64(Storage->GetNumItemsFromCentralStorage(Descriptor)) : 0);
    }
    AActor* ResolveConveyorOwner(const FSFConveyorUpgradeOwner::FOperation& Operation, const FObjectKey& Key)
    {
        if (const auto* New = Operation.Replacements.Find(Key)) return New->Get();
        return Cast<AActor>(Key.ResolveObjectPtr());
    }
    UFGFactoryConnectionComponent* FindConveyorPort(const FSFConveyorUpgradeOwner::FOperation& Operation,
        const FObjectKey& Owner, const TWeakObjectPtr<UFGFactoryConnectionComponent>& Original, FVector Position, uint8 Direction)
    {
        AActor* Actor = ResolveConveyorOwner(Operation, Owner);
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return nullptr;
        if (!Operation.Replacements.Contains(Owner))
        {
            auto* Port = Original.Get();
            return Port && Port->GetOwner() == Actor && static_cast<uint8>(Port->GetDirection()) == Direction
                && Port->GetComponentLocation().Equals(Position, 0.1f) ? Port : nullptr;
        }
        TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
        UFGFactoryConnectionComponent* Found = nullptr;
        for (auto* Port : Ports) if (IsValid(Port) && static_cast<uint8>(Port->GetDirection()) == Direction
            && Port->GetComponentLocation().Equals(Position, 0.1f))
        { if (Found) return nullptr; Found = Port; }
        return Found;
    }
}

bool USFUpgradeExecutionService::IsConstructingConveyorReplacement(const UWorld* World)
{
    return World && IsInGameThread() && ConveyorReplacementWorld == World;
}

// SP/MP DIVERGENCE MAP — Conveyor Upgrade
//  1. Identity — local UI allocates once; RPC retains identity and supplies its authoritative requester.
//  2. Mutation — [MP-AUTH] shared native pre-handler owns the whole lower-level replacement transaction.
//  3. Result — [MP-CLIENT] correlated pending/terminal results cannot complete a newer local request.

FGuid USFUpgradeExecutionService::GetAuthorityWorldGeneration() const
{
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
    return Coordinator && World && World->HasBegunPlay() && !World->bIsTearingDown && World->GetNetMode() != NM_Client
        ? Coordinator->GetWorldGeneration() : FGuid();
}

bool USFUpgradeExecutionService::EnsureUpgradeWorldContext(AFGPlayerController* Requester)
{
    UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
    if (!World || !World->HasBegunPlay() || World->bIsTearingDown || !IsValid(Requester)
        || Requester->GetWorld() != World || !Requester->IsLocalController()) return false;
    if (World->GetNetMode() != NM_Client) return GetAuthorityWorldGeneration().IsValid();
    if (UpgradeContextRequester.Get() != Requester)
    {
        UpgradeContextRequester = Requester;
        UpgradeContextChallenge = FGuid::NewGuid();
        RemoteAuthorityWorldGeneration.Invalidate();
    }
    if (RemoteAuthorityWorldGeneration.IsValid()) return true;
    if (auto* RCO = Requester->GetRemoteCallObjectOfClass<USFRCO>())
        RCO->Server_RequestUpgradeWorldContext(UpgradeContextChallenge);
    return false; // Never hold an intent or auto-submit it after synchronization.
}

void USFUpgradeExecutionService::AcceptUpgradeWorldContext(const FGuid& Challenge, const FGuid& Generation)
{
    UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
    AFGPlayerController* Requester = UpgradeContextRequester.Get();
    if (!World || !World->HasBegunPlay() || World->bIsTearingDown || World->GetNetMode() != NM_Client || !IsValid(Requester)
        || Requester->GetWorld() != World || !Requester->IsLocalController()
        || !Challenge.IsValid() || Challenge != UpgradeContextChallenge || !Generation.IsValid()
        || RemoteAuthorityWorldGeneration.IsValid()) return; // A late response cannot rebind an already captured context.
    RemoteAuthorityWorldGeneration = Generation;
}

bool USFUpgradeExecutionService::AssignRequestIdentity(FSFUpgradeExecutionParams& Params)
{
    if (!IsInGameThread() || !Subsystem || !IsValid(Params.PlayerController)) return false;
    if (!Params.RequestId.IsValid() && Params.RequestSequence == 0)
    {
        UWorld* World = Subsystem->GetWorld();
        if (!World) return false;
        if (World->GetNetMode() == NM_Client && !EnsureUpgradeWorldContext(Params.PlayerController)) return false;
        Params.AuthorityWorldGeneration = World->GetNetMode() == NM_Client
            ? RemoteAuthorityWorldGeneration : GetAuthorityWorldGeneration();
        Params.RequestSequence = Subsystem->AllocateConstructionRequestSequence();
        Params.RequestId = FGuid::NewGuid();
    }
    if (!Params.RequestId.IsValid() || Params.RequestSequence <= 0 || !Params.AuthorityWorldGeneration.IsValid()) return false;
    ExpectedClientRequestId = Params.RequestId;
    ExpectedClientWorldGeneration = Params.AuthorityWorldGeneration;
    ExpectedClientSequence = Params.RequestSequence;
    ExpectedClientRequester = Params.PlayerController;
    return true;
}

void USFUpgradeExecutionService::CancelUpgradeRequest(AFGPlayerController* Requester, const FGuid& RequestId, int64 Sequence)
{
    if (!ConveyorOwner || !ConveyorOwner->Active || !IsValid(Requester) || !Requester->HasAuthority()) return;
    auto Operation = ConveyorOwner->Active;
    if (Operation->Key.Requester != FObjectKey(Requester) || Operation->Key.Sequence != static_cast<uint64>(Sequence)
        || Operation->Result.RequestId != RequestId) return;
    if (auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr)
        Coordinator->RequestCancellation(Operation->Key);
}

void USFUpgradeExecutionService::SendUpgradeResult(AFGPlayerController* Requester, const FSFUpgradeExecutionResult& Result)
{
    if (!IsValid(Requester)) return;
    if (!Requester->IsLocalController())
    {
        if (auto* RCO = Requester->GetRemoteCallObjectOfClass<USFRCO>()) RCO->Client_ReceiveUpgradeResult(Result);
        return;
    }
    if (Result.RequestId != ExpectedClientRequestId || Result.RequestSequence != ExpectedClientSequence
        || Result.AuthorityWorldGeneration != ExpectedClientWorldGeneration) return;
    LastResult = Result;
    if (!Result.bPending) OnUpgradeExecutionCompleted.Broadcast(LastResult);
}

bool USFUpgradeExecutionService::HandleConveyorReplay(const FSFUpgradeExecutionParams& Params)
{
    if (!ConveyorOwner || !IsValid(Params.PlayerController) || Params.RequestSequence <= 0) return false;
    const FSFConstructionOperationKey Key{FObjectKey(Params.PlayerController), static_cast<uint64>(Params.RequestSequence)};
    const auto Replay = ConveyorOwner->ClassifyReplay(Key, Params);
    if (!Replay.IsSet()) return false;
    FSFUpgradeExecutionResult Rejected;
    Rejected.RequestId = Params.RequestId; Rejected.RequestSequence = Params.RequestSequence;
    Rejected.AuthorityWorldGeneration = Params.AuthorityWorldGeneration;
    if (const auto* Prior = ConveyorOwner->Requests.Find(Key))
    {
        if (Replay.GetValue() == ESFConstructionAdmission::Existing)
            SendUpgradeResult(Params.PlayerController, Prior->Result);
        else
        {
            Rejected.ErrorMessage = TEXT("Request identity was reused with different upgrade intent");
            SendUpgradeResult(Params.PlayerController, Rejected);
        }
        return true;
    }
    Rejected.ErrorMessage = TEXT("This upgrade request has expired; submit a new request");
    SendUpgradeResult(Params.PlayerController, Rejected);
    return true;
}

void USFUpgradeExecutionService::StartOwnedConveyorUpgrade()
{
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    AFGPlayerController* Requester = CurrentParams.PlayerController;
    // [MP-AUTH] Never derive the payer from the first player on a dedicated server.
    if (!Coordinator || !IsValid(Requester) || !Requester->HasAuthority()) return;
    if (!ConveyorOwner) ConveyorOwner = MakeShared<FSFConveyorUpgradeOwner>();
    auto Operation = MakeShared<FSFConveyorUpgradeOwner::FOperation>();
    Operation->Key = {FObjectKey(Requester), static_cast<uint64>(CurrentParams.RequestSequence)};
    Operation->WorldGeneration = Coordinator->GetWorldGeneration();
    Operation->Requester = Requester;
    Operation->Payer = Requester->GetPawn();
    Operation->Result = WorkingResult;
    Operation->Result.bPending = true;
    ConveyorOwner->Remember(Operation->Key, CurrentParams);
    ConveyorOwner->Active = Operation;
    bUpgradeInProgress = true;
    FSFConstructionOperationRequest Request;
    Request.Requester = Requester;
    Request.Sequence = Operation->Key.Sequence;
    Request.RequestId = CurrentParams.RequestId;
    Request.WorldGeneration = Operation->WorldGeneration;
    // SameIntent binds the full immutable original payload before any replay reaches Submit.
    Request.IntentDigest = CurrentParams.RequestId.ToString();
    auto Add = [&Request](AActor* Actor)
    {
        Request.ExpectedCohort.Add(FObjectKey(Actor)); Request.AffectedActors.AddUnique(Actor);
    };
    Add(Requester);
    if (Requester->GetPawn()) Add(Requester->GetPawn());
    for (AFGBuildable* Target : PendingUpgrades)
    {
        if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Target)) { Operation->Selected.Add(Conveyor); Add(Conveyor); }
        else
        {
            Operation->Result.ErrorMessage = TEXT("Conveyor replacement must use a separate operation from other upgrade families");
            FinishOwnedConveyorUpgrade(static_cast<uint8>(ESFConstructionOperationState::CancelledBeforeEffects));
            return;
        }
    }
    TWeakObjectPtr<USFUpgradeExecutionService> WeakThis(this);
    const auto Admission = Coordinator->Submit(Request,
        [WeakThis](FSFConstructionOperationRecord& Record)
        {
            if (auto* Service = WeakThis.Get()) Service->RunOwnedConveyorUpgrade(Record);
        },
        [WeakThis](FSFConstructionOperationRecord& Record)
        {
            if (auto* Service = WeakThis.Get()) Service->ObserveOwnedConveyorUpgrade(Record);
        },
        [WeakThis](ESFConstructionOperationState State)
        {
            if (auto* Service = WeakThis.Get()) Service->FinishOwnedConveyorUpgrade(static_cast<uint8>(State));
        });
    if (Admission != ESFConstructionAdmission::Accepted)
    {
        Operation->Result.ErrorMessage = TEXT("Conveyor operation could not acquire its native operation owner; no changes applied");
        FinishOwnedConveyorUpgrade(static_cast<uint8>(ESFConstructionOperationState::CancelledBeforeEffects));
    }
    else
    {
        // Admission is asynchronous. Publish this request's pending identity now,
        // rather than leaving getters/clients with the preceding operation's result.
        SendUpgradeResult(Requester, Operation->Result);
    }
}

void USFUpgradeExecutionService::RunOwnedConveyorUpgrade(FSFConstructionOperationRecord& Record)
{
    auto Operation = ConveyorOwner ? ConveyorOwner->Active : nullptr;
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Operation || !Coordinator || !Coordinator->IsInMutationPhase()) return;
    FString Reason;
    auto Reject = [&](const FString& Message)
    {
        Operation->Result.ErrorMessage = Message;
        Record.RequestCancellation(Operation->WorldGeneration);
        Record.SettleCancellation(Operation->WorldGeneration);
    };
    const auto* Request = ConveyorOwner->Requests.Find(Operation->Key);
    if (!Request || !FSFConveyorUpgradeOwner::SelectionWithinIntent(Request->Intent, Operation->Selected))
    {
        Reject(TEXT("The conveyor replacement selection no longer matches the admitted request"));
        return;
    }
    const auto Status = FSFConveyorCohortValidator::Capture(*Coordinator, Operation->Selected, Operation->Before, Reason);
    if (Status != ESFConveyorCohortStatus::CoherentStructure) { Reject(Reason); return; }
    for (const auto& Actor : Operation->Before.AffectedActors)
        if (!Coordinator->AdoptActor(Operation->Key, Actor.Get())) { Reject(TEXT("Affected conveyor owners conflict with another operation")); return; }
    for (const auto& Pair : Operation->Before.Members)
    {
        auto* Member = Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr());
        auto* Designer = Member ? Member->GetBlueprintDesigner() : nullptr;
        Operation->OriginalDesigners.Add(Pair.Key, Designer);
        if (Designer && (!IsValid(Designer) || !Coordinator->AdoptActor(Operation->Key, Designer)))
        { Reject(TEXT("Conveyor Designer ownership conflicts with another operation")); return; }
    }
    if (CurrentParams.bPreserveDismantleGroupMembership)
        for (const auto& Pair : Operation->Before.Members)
        {
            auto* Member = Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr());
            auto* Group = Member ? Member->GetBlueprintProxy() : nullptr;
            Operation->OriginalGroups.Add(Pair.Key, Group);
            if (Group && !Coordinator->AdoptActor(Operation->Key, Group))
            { Reject(TEXT("Native dismantle-group ownership conflicts with another operation")); return; }
        }
    if (FSFConveyorCohortValidator::SnapshotCargo(*Coordinator, Operation->Before, Operation->BeforeCargo, Reason)
        != ESFConveyorCohortStatus::CoherentStructure) { Reject(Reason); return; }
    for (const auto& Pair : Operation->Before.Chains)
        if (Pair.Value.CargoSlots.Num() >= Pair.Value.CargoLayout.AllocatedCapacity)
        { Reject(TEXT("Source cargo has no spare native migration capacity; ordinary replacement is unsafe")); return; }
    TGuardValue<const UWorld*> IsolatedPreview(ConveyorReplacementWorld, Coordinator->GetWorld());
    auto* Player = Cast<AFGCharacterPlayer>(Operation->Payer.Get());
    if (Operation->Requester.IsValid() && Operation->Requester->GetPawn() != Player)
    { Reject(TEXT("The admitted conveyor payer changed before execution")); return; }
    if (!Player || !Player->GetInventory() || !Player->GetBuildGun()) { Reject(TEXT("The exact requesting player is unavailable")); return; }
    Operation->bFreeBuild = Player->GetInventory()->GetNoBuildCost();
    TSet<FObjectKey> Selected;
    TMap<TSubclassOf<UFGItemDescriptor>, int64> ProjectedAvailable;
    for (const auto& Weak : Operation->Selected)
    {
        auto* Target = Weak.Get();
        if (!IsValid(Target) || Selected.Contains(FObjectKey(Target))) { Reject(TEXT("Stale or duplicate conveyor target")); return; }
        Selected.Add(FObjectKey(Target));
        const auto Recipe = GetTargetRecipeForBuildable(Target, CurrentTargetRecipe);
        if (auto* Lift = Cast<AFGBuildableConveyorLift>(Target))
            if (!SFConveyorLiftUpgradeContract::Capture(Lift, *Operation, *Coordinator, Reason)) { Reject(Reason); return; }
        if (!PreflightConveyorUpgrade(Target, Recipe, Reason)) { Reject(Reason); return; }
        if (const auto* Plan = Operation->Companions.Find(FObjectKey(Target)))
            for (const auto& Child : Plan->Old)
                for (const auto& Stack : Child.Refund)
                    if (Stack.Count > 0 && Stack.Item)
                    {
                        const auto Descriptor = Stack.Item->Get().GetItemClass();
                        Operation->MaterialsBefore.FindOrAdd(Descriptor) = ConveyorMaterialCount(Player, Descriptor);
                    }
        FSFUpgradeSettlementLedger Ledger;
        // Full target debit precedes construction; original refund is earned only by retirement.
        if (PrepareConveyorSettlement(Target, Recipe, Player, Ledger) == 0) { Reject(TEXT("Conveyor plan cannot be priced")); return; }
        for (const auto& Entry : Ledger.NetCharge)
        {
            if (!ProjectedAvailable.Contains(Entry.Key)) ProjectedAvailable.Add(Entry.Key, ConveyorMaterialCount(Player, Entry.Key));
            Operation->MaterialsBefore.FindOrAdd(Entry.Key) = ConveyorMaterialCount(Player, Entry.Key);
            if (ProjectedAvailable[Entry.Key] < Entry.Value) { Reject(TEXT("Full target materials must be reserved before each conveyor replacement")); return; }
            ProjectedAvailable[Entry.Key] -= Entry.Value;
        }
        for (const auto& Entry : Ledger.NetRefund)
        {
            if (!ProjectedAvailable.Contains(Entry.Key)) ProjectedAvailable.Add(Entry.Key, ConveyorMaterialCount(Player, Entry.Key));
            Operation->MaterialsBefore.FindOrAdd(Entry.Key) = ConveyorMaterialCount(Player, Entry.Key);
            ProjectedAvailable[Entry.Key] += Entry.Value;
        }
    }
    for (const auto& Pair : Operation->Before.Members)
    {
        auto* Member = Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr());
        Operation->OwnedMembers.Add(Pair.Key);
        for (auto* Port : {Member->GetConnection0(), Member->GetConnection1()})
        {
            FSFConveyorUpgradeOwner::FEdge Edge;
            Edge.Owner = Pair.Key; Edge.Port = Port->GetFName(); Edge.OriginalPort = Port;
            Edge.Position = Port->GetComponentLocation(); Edge.Direction = static_cast<uint8>(Port->GetDirection());
            if (auto* Peer = Port->GetConnection())
            {
                Edge.PeerOwner = FObjectKey(Peer->GetOwner()); Edge.PeerPort = Peer->GetFName(); Edge.OriginalPeer = Peer;
                Edge.PeerPosition = Peer->GetComponentLocation(); Edge.PeerDirection = static_cast<uint8>(Peer->GetDirection());
            }
            Operation->Edges.Add(MoveTemp(Edge));
        }
    }
    // Keep one unresolved effect through native migration; source-loop return is not completion.
    if (!Record.ClaimEffect(Operation->WorldGeneration, TEXT("NativeSettlement"))) { Reject(TEXT("Operation settlement was already claimed")); return; }
    Operation->bEffectsStarted = true;
    Operation->bExecuting = true;
    Operation->ExecutingRecord = &Record;
    ExecuteUpgradeTargets();
    Operation->ExecutingRecord = nullptr;
    Operation->bExecuting = false;
    const FString PriorError = Operation->Result.ErrorMessage;
    Operation->Result = WorkingResult;
    Operation->Result.bPending = true;
    if (!PriorError.IsEmpty()) Operation->Result.ErrorMessage = PriorError;
    for (const auto& Pair : Operation->Before.Members)
        if (!Operation->RetiredCompanions.Contains(Pair.Key))
            if (auto* Member = Cast<AFGBuildableConveyorBase>(ResolveConveyorOwner(*Operation, Pair.Key))) Operation->Remaining.AddUnique(Member);

    FSFConveyorCohort After;
    FSFConveyorCohort::FCargoSnapshot AfterCargo;
    const auto AfterState = FSFConveyorCohortValidator::Capture(*Coordinator, Operation->Remaining, After, Reason);
    if (AfterState != ESFConveyorCohortStatus::CoherentStructure && AfterState != ESFConveyorCohortStatus::NativePending)
        Operation->bUncertain = true;
    else if (FSFConveyorCohortValidator::SnapshotCargo(*Coordinator, After, AfterCargo, Reason) == ESFConveyorCohortStatus::CoherentStructure)
    {
        int32 ExpectedOwners = Operation->BeforeCargo.Num();
        int32 ActualOwners = AfterCargo.Num();
        for (const auto& Key : Operation->RetiredCompanions)
        {
            ExpectedOwners -= Operation->BeforeCargo.Contains(Key);
            ActualOwners -= AfterCargo.Contains(Key);
        }
        // Native dismantle effects may retain a logically retired child until
        // EndPlay. Its full refund is separate; lifecycle observation must still
        // prove retirement before the operation can complete.
        bool bMatches = ActualOwners == ExpectedOwners;
        for (const auto& Pair : Operation->BeforeCargo)
        {
            if (Operation->RetiredCompanions.Contains(Pair.Key)) continue; // Full native refund is accounted separately.
            AActor* Owner = ResolveConveyorOwner(*Operation, Pair.Key);
            const auto* Actual = IsValid(Owner) ? AfterCargo.Find(FObjectKey(Owner)) : nullptr;
            if (!Actual || Actual->Num() != Pair.Value.Num()) { bMatches = false; break; }
            for (int32 Index = 0; Index < Actual->Num(); ++Index)
                bMatches &= (*Actual)[Index].SameValue(Pair.Value[Index])
                    && FMath::IsNearlyEqual((*Actual)[Index].Offset, Pair.Value[Index].Offset, 0.01f);
        }
        Operation->bCargoVerified = bMatches;
        if (!bMatches) Reason = TEXT("Immediate cargo owner, order or offset differs from the admitted ledger");
    }
    if (!Operation->bCargoVerified) Operation->bUncertain = true;
    if (!FSFConveyorCohortValidator::VerifyBoundaryInventories(*Coordinator, Operation->Before, Reason, &Operation->RetiredCompanions))
    { Operation->bCargoVerified = false; Operation->bUncertain = true; }
    // Earned refunds and proved failed-construction returns remain owed even if
    // a separate cargo/topology observation failed. Deliver their native overflow
    // crate once instead of abandoning those materials in an in-memory map.
    Operation->bResourcesVerified = SettleConveyorResources();
    if (!Operation->bResourcesVerified) Operation->bUncertain = true;
    if (Operation->bUncertain)
    {
        if (Operation->Result.ErrorMessage.IsEmpty()) Operation->Result.ErrorMessage = Reason.IsEmpty()
            ? TEXT("Conveyor settlement requires reconciliation; no retry was performed") : Reason;
        Record.ObserveEffect(Operation->WorldGeneration, TEXT("NativeSettlement"), ESFConstructionEffectOutcome::Uncertain);
        Record.RequestCancellation(Operation->WorldGeneration);
    }
}

void USFUpgradeExecutionService::ObserveOwnedConveyorUpgrade(FSFConstructionOperationRecord& Record)
{
    auto Operation = ConveyorOwner ? ConveyorOwner->Active : nullptr;
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Operation || !Coordinator) return;
    if (!Operation->bEffectsStarted) { Record.SettleCancellation(Operation->WorldGeneration); return; }
    auto ReportUncertain = [&]()
    {
        if (Operation->bReportedUncertain) return;
        Operation->bReportedUncertain = true;
        Operation->Result.bUncertain = true; Operation->Result.bPending = true;
        ConveyorOwner->Requests.FindChecked(Operation->Key).Result = Operation->Result;
        UE_LOG(LogSmartUpgrade, Warning, TEXT("Conveyor upgrade %s remains unsettled: %s"),
            *Operation->Result.RequestId.ToString(), *Operation->Result.ErrorMessage);
        SendUpgradeResult(Operation->Requester.Get(), Operation->Result);
    };
    if (Operation->bUncertain) { ReportUncertain(); return; }
    if (SFConveyorUpgradeCompanions::HasPendingLifecycle(*Operation) && ++Operation->ObservedNativePhases < 120) return;
    bool bTopology = SFConveyorLiftUpgradeContract::Verify(*Operation)
        && SFConveyorUpgradeCompanions::Verify(*Operation);
    for (const auto& Pair : Operation->OriginalDesigners)
    {
        if (Operation->RetiredCompanions.Contains(Pair.Key)) continue;
        auto* Actor = Cast<AFGBuildable>(ResolveConveyorOwner(*Operation, Pair.Key));
        bTopology &= IsValid(Actor) && !Pair.Value.IsStale() && Actor->GetBlueprintDesigner() == Pair.Value.Get();
        if (auto* Designer = Pair.Value.Get(); Designer && IsValid(Actor))
        {
            int32 Occurrences = 0;
            for (auto* Member : Designer->GetBuildablesInBlueprintDesigner()) Occurrences += Member == Actor;
            bTopology &= Occurrences == 1 && Actor->GetBlueprintProxy() == nullptr;
        }
    }
    for (const auto& Edge : Operation->Edges)
    {
        if (Operation->RetiredCompanions.Contains(Edge.Owner)) continue;
        const bool bRetiredPeer = Edge.PeerOwner.IsSet() && Operation->RetiredCompanions.Contains(Edge.PeerOwner.GetValue());
        auto* Port = FindConveyorPort(*Operation, Edge.Owner, Edge.OriginalPort, Edge.Position, Edge.Direction);
        auto* Peer = Edge.PeerOwner.IsSet() && !bRetiredPeer ? FindConveyorPort(*Operation, Edge.PeerOwner.GetValue(), Edge.OriginalPeer, Edge.PeerPosition, Edge.PeerDirection) : nullptr;
        bTopology &= Port && (!Edge.PeerOwner.IsSet() || bRetiredPeer || Peer) && Port->GetConnection() == Peer && Port->IsConnected() == (Peer != nullptr)
            && (!Peer || (Peer->GetConnection() == Port && Peer->IsConnected()));
    }
    for (const auto& Pair : Operation->OriginalGroups)
    {
        if (Operation->RetiredCompanions.Contains(Pair.Key)) continue;
        auto* Actor = Cast<AFGBuildable>(ResolveConveyorOwner(*Operation, Pair.Key));
        auto* Group = Pair.Value.Get();
        bTopology &= IsValid(Actor) && Actor->GetBlueprintProxy() == Group && !Pair.Value.IsStale();
        if (Group && Actor)
        {
            int32 Occurrences = 0;
            for (AFGBuildable* Member : Group->GetBuildables()) Occurrences += Member == Actor;
            bTopology &= Occurrences == 1;
        }
    }
    FSFConveyorCohort Settled;
    FString Reason;
    const auto Status = FSFConveyorCohortValidator::Capture(*Coordinator, Operation->Remaining, Settled, Reason);
    if (Status == ESFConveyorCohortStatus::NativePending && ++Operation->ObservedNativePhases < 120) return;
    if (!bTopology || Status != ESFConveyorCohortStatus::CoherentStructure)
    {
        Operation->bUncertain = true;
        Operation->Result.ErrorMessage = bTopology ? Reason : TEXT("Native replacement did not preserve the expected reciprocal graph");
        Record.ObserveEffect(Operation->WorldGeneration, TEXT("NativeSettlement"), ESFConstructionEffectOutcome::Uncertain);
        Record.RequestCancellation(Operation->WorldGeneration); ReportUncertain(); return;
    }
    Record.ObserveEffect(Operation->WorldGeneration, TEXT("NativeSettlement"), ESFConstructionEffectOutcome::Applied);
    for (auto Obligation : {ESFConstructionObligation::Topology, ESFConstructionObligation::NativeMembership,
        ESFConstructionObligation::Cargo, ESFConstructionObligation::DeferredWork, ESFConstructionObligation::Resources})
        Record.VerifyObligation(Operation->WorldGeneration, Obligation);
    if (!Record.Complete(Operation->WorldGeneration)) Record.SettleCancellation(Operation->WorldGeneration);
}

void USFUpgradeExecutionService::FinishOwnedConveyorUpgrade(uint8 State)
{
    auto Operation = ConveyorOwner ? ConveyorOwner->Active : nullptr;
    if (!Operation) return;
    auto Result = Operation->Result;
    Result.bPending = false;
    Result.bCompleted = State == static_cast<uint8>(ESFConstructionOperationState::Completed);
    Result.bUncertain = State == static_cast<uint8>(ESFConstructionOperationState::Uncertain);
    if (!Result.bCompleted && Result.ErrorMessage.IsEmpty())
        Result.ErrorMessage = Result.bUncertain
            ? TEXT("Conveyor operation ended with unresolved effects; completion and rollback are not established")
            : State == static_cast<uint8>(ESFConstructionOperationState::PartiallyApplied)
                ? TEXT("Conveyor operation cancelled after partial replacement; applied effects have settled")
                : TEXT("Conveyor operation cancelled before changes");
    const auto Requester = Operation->Requester;
    ConveyorOwner->Settle(Operation->Key, Result);
    ConveyorOwner->Active.Reset();
    bUpgradeInProgress = false;
    PendingUpgrades.Reset(); OldToNewConveyorMap.Reset(); OldToNewBuildableMap.Reset();
    ExpectedConnectionEdges.Reset();
    SavedConnectionPairs.Reset(); SavedPipeConnectionPairs.Reset();
    OverflowItems.Reset(); BatchChargedTotals.Reset(); BatchRefundedTotals.Reset();
    UE_LOG(LogSmartUpgrade, Log, TEXT("Conveyor upgrade %s: world=%s sequence=%llu state=%u selected=%d processed=%d replaced=%d failed=%d"),
        *Result.RequestId.ToString(), *Operation->WorldGeneration.ToString(), Operation->Key.Sequence,
        State, Operation->Selected.Num(), Result.TotalProcessed, Result.SuccessCount, Result.FailCount);
    SendUpgradeResult(Requester.Get(), Result);
}

bool USFUpgradeExecutionService::ReserveConveyorCharge(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* PlayerChar)
{
    auto Operation = ConveyorOwner ? ConveyorOwner->Active : nullptr;
    if (!Operation || !Operation->bExecuting || !Operation->ExecutingRecord || !PlayerChar || PlayerChar != Operation->Payer.Get()) return false;
    if (!Operation->ReservedCharges.IsEmpty())
    {
        Operation->bUncertain = true;
        Operation->Result.ErrorMessage = TEXT("The preceding conveyor reservation has not been consumed or returned");
        return false;
    }
    auto* Inventory = PlayerChar->GetInventory();
    if (!Inventory || Ledger.bFreeBuild != Operation->bFreeBuild || Inventory->GetNoBuildCost() != Operation->bFreeBuild)
    { Operation->bUncertain = true; Operation->Result.ErrorMessage = TEXT("Conveyor payment policy changed during execution"); return false; }
    if (Ledger.bFreeBuild) return true;
    TArray<FInventoryStack> Stock;
    Inventory->GetInventoryStacks(Stock);
    for (const auto& Stack : Stock)
        if (Ledger.NetCharge.Contains(Stack.Item.GetItemClass())
            && (Stack.Item.HasState() || Stack.Item.LegacyItemStateActor != nullptr))
        {
            Operation->Result.ErrorMessage = TEXT("Stateful construction materials cannot use a stateless reservation/refund");
            return false;
        }
    auto* Storage = AFGCentralStorageSubsystem::Get(PlayerChar->GetWorld());
    auto* State = PlayerChar->GetPlayerState<AFGPlayerState>();
    const bool bInventoryFirst = !State || State->GetTakeFromInventoryBeforeCentralStorage();
    for (const auto& Entry : Ledger.NetCharge)
    {
        if (!Entry.Key || Entry.Value <= 0 || !Operation->MaterialsBefore.Contains(Entry.Key))
        { Operation->bUncertain = true; return false; }
        const int64 Before = int64(Inventory->GetNumItems(Entry.Key)) + (Storage ? Storage->GetNumItemsFromCentralStorage(Entry.Key) : 0);
        UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(Inventory, Storage, bInventoryFirst, Entry.Key, Entry.Value);
        const int64 After = int64(Inventory->GetNumItems(Entry.Key)) + (Storage ? Storage->GetNumItemsFromCentralStorage(Entry.Key) : 0);
        const int64 Debited = Before - After;
        if (Debited >= 0 && Debited <= Entry.Value)
        {
            BatchChargedTotals.FindOrAdd(Entry.Key) += static_cast<int32>(Debited);
            if (Debited > 0) Operation->ReservedCharges.FindOrAdd(Entry.Key) += static_cast<int32>(Debited);
        }
        if (Debited != Entry.Value)
        {
            Operation->Result.ErrorMessage = TEXT("Materials changed before construction; only the measured reservation is returned");
            if (Debited >= 0 && Debited <= Entry.Value) RollbackUpgradeCharge(Ledger, PlayerChar);
            else
            {
                Operation->bUncertain = true;
                Operation->Result.ErrorMessage = TEXT("Material ownership changed unexpectedly during debit; no speculative refund was issued");
            }
            return false;
        }
    }
    return true;
}

bool USFUpgradeExecutionService::PrepareConveyorUpgradeHologram(AFGBuildableConveyorBase* Old, AFGHologram* Holo, FString& Reason)
{
    auto* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    auto Fail = [&Reason](const TCHAR* Message) { Reason = Message; return false; };
    auto* Player = CurrentParams.PlayerController ? Cast<AFGCharacterPlayer>(CurrentParams.PlayerController->GetPawn()) : nullptr;
    if (!Coordinator || !Coordinator->IsInMutationPhase() || !IsValid(Old) || !IsValid(Holo)
        || Old->GetWorld() != Coordinator->GetWorld() || Holo->GetWorld() != Coordinator->GetWorld()
        || !Old->HasAuthority() || !Holo->HasAuthority() || Old->IsActorBeingDestroyed() || Holo->IsActorBeingDestroyed()
        || !Holo->HasActorBegunPlay() || !Player || !Player->GetInventory())
        return Fail(TEXT("Native replacement hologram is not initialized in the admitted authority phase"));
    if (auto* Lift = Cast<AFGBuildableConveyorLift>(Old))
        if (!SFConveyorLiftUpgradeContract::Prepare(Lift, Holo, Reason)) return false;
    auto* Component = Old->GetComponentByClass<UPrimitiveComponent>();
    if (!IsValid(Component) || Component->GetOwner() != Old || !Component->IsRegistered())
        return Fail(TEXT("Selected conveyor has no live registered hit component"));
    FHitResult Hit(Old, Component, Old->GetActorLocation(), Old->GetActorRotation().Vector());
    Hit.bBlockingHit = true;
    if (!Holo->IsValidHitResult(Hit)) return Fail(TEXT("Native placement rejected the selected conveyor hit"));

    // Native gun TickState resets each placement frame, updates hit/visibility and
    // Pre/TryUpgrade/Post placement, then validates. TryUpgrade alone leaves the
    // initial disqualifiers and hidden-actor invalid-aim state in place.
    Holo->ResetConstructDisqualifiers();
    Holo->UpdateHologramPlacement(Hit);
    if (auto* Belt = Cast<AFGConveyorBeltHologram>(Holo); Belt && Holo->GetUpgradedActor() == Old)
        Belt->GenerateAndUpdateSpline(Hit);
    if (!Holo->IsUpgrade() || Holo->GetUpgradedActor() != Old)
        return Fail(TEXT("Native conveyor upgrade did not retain the selected source"));
    TInlineComponentArray<UFGFactoryConnectionComponent*> SourcePorts(Old);
    if (SourcePorts.Num() != 2 || !SourcePorts.Contains(Old->GetConnection0())
        || !SourcePorts.Contains(Old->GetConnection1()) || Old->GetConnection0() == Old->GetConnection1())
        return Fail(TEXT("Native conveyor message initialization requires two unique source ports"));
    // Authority creates this hologram from live value intent, without a wire archive.
    // The native paired hooks cache/restore its pose and derive the old actor's peer
    // targets for ConfigureComponents, before the replacement begins play.
    Holo->PreConstructMessageSerialization();
    Holo->PostConstructMessageDeserialization();
    int32 PlacementSteps = 0;
    while (Holo->CanTakeNextBuildStep() && !Holo->DoMultiStepPlacement(true))
        if (++PlacementSteps >= 8) return Fail(TEXT("Native replacement did not finish its bounded placement steps"));
    if (auto* Belt = Cast<AFGConveyorBeltHologram>(Holo)) Belt->GenerateAndUpdateSpline(Hit);
    if (!Holo->GetActorTransform().Equals(Old->GetActorTransform(), 0.1f))
        return Fail(TEXT("Native conveyor message initialization changed the selected pose"));
    Holo->ValidatePlacementAndCost(Player->GetInventory());
    if (auto* Lift = Cast<AFGBuildableConveyorLift>(Old))
        if (!SFConveyorLiftUpgradeContract::MatchesHologram(Lift, Holo))
            return Fail(TEXT("Native replacement preparation changed the lift's signed geometry or exact floor-hole slots"));
    const auto Family = USFUpgradeTraversalService::GetUpgradeFamily(Old);
    FString Failure;
    if (!Holo->IsUpgrade() || Holo->GetUpgradedActor() != Old)
        Failure = TEXT("Native conveyor upgrade did not retain the selected source");
    else if (Holo->GetBuildClass() != GetBuildableClass(Family, CurrentParams.TargetTier))
        Failure = TEXT("The target recipe did not produce the requested conveyor tier");
    else if (!Holo->CanConstruct())
    {
        Failure = TEXT("Native placement rejected the conveyor replacement.");
        TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
        Holo->GetConstructDisqualifiers(Disqualifiers);
        bool bHasHardReason = false;
        for (const auto& Disqualifier : Disqualifiers)
        {
            if (Disqualifier && !UFGConstructDisqualifier::GetIsSoftDisqualifier(Disqualifier))
            {
                Failure += TEXT(" ") + UFGConstructDisqualifier::GetDisqualifyingText(Disqualifier).ToString();
                bHasHardReason = true;
            }
        }
        if (!bHasHardReason) Failure += TEXT(" Native validation provided no blocking reason.");
    }
    if (!Failure.IsEmpty()) { Reason = MoveTemp(Failure); return false; }
    if (ConveyorOwner && ConveyorOwner->Active
        && !SFConveyorUpgradeCompanions::MatchesPrepared(Old, Holo, *ConveyorOwner->Active))
        return Fail(TEXT("Native conveyor companion geometry or materials changed before construction"));
    return true;
}

bool USFUpgradeExecutionService::PreflightConveyorUpgrade(AFGBuildableConveyorBase* Old, TSubclassOf<UFGRecipe> Recipe, FString& Reason)
{
    auto Fail = [&Reason](const TCHAR* Message) { Reason = Message; return false; };
    const auto Family = USFUpgradeTraversalService::GetUpgradeFamily(Old);
    const int32 Tier = USFUpgradeTraversalService::GetBuildableTier(Old);
    if (!IsValid(Old) || Old->IsActorBeingDestroyed() || Old->GetClass() != GetBuildableClass(Family, Tier)
        || !FMath::IsFinite(Old->GetLength()) || Old->GetLength() <= 0
        || Old->GetIsDismantled() || Old->IsAboutToBeDismantled())
        return Fail(TEXT("Conveyor class, geometry or retirement state is incompatible with native replacement"));
    auto* Manager = AFGRecipeManager::Get(Old->GetWorld());
    auto* Player = Cast<AFGCharacterPlayer>(CurrentParams.PlayerController->GetPawn());
    if (!Manager || !Recipe || !Manager->IsRecipeAvailable(Recipe) || !Player) return Fail(TEXT("Target recipe or payer unavailable"));
    AFGHologram* Holo = AFGHologram::SpawnHologramFromRecipe(Recipe, Player->GetBuildGun(), Old->GetActorLocation(), Player);
    if (!Holo) return Fail(TEXT("Cannot prepare native replacement hologram"));
    Holo->SetInsideBlueprintDesigner(Old->GetBlueprintDesigner());
    bool bValid = PrepareConveyorUpgradeHologram(Old, Holo, Reason);
    if (bValid) bValid = SFConveyorUpgradeCompanions::Capture(Old, Holo, *ConveyorOwner->Active,
        *Subsystem->GetConstructionCoordinator(), Reason);
    Holo->Destroy();
    return bValid;
}

bool USFUpgradeExecutionService::AdoptConveyorReplacement(AFGBuildableConveyorBase* Old,
    AFGBuildableConveyorBase* Replacement, const TArray<AActor*>& Children)
{
    auto Operation = ConveyorOwner->Active;
    auto* Coordinator = Subsystem->GetConstructionCoordinator();
    bool bValid = IsValid(Replacement) && Replacement->HasActorBegunPlay() && !Replacement->IsActorBeingDestroyed()
        && Coordinator->AdoptActor(Operation->Key, Replacement);
    if (bValid)
    {
        Operation->Replacements.Add(FObjectKey(Old), Replacement);
        Operation->OwnedMembers.Add(FObjectKey(Replacement));
        bValid = Replacement->GetClass() == GetBuildableClass(USFUpgradeTraversalService::GetUpgradeFamily(Old), CurrentParams.TargetTier)
            && FMath::IsNearlyEqual(Old->GetLength(), Replacement->GetLength(), 0.1f)
            && Replacement->GetActorTransform().Equals(Old->GetActorTransform(), 0.1f);
    }
    for (auto* Child : Children) if (IsValid(Child)) Coordinator->AdoptActor(Operation->Key, Child);
    FString CompanionReason;
    if (bValid) bValid = SFConveyorUpgradeCompanions::Adopt(Old, Children, *Operation, *Coordinator,
        CurrentParams.bPreserveDismantleGroupMembership, CompanionReason);
    if (!bValid)
    {
        RetainUnexpectedConveyorConstruction(Replacement, Children);
        Operation->Result.ErrorMessage = CompanionReason.IsEmpty()
            ? TEXT("Native construction returned an unexpected identity, geometry, lifecycle or child set") : CompanionReason;
        return false;
    }
    // A returned live target now owns this paid reservation. Failed construction
    // is the only path permitted to return it; retirement earns the old refund.
    Operation->ReservedCharges.Reset();
    return true;
}

bool USFUpgradeExecutionService::PrepareConveyorCargoTransfer(AFGBuildableConveyorBase* Old, bool bExplicitExport)
{
    auto Operation = ConveyorOwner->Active;
    auto* Chain = Old->GetConveyorChainActor(); // Fresh after Construct; never a saved pre-Construct receiver.
    if (!Chain) return true;
    FString Reason;
    if (!FSFConveyorCohortValidator::CanExportChain(*Subsystem->GetConstructionCoordinator(), Chain, Operation->OwnedMembers, Reason))
    {
        Operation->bUncertain = true; Operation->Result.ErrorMessage = Reason; return false;
    }
    if (bExplicitExport) Chain->MoveItemsFromChainToBelts();
    return true;
}

bool USFUpgradeExecutionService::RetireConveyorSource(AFGBuildableConveyorBase* Old)
{
    auto Operation = ConveyorOwner->Active;
    if (!IsValid(Old) || Old->IsActorBeingDestroyed())
    {
        Operation->bUncertain = true;
        Operation->Result.ErrorMessage = TEXT("Native conveyor source retired outside the owned retirement step");
        return false;
    }
    const FObjectKey SourceIdentity(Old);
    FString Reason;
    if (auto* Lift = Cast<AFGBuildableConveyorLift>(Old); Lift && Lift->GetSnappedPassthroughs().ContainsByPredicate(
        [](AFGBuildablePassthrough* Hole) { return Hole != nullptr; }))
    {
        Operation->bUncertain = true;
        Operation->Result.ErrorMessage = TEXT("Native lift Upgrade did not relinquish the old floor-hole references; source retirement withheld");
        return false;
    }
    auto* Coordinator = Subsystem->GetConstructionCoordinator();
    if (!FSFConveyorCohortValidator::CanRetire(*Coordinator, Old, Operation->OwnedMembers, Reason)
        || !SFConveyorUpgradeCompanions::PrepareRetirement(Old, *Operation, *Coordinator, Reason))
    {
        Operation->bUncertain = true; Operation->Result.ErrorMessage = Reason; return false;
    }
    if (!Old->Destroy() || !Old->IsActorBeingDestroyed())
    {
        Operation->bUncertain = true; Operation->Result.ErrorMessage = TEXT("Native conveyor source retirement is not settled"); return false;
    }
    Operation->RetiredSources.Add(SourceIdentity);
    if (!SFConveyorUpgradeCompanions::Retire(SourceIdentity, *Operation, *Coordinator, Reason))
    {
        Operation->bUncertain = true; Operation->Result.ErrorMessage = Reason; return false;
    }
    return true;
}

bool USFUpgradeExecutionService::DeliverConveyorCompanionRefunds()
{
    auto Operation = ConveyorOwner->Active;
    auto* Player = Cast<AFGCharacterPlayer>(Operation->Payer.Get());
    if (!Player || !Player->GetInventory()) return false;
    TStrongObjectPtr<AFGCharacterPlayer> PlayerPin(Player);
    // Native refund distribution preserves full item values and tries the payer
    // inventory before dropping the positive remainder. Every call is made once;
    // callbacks and ambiguous receipts never authorize a retry.
    if (Operation->bCompanionRefundDeliveryFailed) return false;
    if (Operation->DeliveredCompanionRefunds < Operation->CompanionRefunds.Num())
    {
        Operation->bCompanionRefundDeliveryFailed = true; // Cleared only after every receipt in this invocation.
        const int32 FirstRefund = Operation->DeliveredCompanionRefunds;
        TArray<FSFInventoryStackSnapshot> PendingOverflow;
        for (int32 Index = FirstRefund; Index < Operation->CompanionRefunds.Num(); ++Index)
        {
            auto& Refund = Operation->CompanionRefunds[Index];
            if (Refund.Count <= 0 || !Refund.Item) continue;
            FInventoryStack Stack = Refund.ToStack();
            // The source has entered native retirement and this copy is not stored
            // in an inventory. Use the documented authority conversion, not a
            // descriptor-only reconstruction or a clone of the legacy UObject.
            if (Stack.Item.LegacyItemStateActor)
            {
                const FObjectKey LegacyIdentity(Stack.Item.LegacyItemStateActor.Get());
                if (const auto* Converted = Operation->LegacyRefundConversions.Find(LegacyIdentity))
                {
                    if ((*Converted)->Get().GetItemClass() != Stack.Item.GetItemClass()) return false;
                    Refund.Item = *Converted; Stack.Item = Refund.Item->Get();
                }
                else
                {
                    Stack.Item.Internal_ConvertLegacyItemState();
                    if (!FSFInventoryItemSnapshot::IsValidValue(Stack.Item) || Stack.Item.LegacyItemStateActor) return false;
                    Refund.Item = MakeShared<FSFInventoryItemSnapshot>(Stack.Item);
                    Operation->LegacyRefundConversions.Add(LegacyIdentity, Refund.Item);
                }
            }
            auto* Inventory = Player->GetInventory();
            TStrongObjectPtr<UFGInventoryComponent> InventoryPin(Inventory);
            TArray<FInventoryStack> BeforeStacks; Inventory->GetInventoryStacks(BeforeStacks);
            TArray<FSFInventoryStackSnapshot> Expected;
            if (!SFInventorySnapshots::Capture(BeforeStacks, Expected)) return false;
            const int32 Added = Inventory->AddStack(Stack, true);
            if (Added < 0 || Added > Stack.NumItems || !IsValid(Player) || Player->IsActorBeingDestroyed()
                || !IsValid(Inventory) || Player->GetInventory() != Inventory) return false;
            if (Added > 0)
            {
                auto Accepted = Refund; Accepted.Count = Added; Expected.Add(MoveTemp(Accepted));
            }
            TArray<FInventoryStack> AfterStacks; Inventory->GetInventoryStacks(AfterStacks);
            if (!SFInventorySnapshots::SameContents(Expected, AfterStacks)) return false;
            if (Added < Stack.NumItems)
            {
                auto Remainder = Refund; Remainder.Count -= Added;
                PendingOverflow.Add(Remainder);
                Operation->CompanionOverflowRefunds.Add(MoveTemp(Remainder));
            }
        }
        if (!PendingOverflow.IsEmpty())
        {
            TArray<FInventoryStack> Stacks;
            for (const auto& Stack : PendingOverflow) Stacks.Add(Stack.ToStack());
            FSpawnInventoryCrateParameters SpawnParameters;
            SpawnParameters.bAllowCrateMerging = false; // The receipt owns only this operation's outputs.
            AFGCrate* Crate = AFGItemPickup_Spawnable::SpawnInventoryCrate(Player->GetWorld(), Stacks,
                Player->GetActorLocation() + FVector(0, 0, 50), SpawnParameters);
            if (Crate) Operation->CompanionRefundCrates.Add(Crate);
            if (!IsValid(Crate) || !Crate->GetInventory()
                || !Subsystem->GetConstructionCoordinator()->AdoptActor(Operation->Key, Crate)) return false;
            TArray<FInventoryStack> Contents; Crate->GetInventory()->GetInventoryStacks(Contents);
            if (!SFInventorySnapshots::SameContents(PendingOverflow, Contents)) return false;
        }
        for (int32 Index = FirstRefund; Index < Operation->CompanionRefunds.Num(); ++Index)
        {
            const auto& Stack = Operation->CompanionRefunds[Index];
            if (Stack.Count > 0 && Stack.Item)
            {
                auto& Total = BatchRefundedTotals.FindOrAdd(Stack.Item->Get().GetItemClass());
                const int64 Combined = int64(Total) + Stack.Count;
                if (Combined > MAX_int32) return false;
                Total = static_cast<int32>(Combined);
            }
        }
        Operation->DeliveredCompanionRefunds = Operation->CompanionRefunds.Num();
        Operation->bCompanionRefundDeliveryFailed = false;
    }
    return true;
}

bool USFUpgradeExecutionService::SettleConveyorResources()
{
    auto Operation = ConveyorOwner->Active;
    auto* Player = Cast<AFGCharacterPlayer>(Operation->Payer.Get());
    if (!Player || !Player->GetInventory() || !DeliverConveyorCompanionRefunds()) return false;
    for (const auto& Entry : BatchChargedTotals) if (!Operation->MaterialsBefore.Contains(Entry.Key)) return false;
    for (const auto& Entry : BatchRefundedTotals) if (!Operation->MaterialsBefore.Contains(Entry.Key)) return false;
    if (!OverflowItems.IsEmpty())
    {
        TArray<FInventoryStack> Stacks;
        for (const auto& Entry : OverflowItems) if (Entry.Key && Entry.Value > 0) Stacks.Add(FInventoryStack(Entry.Value, Entry.Key));
        FSpawnInventoryCrateParameters SpawnParameters;
        SpawnParameters.bAllowCrateMerging = false;
        AFGCrate* Crate = AFGItemPickup_Spawnable::SpawnInventoryCrate(Player->GetWorld(), Stacks,
            Player->GetActorLocation() + FVector(0, 0, 50), SpawnParameters);
        Operation->OverflowCrate = Crate;
        if (!IsValid(Crate) || !Crate->GetInventory() || !Subsystem->GetConstructionCoordinator()->AdoptActor(Operation->Key, Crate)) return false;
        for (const auto& Entry : OverflowItems) if (Crate->GetInventory()->GetNumItems(Entry.Key) != Entry.Value) return false;
    }
    for (const auto& Entry : Operation->MaterialsBefore)
    {
        int64 Actual = ConveyorMaterialCount(Player, Entry.Key);
        if (Operation->OverflowCrate.IsValid()) Actual += Operation->OverflowCrate->GetInventory()->GetNumItems(Entry.Key);
        for (const auto& Crate : Operation->CompanionRefundCrates)
        {
            if (!Crate.IsValid() || !Crate->GetInventory()) return false;
            Actual += Crate->GetInventory()->GetNumItems(Entry.Key);
        }
        const int64 Expected = Entry.Value - BatchChargedTotals.FindRef(Entry.Key) + BatchRefundedTotals.FindRef(Entry.Key);
        if (Actual != Expected) return false;
    }
    return true;
}

int32 USFUpgradeExecutionService::PrepareConveyorSettlement(AFGBuildable* Old, TSubclassOf<UFGRecipe> Recipe,
    AFGCharacterPlayer* Player, FSFUpgradeSettlementLedger& Ledger) const
{
    Ledger = FSFUpgradeSettlementLedger();
    if (!Player || !Player->GetInventory()) return 0;
    Ledger.bFreeBuild = Player->GetInventory()->GetNoBuildCost();
    if (Ledger.bFreeBuild) return 1;
    if (!ComputeUpgradeSettlement(Old, Recipe, Ledger.NetCharge, Ledger.NetRefund)) return 0;
    if (ConveyorOwner && ConveyorOwner->Active)
        if (const auto* Plan = ConveyorOwner->Active->Companions.Find(FObjectKey(Old)))
        {
            auto Add = [](auto& Totals, const auto& Values)
            {
                for (const auto& Value : Values)
                    for (const auto& Entry : Value.Materials)
                    {
                        const int64 Total = int64(Totals.FindRef(Entry.Key)) + Entry.Value;
                        if (Total > MAX_int32 || Total < 0) return false;
                        Totals.Add(Entry.Key, static_cast<int32>(Total));
                    }
                return true;
            };
            // Eligible old children are reselected after Upgrade and receive the
            // full native refund then. Do not promise a refund for stale children.
            if (!Add(Ledger.NetCharge, Plan->Expected)) return 0;
        }
    TArray<FInventoryStack> Stock;
    Player->GetInventory()->GetInventoryStacks(Stock);
    for (const auto& Stack : Stock)
        if (Ledger.NetCharge.Contains(Stack.Item.GetItemClass())
            && (Stack.Item.HasState() || Stack.Item.LegacyItemStateActor != nullptr)) return 0;
    for (const auto& Entry : Ledger.NetCharge)
        if (Entry.Value < 0 || !Entry.Key) return 0;
        else if (ConveyorMaterialCount(Player, Entry.Key) < Entry.Value) return -1;
    return 1;
}

void USFUpgradeExecutionService::RetainUnexpectedConveyorConstruction(AActor* Main, const TArray<AActor*>& Children)
{
    auto Operation = ConveyorOwner->Active;
    Operation->bUncertain = true;
    Operation->Result.ErrorMessage = TEXT("Construction returned unexpected actors; the proved target debit is retained pending reconciliation");
    auto Retain = [&](AActor* Actor)
    {
        if (!Actor) return;
        Operation->UnexpectedOutputs.AddUnique(Actor);
        if (IsValid(Actor)) Subsystem->GetConstructionCoordinator()->AdoptActor(Operation->Key, Actor);
    };
    Retain(Main);
    for (auto* Child : Children) Retain(Child);
}
