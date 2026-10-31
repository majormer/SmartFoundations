// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFConveyorConstructionLinks.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "Patching/NativeHookManager.h"
#include "Core/Construction/SFNativeConstructionContext.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFConveyorConnectionMutation.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFHologramDataRegistry.h"

// SP/MP DIVERGENCE MAP — construction links
//  1. [MP-AUTH] Only authority records built identities or submits a connection batch.
//  2. No client nearest-actor reconstruction and no replicated client graph mutation.
namespace
{
struct FConfiguredKey
{
    FObjectKey Preview;
    int8 Player = -1;
    uint16 Server = 0;
    uint16 Client = 0;
    bool operator==(const FConfiguredKey& Other) const
    { return Preview == Other.Preview && Player == Other.Player && Server == Other.Server && Client == Other.Client; }
    friend uint32 GetTypeHash(const FConfiguredKey& Key)
    { return HashCombine(GetTypeHash(Key.Preview), HashCombine(GetTypeHash(Key.Player), HashCombine(GetTypeHash(Key.Server), GetTypeHash(Key.Client)))); }
};
struct FConfigured
{
    FObjectKey World;
    FGuid Generation;
    TWeakObjectPtr<const AFGHologram> Preview;
    TWeakObjectPtr<AFGBuildable> Built;
    uint64 Frame = 0;
};
struct FBatch
{
    FObjectKey WorldKey;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<USFConstructionOperationCoordinator> Owner;
    TWeakObjectPtr<APlayerController> Requester;
    FGuid Generation;
    FGuid RequestId;
    uint64 Sequence = 0;
    TArray<FSFConveyorConstructionEdge> Edges;
};
struct FBatchKey
{
    FObjectKey World;
    FGuid Generation;
    FObjectKey Requester;
    FObjectKey Root;
    int8 Player = -1;
    uint16 Server = 0;
    uint16 Client = 0;
    bool operator==(const FBatchKey& Other) const
    {
        return World == Other.World && Generation == Other.Generation && Requester == Other.Requester
            && Root == Other.Root && Player == Other.Player && Server == Other.Server && Client == Other.Client;
    }
    friend uint32 GetTypeHash(const FBatchKey& Key)
    {
        return HashCombine(HashCombine(GetTypeHash(Key.World), GetTypeHash(Key.Generation)),
            HashCombine(HashCombine(GetTypeHash(Key.Requester), GetTypeHash(Key.Root)),
                HashCombine(GetTypeHash(Key.Player), HashCombine(GetTypeHash(Key.Server), GetTypeHash(Key.Client)))));
    }
};
struct FPreparedContext
{
    FObjectKey WorldKey;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<USFConstructionOperationCoordinator> Owner;
    TWeakObjectPtr<APlayerController> Requester;
    FGuid Generation;
    FBatchKey BatchKey;
    uint64 Frame = 0;
};
TMap<FConfiguredKey, FConfigured> Configured;
TMap<FConfiguredKey, FSFConveyorConstructionEndpoint> PreviewPorts;
TMap<FConfiguredKey, FPreparedContext> Contexts;
TArray<FNetConstructionID> NativeConstructionIds;
TMap<FBatchKey, TSharedPtr<FBatch>> Pending;
// A failed admission cannot erase the fact that the native parent has already
// materialized/paid. Retain its exact intent for diagnosis through world cleanup.
TMap<FBatchKey, TSharedPtr<FBatch>> Unresolved;

void BindCleanup()
{
    static const FDelegateHandle Handle = FWorldDelegates::OnWorldCleanup.AddLambda(
        [](UWorld* World, bool, bool)
        {
            const FObjectKey Key(World);
            int32 Outstanding = 0;
            for (auto It = Pending.CreateIterator(); It; ++It)
                if (It.Value()->WorldKey == Key) { ++Outstanding; It.RemoveCurrent(); }
            for (auto It = Unresolved.CreateIterator(); It; ++It)
                if (It.Value()->WorldKey == Key) { ++Outstanding; It.RemoveCurrent(); }
            for (auto It = Configured.CreateIterator(); It; ++It)
                if (It.Value().World == Key) It.RemoveCurrent();
            for (auto It = PreviewPorts.CreateIterator(); It; ++It)
                if (It.Value().WorldKey == Key) It.RemoveCurrent();
            for (auto It = Contexts.CreateIterator(); It; ++It)
                if (It.Value().WorldKey == Key) It.RemoveCurrent();
            if (Outstanding)
                UE_LOG(LogSmartFoundations, Warning, TEXT("World ended with %d unresolved native construction-link batch(es); parent effects/payment are not rolled back or certified."), Outstanding);
        });
    (void)Handle;
}

UFGFactoryConnectionComponent* Resolve(const FSFConveyorConstructionEndpoint& Intent, const FBatch& Batch,
    bool bRequireBegunPlay = true)
{
    if (!Intent.bValid || Intent.World != Batch.World || Intent.WorldKey != Batch.WorldKey
        || Intent.Generation != Batch.Generation) return nullptr;
    if (!Intent.bPreview)
    {
        auto* Port = Intent.ExistingPort.Get();
        auto* Actor = Port ? Port->GetOwner() : nullptr;
        return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->HasActorBegunPlay()
            && Actor->GetWorld() == Batch.World.Get() && Actor->HasAuthority()
            && static_cast<int32>(Port->GetDirection()) == Intent.Direction
            && Port->GetComponentLocation().Equals(Intent.Position, .2)
            && Port->GetConnectorNormal().Equals(Intent.Normal, .0001)
            && FMath::IsNearlyEqual(Port->GetConnectorClearance(), Intent.Clearance, .01f) ? Port : nullptr;
    }
    const FConfiguredKey Lookup{Intent.PreviewOwner, Intent.NetPlayerId, Intent.ServerId, Intent.ClientId};
    const auto* Identity = Configured.Find(Lookup);
    auto* Built = Identity ? Identity->Built.Get() : nullptr;
    if (!Identity || Identity->World != Batch.WorldKey || Identity->Generation != Batch.Generation
        || !IsValid(Built) || Built->IsActorBeingDestroyed() || (bRequireBegunPlay && !Built->HasActorBegunPlay())
        || (!bRequireBegunPlay && Identity->Frame != GFrameCounter)
        || Built->GetWorld() != Batch.World.Get() || !Built->HasAuthority()
        || Built->GetClass() != Intent.ExpectedClass.Get()) return nullptr;
    UFGFactoryConnectionComponent* Match = nullptr;
    TArray<UFGFactoryConnectionComponent*> Ports; Built->GetComponents(Ports);
    const bool bLift = Built->IsA<AFGBuildableConveyorLift>();
    for (auto* Port : Ports)
    {
        if (!IsValid(Port) || static_cast<int32>(Port->GetDirection()) != Intent.Direction) continue;
        if (!Port->GetComponentLocation().Equals(Intent.Position, .2)
            || !Port->GetConnectorNormal().Equals(Intent.Normal, .0001)
            || !FMath::IsNearlyEqual(Port->GetConnectorClearance(), Intent.Clearance, .01f)) continue;
        // Reversed lifts reorder native indices: preserve the captured physical
        // mouth and flow on the exact returned actor, never a nearby shaft.
        if (bLift ? !Port->GetComponentLocation().Equals(Intent.Position, 0.2)
                  : Port->GetFName() != Intent.PortName) continue;
        if (Match) return nullptr;
        Match = Port;
    }
    return Match;
}

void Submit(const FBatchKey& Key, const TSharedPtr<FBatch>& Batch)
{
    if (Pending.FindRef(Key) != Batch) return;
    Pending.Remove(Key);
    for (auto It = Contexts.CreateIterator(); It; ++It)
        if (It.Value().BatchKey == Key) It.RemoveCurrent();
    auto Reject = [&](const TCHAR* Reason)
    {
        Unresolved.Add(Key, Batch);
        UE_LOG(LogSmartFoundations, Warning, TEXT("Construction links %s unresolved before link effects: %s. Native parent actors/payment remain separate; no nearest-peer fallback."),
            *Batch->RequestId.ToString(), Reason);
    };
    auto* World = Batch->World.Get(); auto* Owner = Batch->Owner.Get();
    if (!IsValid(World) || World->bIsTearingDown || !Owner || Owner->GetWorld() != World
        || Owner->GetWorldGeneration() != Batch->Generation || !Batch->Requester.IsValid())
    { Reject(TEXT("original_world_or_requester_expired")); return; }
    TArray<FSFConveyorConnectionPair> Edges;
    for (const auto& Edge : Batch->Edges)
    {
        auto* A = Resolve(Edge.A, *Batch); auto* B = Resolve(Edge.B, *Batch);
        if (!A || !B) { Reject(TEXT("exact_constructed_endpoint_unavailable")); return; }
        Edges.Add({A, B});
    }
    const auto Admission = FSFConveyorConnectionMutation::Queue(*Owner, Batch->Requester.Get(),
        Batch->Sequence, Batch->RequestId, Edges);
    if (Admission != ESFConstructionAdmission::Accepted)
    { Reject(TEXT("native_phase_link_admission_rejected")); return; }
    // No SetConnection, RemoveConveyor, or AddConveyor is permitted in this timer.
    // The queued adapter alone owns native mutation and subsequent observation.
    for (auto It = Configured.CreateIterator(); It; ++It)
    {
        if (!It.Value().Built.IsValid()) { It.RemoveCurrent(); continue; }
        if (It.Value().Preview.IsValid()) continue;
        bool bRequired = false;
        for (const auto& Work : Pending)
            for (const auto& Edge : Work.Value->Edges)
                bRequired |= Edge.A.PreviewOwner == It.Key().Preview || Edge.B.PreviewOwner == It.Key().Preview;
        for (const auto& Work : Unresolved)
            for (const auto& Edge : Work.Value->Edges)
                bRequired |= Edge.A.PreviewOwner == It.Key().Preview || Edge.B.PreviewOwner == It.Key().Preview;
        if (!bRequired) It.RemoveCurrent();
    }
}
}

