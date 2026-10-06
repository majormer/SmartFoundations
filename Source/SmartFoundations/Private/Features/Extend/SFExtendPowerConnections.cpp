// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendPowerConnections.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGDismantleInterface.h"
#include "Buildables/FGBuildableWire.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFRestoreGrid.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Shared/Power/SFWireDesignerRegistration.h"

UFGCircuitConnectionComponent* SFExtendPowerConnections::Resolve(AActor* Actor, const FString& Name)
{
    if (!IsValid(Actor)) return nullptr;
    TArray<UFGCircuitConnectionComponent*> Ports;
    Actor->GetComponents(Ports);
    UFGCircuitConnectionComponent* Match = nullptr;
    for (UFGCircuitConnectionComponent* Port : Ports)
    {
        if (!IsValid(Port) || (!Name.IsEmpty() && Port->GetName() != Name)) continue;
        if (Match) return nullptr;
        Match = Port;
    }
    return Match;
}

float SFExtendPowerConnections::MaxWireLength(const AFGBuildableWire* Wire,
    UFGCircuitConnectionComponent* From, UFGCircuitConnectionComponent* To)
{
    if (!IsValid(Wire) || !IsValid(From) || !IsValid(To)) return 0;
    const UFGPowerConnectionComponent* PowerFrom = Cast<UFGPowerConnectionComponent>(From);
    const UFGPowerConnectionComponent* PowerTo = Cast<UFGPowerConnectionComponent>(To);
    const bool Towers = PowerFrom && PowerTo
        && PowerFrom->GetPowerConnectionType() == EPowerConnectionType::PCT_PowerTower
        && PowerTo->GetPowerConnectionType() == EPowerConnectionType::PCT_PowerTower;
    return Towers ? Wire->mMaxPowerTowerLength : Wire->mMaxLength;
}

bool SFExtendPowerConnections::Connect(UWorld* World, UClass* WireClass,
    UFGCircuitConnectionComponent* From, UFGCircuitConnectionComponent* To)
{
    if (!World || !WireClass || !IsValid(From) || !IsValid(To) || From == To
        || !From->GetOwner()->HasAuthority() || !To->GetOwner()->HasAuthority()) return false;
    if (!WireClass->IsChildOf(AFGBuildableWire::StaticClass())) return false;
    const double Limit = MaxWireLength(WireClass->GetDefaultObject<AFGBuildableWire>(), From, To);
    const double Distance = FVector::Dist(From->GetComponentLocation(), To->GetComponentLocation());
    if (!FMath::IsFinite(Limit) || Limit <= 0 || !FMath::IsFinite(Distance) || Distance > Limit) return false;
    TArray<UFGCircuitConnectionComponent*> Peers;
    From->GetConnections(Peers);
    if (Peers.Contains(To) || From->GetNumFreeConnections() <= 0 || To->GetNumFreeConnections() <= 0) return false;
    AFGBuildableWire* Wire = SFWireDesigner::SpawnWireForEndpoints(
        World, WireClass, From->GetComponentLocation(), From, To);
    if (!Wire) return false;
    if (Wire->Connect(From, To)) return true;
    // Connect can register one end before failing. Destroy alone leaves a saved dangling wire.
    IFGDismantleInterface::Execute_Dismantle(Wire);
    return false;
}

const FSFCloneHologram* SFExtendPowerConnections::Find(const FSFCloneTopology* Topology, const FString& Id)
{
    return Topology ? Topology->ChildHolograms.FindByPredicate(
        [&](const FSFCloneHologram& Holo) { return Holo.HologramId == Id; }) : nullptr;
}

void SFExtendPowerConnections::RefreshPreview(ASFWireHologram* Wire, const FSFCloneHologram& Plan)
{
    if (IsValid(Wire) && Plan.bHasSplineData && Plan.SplineData.Points.Num() >= 2)
    {
        Wire->SetupWirePreviewFromPositions(Plan.SplineData.Points[0].World.ToFVector(),
            Plan.SplineData.Points.Last().World.ToFVector());
    }
}

