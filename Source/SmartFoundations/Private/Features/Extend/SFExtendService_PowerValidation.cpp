// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Data/SFHologramDataRegistry.h"
#include "Hologram/FGHologram.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWire.h"
#include "Constants/SFAssetPaths.h"
#include "FGCircuitConnectionComponent.h"

bool USFExtendService::ValidatePowerPlanForConstruction(AFGHologram* Parent, FString& OutReason) const
{
    if (!IsValid(Parent) || !StoredCloneTopology.IsValid())
    {
        OutReason = TEXT("Extend construction has no reconstructed topology");
        return false;
    }
    FSFCloneTopology Checked = *StoredCloneTopology;
    if (!Checked.PowerPlanError.IsEmpty())
    {
        OutReason = Checked.PowerPlanError;
        return false;
    }
    TMap<FString, AFGHologram*> Owners;
    Owners.Add(TEXT("parent"), Parent);
    TArray<AFGHologram*> Pending;
    Pending.Add(Parent);
    TSet<AFGHologram*> Visited;
    while (!Pending.IsEmpty())
    {
        AFGHologram* Holo = Pending.Pop(EAllowShrinking::No);
        if (!IsValid(Holo) || Visited.Contains(Holo)) continue;
        Visited.Add(Holo);
        if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(Holo); Data && !Data->JsonCloneId.IsEmpty())
        {
            AFGHologram* Existing = Owners.FindRef(Data->JsonCloneId);
            if (Existing && Existing != Holo)
            {
                OutReason = TEXT("Extend construction has duplicate owner identities");
                return false;
            }
            Owners.Add(Data->JsonCloneId, Holo);
        }
        for (AFGHologram* Child : Holo->GetHologramChildren()) Pending.Add(Child);
    }

    UClass* WireClass = nullptr;
    for (FSFCloneHologram& Wire : Checked.ChildHolograms)
    {
        if (Wire.Role != TEXT("wire_cost")) continue;
        if (!WireClass) WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
        if (!WireClass || Wire.SplineData.Points.Num() < 2)
        {
            OutReason = TEXT("Extend cable plan has no native wire class or endpoints");
            return false;
        }
        UFGCircuitConnectionComponent* Ports[2] = {};
        FVector Positions[2];
        int32* Capacities[2] = {&Wire.PowerFromCapacity, &Wire.PowerToCapacity};
        const FSFConnectionRef* Refs[2] = {&Wire.PowerFrom, &Wire.PowerTo};
        for (int32 End = 0; End < 2; ++End)
        {
            const FSFConnectionRef& Ref = *Refs[End];
            const bool Source = Ref.Target.StartsWith(TEXT("source:"));
            AActor* Owner = Source ? static_cast<AActor*>(GetSourceBuildableByName(Ref.Target.Mid(7))) : Owners.FindRef(Ref.Target);
            // New owners inherit the construction parent's designer. A live source
            // outside it (or in another designer) must be refused before cable cost,
            // not only by the post-build designer-aware materializer.
            const AFGBuildable* SourceOwner = Source ? Cast<AFGBuildable>(Owner) : nullptr;
            if (SourceOwner && SourceOwner->GetBlueprintDesigner() != Parent->GetBlueprintDesigner())
            {
                OutReason = TEXT("Power cables cannot cross a Blueprint Designer boundary");
                return false;
            }
            Ports[End] = SFExtendPowerConnections::Resolve(Owner, Ref.Connector);
            if (Ports[End])
            {
                Positions[End] = Ports[End]->GetComponentLocation();
            }
            else if (!Source && !bRestoredCloneTopologyActive && IsValid(Owner))
            {
                // Live clones have an authoritative same-class source. Some custom
                // child holograms omit native components; derive their future socket
                // from that exact source instead of a guessed height/component.
                const FSFCloneHologram* Entry = SFExtendPowerConnections::Find(&Checked, Ref.Target);
                AFGBuildable* Original = GetSourceBuildableByName(Entry ? Entry->SourceId : Checked.SourceFactoryId);
                AFGHologram* Preview = Cast<AFGHologram>(Owner);
                if (Original && Preview && Preview->GetBuildClass() == Original->GetClass())
                {
                    Ports[End] = SFExtendPowerConnections::Resolve(Original, Ref.Connector);
                    if (Ports[End]) Positions[End] = Owner->GetActorTransform().TransformPosition(
                        Original->GetActorTransform().InverseTransformPosition(Ports[End]->GetComponentLocation()));
                }
            }
            if (!Ports[End])
            {
                OutReason = FString::Printf(TEXT("Cannot validate power socket %s.%s before construction"), *Ref.Target, *Ref.Connector);
                return false;
            }
            *Capacities[End] = Source ? Ports[End]->GetNumFreeConnections() : Ports[End]->GetMaxNumConnections();
            const FVector Expected = (End == 0 ? Wire.SplineData.Points[0] : Wire.SplineData.Points.Last()).World.ToFVector();
            if (Positions[End].ContainsNaN() || !Positions[End].Equals(Expected, 1.0))
            {
                OutReason = FString::Printf(TEXT("Power socket moved relative to the quoted cable %s"), *Wire.HologramId);
                return false;
            }
        }
        Wire.PowerMaxLength = SFExtendPowerConnections::MaxWireLength(WireClass->GetDefaultObject<AFGBuildableWire>(), Ports[0], Ports[1]);
        if (!FMath::IsFinite(Wire.PowerMaxLength) || Wire.PowerMaxLength <= 0)
        {
            OutReason = TEXT("Power cable has no valid native range");
            return false;
        }
    }
    return SFExtendPowerConnections::ValidateCapacity(Checked, OutReason);
}