void FSFConveyorConstructionLinks::InstallHooks()
{
    static bool Installed = false;
    if (Installed) return;
    Installed = true;
    // Identity bookkeeping only. Explicitly invoke the next handler/native body
    // while the construction ID is in scope, including automatic native branches
    // of the feature-specific Construct hook. No topology or costs are modified.
    const auto NativeConstructBookkeeping = [](auto& Scope, AFGBuildableHologram* Preview, TArray<AActor*>& Children, FNetConstructionID Id)
        {
            if (!IsInGameThread() || !IsValid(Preview)) return;
            const FConfiguredKey Previous{FObjectKey(Preview), Id.NetPlayerID, Id.Server_ID, Id.Client_ID};
            const TWeakObjectPtr<UWorld> World = Preview->GetWorld();
            const TWeakObjectPtr<UClass> ExpectedClass = Preview->GetBuildClass().Get();
            const TWeakObjectPtr<const AFGHologram> PreviewIdentity = Preview;
            auto* Subsystem = USFSubsystem::Get(World.Get());
            auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
            const FGuid Generation = Owner ? Owner->GetWorldGeneration() : FGuid();
            Configured.Remove(Previous);
            FSFNativeConstructionContext::FScope NativeContext(Preview, Id);
            NativeConstructionIds.Add(Id);
            ON_SCOPE_EXIT { NativeConstructionIds.Pop(EAllowShrinking::No); };
            auto* Built = Cast<AFGBuildable>(Scope(Preview, Children, Id));
            // Spline subclasses can bypass the base ConfigureActor body. Bind
            // only this invocation's actual return, using pre-cleanup identity.
            // Do not dereference Preview after native construction retires it.
            auto* CurrentSubsystem = World.IsValid() ? USFSubsystem::Get(World.Get()) : nullptr;
            auto* CurrentOwner = CurrentSubsystem ? CurrentSubsystem->GetConstructionCoordinator() : nullptr;
            if (IsValid(Built) && Built->HasAuthority() && !Built->IsActorBeingDestroyed()
                && Built->GetWorld() == World.Get() && Built->GetClass() == ExpectedClass.Get()
                && Generation.IsValid() && CurrentOwner && CurrentOwner->GetWorldGeneration() == Generation)
            {
                BindCleanup();
                Configured.Add(Previous, FConfigured{FObjectKey(World.Get()), Generation, PreviewIdentity, Built, GFrameCounter});
            }
        };
    SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableHologram::Construct, GetMutableDefault<AFGBuildableHologram>(), NativeConstructBookkeeping);
    // These native overrides need not invoke the base callback. Record the
    // exact configured identity before children need the parent's components.
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGConveyorBeltHologram::ConfigureActor, GetMutableDefault<AFGConveyorBeltHologram>(),
        [](const AFGConveyorBeltHologram* Preview, AFGBuildable* Built) { FSFConveyorConstructionLinks::RecordConfigured(Preview, Built); });
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGConveyorLiftHologram::ConfigureActor, GetMutableDefault<AFGConveyorLiftHologram>(),
        [](const AFGConveyorLiftHologram* Preview, AFGBuildable* Built) { FSFConveyorConstructionLinks::RecordConfigured(Preview, Built); });
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGResourceExtractorHologram::ConfigureActor, GetMutableDefault<AFGResourceExtractorHologram>(),
        [](const AFGResourceExtractorHologram* Preview, AFGBuildable* Built) { FSFConveyorConstructionLinks::RecordConfigured(Preview, Built); });
}

