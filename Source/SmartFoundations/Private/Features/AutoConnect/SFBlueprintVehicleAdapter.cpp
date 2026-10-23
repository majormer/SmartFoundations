// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintVehicleAdapter.h"
#include "SmartFoundations.h"
#include "SFBlueprintVehicleNativeFrame.h"
#include "Hologram/BlueprintOpenVehiclePathConnectionManager.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "Misc/ScopeExit.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"

// SP/MP DIVERGENCE MAP — repeated Blueprint native Vehicle Path adapter
// [MP-CLIENT] One native manager evaluates one directed source tail and a sibling segment.
// [MP-AUTH] Repeat the same native predicate with the exact constructed counterparts.
// Native managers own all node migration, bridge construction and network registration.
namespace
{
bool VehicleIsTail(const AFGVehiclePathNode* Node)
{
    return IsValid(Node) && Node->GetArrivingConnections().Num() == 1 && Node->GetLeavingConnections().IsEmpty();
}
bool VehicleIsHead(const AFGVehiclePathNode* Node)
{
    return IsValid(Node) && Node->GetArrivingConnections().IsEmpty() && Node->GetLeavingConnections().Num() == 1;
}
bool VehicleSamePose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .1)
        && A.GetRotation().Equals(B.GetRotation(), .0001)
        && A.GetScale3D().Equals(B.GetScale3D(), .0001);
}
bool VehicleNativeClass(const AActor* Actor)
{
    return IsValid(Actor) && Actor->GetClass()->GetPathName().StartsWith(TEXT("/Game/FactoryGame/"));
}
}

struct FSFBlueprintVehicleSelection::FImpl
{
    FSFBlueprintVehiclePair Pair;
    TUniquePtr<FBlueprintOpenVehiclePathConnectionManager> Manager;
    bool bValid = false;
    TWeakObjectPtr<AFGBlueprintHologram> Owner;
    TArray<TWeakObjectPtr<AFGHologram>> Children;
    TArray<TWeakObjectPtr<UActorComponent>> Components;
    ~FImpl()
    {
        if (Manager && Owner.IsValid()) Manager->ResetAutomaticConnections();
        Manager.Reset();
        for (const auto& Child : Children) if (Child.IsValid())
        {
            auto* SS = USFSubsystem::Get(Child->GetWorld());
            if (SS && SS->GetHologramHelper()) SS->GetHologramHelper()->QueueChildForDestroy(Child.Get());
            else Child->Destroy();
        }
        for (const auto& Component : Components) if (Component.IsValid()) Component->DestroyComponent();
    }
};

FSFBlueprintVehicleSelection::~FSFBlueprintVehicleSelection() = default;
bool FSFBlueprintVehicleSelection::IsValid() const { return Impl && Impl->bValid; }
const FSFBlueprintVehiclePair& FSFBlueprintVehicleSelection::GetPair() const
{
    check(Impl);
    return Impl->Pair;
}
FString FSFBlueprintVehicleSelection::Describe() const
{
    return IsValid() ? TEXT("native directed tail/head selected") : TEXT("native directed endpoint unavailable");
}
void FSFBlueprintVehicleSelection::GetBridges(TArray<AFGHologram*>& Out) const
{
    Out.Reset();
    if (Impl) for (const auto& Child : Impl->Children)
        if (Child.IsValid() && Child->IsA<AFGVehiclePathSegmentHologram>()) Out.Add(Child.Get());
}

TSharedPtr<FSFBlueprintVehicleSelection> FSFBlueprintVehicleAdapter::Preview(AFGBlueprintHologram* Source,
    AFGVehiclePathNode* Tail, AFGBlueprintHologram* Neighbor, AFGVehiclePathSegment* Segment)
{
    if (!IsValid(Source) || !IsValid(Neighbor) || Source == Neighbor || !VehicleIsTail(Tail)
        || !VehicleNativeClass(Tail) || !VehicleNativeClass(Segment) || !Source->mBuildableToNewRoot.Contains(Tail)
        || !Neighbor->mBuildableToNewRoot.Contains(Segment)) return nullptr;
    auto* Head = Segment->GetStartNode();
    if (!VehicleIsHead(Head) || !Neighbor->mBuildableToNewRoot.Contains(Head)) return nullptr;
    auto* SourceRoot = Source->mBuildableToNewRoot.FindRef(Tail).Get();
    auto* TargetRoot = Neighbor->mBuildableToNewRoot.FindRef(Head).Get();
    if (!IsValid(SourceRoot) || !IsValid(TargetRoot)) return nullptr;

    auto Result = MakeShared<FSFBlueprintVehicleSelection>();
    Result->Impl = MakeShared<FSFBlueprintVehicleSelection::FImpl>();
    auto& State = *Result->Impl;
    State.Owner = Source;
    State.Pair.SourceTail = Tail; State.Pair.TargetHead = Head; State.Pair.TargetSegment = Segment;
    State.Pair.SourceOriginal = Tail->GetActorTransform(); State.Pair.TargetOriginal = Head->GetActorTransform();
    State.Pair.SourceWorld = SourceRoot->GetComponentTransform(); State.Pair.TargetWorld = TargetRoot->GetComponentTransform();
    State.Manager = MakeUnique<FBlueprintOpenVehiclePathConnectionManager>(Source);
    const auto ChildrenBefore = Source->GetHologramChildren();
    TInlineComponentArray<UActorComponent*> ComponentsBefore(Source);
    ON_SCOPE_EXIT
    {
        for (auto* Child : Source->GetHologramChildren()) if (Child && !ChildrenBefore.Contains(Child))
        {
            Child->Tags.AddUnique(TEXT("SF_BlueprintVehicleSeam"));
            State.Children.AddUnique(Child);
        }
        TInlineComponentArray<UActorComponent*> ComponentsAfter(Source);
        for (auto* Component : ComponentsAfter) if (!ComponentsBefore.Contains(Component)) State.Components.AddUnique(Component);
    };
    // One endpoint supplies one native source state. This avoids inspecting the
    // final manager's private arrays or guessing which of several ports it chose.
    State.Manager->Initialize({Tail});
    TArray<TPair<AActor*, FTransform>> Poses;
    ON_SCOPE_EXIT
    {
        for (const auto& Pose : Poses) if (IsValid(Pose.Key)) Pose.Key->SetActorTransform(Pose.Value);
    };
    for (const auto& Entry : Neighbor->mBuildableToNewRoot)
    {
        auto* Template = Entry.Key.Get();
        if (!Template || (!Template->IsA<AFGVehiclePathNode>() && !Template->IsA<AFGVehiclePathSegment>())) continue;
        // Never move a paid/main-world actor to make a native predicate pass.
        if (Template->GetWorld() == Source->GetWorld() || !IsValid(Entry.Value.Get())) return nullptr;
        Poses.Add({Template, Template->GetActorTransform()});
        Template->SetActorTransform(Entry.Value->GetComponentTransform());
    }
    State.Manager->RegisterNearbyActor(Segment);
    bool PlayEffects = false;
    State.Manager->UpdateAutomaticConnections(FHitResult(), PlayEffects);
    State.bValid = State.Manager->CanSnapConnectionStates();
    return Result;
}

