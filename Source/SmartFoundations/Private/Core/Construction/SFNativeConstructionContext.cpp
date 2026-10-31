// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFNativeConstructionContext.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"
#include "Buildables/FGBuildable.h"
#include "FGBuildableSubsystem.h"
#include "FGBlueprintProxy.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Data/SFHologramDataRegistry.h"

// SP/MP DIVERGENCE MAP — native construction grouping
// [MP-AUTH] Membership uses the actual native request root/tree on authority.
// Native Blueprint copies, Designer members and owned replacement operations
// retain their respective native/feature owners; no global preview is consulted.
namespace
{
struct FContext
{
    FObjectKey RootKey;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AFGHologram> Root;
    TSharedPtr<TStrongObjectPtr<AFGHologram>> RootPin;
    FNetConstructionID Id;
    FTransform Pose;
    TWeakObjectPtr<AFGBlueprintProxy> Group;
    bool bBlueprint = false;
    bool bHadDesigner = false;
    bool bMultiGrid = false;
    bool bCreatingGroup = false;
    bool bCreationAttempted = false;
};
TArray<TSharedPtr<FContext>> Contexts;
bool SameInput(const FNetConstructionID& A, const FNetConstructionID& B)
{
    return A.NetPlayerID == B.NetPlayerID && A.Server_ID == B.Server_ID && A.Client_ID == B.Client_ID;
}
bool HasGrid(AFGHologram* Root)
{
    if (!IsValid(Root)) return false;
    for (auto* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->ActorHasTag(TEXT("SF_GridChild"))) return true;
    return false;
}
AFGHologram* ParentOf(AFGHologram* Preview)
{
    if (auto* Parent = Preview->GetParentHologram()) return Parent;
    const auto* Data = USFHologramDataRegistry::GetData(Preview);
    return Data ? Data->ParentHologram.Get() : nullptr;
}
}

FSFNativeConstructionContext::FScope::FScope(AFGHologram* Preview, const FNetConstructionID& Id)
{
    if (!IsInGameThread() || !IsValid(Preview)) return;
    bEntered = true;
    auto* World = Preview->GetWorld();
    TSharedPtr<FContext> Outer;
    if (!Contexts.IsEmpty()) Outer = Contexts.Last();
    auto* Root = Preview;
    TSet<AFGHologram*> Seen;
    while (Root)
    {
        if (Outer && Outer->World.Get() == World && SameInput(Outer->Id, Id)
            && FObjectKey(Root) == Outer->RootKey)
        { Contexts.Add(Outer); return; }
        if (!IsValid(Root) || Seen.Contains(Root) || Root->GetWorld() != World || !Root->HasAuthority())
        { Contexts.Add(TSharedPtr<FContext>()); return; }
        Seen.Add(Root);
        auto* Parent = ParentOf(Root);
        if (!Parent) break;
        Root = Parent;
    }
    if (!IsValid(Root) || !IsValid(World) || World->bIsTearingDown || World->GetNetMode() == NM_Client)
    { Contexts.Add(TSharedPtr<FContext>()); return; }
    auto Basis = MakeShared<FContext>();
    Basis->RootKey = FObjectKey(Root); Basis->Root = Root; Basis->World = World;
    Basis->RootPin = MakeShared<TStrongObjectPtr<AFGHologram>>(Root);
    Basis->Id = Id; Basis->Pose = Root->GetActorTransform();
    Basis->bBlueprint = Root->IsA<AFGBlueprintHologram>();
    Basis->bHadDesigner = Root->GetBlueprintDesigner() != nullptr;
    Basis->bMultiGrid = HasGrid(Root);
    Contexts.Add(MoveTemp(Basis));
}
FSFNativeConstructionContext::FScope::~FScope()
{
    if (bEntered) Contexts.Pop(EAllowShrinking::No);
}
AFGHologram* FSFNativeConstructionContext::GetRoot()
{
    return IsInGameThread() && !Contexts.IsEmpty() && Contexts.Last() ? Contexts.Last()->Root.Get() : nullptr;
}
bool FSFNativeConstructionContext::MatchesInput(const FObjectKey& Root, const FNetConstructionID& Id, const UWorld* World)
{
    if (!IsInGameThread() || Contexts.IsEmpty()) return false;
    const auto Basis = Contexts.Last();
    return Basis && Basis->RootKey == Root && Basis->World.Get() == World && SameInput(Basis->Id, Id);
}