bool FSFConveyorConstructionLinks::GetCurrentConstructionId(FNetConstructionID& OutId)
{
    if (!IsInGameThread() || NativeConstructionIds.IsEmpty()) return false;
    OutId = NativeConstructionIds.Last();
    return true;
}

FSFConveyorConstructionEndpoint FSFConveyorConstructionLinks::Capture(UFGFactoryConnectionComponent* Port,
    const FNetConstructionID* ConstructionId)
{
    FSFConveyorConstructionEndpoint Result;
    if (!IsInGameThread() || !IsValid(Port) || !IsValid(Port->GetOwner())) return Result;
    if (ConstructionId)
    {
        const FConfiguredKey Lookup{FObjectKey(Port), ConstructionId->NetPlayerID, ConstructionId->Server_ID, ConstructionId->Client_ID};
        if (const auto* Defined = PreviewPorts.Find(Lookup)) return *Defined;
    }
    Result.World = Port->GetWorld(); Result.WorldKey = FObjectKey(Port->GetWorld()); Result.PortName = Port->GetFName();
    auto* Subsystem = USFSubsystem::Get(Port->GetWorld());
    auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner || !Owner->GetWorldGeneration().IsValid()) return Result;
    Result.Generation = Owner->GetWorldGeneration();
    Result.Position = Port->GetComponentLocation(); Result.Direction = static_cast<int32>(Port->GetDirection());
    Result.Normal = Port->GetConnectorNormal(); Result.Clearance = Port->GetConnectorClearance();
    if (auto* Preview = Cast<AFGHologram>(Port->GetOwner()))
    {
        Result.PreviewOwner = FObjectKey(Preview); Result.ExpectedClass = Preview->GetBuildClass().Get();
        Result.bPreview = true;
        if (ConstructionId)
        {
            Result.NetPlayerId = ConstructionId->NetPlayerID;
            Result.ServerId = ConstructionId->Server_ID; Result.ClientId = ConstructionId->Client_ID;
            Result.bValid = Result.ExpectedClass.IsValid();
        }
    }
    else if (Cast<AFGBuildable>(Port->GetOwner()))
    { Result.ExistingPort = Port; Result.bValid = true; }
    return Result;
}