bool SFExtendPowerConnections::ValidateCapacity(const FSFCloneTopology& Topology, FString& OutReason)
{
    if (!Topology.PowerPlanError.IsEmpty()) { OutReason = Topology.PowerPlanError; return false; }
    TMap<FString, int32> Limits;
    TMap<FString, int32> Reservations;
    auto Reserve = [&](const FSFConnectionRef& Ref, int32 Capacity)
    {
        if (Ref.Target.IsEmpty()) return; // Pre-identity saved topology: authority resolves unique ports only.
        const FString Key = Ref.Target + TEXT(".") + Ref.Connector;
        ++Reservations.FindOrAdd(Key);
        if (Capacity >= 0)
        {
            int32* Limit = Limits.Find(Key);
            if (Limit) *Limit = FMath::Min(*Limit, Capacity);
            else Limits.Add(Key, Capacity);
        }
    };
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (Holo.Role != TEXT("wire_cost")) continue;
        if (Holo.PowerMaxLength > 0 && Holo.SplineData.Points.Num() >= 2)
        {
            const double Length = FVector::Dist(Holo.SplineData.Points[0].World.ToFVector(), Holo.SplineData.Points.Last().World.ToFVector());
            if (!FMath::IsFinite(Length) || Length > Holo.PowerMaxLength)
            {
                OutReason = FString::Printf(TEXT("Power cable %s exceeds its native length limit"), *Holo.HologramId);
                return false;
            }
        }
        Reserve(Holo.PowerFrom, Holo.PowerFromCapacity);
        Reserve(Holo.PowerTo, Holo.PowerToCapacity);
    }
    for (const auto& Reservation : Reservations)
    {
        const int32* Limit = Limits.Find(Reservation.Key);
        if (Limit && Reservation.Value > *Limit)
        {
            OutReason = FString::Printf(TEXT("Power connector %s needs %d connections but has %d slots"),
                *Reservation.Key, Reservation.Value, *Limit);
            return false;
        }
    }
    OutReason.Reset();
    return true;
}

namespace
{
    bool IsFactoryDaisyWire(const FSFCloneHologram& Holo)
    {
        return Holo.Role == TEXT("wire_cost") && Holo.HologramId.Contains(TEXT("factory_daisy"));
    }

    // Same key and limit semantics as ValidateCapacity: empty targets are unchecked legacy
    // captures, and a socket's limit is the smallest capacity any cable quotes for it.
    bool HasFreeSlot(const FSFConnectionRef& Ref, int32 Capacity, const SFExtendPowerConnections::FSocketBudget& Budget)
    {
        if (Ref.Target.IsEmpty()) return true;
        const FString Key = Ref.Target + TEXT(".") + Ref.Connector;
        int32 Limit = Capacity;
        if (const int32* Known = Budget.Limits.Find(Key)) Limit = Capacity >= 0 ? FMath::Min(*Known, Capacity) : *Known;
        return Limit < 0 || Budget.Used.FindRef(Key) + 1 <= Limit;
    }

    void Reserve(const FSFConnectionRef& Ref, int32 Capacity, SFExtendPowerConnections::FSocketBudget& Budget)
    {
        if (Ref.Target.IsEmpty()) return;
        const FString Key = Ref.Target + TEXT(".") + Ref.Connector;
        ++Budget.Used.FindOrAdd(Key);
        if (Capacity >= 0)
        {
            if (int32* Limit = Budget.Limits.Find(Key)) *Limit = FMath::Min(*Limit, Capacity);
            else Budget.Limits.Add(Key, Capacity);
        }
    }
}

void SFExtendPowerConnections::ReserveAll(const FSFCloneTopology& Topology, FSocketBudget& Budget)
{
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (Holo.Role != TEXT("wire_cost")) continue;
        Reserve(Holo.PowerFrom, Holo.PowerFromCapacity, Budget);
        Reserve(Holo.PowerTo, Holo.PowerToCapacity, Budget);
    }
}

int32 SFExtendPowerConnections::PruneDaisyOverCapacity(FSFCloneTopology& Topology, FSocketBudget& Budget)
{
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (Holo.Role != TEXT("wire_cost") || IsFactoryDaisyWire(Holo)) continue;
        Reserve(Holo.PowerFrom, Holo.PowerFromCapacity, Budget);
        Reserve(Holo.PowerTo, Holo.PowerToCapacity, Budget);
    }
    TSet<FString> Dropped;
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (!IsFactoryDaisyWire(Holo)) continue;
        if (HasFreeSlot(Holo.PowerFrom, Holo.PowerFromCapacity, Budget) && HasFreeSlot(Holo.PowerTo, Holo.PowerToCapacity, Budget))
        {
            Reserve(Holo.PowerFrom, Holo.PowerFromCapacity, Budget);
            Reserve(Holo.PowerTo, Holo.PowerToCapacity, Budget);
        }
        else
        {
            Dropped.Add(Holo.HologramId);
        }
    }
    return Topology.ChildHolograms.RemoveAll([&Dropped](const FSFCloneHologram& Holo)
    {
        return Dropped.Contains(Holo.HologramId);
    });
}

