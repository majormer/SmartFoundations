// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendFactoryPower.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFRestoreGrid.h"
#include "Subsystem/SFSubsystem.h"
#include "Constants/SFAssetPaths.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildableFactory.h"
#include "Hologram/FGHologram.h"
#include "FGCircuitConnectionComponent.h"
#include "FGUnlockSubsystem.h"

bool SFExtendFactoryPower::IsRequested(const FSFExtendFactoryPower& Power,
    bool Enabled, bool Poleless, bool Unlocked)
{
    return Enabled && Unlocked && !Power.Connector.IsEmpty() && Power.Capacity >= 2
        && FMath::IsFinite(Power.MaxWireLength) && Power.MaxWireLength > 0
        && !Power.LocalPosition.ToFVector().ContainsNaN()
        && (Power.bContinuesChain || (!Power.bHasWire && Poleless));
}

FSFExtendFactoryPower SFExtendFactoryPower::Capture(AActor* Factory)
{
    FSFExtendFactoryPower Result;
    if (!IsValid(Factory)) return Result;
    const AFGHologram* Preview = Cast<AFGHologram>(Factory);
    UClass* FactoryClass = Preview ? Preview->GetBuildClass().Get() : Factory->GetClass();
    if (!FactoryClass || !FactoryClass->IsChildOf(AFGBuildableFactory::StaticClass())) return Result;
    UFGCircuitConnectionComponent* Port = SFExtendPowerConnections::Resolve(Factory, FString());
    if (!Port) return Result; // Multiple native components need explicit source identity, not component zero.
    Result.Connector = Port->GetName();
    Result.LocalPosition = FSFVec3(Factory->GetActorTransform().InverseTransformPosition(Port->GetComponentLocation()));
    Result.Capacity = Port->GetMaxNumConnections();
    Result.SourceFreeConnections = Port->GetNumFreeConnections();
    Result.bHasWire = Port->GetNumConnections() > 0;
    TArray<UFGCircuitConnectionComponent*> Peers;
    Port->GetConnections(Peers);
    for (UFGCircuitConnectionComponent* Peer : Peers)
        if (IsValid(Peer) && IsValid(Peer->GetOwner()) && Peer->GetOwner()->GetClass() == Factory->GetClass())
            Result.bContinuesChain = true;
    if (UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass))
        Result.MaxWireLength = SFExtendPowerConnections::MaxWireLength(WireClass->GetDefaultObject<AFGBuildableWire>(), Port, Port);
    if (USFSubsystem* SS = USFSubsystem::Get(Factory->GetWorld()))
    {
        const auto& Settings = SS->GetAutoConnectRuntimeSettings();
        const AFGUnlockSubsystem* Unlocks = AFGUnlockSubsystem::Get(Factory->GetWorld());
        Result.bRequested = IsRequested(Result, Settings.bExtendDaisyChain, Settings.bExtendDaisyChainPoleless,
            Unlocks && Unlocks->IsCircuitDaisyChainingUnlocked());
    }
    return Result;
}

namespace
{
    FSFCloneHologram MakeWire(const FString& Id, const FSFExtendFactoryPower& Power,
        const FString& FromId, const FVector& From, int32 FromCapacity,
        const FString& ToId, const FVector& To, bool SourceWire)
    {
        FSFCloneHologram Wire;
        Wire.HologramId = Id;
        Wire.Role = TEXT("wire_cost");
        Wire.SourceClass = Wire.BuildClass = TEXT("Build_PowerLine_C");
        Wire.HologramClass = TEXT("ASFWireHologram");
        Wire.RecipeClass = TEXT("Recipe_PowerLine_C");
        Wire.bConstructible = false;
        Wire.bPreviewOnly = true;
        Wire.bHasSplineData = true;
        Wire.bIsSourceToCloneWire = SourceWire;
        Wire.PowerFrom = FSFConnectionRef(FromId, Power.Connector);
        Wire.PowerTo = FSFConnectionRef(ToId, Power.Connector);
        Wire.PowerFromCapacity = FromCapacity;
        Wire.PowerToCapacity = Power.Capacity;
        Wire.PowerMaxLength = Power.MaxWireLength;
        Wire.Transform = FSFTransform(From, (To - From).Rotation());
        Wire.SplineData.Length = FVector::Dist(From, To);
        FSFSplinePoint Start, End;
        Start.World = FSFVec3(From); End.World = FSFVec3(To);
        End.Local = FSFVec3(FVector(Wire.SplineData.Length, 0, 0));
        Wire.SplineData.Points = {Start, End};
        return Wire;
    }

    FTransform Pose(const FSFTransform& Transform)
    {
        return FTransform(Transform.Rotation.ToFRotator(), Transform.Location.ToFVector());
    }
}

void SFExtendFactoryPower::AddSourceWire(FSFCloneTopology& Clone, const FSFSourceFactory& Source)
{
    Clone.FactoryPower = Source.Power;
    Clone.ChildHolograms.RemoveAll([](const FSFCloneHologram& H) { return H.HologramId == TEXT("wire_source_factory_daisy"); });
    if (!Source.Power.bRequested || Source.Power.SourceFreeConnections <= 0) return;
    const FVector Socket = Source.Power.LocalPosition.ToFVector();
    Clone.ChildHolograms.Add(MakeWire(TEXT("wire_source_factory_daisy"), Source.Power,
        TEXT("source:") + Source.Id, Pose(Source.Transform).TransformPosition(Socket), Source.Power.SourceFreeConnections,
        TEXT("parent"), Pose(Clone.ParentTransform).TransformPosition(Socket), true));
}

void SFExtendFactoryPower::AddRestoreWires(FSFCloneTopology& Clone, const TMap<FIntVector, FTransform>& Cells)
{
    Clone.ChildHolograms.RemoveAll([](const FSFCloneHologram& H) { return H.HologramId.Contains(TEXT("wire_factory_daisy")); });
    if (!Clone.FactoryPower.bRequested) return;
    auto Id = [](const FIntVector& Cell)
    {
        return Cell == FIntVector::ZeroValue ? FString(TEXT("parent"))
            : SFRestoreGrid::Prefix(Cell.X, Cell.Y, Cell.Z) + TEXT("factory");
    };
    TArray<FIntVector> Ordered;
    Cells.GetKeys(Ordered);
    Ordered.Sort([](const FIntVector& A, const FIntVector& B)
    { return A.Z != B.Z ? A.Z < B.Z : A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
    for (const FIntVector& Cell : Ordered)
    {
        if (Cell.X <= 0) continue;
        const FIntVector Previous(Cell.X - 1, Cell.Y, Cell.Z);
        const FTransform* From = Cells.Find(Previous);
        if (!From) continue; // Do not bridge a missing cell, row or layer.
        const FVector Socket = Clone.FactoryPower.LocalPosition.ToFVector();
        Clone.ChildHolograms.Add(MakeWire(SFRestoreGrid::Prefix(Cell.X, Cell.Y, Cell.Z) + TEXT("wire_factory_daisy"),
            Clone.FactoryPower, Id(Previous), From->TransformPosition(Socket), Clone.FactoryPower.Capacity,
            Id(Cell), Cells[Cell].TransformPosition(Socket), false));
    }
}