bool FSFConveyorConstructionLinks::DefinePreviewPort(UFGFactoryConnectionComponent* Port, UClass* BuiltClass,
    FName BuiltPortName, const FNetConstructionID& Id)
{
    if (!IsInGameThread() || !IsValid(Port) || !IsValid(BuiltClass) || BuiltPortName.IsNone()) return false;
    const FConfiguredKey Lookup{FObjectKey(Port), Id.NetPlayerID, Id.Server_ID, Id.Client_ID};
    PreviewPorts.Remove(Lookup);
    Configured.Remove(Lookup);
    auto Intent = Capture(Port, &Id);
    if (!Intent.Generation.IsValid()) return false;
    Intent.PreviewOwner = FObjectKey(Port); // Blueprint duplicate identity, not its aggregate owner.
    Intent.ExpectedClass = BuiltClass; Intent.PortName = BuiltPortName;
    Intent.bPreview = true; Intent.bValid = true;
    PreviewPorts.Add(Lookup, Intent);
    BindCleanup();
    return true;
}

bool FSFConveyorConstructionLinks::RecordPreviewPort(const FSFConveyorConstructionEndpoint& Intent, AFGBuildable* Built)
{
    if (!IsInGameThread() || !Intent.bValid || !Intent.bPreview || !IsValid(Built)
        || !Built->HasAuthority() || Built->IsActorBeingDestroyed() || !Built->HasActorBegunPlay()
        || Built->GetWorld() != Intent.World.Get() || Built->GetClass() != Intent.ExpectedClass.Get()) return false;
    auto* Subsystem = USFSubsystem::Get(Built->GetWorld());
    auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner || Owner->GetWorldGeneration() != Intent.Generation) return false;
    const FConfiguredKey Lookup{Intent.PreviewOwner, Intent.NetPlayerId, Intent.ServerId, Intent.ClientId};
    const FConfigured Identity{FObjectKey(Built->GetWorld()), Intent.Generation, nullptr, Built, GFrameCounter};
    Configured.Add(Lookup, Identity);
    PreviewPorts.Remove(Lookup);
    return true;
}

