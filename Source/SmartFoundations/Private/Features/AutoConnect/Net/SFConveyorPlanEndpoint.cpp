// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Net/SFConveyorPlanEndpoint.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Buildables/FGBuildable.h"
#include "FGFactoryConnectionComponent.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Features/Scaling/SFGridCoordComponent.h"

// SP/MP DIVERGENCE MAP — belt endpoint intent
// [MP-CLIENT] Capture the selected component and its native/template role.
// [MP-AUTH] Resolve against the original existing actor or authority preview tree.
// Actual built identities are subsequently supplied by native construction receipts.
namespace
{
bool SameConveyorEndpointPose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .2)
        && A.GetRotation().Equals(B.GetRotation(), .0001) && A.GetScale3D().Equals(B.GetScale3D(), .0001);
}
bool CopyRole(AFGBlueprintHologram* Copy, AFGHologram* Root, FIntVector& Cell)
{
    if (Copy == Root) { Cell = FIntVector::ZeroValue; return true; }
    return USFGridCoordComponent::TryGetCell(Copy, Cell);
}
void WalkConveyorEndpointTree(AFGHologram* Root, TArray<AFGHologram*>& Tree)
{
    if (!IsValid(Root)) return;
    TSet<AFGHologram*> Seen;
    Tree.Add(Root);
    for (int32 Index = 0; Index < Tree.Num() && Tree.Num() <= 100000; ++Index)
    {
        auto* Node = Tree[Index];
        if (!IsValid(Node) || Seen.Contains(Node)) continue;
        Seen.Add(Node);
        for (auto* Child : Node->GetHologramChildren())
            if (IsValid(Child) && Child->GetWorld() == Root->GetWorld() && !Seen.Contains(Child)) Tree.AddUnique(Child);
    }
    if (Tree.Num() > 100000) Tree.Reset();
}
}

FSFConveyorPlanEndpoint FSFConveyorPlanEndpoint::Capture(UFGFactoryConnectionComponent* Port, AFGHologram* Root, bool bRetainPlacementAnchor)
{
    FSFConveyorPlanEndpoint E;
    if (!Port) return E;
    E.bPresent = true; // An invalid selected port must not turn into an intentionally open end.
    if (!IsValid(Port) || !IsValid(Port->GetOwner()) || !IsValid(Root)
        || Port->GetWorld() != Root->GetWorld()) return E;
    if (!bRetainPlacementAnchor && Port->GetDirection() == EFactoryConnectionDirection::FCD_SNAP_ONLY)
    { E.bPresent = false; return E; } // Support positioning anchors are not materialized factory peers.
    E.PortPose = Port->GetComponentTransform(); E.PortName = Port->GetFName();
    E.Direction = static_cast<int32>(Port->GetDirection()); E.Clearance = Port->GetConnectorClearance();
    E.OwnerPose = Port->GetOwner()->GetActorTransform();
    if (auto* Blueprint = Cast<AFGBlueprintHologram>(Port->GetOwner()))
    {
        E.bBlueprintMember = true;
        TArray<UFGFactoryConnectionComponent*> Duplicates;
        FSFBlueprintSeamService::GetDuplicatedBeltConnectors(Blueprint, Duplicates);
        const int32 Index = Duplicates.IndexOfByKey(Port);
        auto* Original = FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, Index);
        auto* Owner = IsValid(Original) ? Cast<AFGBuildable>(Original->GetOwner()) : nullptr;
        if (!IsValid(Owner) || !CopyRole(Blueprint, Root, E.CopyCell)) return E;
        E.BuildClass = Owner->GetClass(); E.OriginalOwnerPose = Owner->GetActorTransform();
        E.PortName = Original->GetFName();
    }
    else if (auto* Preview = Cast<AFGHologram>(Port->GetOwner())) E.BuildClass = Preview->GetBuildClass();
    else
    {
        E.ExistingActor = Cast<AFGBuildable>(Port->GetOwner()); E.bExisting = true;
        if (E.ExistingActor) E.BuildClass = E.ExistingActor->GetClass();
    }
    return E;
}

bool FSFConveyorPlanEndpoint::IsPlacementAnchor() const
{
    return bPresent && Direction == static_cast<int32>(EFactoryConnectionDirection::FCD_SNAP_ONLY);
}