void SFExtendPowerConnections::RemapCellTargets(FSFCloneHologram& Wire,
    const FString& CellPrefix, const FString& PreviousPrefix)
{
    const FString OriginalTo = Wire.PowerTo.Target;
    auto Remap = [&](FSFConnectionRef& Ref)
    {
        if (Ref.Target.IsEmpty()) return;
        if (Ref.Target == TEXT("parent")) Ref.Target = CellPrefix + TEXT("factory");
        else if (Ref.Target.StartsWith(TEXT("source:")))
            Ref.Target = OriginalTo == TEXT("parent")
                ? (PreviousPrefix.IsEmpty() ? FString(TEXT("parent")) : PreviousPrefix + TEXT("factory"))
                : PreviousPrefix + OriginalTo;
        else Ref.Target = CellPrefix + Ref.Target;
    };
    Remap(Wire.PowerFrom);
    Remap(Wire.PowerTo);
}

void SFExtendPowerConnections::AddRestoredChainWires(FSFCloneTopology& Topology)
{
    Topology.ChildHolograms.RemoveAll([](const FSFCloneHologram& Holo)
    {
        return Holo.Role == TEXT("wire_cost") && Holo.HologramId.Contains(TEXT("wire_chain_"));
    });
    // The factory cable supplies the exact socket and native per-face capacity.
    // Build only adjacent X edges within a row/layer, and price the same edges we build.
    TMap<FString, const FSFCloneHologram*> FactoryCables;
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (Holo.Role == TEXT("wire_cost") && Holo.HologramId.Contains(TEXT("wire_factory_pole_"))
            && !Holo.PowerTo.Target.IsEmpty() && Holo.SplineData.Points.Num() >= 2)
            FactoryCables.Add(Holo.PowerTo.Target + TEXT(".") + Holo.PowerTo.Connector, &Holo);
    }
    TArray<FSFCloneHologram> Chains;
    for (const auto& Cable : FactoryCables)
    {
        const int32 Marker = Cable.Key.Find(TEXT("power_pole_"));
        if (Marker == INDEX_NONE) continue;
        const FString Prefix = Cable.Key.Left(Marker);
        FIntVector Cell = FIntVector::ZeroValue;
        if (Prefix.IsEmpty() || !SFRestoreGrid::ParsePrefix(Prefix, Cell) || Cell.X <= 0) continue;
        const FString PreviousPrefix = Cell.X == 1 && Cell.Y == 0 && Cell.Z == 0
            ? FString() : SFRestoreGrid::Prefix(Cell.X - 1, Cell.Y, Cell.Z);
        const FSFCloneHologram* const* Previous = FactoryCables.Find(PreviousPrefix + Cable.Key.Mid(Marker));
        if (!Previous) continue;
        const FSFCloneHologram& From = **Previous;
        const FSFCloneHologram& To = *Cable.Value;
        FSFCloneHologram Wire = To;
        Wire.HologramId = Prefix + TEXT("wire_chain_") + To.HologramId.Mid(Prefix.Len());
        Wire.PowerFrom = From.PowerTo;
        Wire.PowerTo = To.PowerTo;
        Wire.PowerFromCapacity = From.PowerToCapacity;
        Wire.PowerToCapacity = To.PowerToCapacity;
        const FVector Start = From.SplineData.Points.Last().World.ToFVector();
        const FVector End = To.SplineData.Points.Last().World.ToFVector();
        Wire.SplineData.Points = {From.SplineData.Points.Last(), To.SplineData.Points.Last()};
        Wire.SplineData.Length = FVector::Dist(Start, End);
        Wire.Transform = FSFTransform(Start, (End - Start).Rotation());
        Chains.Add(MoveTemp(Wire));
    }
    Topology.ChildHolograms.Append(Chains);
}