bool FSFNativeConstructionContext::AdoptGroup(AFGHologram* Root, const FNetConstructionID& Id, AFGBlueprintProxy* Group)
{
    if (!IsInGameThread() || Contexts.IsEmpty() || !IsValid(Root) || !IsValid(Group)) return false;
    const auto Basis = Contexts.Last();
    if (!Basis || Basis->RootKey != FObjectKey(Root) || !SameInput(Basis->Id, Id)
        || Basis->bBlueprint || Basis->bHadDesigner || Root->GetBlueprintDesigner()
        || Basis->World.Get() != Root->GetWorld() || Group->GetWorld() != Root->GetWorld()
        || !Group->HasAuthority() || Group->IsActorBeingDestroyed()) return false;
    if (Basis->Group.IsValid() && Basis->Group.Get() != Group) return false;
    Basis->Group = Group; Basis->bCreationAttempted = true;
    return true;
}
AFGBlueprintProxy* FSFNativeConstructionContext::GetGroup(const UWorld* World)
{
    if (!IsInGameThread() || Contexts.IsEmpty()) return nullptr;
    const auto Basis = Contexts.Last();
    auto* Group = Basis ? Basis->Group.Get() : nullptr;
    return Basis && !Basis->bBlueprint && !Basis->bHadDesigner && Basis->World.Get() == World
        && IsValid(Group) && !Group->IsActorBeingDestroyed() && Group->HasAuthority() && Group->GetWorld() == World ? Group : nullptr;
}

AFGBlueprintProxy* FSFNativeConstructionContext::GroupBuildable(AFGBuildable* Buildable, bool& bCreatedProxy)
{
    bCreatedProxy = false;
    if (!IsInGameThread() || Contexts.IsEmpty() || !IsValid(Buildable)) return nullptr;
    const auto Basis = Contexts.Last();
    if (!Basis || Basis->bBlueprint || Basis->bHadDesigner || Basis->bCreatingGroup) return nullptr;
    auto* World = Basis->World.Get();
    if (!IsValid(World) || World->bIsTearingDown || World->GetNetMode() == NM_Client
        || Buildable->GetWorld() != World || !Buildable->HasAuthority() || Buildable->IsActorBeingDestroyed()
        || Buildable->GetBlueprintDesigner() || Buildable->GetBlueprintProxy()) return nullptr;
    // A spec handler may expand after the outer base-hook entry. Learn that tree
    // before its first native spawn, then preserve the verdict through preview cleanup.
    Basis->bMultiGrid |= HasGrid(Basis->Root.Get());
    if (!Basis->bMultiGrid) return nullptr;
    auto* Group = Basis->Group.Get();
    if (!Group)
    {
        if (Basis->bCreationAttempted) return nullptr; // Do not retry an uncertain native spawn.
        Basis->bCreationAttempted = true; Basis->bCreatingGroup = true;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Group = World->SpawnActor<AFGBlueprintProxy>(AFGBlueprintProxy::StaticClass(), Basis->Pose, Params);
        Basis->bCreatingGroup = false;
        Basis->Group = Group;
        if (!IsValid(Group) || Group->IsActorBeingDestroyed() || Group->GetWorld() != World || !Group->HasAuthority())
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("Native construction could not create its exact dismantle group; built actors retain native membership, no grouping success or rollback inferred."));
            return nullptr;
        }
        bCreatedProxy = true;
    }
    if (!IsValid(Buildable) || Buildable->IsActorBeingDestroyed() || Buildable->GetWorld() != World
        || Buildable->GetBlueprintDesigner() || Buildable->GetBlueprintProxy()) return Group;
    Buildable->SetBlueprintProxy(Group);
    Group->RegisterBuildable(Buildable);
    return Group;
}