bool FSFConveyorPlanEndpoint::IsBounded() const
{
    if (!bPresent) return !bExisting && !bBlueprintMember && !ExistingActor && !BuildClass;
    if (!BuildClass || PortName.IsNone() || PortName.ToString().Len() > 1024
        || Direction < 0 || (Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_ANY) && !IsPlacementAnchor())
        || OwnerPose.ContainsNaN() || PortPose.ContainsNaN() || OriginalOwnerPose.ContainsNaN()
        || !FMath::IsFinite(Clearance) || Clearance < 0 || Clearance > 100000) return false;
    if (bExisting ? (!ExistingActor || bBlueprintMember) : ExistingActor != nullptr) return false;
    return FMath::Abs(int64(CopyCell.X)) <= 2000 && FMath::Abs(int64(CopyCell.Y)) <= 2000 && FMath::Abs(int64(CopyCell.Z)) <= 2000;
}

UFGFactoryConnectionComponent* FSFConveyorPlanEndpoint::Resolve(AFGHologram* Root) const
{
    if (!bPresent || !IsBounded() || !IsValid(Root) || !Root->HasAuthority()
        || Root->GetNetMode() == NM_Client || !Root->GetWorld() || Root->GetWorld()->bIsTearingDown) return nullptr;
    UFGFactoryConnectionComponent* Found = nullptr;
    auto Match = [&](UFGFactoryConnectionComponent* Port)
    {
        return IsValid(Port) && !Port->IsConnected() && !Port->GetConnection()
            && static_cast<int32>(Port->GetDirection()) == Direction
            && SameConveyorEndpointPose(Port->GetComponentTransform(), PortPose)
            && FMath::IsNearlyEqual(Port->GetConnectorClearance(), Clearance, .01f);
    };
    if (bExisting)
    {
        auto* Actor = ExistingActor.Get();
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasAuthority() || !Actor->HasActorBegunPlay()
            || Actor->GetWorld() != Root->GetWorld() || Actor->GetClass() != BuildClass || !SameConveyorEndpointPose(Actor->GetActorTransform(), OwnerPose)) return nullptr;
        TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
        for (auto* Port : Ports) if (Port->GetFName() == PortName && Match(Port))
        { if (Found) return nullptr; Found = Port; }
        return Found;
    }
    TArray<AFGHologram*> Tree; WalkConveyorEndpointTree(Root, Tree);
    for (auto* Node : Tree)
    {
        if (!IsValid(Node) || Node->IsActorBeingDestroyed() || !Node->HasAuthority() || Node->GetWorld() != Root->GetWorld()) continue;
        if (bBlueprintMember)
        {
            auto* Copy = Cast<AFGBlueprintHologram>(Node); FIntVector Cell;
            if (!Copy || !CopyRole(Copy, Root, Cell) || Cell != CopyCell) continue;
            TArray<UFGFactoryConnectionComponent*> Duplicates;
            FSFBlueprintSeamService::GetDuplicatedBeltConnectors(Copy, Duplicates);
            for (int32 Index = 0; Index < Duplicates.Num(); ++Index)
            {
                auto* Original = FSFBlueprintSeamService::GetOriginalBeltConnector(Copy, Index);
                auto* Owner = IsValid(Original) ? Original->GetOwner() : nullptr;
                if (!IsValid(Owner) || Owner->GetClass() != BuildClass || Original->GetFName() != PortName
                    || !SameConveyorEndpointPose(Owner->GetActorTransform(), OriginalOwnerPose) || !Match(Duplicates[Index])) continue;
                if (Found) return nullptr; Found = Duplicates[Index];
            }
        }
        else if (Node->GetBuildClass() == BuildClass && SameConveyorEndpointPose(Node->GetActorTransform(), OwnerPose))
        {
            TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Node);
            for (auto* Port : Ports) if (Port->GetFName() == PortName && Match(Port))
            { if (Found) return nullptr; Found = Port; }
        }
    }
    return Found;
}

void FSFConveyorPlanEndpoint::AddPreviewWorldOffset(const FVector& Shift)
{
    if (!bPresent || bExisting) return;
    PortPose.AddToTranslation(Shift);
    if (!bBlueprintMember) OwnerPose.AddToTranslation(Shift);
}