bool SFExtendPowerConnections::PromoteLegacyRestoreWires(FSFCloneTopology& Topology)
{
    Topology.PowerPlanError.Reset();
    auto Fail = [&](const FString& Id)
    {
        Topology.PowerPlanError = FString::Printf(
            TEXT("Saved power cable %s lacks unambiguous endpoint identity. Re-import this layout from its original build."), *Id);
        return false;
    };
    // Restore has no live source endpoint. Its adjacent-cell cables are regenerated
    // (and priced) after grid expansion, including for older presets.
    Topology.ChildHolograms.RemoveAll([](const FSFCloneHologram& Holo)
    {
        return Holo.Role == TEXT("wire_cost") && (Holo.bIsSourceToCloneWire
            || Holo.HologramId.Contains(TEXT("wire_source_clone_"))
            || Holo.HologramId.Contains(TEXT("wire_chain_"))
            || Holo.HologramId.Contains(TEXT("wire_factory_daisy")));
    });
    TMap<FString, const FSFCloneHologram*> Owners;
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        if (Owners.Contains(Holo.HologramId)) return Fail(Holo.HologramId);
        Owners.Add(Holo.HologramId, &Holo);
    }
    for (FSFCloneHologram& Wire : Topology.ChildHolograms)
    {
        if (Wire.Role != TEXT("wire_cost")) continue;
        const bool FromMissing = Wire.PowerFrom.Target.IsEmpty();
        const bool ToMissing = Wire.PowerTo.Target.IsEmpty();
        if (FromMissing != ToMissing) return Fail(Wire.HologramId); // partial metadata is not legacy
        if (FromMissing)
        {
            const int32 FactoryMarker = Wire.HologramId.Find(TEXT("wire_factory_pole_"));
            const int32 PumpMarker = Wire.HologramId.Find(TEXT("wire_pump_"));
            if (FactoryMarker != INDEX_NONE)
            {
                const FString Prefix = Wire.HologramId.Left(FactoryMarker);
                const FString PoleId = Prefix + TEXT("power_pole_") + Wire.HologramId.Mid(FactoryMarker + 18);
                const FSFCloneHologram* Pole = Owners.FindRef(PoleId);
                if (!Pole || Pole->Role != TEXT("power_pole")) return Fail(Wire.HologramId);
                Wire.PowerFrom = FSFConnectionRef(Prefix.IsEmpty() ? TEXT("parent") : Prefix + TEXT("factory"), Pole->FactoryPowerConnectorName);
                Wire.PowerTo = FSFConnectionRef(PoleId, Pole->PowerConnectorName);
                Wire.PowerToCapacity = Pole->PowerPoleMaxConnections > 0 ? Pole->PowerPoleMaxConnections : -1;
            }
            else if (PumpMarker != INDEX_NONE)
            {
                const FString PumpId = Wire.HologramId.Left(PumpMarker) + Wire.HologramId.Mid(PumpMarker + 10);
                const FSFCloneHologram* Pump = Owners.FindRef(PumpId);
                if (!Pump || Pump->Role != TEXT("pipe_attachment") || Pump->ConnectedPowerPoleHologramId.IsEmpty()) return Fail(Wire.HologramId);
                Wire.PowerFrom = FSFConnectionRef(PumpId, Pump->PowerConnectorName);
                Wire.PowerTo = FSFConnectionRef(Pump->ConnectedPowerPoleHologramId, Pump->ConnectedPowerPoleConnectorName);
            }
            else return Fail(Wire.HologramId);
        }
        for (const FSFConnectionRef* Ref : {&Wire.PowerFrom, &Wire.PowerTo})
        {
            if (Ref->Target == TEXT("parent")) continue;
            const int32 FactoryMarker = Wire.HologramId.Find(TEXT("wire_factory_pole_"));
            if (FactoryMarker > 0 && Ref->Target == Wire.HologramId.Left(FactoryMarker) + TEXT("factory")) continue;
            const FSFCloneHologram* Owner = Owners.FindRef(Ref->Target);
            if (!Owner) return Fail(Wire.HologramId);
            // The two outlet faces share a circuit and rotation, but not wire capacity.
            // Old captures without a face must not quietly choose component zero.
            if (Ref->Connector.IsEmpty() && Owner->BuildClass.Contains(TEXT("PowerPoleWallDouble")))
                return Fail(Wire.HologramId);
        }
    }
    return true;
}
