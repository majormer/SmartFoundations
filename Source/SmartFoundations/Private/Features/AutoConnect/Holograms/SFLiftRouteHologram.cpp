// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "SmartFoundations.h"
#include "Core/Construction/SFConveyorConstructionLinks.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "Features/AutoConnect/Preview/SFLiftDocking.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"
#include "FGConstructDisqualifier.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UFGFactoryConnectionComponent* ResolveBuiltEndpoint(UFGFactoryConnectionComponent* Source, const FNetConstructionID* Id = nullptr)
    {
        if (!IsValid(Source)) return nullptr;
        FNetConstructionID CurrentId;
        if (!Id && FSFConveyorConstructionLinks::GetCurrentConstructionId(CurrentId)) Id = &CurrentId;
        if (auto* Proxy = Cast<ASFLiftRouteProxyHologram>(Source->GetOwner()))
            return Id ? Proxy->ResolveBuiltPortForConstruction(Source->GetFName(), *Id) : nullptr;
        if (auto* Lift = Cast<ASFLiftRouteHologram>(Source->GetOwner()))
        {
            auto* Built = Id ? Lift->GetBuiltLiftForConstruction(*Id) : nullptr;
            if (!Built) return nullptr;
            TArray<UFGFactoryConnectionComponent*> Ports;
            Built->GetComponents(Ports);
            UFGFactoryConnectionComponent* Match = nullptr;
            for (auto* Port : Ports)
                if (Port->GetDirection() == Source->GetDirection()
                    && Port->GetComponentLocation().Equals(Source->GetComponentLocation(), 0.2))
                { if (Match) return nullptr; Match = Port; }
            return Match;
        }
        if (Cast<AFGBuildable>(Source->GetOwner())) return Source;
        // Only the actual ConfigureActor result for this native construction
        // may satisfy a preview endpoint. A matching class/pose in the world can
        // be an older coincident actor and is never an identity receipt.
        return FSFConveyorConstructionLinks::ResolveConfigured(Source, Id);
    }

    void Wire(UFGFactoryConnectionComponent* Mouth, UFGFactoryConnectionComponent* Peer)
    {
        auto* NewConveyor = Mouth ? Cast<AFGBuildableConveyorBase>(Mouth->GetOwner()) : nullptr;
        // This is only the native ConfigureComponents seam, before BeginPlay
        // registration. Live conveyors must use the retained mutation adapter.
        if (!IsValid(NewConveyor) || !NewConveyor->HasAuthority() || NewConveyor->GetNetMode() == NM_Client
            || NewConveyor->HasActorBegunPlay() || NewConveyor->GetConveyorBucketID() != INDEX_NONE
            || NewConveyor->GetConveyorChainActor()) return; // [MP-AUTH]
        auto* PeerOwner = Peer ? Peer->GetOwner() : nullptr;
        if (IsValid(Mouth) && IsValid(Peer) && IsValid(PeerOwner) && PeerOwner->HasAuthority()
            && !PeerOwner->IsActorBeingDestroyed() && PeerOwner->GetWorld() == NewConveyor->GetWorld()
            && !Mouth->IsConnected() && !Peer->IsConnected() && Mouth->CanConnectTo(Peer))
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
    if (GetNetMode() == NM_Client || !HasAuthority()) return nullptr; // [MP-AUTH]
    TStrongObjectPtr<ASFLiftRouteHologram> KeepPreview(this);
    if (!FSFConveyorConstructionLinks::Prepare(this, ID)) return nullptr;
    auto Reject = [&](const TCHAR* Reason) -> AActor*
    { FSFConveyorConstructionLinks::RejectPrepared(this, ID, Reason); return nullptr; };
    auto* Peer = ResolveBuiltEndpoint(Anchor, &ID);
    if (!Peer || !IsValid(Peer->GetOwner()) || Peer->GetOwner()->GetWorld() != GetWorld()
        || !Peer->GetOwner()->HasAuthority() || Peer->GetOwner()->IsActorBeingDestroyed() || Peer->IsConnected()) return Reject(TEXT("selected_lift_route_anchor_expired"));
    auto* FarPeer = DirectPeer ? ResolveBuiltEndpoint(DirectPeer, &ID) : nullptr;
    if (DirectPeer && (!FarPeer || !IsValid(FarPeer->GetOwner()) || FarPeer->GetOwner()->GetWorld() != GetWorld()
        || !FarPeer->GetOwner()->HasAuthority() || FarPeer->GetOwner()->IsActorBeingDestroyed() || FarPeer->IsConnected())) return Reject(TEXT("selected_lift_route_remote_peer_expired"));
    if (Peer->GetDirection() >= EFactoryConnectionDirection::FCD_ANY
        || (FarPeer && (FarPeer->GetDirection() >= EFactoryConnectionDirection::FCD_ANY
            || Peer->GetDirection() == FarPeer->GetDirection()))) return Reject(TEXT("selected_lift_route_flow_unresolved"));
    const auto AnchorIntent = FSFConveyorConstructionLinks::Capture(Peer);
    const auto RemoteIntent = FSFConveyorConstructionLinks::Capture(FarPeer);
    if (!AnchorIntent.bValid || (FarPeer && !RemoteIntent.bValid)) return Reject(TEXT("selected_lift_route_identity_unavailable"));
    SetAnchor(Peer);
    SetDirectPeer(FarPeer);
    BuiltLift.Reset();
    BuiltPlayerId = ID.NetPlayerID; BuiltServerId = ID.Server_ID; BuiltClientId = ID.Client_ID;
    AActor* Result = AFGConveyorLiftHologram::Construct(OutChildren, ID);
    auto* Lift = Cast<AFGBuildableConveyorLift>(Result);
    TArray<FSFConveyorConstructionEdge> Edges;
    auto Observe = [&](const FSFConveyorConstructionEndpoint& Intent)
    {
        if (!Intent.bValid) return true;
        auto* Selected = Intent.ExistingPort.Get();
        auto* Mouth = IsValid(Selected) ? Selected->GetConnection() : nullptr;
        if (!IsValid(Lift) || !IsValid(Mouth) || Mouth->GetOwner() != Lift
            || Mouth->GetConnection() != Selected || !Mouth->IsConnected() || !Selected->IsConnected()) return false;
        Edges.Add({FSFConveyorConstructionLinks::Capture(Mouth), Intent});
        return true;
    };
    if (!Observe(AnchorIntent) || !Observe(RemoteIntent) || !FSFConveyorConstructionLinks::Defer(this, ID, Edges))
        FSFConveyorConstructionLinks::RejectPrepared(this, ID, TEXT("native_lift_route_results_or_observation_unresolved"));
    return Result;
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

UFGFactoryConnectionComponent* ASFLiftRouteHologram::ResolveEndpointForConstruction(UFGFactoryConnectionComponent* Source,
    const FNetConstructionID& Id) { return ResolveBuiltEndpoint(Source, &Id); }

AFGBuildableConveyorLift* ASFLiftRouteHologram::GetBuiltLiftForConstruction(const FNetConstructionID& Id) const
{
    auto* Actor = BuiltLift.Get();
    return Id.NetPlayerID == BuiltPlayerId && Id.Server_ID == BuiltServerId && Id.Client_ID == BuiltClientId
        && IsValid(Actor) && Actor->HasActorBegunPlay() && !Actor->IsActorBeingDestroyed() ? Actor : nullptr;
}