void FSFConveyorConstructionLinks::RecordConfigured(const AFGHologram* Preview, AFGBuildable* Built)
{
    // Bookkeeping only: the existing native ConfigureActor callback supplies the
    // actual returned identity before BeginPlay; no world search or topology edit.
    if (!IsInGameThread() || !IsValid(Preview) || !IsValid(Built) || !Built->HasAuthority()
        || Preview->GetWorld() != Built->GetWorld() || Preview->GetBuildClass() != Built->GetClass()) return;
    auto* Subsystem = USFSubsystem::Get(Built->GetWorld());
    auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner || !Owner->GetWorldGeneration().IsValid() || NativeConstructionIds.IsEmpty()) return;
    BindCleanup();
    // Ordinary vanilla builds also traverse this callback. Retain their weak
    // mapping for the synchronous construction window, without growing a
    // world-lifetime registry of every object the player has ever placed.
    for (auto It = Configured.CreateIterator(); It; ++It)
    {
        if (GFrameCounter <= It.Value().Frame + 2) continue;
        bool Required = false;
        for (const auto& Work : Pending)
            for (const auto& Edge : Work.Value->Edges)
                Required |= Edge.A.PreviewOwner == It.Key().Preview || Edge.B.PreviewOwner == It.Key().Preview;
        for (const auto& Work : Unresolved)
            for (const auto& Edge : Work.Value->Edges)
                Required |= Edge.A.PreviewOwner == It.Key().Preview || Edge.B.PreviewOwner == It.Key().Preview;
        if (!Required) It.RemoveCurrent();
    }
    const auto& Id = NativeConstructionIds.Last();
    const FConfiguredKey Lookup{FObjectKey(Preview), Id.NetPlayerID, Id.Server_ID, Id.Client_ID};
    const FConfigured Identity{FObjectKey(Built->GetWorld()), Owner->GetWorldGeneration(), Preview, Built, GFrameCounter};
    Configured.Add(Lookup, Identity);
}

