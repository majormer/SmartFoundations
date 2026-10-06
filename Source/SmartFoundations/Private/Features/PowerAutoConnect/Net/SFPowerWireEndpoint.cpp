// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/Net/SFPowerWireEndpoint.h"

#include "Buildables/FGBuildable.h"
#include "FGCircuitConnectionComponent.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Features/PowerAutoConnect/SFBlueprintPowerService.h"
#include "FGBlueprintProxy.h"

FSFPowerWireEndpoint FSFPowerWireEndpoint::Capture(UFGCircuitConnectionComponent* Connection)
{
    FSFPowerWireEndpoint Result;
    if (!IsValid(Connection) || !IsValid(Connection->GetOwner())) return Result;
    AActor* Owner = Connection->GetOwner();
    if (Owner->IsA<AFGBlueprintHologram>())
        return FSFBlueprintPowerService::CaptureEndpoint(Cast<UFGPowerConnectionComponent>(Connection));
    Result.ComponentName = Connection->GetFName();
    Result.OwnerLocation = Owner->GetActorLocation();
    if (AFGHologram* Hologram = Cast<AFGHologram>(Owner))
        Result.BuildClass = Hologram->GetBuildClass();
    else
    {
        Result.bExisting = true;
        Result.ExistingActor = Cast<AFGBuildable>(Owner);
        Result.BuildClass = Owner->GetClass();
    }
    return Result;
}

UFGCircuitConnectionComponent* FSFPowerWireEndpoint::Resolve(AActor* BuiltParent, const TArray<AActor*>& BuiltChildren) const
{
    if (ComponentName.IsNone() || !BuildClass) return nullptr;
    AActor* Match = nullptr;
    if (bExisting)
    {
        Match = ExistingActor.Get();
        if (!IsValid(Match) || Match->GetClass() != BuildClass) return nullptr;
    }
    else
    {
        bool bAmbiguous = false;
        const auto Consider = [&](AActor* Candidate)
        {
            if (IsValid(Candidate) && Candidate->GetClass() == BuildClass
                && Candidate->GetActorLocation().Equals(OwnerLocation, 1.0))
            {
                if (Match && Match != Candidate) bAmbiguous = true;
                Match = Candidate;
            }
        };
        const auto ConsiderContent = [&](AActor* Candidate)
        {
            Consider(Candidate);
            if (AFGBlueprintProxy* Proxy = Cast<AFGBlueprintProxy>(Candidate))
                for (AFGBuildable* Building : Proxy->GetBuildables()) Consider(Building);
        };
        ConsiderContent(BuiltParent);
        for (AActor* Child : BuiltChildren) ConsiderContent(Child);
        if (bAmbiguous) return nullptr;
    }
    if (!IsValid(Match)) return nullptr;
    TInlineComponentArray<UFGCircuitConnectionComponent*> Ports(Match);
    for (UFGCircuitConnectionComponent* Port : Ports)
        if (IsValid(Port) && Port->GetFName() == ComponentName) return Port;
    return nullptr;
}