bool FSFBlueprintVehicleAdapter::Construct(AFGBlueprintHologram* Source, const TSharedPtr<FSFBlueprintVehicleSelection>& Selection,
    AFGVehiclePathNode* BuiltTail, AFGVehiclePathNode* BuiltHead, AFGVehiclePathSegment* BuiltTarget,
    FNetConstructionID ID, TArray<AFGBuildable*>& OutBridges, bool& bAttempted)
{
    bAttempted = false;
    if (!Selection || !Selection->IsValid() || !Selection->Impl->Manager) return false;
    const auto& Expected = Selection->GetPair();
    auto* Original = Expected.SourceTail.Get();
    if (!IsValid(Source) || !Source->HasAuthority() || !VehicleIsTail(Original) || !VehicleIsTail(BuiltTail) || !VehicleIsHead(BuiltHead)
        || !IsValid(BuiltTarget) || BuiltTarget->GetStartNode() != BuiltHead
        || BuiltTail->GetWorld() != Source->GetWorld() || BuiltHead->GetWorld() != Source->GetWorld()
        || BuiltTarget->GetWorld() != Source->GetWorld() || BuiltTail->GetClass() != Original->GetClass()
        || !Expected.TargetHead.IsValid() || BuiltHead->GetClass() != Expected.TargetHead->GetClass()
        || !Expected.TargetSegment.IsValid() || BuiltTarget->GetClass() != Expected.TargetSegment->GetClass()
        || !VehicleSamePose(Original->GetActorTransform(), Expected.SourceOriginal)
        || !VehicleSamePose(BuiltTail->GetActorTransform(), Expected.SourceWorld)
        || !VehicleSamePose(BuiltHead->GetActorTransform(), Expected.TargetWorld)) return false;

    if (!IsValid(BuiltTail->GetRootComponent())) return false;
    // Preserve the native attached visual frame. Initialize and candidate lookup
    // consume it; HandleBuildableConnectionRemapping supplies the built node
    // separately. Replacing the visual with the built node's root loses its
    // attachment frame and violates the native candidate lookup contract.
    const auto* Visual = Source->mBuildableToNewRoot.FindRef(Original).Get();
    if (!SFBlueprintVehicleNativeFrame::Matches(Visual, Expected.SourceWorld)) return false;
    // The authority preview already owns the validated native bridge. Remap
    // that manager into these exact built counterparts instead of allocating
    // a second bridge with the same native child name on the same Blueprint.
    auto& Native = *Selection->Impl->Manager;
    Native.UnregisterNearbyActor(Expected.TargetSegment.Get());
    Native.HandleBuildableConnectionRemapping(BuiltTail, 0);
    Native.RegisterNearbyActor(BuiltTarget);
    bool PlayEffects = false;
    Native.UpdateAutomaticConnections(FHitResult(), PlayEffects);
    if (!Native.CanSnapConnectionStates()) return false;
    // Every candidate still belongs to the caller's exact copy. Native code
    // decides direct node migration versus a routed connecting span and its cost.
    auto* Incoming = BuiltTail->GetArrivingConnections()[0];
    bAttempted = true;
    Native.Construct(OutBridges, ID);
    Native.ResetAutomaticConnections();
    // Construction can migrate/remove the redundant tail node. Inspect the
    // native segments afterwards, never dereference a possibly removed node or
    // mistake an entered native call for a completed directed connection.
    auto* Cursor = IsValid(Incoming) ? Incoming->GetEndNode() : nullptr;
    TSet<AFGVehiclePathNode*> Visited;
    while (IsValid(Cursor) && !Visited.Contains(Cursor))
    {
        if (Cursor == BuiltTarget->GetStartNode()) return true;
        Visited.Add(Cursor);
        AFGVehiclePathSegment* Next = nullptr;
        for (auto* Actor : OutBridges)
            if (auto* Bridge = Cast<AFGVehiclePathSegment>(Actor); IsValid(Bridge) && Bridge->GetStartNode() == Cursor)
            {
                if (Next) return false;
                Next = Bridge;
            }
        Cursor = Next ? Next->GetEndNode() : nullptr;
    }
    return false;
}
