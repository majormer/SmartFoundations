// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "Features/AutoConnect/Preview/SFLiftDocking.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"
#include "FGConstructDisqualifier.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

namespace
{
    UFGFactoryConnectionComponent* ResolveBuiltEndpoint(UFGFactoryConnectionComponent* Source)
    {
        if (!IsValid(Source)) return nullptr;
        if (auto* Proxy = Cast<ASFLiftRouteProxyHologram>(Source->GetOwner()))
            return Proxy->ResolveBuiltPort(Source->GetFName());
        if (auto* Lift = Cast<ASFLiftRouteHologram>(Source->GetOwner()))
        {
            if (!Lift->GetBuiltLift()) return nullptr;
            TArray<UFGFactoryConnectionComponent*> Ports;
            Lift->GetBuiltLift()->GetComponents(Ports);
            for (auto* Port : Ports) if (Port->GetFName() == Source->GetFName()) return Port;
            return nullptr;
        }
        if (Cast<AFGBuildable>(Source->GetOwner())) return Source;
        auto* Hologram = Cast<AFGBuildableHologram>(Source->GetOwner());
        if (!Hologram) return nullptr;
        // A new distributor has been constructed before its native children. Match
        // its class, full pose and named port uniquely, never a nearby free port.
        UFGFactoryConnectionComponent* Match = nullptr;
        for (TActorIterator<AFGBuildable> It(Source->GetWorld()); It; ++It)
        {
            if (It->GetClass() != Hologram->GetBuildClass()
                || !It->GetActorLocation().Equals(Hologram->GetActorLocation(), 0.1)
                || !It->GetActorQuat().Equals(Hologram->GetActorQuat(), 0.0001)
                || !It->GetActorScale3D().Equals(Hologram->GetActorScale3D(), 0.0001)) continue;
            TArray<UFGFactoryConnectionComponent*> Ports;
            It->GetComponents(Ports);
            for (auto* Port : Ports)
                if (Port->GetFName() == Source->GetFName())
                { if (Match) return nullptr; Match = Port; }
        }
        return Match;
    }

    void Wire(UFGFactoryConnectionComponent* Mouth, UFGFactoryConnectionComponent* Peer)
    {
        if (IsValid(Mouth) && IsValid(Peer) && !Mouth->IsConnected() && !Peer->IsConnected() && Mouth->CanConnectTo(Peer))
            Mouth->SetConnection(Peer);
    }
}

void ASFLiftRouteHologram::SetAnchor(UFGFactoryConnectionComponent* Connection)
{
    Anchor = Connection;
    const auto* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    if (Property) Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 0), Connection);
}

UFGFactoryConnectionComponent* ASFLiftRouteHologram::ResolveAnchor() const { return ResolveBuiltEndpoint(Anchor); }

void ASFLiftRouteHologram::SetDirectPeer(UFGFactoryConnectionComponent* Connection)
{
    DirectPeer = Connection;
    const auto* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    if (Property) Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 1), Connection);
}

bool ASFLiftRouteHologram::TrySetDirectPeer(UFGFactoryConnectionComponent* Connection)
{
    auto* Mouth = GetMouth(1);
    if (!Mouth || !IsValid(Connection) || Connection->IsConnected()
        || !Mouth->CanConnectTo(Connection)
        || !SFLiftDocking::Matches(Mouth->GetComponentLocation(), Mouth->GetConnectorNormal(), Mouth->GetConnectorClearance(),
            Connection->GetComponentLocation(), Connection->GetConnectorNormal(), Connection->GetConnectorClearance())) return false;
    SetDirectPeer(Connection);
    return true;
}

UFGFactoryConnectionComponent* ASFLiftRouteHologram::GetMouth(int32 Index) const
{
    const auto* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mConnectionComponents"));
    return Property && Index >= 0 && Index < Property->ArrayDim
        ? Cast<UFGFactoryConnectionComponent>(Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, Index))) : nullptr;
}

