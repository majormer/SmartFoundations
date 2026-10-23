// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGConstructDisqualifier.h"
void ASFLiftRouteBeltHologram::SetRouteEndpoints(UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To)
{ RouteFrom = From; RouteTo = To; }

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
    auto* From = ASFLiftRouteHologram::ResolveEndpoint(RouteFrom);
    auto* To = ASFLiftRouteHologram::ResolveEndpoint(RouteTo);
    if (!From || !To || From->IsConnected() || To->IsConnected()) return nullptr;
    SetSnappedConnections(From, To);
    return AFGConveyorBeltHologram::Construct(OutChildren, ID);
}

void ASFLiftRouteBeltHologram::ConfigureComponents(AFGBuildable* Buildable) const
{
    AFGConveyorBeltHologram::ConfigureComponents(Buildable);
    if (auto* Belt = Cast<AFGBuildableConveyorBelt>(Buildable))
    {
        auto* From = ASFLiftRouteHologram::ResolveEndpoint(RouteFrom);
        auto* To = ASFLiftRouteHologram::ResolveEndpoint(RouteTo);
        if (From && !From->IsConnected() && !Belt->GetConnection0()->IsConnected() && Belt->GetConnection0()->CanConnectTo(From)) Belt->GetConnection0()->SetConnection(From);
        if (To && !To->IsConnected() && !Belt->GetConnection1()->IsConnected() && Belt->GetConnection1()->CanConnectTo(To)) Belt->GetConnection1()->SetConnection(To);
    }
}