UFGFactoryConnectionComponent* FSFConveyorConstructionLinks::ResolveConfigured(UFGFactoryConnectionComponent* Port,
    const FNetConstructionID* ConstructionId)
{
    if (!IsInGameThread() || !IsValid(Port)) return nullptr;
    const auto* Id = ConstructionId ? ConstructionId
        : (NativeConstructionIds.IsEmpty() ? nullptr : &NativeConstructionIds.Last());
    auto* World = Port->GetWorld();
    auto* Subsystem = World ? USFSubsystem::Get(World) : nullptr;
    auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner) return nullptr;
    FBatch Batch;
    Batch.World = World; Batch.WorldKey = FObjectKey(World); Batch.Generation = Owner->GetWorldGeneration();
    // Native parent ConfigureComponents precedes child construction, but the
    // parent's DeferredBeginPlay may follow it. Permit that synchronous window
    // only for this active full native ID and this frame's configured identity.
    // Timer submission always uses Resolve's live-actor default.
    const auto* ActiveId = NativeConstructionIds.IsEmpty() ? nullptr : &NativeConstructionIds.Last();
    const bool bSynchronousInput = Id && ActiveId && Id->NetPlayerID == ActiveId->NetPlayerID
        && Id->Server_ID == ActiveId->Server_ID && Id->Client_ID == ActiveId->Client_ID;
    return Resolve(Capture(Port, Id), Batch, !bSynchronousInput);
}

bool FSFConveyorConstructionLinks::Prepare(AFGHologram* Context, const FNetConstructionID& ConstructionId)
{
    if (!IsInGameThread() || !IsValid(Context) || !Context->HasAuthority()) return false;
    static uint64 LastPrunedFrame = MAX_uint64;
    if (LastPrunedFrame != GFrameCounter)
    {
        LastPrunedFrame = GFrameCounter;
        for (auto It = Contexts.CreateIterator(); It; ++It)
            if (GFrameCounter > It.Value().Frame + 2 && !Pending.Contains(It.Value().BatchKey)
                && !Unresolved.Contains(It.Value().BatchKey)) It.RemoveCurrent();
    }
    const FConfiguredKey Input{FObjectKey(Context), ConstructionId.NetPlayerID, ConstructionId.Server_ID, ConstructionId.Client_ID};
    if (const auto* Existing = Contexts.Find(Input))
        return !Unresolved.Contains(Existing->BatchKey)
            && Existing->World.Get() == Context->GetWorld() && Existing->Owner.IsValid()
            && Existing->Owner->GetWorldGeneration() == Existing->Generation && Existing->Requester.IsValid();
    auto* World = Context->GetWorld();
    auto* Subsystem = World ? USFSubsystem::Get(World) : nullptr;
    auto* Owner = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
    auto* Root = Context;
    TSet<AFGHologram*> Visited;
    auto ParentOf = [](AFGHologram* Preview) -> AFGHologram*
    {
        if (auto* Parent = Preview->GetParentHologram()) return Parent;
        const auto* Data = USFHologramDataRegistry::GetData(Preview);
        return Data ? Data->ParentHologram.Get() : nullptr;
    };
    while (auto* Parent = ParentOf(Root))
    {
        if (Visited.Contains(Root) || !IsValid(Parent)) return false;
        Visited.Add(Root); Root = Parent;
        if (Root->GetWorld() != World || !Root->HasAuthority()) return false;
    }
    auto* Pawn = Context->GetConstructionInstigator();
    auto* RootPawn = Root->GetConstructionInstigator();
    if (Pawn && RootPawn && Pawn != RootPawn) return false;
    if (RootPawn) Pawn = RootPawn;
    auto* Requester = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!IsValid(World) || World->GetNetMode() == NM_Client || World->bIsTearingDown || !Owner || !Requester
        || Requester->GetWorld() != World || !Requester->HasAuthority()) return false; // [MP-AUTH]
    const FGuid Generation = Owner->GetWorldGeneration();
    if (!Generation.IsValid()) return false;
    const FBatchKey Key{FObjectKey(World), Generation, FObjectKey(Requester), FObjectKey(Root),
        ConstructionId.NetPlayerID, ConstructionId.Server_ID, ConstructionId.Client_ID};
    if (Unresolved.Contains(Key)) return false; // Never silently reissue an unresolved native input.
    const FPreparedContext Prepared{FObjectKey(World), World, Owner, Requester, Generation, Key, GFrameCounter};
    Contexts.Add(Input, Prepared);
    BindCleanup();
    return true;
}

