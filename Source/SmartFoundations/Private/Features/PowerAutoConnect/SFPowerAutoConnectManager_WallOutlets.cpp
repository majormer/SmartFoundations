// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"

#include "Features/PowerAutoConnect/SFPowerConnectionPolicy.h"
#include "Features/PowerAutoConnect/SFPowerBuildingTarget.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Subsystem/SFSubsystem.h"
#include "Config/Smart_ConfigStruct.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Hologram/FGPowerPoleWallHologram.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "Constants/SFAssetPaths.h"
#include "EngineUtils.h"

// SP/MP DIVERGENCE MAP — port-aware power preview
// Local named endpoints and native budgets author previews only. CaptureConduitPlan carries their
// exact identities to the shared authority post-construct seam; no legacy deferred pole queue.
void FSFPowerAutoConnectManager::ProcessWallOutlets(AFGHologram* Parent)
{
    if (!Subsystem || !IsValid(Parent)) return;
    const AFGBuildableHologram* BuildableParent = Cast<AFGBuildableHologram>(Parent);
    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
    bool bNativeWireInsertion = false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
    {
        if (IsValid(Child) && !Child->Tags.Contains(TEXT("SF_PowerAutoConnectChild"))
            && (Child->GetClass()->GetName().Contains(TEXT("Wire")) || Child->GetClass()->GetName().Contains(TEXT("PowerLine"))))
            bNativeWireInsertion = true;
    }
    if (!Settings.bConnectPower || Parent->GetUpgradedActor() || bNativeWireInsertion
        || (BuildableParent && BuildableParent->GetZoopInstanceTransforms().Num() > 0))
    {
        ClearPowerLinePreviews();
        return;
    }

    TMap<FIntVector, AFGHologram*> Grid;
    Grid.Add(FIntVector::ZeroValue, Parent);
    for (AFGHologram* Child : Parent->GetHologramChildren())
    {
        FIntVector Cell;
        if (IsValid(Child) && Child->Tags.Contains(TEXT("SF_GridChild"))
            && Child->GetBuildClass() == Parent->GetBuildClass() && USFGridCoordComponent::TryGetCell(Child, Cell))
            Grid.Add(Cell, Child);
    }
    TMap<UFGPowerConnectionComponent*, int32> Planned;
    TArray<UFGPowerConnectionComponent*> SourcePorts;
    TMap<AFGHologram*, UFGPowerConnectionComponent*> BackbonePorts;
    FName BackboneName;
    if (const AFGPowerPoleWallHologram* Wall = Cast<AFGPowerPoleWallHologram>(Parent))
        if (IsValid(Wall->GetSnapConnection())) BackboneName = Wall->GetSnapConnection()->GetFName();
    if (BackboneName.IsNone())
        BackboneName = Parent->GetBuildClass()->GetName().Contains(TEXT("Double")) ? TEXT("PowerConnection1") : TEXT("PowerConnection");

    for (const auto& Cell : Grid)
    {
        TInlineComponentArray<UFGPowerConnectionComponent*> Ports(Cell.Value);
        for (UFGPowerConnectionComponent* Port : Ports)
        {
            if (!IsValid(Port) || Port->IsHidden()) continue;
            SourcePorts.Add(Port);
            if (Port->GetFName() == BackboneName) BackbonePorts.Add(Cell.Value, Port);
        }
    }
    TArray<TPair<UFGPowerConnectionComponent*, UFGPowerConnectionComponent*>> Desired;
    const UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
    const double MaxLength = WireClass ? FMath::Min(10000.0f, WireClass->GetDefaultObject<AFGBuildableWire>()->mMaxLength) : 0.0;
    const auto Designer = [](const UFGPowerConnectionComponent* Port)
    {
        if (const AFGHologram* Hologram = Cast<AFGHologram>(Port->GetOwner())) return Hologram->GetBlueprintDesigner();
        const AFGBuildable* Building = Cast<AFGBuildable>(Port->GetOwner());
        return Building ? Building->GetBlueprintDesigner() : nullptr;
    };
    const auto FreeSlots = [&](UFGPowerConnectionComponent* Port, int32 Reserved)
    {
        return SFPowerConnectionPolicy::AvailableSlots(Port->GetMaxNumConnections(), Port->GetNumConnections(),
            Planned.FindRef(Port), Reserved);
    };
    const auto AddSpan = [&](UFGPowerConnectionComponent* Start, UFGPowerConnectionComponent* End, int32 Reserved)
    {
        if (!IsValid(Start) || !IsValid(End) || Start == End || Start->GetOwner() == End->GetOwner()
            || FreeSlots(Start, Reserved) <= 0 || FreeSlots(End, Reserved) <= 0
            || Designer(Start) != Designer(End)) return false;
        const double Distance = FVector::Dist(Start->GetComponentLocation(), End->GetComponentLocation());
        if (Distance <= 1.0 || Distance > MaxLength) return false;
        Desired.Emplace(Start, End);
        ++Planned.FindOrAdd(Start);
        ++Planned.FindOrAdd(End);
        return true;
    };

    TArray<FIntVector> Cells;
    Grid.GetKeys(Cells);
    for (const auto& Edge : SFPowerConnectionPolicy::GridEdges(Cells, Settings.PowerGridAxis))
        AddSpan(BackbonePorts.FindRef(Grid[Edge.Key]), BackbonePorts.FindRef(Grid[Edge.Value]), 0);

    // Distance is measured at named connectors. Each double face reserves its own free slots.
    const float Range = FMath::Clamp(float(FSmart_ConfigStruct::GetActiveConfig(Subsystem).PowerConnectRange) * 100.0f, 0.0f, 10000.0f);
    struct FCandidate { UFGPowerConnectionComponent* Source; UFGPowerConnectionComponent* Target; double Distance; };
    TArray<FCandidate> Candidates;
    if (Range > 0.0f)
    {
        for (TActorIterator<AFGBuildable> It(Parent->GetWorld()); It; ++It)
        {
            AFGBuildable* Building = *It;
            if (!IsValid(Building) || Building->IsA<AFGBuildablePowerPole>()) continue;
            TInlineComponentArray<UFGPowerConnectionComponent*> Ports(Building);
            for (UFGPowerConnectionComponent* Target : Ports)
            {
                if (!IsValid(Target) || !SFPowerBuildingTarget::IsEligible(Target->IsHidden(), Target->IsConnected() != 0,
                    Target->GetNumFreeConnections())) continue;
                for (UFGPowerConnectionComponent* Source : SourcePorts)
                {
                    const double Distance = FVector::Dist(Source->GetComponentLocation(), Target->GetComponentLocation());
                    if (Distance <= Range) Candidates.Add({Source, Target, Distance});
                }
            }
        }
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        if (!FMath::IsNearlyEqual(A.Distance, B.Distance, 0.001)) return A.Distance < B.Distance;
        const FString AKey = A.Source->GetPathName() + A.Target->GetPathName();
        const FString BKey = B.Source->GetPathName() + B.Target->GetPathName();
        return AKey < BKey;
    });
    TSet<AActor*> AssignedBuildings;
    for (const FCandidate& Candidate : Candidates)
    {
        if (AssignedBuildings.Contains(Candidate.Target->GetOwner()) || FreeSlots(Candidate.Source, Settings.PowerReserved) <= 0) continue;
        if (AddSpan(Candidate.Source, Candidate.Target, 0)) AssignedBuildings.Add(Candidate.Target->GetOwner());
    }

    // Reuse unchanged endpoint pairs and geometry. A stationary grid never re-locks or repaints wires.
    TArray<TSharedPtr<FPowerLinePreviewHelper>> Existing;
    for (const auto& Group : PowerLinePreviews) Existing.Append(Group.Value);
    TMap<UFGPowerConnectionComponent*, TArray<TSharedPtr<FPowerLinePreviewHelper>>> BySource;
    for (const auto& Preview : Existing)
        if (Preview.IsValid()) BySource.FindOrAdd(Preview->GetStartConnection()).Add(Preview);
    TArray<TSharedPtr<FPowerLinePreviewHelper>> Retained;
    TSet<FPowerLinePreviewHelper*> RetainedSet;
    for (const auto& Span : Desired)
    {
        TSharedPtr<FPowerLinePreviewHelper> Preview;
        if (const auto* Matches = BySource.Find(Span.Key))
            for (const auto& Match : *Matches)
                if (Match->IsPreviewValid() && Match->GetEndConnection() == Span.Value) { Preview = Match; break; }
        if (!Preview.IsValid()) Preview = MakeShared<FPowerLinePreviewHelper>(Parent->GetWorld(), Parent);
        const TPair<FVector, FVector> Positions(Span.Key->GetComponentLocation(), Span.Value->GetComponentLocation());
        const auto* Previous = WallPreviewPositions.Find(Preview.Get());
        if (!Previous || !Previous->Key.Equals(Positions.Key, 0.01) || !Previous->Value.Equals(Positions.Value, 0.01))
        {
            if (!Preview->UpdatePreview(Span.Key, Span.Value)) continue;
            Preview->GetHologram()->Tags.AddUnique(TEXT("SF_ExactPowerPlan"));
            WallPreviewPositions.Add(Preview.Get(), Positions);
        }
        if (Preview->GetHologram()->GetBlueprintDesigner() != Parent->GetBlueprintDesigner())
            Preview->GetHologram()->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
        Retained.Add(Preview);
        RetainedSet.Add(Preview.Get());
    }
    for (const auto& Preview : Existing)
        if (Preview.IsValid() && !RetainedSet.Contains(Preview.Get()))
        {
            WallPreviewPositions.Remove(Preview.Get());
            Preview->DestroyPreview();
        }
    PowerLinePreviews.Empty();
    PowerLinePreviews.Add(Parent, MoveTemp(Retained));
}