void ASFLiftRouteHologram::SetRouteGeometry(const FTransform& Pose, const FTransform& Top)
{
    SetActorTransform(Pose);
    SetTopTransform(Top);
    const bool bAnchorInput = Anchor && Anchor->GetDirection() == EFactoryConnectionDirection::FCD_INPUT;
    if (auto* Reversed = FindFProperty<FBoolProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mIsReversed")))
        Reversed->SetPropertyValue_InContainer(this, bAnchorInput);
    if (auto* Step = FindFProperty<FIntProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mActivePointIdx")))
        Step->SetPropertyValue_InContainer(this, 1);
    // Native output anchors have origin INPUT / remote OUTPUT; input anchors
    // reverse both mouths. Signed top height independently selects up or down.
    if (auto* Arrow = FindFProperty<FEnumProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mArrowDirection")))
        Arrow->GetUnderlyingProperty()->SetIntPropertyValue(Arrow->ContainerPtrToValuePtr<void>(this), static_cast<int64>(bAnchorInput ? EFactoryConnectionDirection::FCD_INPUT : EFactoryConnectionDirection::FCD_OUTPUT));
    if (auto* Refresh = FindFunction(TEXT("OnRep_TopTransform"))) ProcessEvent(Refresh, nullptr);
    if (auto* Refresh = FindFunction(TEXT("OnRep_ArrowDirection"))) ProcessEvent(Refresh, nullptr);
    SetActorTransform(Pose);
    // OnRep rebuilds the native meshes/clearance, but placement normally updates
    // the mouth scene components separately. Carry the same selected transforms.
    if (auto* Origin = GetMouth(0)) Origin->SetRelativeTransform(FTransform::Identity);
    if (auto* Remote = GetMouth(1)) Remote->SetRelativeTransform(Top);
    UpdateConnectionDirections();
}

void ASFLiftRouteHologram::CheckValidPlacement()
{
    // The build gun resets the initial disqualifier before each native validation.
    // This child is evaluated directly by its route owner, so perform that same
    // reset here; never exempt an actual native geometry rejection.
    ResetConstructDisqualifiers();
    AFGConveyorLiftHologram::CheckValidPlacement();
}

bool ASFLiftRouteHologram::IsNativePlacementValid()
{
    CheckValidPlacement();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;
    GetConstructDisqualifiers(Reasons);
    for (auto Reason : Reasons)
        if (Reason && !UFGConstructDisqualifier::GetIsSoftDisqualifier(Reason)) return false;
    return true;
}

AActor* ASFLiftRouteHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID)
{
    auto* Peer = ResolveAnchor();
    if (!Peer || Peer->IsConnected()) return nullptr;
    auto* FarPeer = DirectPeer ? ResolveBuiltEndpoint(DirectPeer) : nullptr;
    if (DirectPeer && (!FarPeer || FarPeer->IsConnected())) return nullptr;
    SetAnchor(Peer);
    SetDirectPeer(FarPeer);
    return AFGConveyorLiftHologram::Construct(OutChildren, ID);
}

void ASFLiftRouteHologram::ConfigureComponents(AFGBuildable* Buildable) const
{
    AFGConveyorLiftHologram::ConfigureComponents(Buildable);
    BuiltLift = Cast<AFGBuildableConveyorLift>(Buildable);
    if (BuiltLift.IsValid())
    {
        auto* Peer = ResolveAnchor();
        if (Peer) Wire(Peer->GetDirection() == EFactoryConnectionDirection::FCD_INPUT
            ? BuiltLift->GetConnection1() : BuiltLift->GetConnection0(), Peer);
        auto* FarPeer = ResolveBuiltEndpoint(DirectPeer);
        if (FarPeer) Wire(FarPeer->GetDirection() == EFactoryConnectionDirection::FCD_INPUT
            ? BuiltLift->GetConnection1() : BuiltLift->GetConnection0(), FarPeer);
    }
    // BeginPlay owns bucket/chain registration. Never AddConveyor a second time.
}


UFGFactoryConnectionComponent* ASFLiftRouteHologram::ResolveEndpoint(UFGFactoryConnectionComponent* Source) { return ResolveBuiltEndpoint(Source); }