bool FSFConveyorConstructionLinks::Defer(AFGHologram* Context, const FNetConstructionID& ConstructionId,
    const TArray<FSFConveyorConstructionEdge>& Edges)
{
    if (!IsInGameThread() || !Context || Edges.IsEmpty()) return false;
    const FConfiguredKey Input{FObjectKey(Context), ConstructionId.NetPlayerID, ConstructionId.Server_ID, ConstructionId.Client_ID};
    const auto* Original = Contexts.Find(Input);
    if (!Original) return false;
    const auto Prepared = *Original;
    auto* World = Prepared.World.Get(); auto* Owner = Prepared.Owner.Get();
    if (!IsValid(World) || World->bIsTearingDown || !Owner || Owner->GetWorld() != World
        || Owner->GetWorldGeneration() != Prepared.Generation || !Prepared.Requester.IsValid()) return false;
    auto* Subsystem = USFSubsystem::Get(World);
    if (!Subsystem) return false;
    for (const auto& Edge : Edges)
        if (!Edge.A.bValid || !Edge.B.bValid || Edge.A.WorldKey != Prepared.WorldKey || Edge.B.WorldKey != Prepared.WorldKey
            || Edge.A.Generation != Prepared.Generation || Edge.B.Generation != Prepared.Generation) return false;
    const auto& Key = Prepared.BatchKey;
    if (Unresolved.Contains(Key)) return false;
    if (const auto Existing = Pending.FindRef(Key)) { Existing->Edges.Append(Edges); return true; }
    const int64 Sequence = Subsystem->AllocateConstructionRequestSequence();
    if (Sequence <= 0) return false;
    BindCleanup();
    auto Batch = MakeShared<FBatch>();
    Batch->WorldKey = Prepared.WorldKey; Batch->World = World; Batch->Owner = Owner; Batch->Requester = Prepared.Requester;
    Batch->Generation = Prepared.Generation; Batch->RequestId = FGuid::NewGuid(); Batch->Sequence = static_cast<uint64>(Sequence);
    Batch->Edges = Edges; Pending.Add(Key, Batch);
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Key, Batch]() { Submit(Key, Batch); }));
    return true;
}

void FSFConveyorConstructionLinks::RejectPrepared(AFGHologram* Context, const FNetConstructionID& Id, const TCHAR* Reason)
{
    if (!IsInGameThread() || !Context) return;
    const FConfiguredKey Input{FObjectKey(Context), Id.NetPlayerID, Id.Server_ID, Id.Client_ID};
    const auto* Prepared = Contexts.Find(Input);
    if (!Prepared) return;
    const auto Key = Prepared->BatchKey;
    auto Batch = Pending.FindRef(Key);
    if (!Batch)
    {
        Batch = MakeShared<FBatch>();
        Batch->WorldKey = Prepared->WorldKey; Batch->World = Prepared->World;
        Batch->Owner = Prepared->Owner; Batch->Requester = Prepared->Requester;
        Batch->Generation = Prepared->Generation; Batch->RequestId = FGuid::NewGuid();
    }
    Pending.Remove(Key); // The scheduled callback checks identity and now cannot submit.
    Unresolved.Add(Key, Batch);
    UE_LOG(LogSmartFoundations, Warning, TEXT("Construction links %s retained unresolved: %s; no partial link batch or native-parent rollback is inferred."),
        *Batch->RequestId.ToString(), Reason);
}
