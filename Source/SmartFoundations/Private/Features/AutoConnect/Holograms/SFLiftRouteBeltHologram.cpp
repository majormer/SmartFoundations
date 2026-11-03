// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGConstructDisqualifier.h"
#include "Core/Construction/SFConveyorConstructionLinks.h"
#include "UObject/StrongObjectPtr.h"
void ASFLiftRouteBeltHologram::SetRouteEndpoints(UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To)
{
    RouteFrom = From;
    RouteTo = To;
    SetSnappedConnections(From, To);
    // Routing the spline does not initialize the native hologram mouth roles.
    // ConfigureComponents copies those roles to the built belt before connecting it.
    SetupSnappedConnectionDirections();
}

bool ASFLiftRouteBeltHologram::IsNativePlacementValid()
{
    ResetConstructDisqualifiers();
    AFGConveyorBeltHologram::CheckValidPlacement();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;
    GetConstructDisqualifiers(Reasons);
    for (auto Reason : Reasons)
        if (Reason && !UFGConstructDisqualifier::GetIsSoftDisqualifier(Reason)) return false;
    return true;
}

AActor* ASFLiftRouteBeltHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID)
{
    if (GetNetMode() == NM_Client || !HasAuthority()) return nullptr; // [MP-AUTH]
    TStrongObjectPtr<ASFLiftRouteBeltHologram> KeepPreview(this);
    if (!bDeferRouteConnections && !FSFConveyorConstructionLinks::Prepare(this, ID)) return nullptr;
    auto Reject = [&]() -> AActor*
    {
        if (!bDeferRouteConnections) FSFConveyorConstructionLinks::RejectPrepared(this, ID, TEXT("selected_lift_route_belt_peer_expired"));
        return nullptr;
    };
    auto* From = ASFLiftRouteHologram::ResolveEndpointForConstruction(RouteFrom, ID);
    auto* To = ASFLiftRouteHologram::ResolveEndpointForConstruction(RouteTo, ID);
    if (!From || !To || !IsValid(From->GetOwner()) || !IsValid(To->GetOwner())
        || From->GetOwner()->GetWorld() != GetWorld() || To->GetOwner()->GetWorld() != GetWorld()
        || !From->GetOwner()->HasAuthority() || !To->GetOwner()->HasAuthority()
        || From->GetOwner()->IsActorBeingDestroyed() || To->GetOwner()->IsActorBeingDestroyed()
        || From->IsConnected() || To->IsConnected()
        || From->GetDirection() >= EFactoryConnectionDirection::FCD_ANY || To->GetDirection() >= EFactoryConnectionDirection::FCD_ANY
        || From->GetDirection() == To->GetDirection()) return Reject();
    const auto FromIntent = FSFConveyorConstructionLinks::Capture(From);
    const auto ToIntent = FSFConveyorConstructionLinks::Capture(To);
    SetSnappedConnections(bDeferRouteConnections ? nullptr : From, bDeferRouteConnections ? nullptr : To);
    SetupSnappedConnectionDirections();
    AActor* Result = AFGConveyorBeltHologram::Construct(OutChildren, ID);
    if (bDeferRouteConnections) return Result; // Blueprint seam owner retains the complete shared batch.
    auto* Belt = Cast<AFGBuildableConveyorBelt>(Result);
    TArray<FSFConveyorConstructionEdge> Edges;
    for (const auto& Intent : {FromIntent, ToIntent})
    {
        auto* Peer = Intent.ExistingPort.Get();
        auto* Mouth = IsValid(Peer) ? Peer->GetConnection() : nullptr;
        if (!IsValid(Belt) || !IsValid(Mouth) || Mouth->GetOwner() != Belt || Mouth->GetConnection() != Peer
            || !Mouth->IsConnected() || !Peer->IsConnected())
        { FSFConveyorConstructionLinks::RejectPrepared(this, ID, TEXT("native_lift_route_belt_edge_unresolved")); return Result; }
        Edges.Add({FSFConveyorConstructionLinks::Capture(Mouth), Intent});
    }
    if (!FSFConveyorConstructionLinks::Defer(this, ID, Edges))
        FSFConveyorConstructionLinks::RejectPrepared(this, ID, TEXT("native_lift_route_belt_observation_not_retained"));
    return Result;
}

void ASFLiftRouteBeltHologram::ConfigureComponents(AFGBuildable* Buildable) const
{
    AFGConveyorBeltHologram::ConfigureComponents(Buildable);
    if (bDeferRouteConnections) return;
    if (auto* Belt = Cast<AFGBuildableConveyorBelt>(Buildable))
    {
        if (!Belt->HasAuthority() || Belt->GetNetMode() == NM_Client || Belt->HasActorBegunPlay()
            || Belt->GetConveyorBucketID() != INDEX_NONE || Belt->GetConveyorChainActor()) return; // [MP-AUTH]
        auto* From = ASFLiftRouteHologram::ResolveEndpoint(RouteFrom);
        auto* To = ASFLiftRouteHologram::ResolveEndpoint(RouteTo);
        if (From && !From->IsConnected() && !Belt->GetConnection0()->IsConnected() && Belt->GetConnection0()->CanConnectTo(From)) Belt->GetConnection0()->SetConnection(From);
        if (To && !To->IsConnected() && !Belt->GetConnection1()->IsConnected() && Belt->GetConnection1()->CanConnectTo(To)) Belt->GetConnection1()->SetConnection(To);
    }
}
