// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Core/Construction/SFConveyorConstructionLinks.h"

// SP/MP DIVERGENCE MAP — Blueprint conveyor port bindings
// [MP-AUTH] Captured against authority's reconstructed previews before native
// copy construction. Only that copy's returned actors can satisfy a binding.
namespace
{
struct FPortBinding
{
    FSFConveyorConstructionEndpoint Intent;
    FTransform OwnerPose;
};
struct FCopyBindings
{
    TWeakObjectPtr<UWorld> World;
    FNetConstructionID Id;
    TArray<FPortBinding> Ports;
};
TMap<TWeakObjectPtr<AFGBlueprintHologram>, FCopyBindings> Copies;
bool SameBlueprintBindingPose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .2)
        && A.GetRotation().Equals(B.GetRotation(), .0001) && A.GetScale3D().Equals(B.GetScale3D(), .0001);
}
}

void FSFBlueprintSeamService::CaptureBeltConstructionBindings(AFGBlueprintHologram* Blueprint, const FNetConstructionID& Id)
{
    if (!IsInGameThread() || !IsValid(Blueprint) || !Blueprint->HasAuthority() || Blueprint->GetNetMode() == NM_Client) return;
    if (const auto* Existing = Copies.Find(Blueprint); Existing && Existing->World.Get() == Blueprint->GetWorld()
        && Existing->Id.NetPlayerID == Id.NetPlayerID && Existing->Id.Server_ID == Id.Server_ID
        && Existing->Id.Client_ID == Id.Client_ID) return; // Never replace the pre-cleanup intent with a later empty map.
    for (auto It = Copies.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().World.IsValid()) It.RemoveCurrent();
    FCopyBindings Capture;
    Capture.World = Blueprint->GetWorld(); Capture.Id = Id;
    for (const auto& Mapping : Blueprint->mDuplicateConnectionToOriginalMap)
    {
        auto* Duplicate = Cast<UFGFactoryConnectionComponent>(Mapping.Key.Get());
        auto* Original = Cast<UFGFactoryConnectionComponent>(Mapping.Value.Get());
        auto* Actor = Original ? Cast<AFGBuildable>(Original->GetOwner()) : nullptr;
        if (!IsValid(Duplicate) || !IsValid(Original) || !IsValid(Actor)) continue;
        if (!FSFConveyorConstructionLinks::DefinePreviewPort(Duplicate, Actor->GetClass(), Original->GetFName(), Id)) continue;
        const auto PortLocal = Original->GetComponentTransform().GetRelativeTransform(Actor->GetActorTransform());
        Capture.Ports.Add({FSFConveyorConstructionLinks::Capture(Duplicate, &Id),
            PortLocal.Inverse() * Duplicate->GetComponentTransform()});
    }
    Copies.Add(Blueprint, MoveTemp(Capture));
}

void FSFBlueprintSeamService::RecordBeltCopyBindings(AFGBlueprintHologram* Blueprint,
    const TArray<AActor*>& ReturnedMembers, const FNetConstructionID& Id)
{
    const auto* Stored = Copies.Find(Blueprint);
    if (!IsInGameThread() || !Blueprint || !Stored || !Stored->World.IsValid()
        || Stored->Id.NetPlayerID != Id.NetPlayerID || Stored->Id.Server_ID != Id.Server_ID
        || Stored->Id.Client_ID != Id.Client_ID) return;
    auto* World = Stored->World.Get();
    if (World->bIsTearingDown || World->GetNetMode() == NM_Client) return;
    // Native construction can retire preview components. The value snapshot
    // above, not the live duplicate map or an actor-name lookup, owns provenance.
    const auto Ports = Stored->Ports;
    Copies.Remove(Blueprint);
    int32 Unresolved = 0;
    for (const auto& Entry : Ports)
    {
        AFGBuildable* Match = nullptr;
        bool Ambiguous = false;
        TSet<AActor*> Seen;
        for (auto* Actor : ReturnedMembers)
        {
            auto* Candidate = Cast<AFGBuildable>(Actor);
            if (!IsValid(Candidate) || Seen.Contains(Candidate) || !Candidate->HasAuthority()
                || Candidate->IsActorBeingDestroyed() || Candidate->GetWorld() != World
                || Candidate->GetClass() != Entry.Intent.ExpectedClass.Get()
                || !SameBlueprintBindingPose(Candidate->GetActorTransform(), Entry.OwnerPose)) continue;
            Seen.Add(Candidate);
            TInlineComponentArray<UFGFactoryConnectionComponent*> ActualPorts(Candidate);
            bool PortMatched = false;
            for (auto* Port : ActualPorts)
            {
                if (!IsValid(Port) || static_cast<int32>(Port->GetDirection()) != Entry.Intent.Direction
                    || !Port->GetComponentLocation().Equals(Entry.Intent.Position, .2)) continue;
                if (!Candidate->IsA<AFGBuildableConveyorLift>() && Port->GetFName() != Entry.Intent.PortName) continue;
                if (PortMatched) { Ambiguous = true; break; }
                PortMatched = true;
            }
            if (!PortMatched || Ambiguous) continue;
            if (Match && Match != Candidate) { Ambiguous = true; break; }
            Match = Candidate;
        }
        if (Ambiguous || !Match || !FSFConveyorConstructionLinks::RecordPreviewPort(Entry.Intent, Match)) ++Unresolved;
    }
    if (Unresolved)
        UE_LOG(LogSmartFoundations, Warning, TEXT("Blueprint copy retains %d unresolved exact conveyor port binding(s); no foreign actor or nearest-port substitution is allowed."), Unresolved);
}
