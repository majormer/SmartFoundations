// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFHypertubeEntranceState.h"
#include "SmartFoundations.h"
#include "Core/SFNativeHit.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFHologramDataService.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Hologram/FGPipePartHologram.h"
#include "Buildables/FGBuildablePipeHyper.h"
#include "FGCharacterPlayer.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — scaled entrance endpoint binding
//  1. Preview remains lightweight; unattached entrance grids retain their ordinary workflow.
//  2. [MP-AUTH] Resolve prospective endpoint snaps and native legality before local construction/payment.
//  3. [MP-SEAM] Resolve staged poses against authority-owned tubes, never a client-supplied peer pointer.
// Native recipe holograms exist only during one request. Native ConfigureComponents owns the links.
namespace
{
    bool bEntranceRequestActive = false;
    TMap<TWeakObjectPtr<AFGHologram>, bool> EntrancePreparedRoots;
    TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> EntrancePreparedCells;
    TArray<TWeakObjectPtr<AFGHologram>> EntranceRequestCells;

    UFGPipeConnectionComponentBase* GetEntranceBinding(AFGHologram* Hologram)
    {
        const FObjectPropertyBase* Binding = FindFProperty<FObjectPropertyBase>(
            AFGPipePartHologram::StaticClass(), TEXT("mSnappedConnectionComponent"));
        return Binding ? Cast<UFGPipeConnectionComponentBase>(Binding->GetObjectPropertyValue_InContainer(Hologram)) : nullptr;
    }

    AFGHologram* SpawnEntranceCell(AFGHologram* Root, const FTransform& Pose)
    {
        const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
        if (!Parent || !Root->GetRecipe()) return nullptr;
        AFGHologram* Cell = AFGHologram::SpawnHologramFromRecipe(Root->GetRecipe(), Root->GetOwner(),
            Pose.GetLocation(), Root->GetConstructionInstigator(), [Root, Parent](AFGHologram* Spawned)
            { Parent->SetObjectPropertyValue_InContainer(Spawned, Root); });
        if (!IsValid(Cell) || Cell->GetClass() != Root->GetClass())
        { if (IsValid(Cell)) Cell->Destroy(); return nullptr; }
        EntranceRequestCells.Add(Cell);
        Cell->Tags.AddUnique(TEXT("SF_EntranceConstructionCell"));
        Cell->SetConstructionInstigator(Root->GetConstructionInstigator());
        Cell->SetActorHiddenInGame(true);
        Cell->SetActorTickEnabled(false);
        Cell->SetActorEnableCollision(false);
        TArray<UPrimitiveComponent*> Primitives;
        Cell->GetComponents(Primitives);
        for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        USFHologramDataService::MarkAsChild(Cell, Root, ESFChildHologramType::ScalingGrid);
        USFHologramDataService::EnableValidation(Cell);
        return Cell;
    }

    enum class EEntranceResolution { Unattached, Accepted, Rejected };
    EEntranceResolution ResolveEntranceCell(AFGHologram* Cell, AFGHologram* Root, const FTransform& Pose)
    {
        // Scaling a snapped entrance must resolve a different native endpoint for each cell.
        // A grid cell without an endpoint stays an ordinary freestanding entrance; snapping
        // the parent does not turn free placement into an endpoint-only player mode.
        auto* Part = Cast<AFGPipePartHologram>(Root);
        if (!Part || !Part->GetSnapConnection()) return EEntranceResolution::Rejected;
        const FVector Aim = Pose.TransformPosition(Root->GetActorTransform().InverseTransformPosition(
            Part->GetSnapConnection()->GetComponentLocation()));
        const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
        if (!Parent) return EEntranceResolution::Rejected;
        ON_SCOPE_EXIT { if (IsValid(Cell)) Cell->SetActorHiddenInGame(true); };
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartEntranceEndpointCell), false, Cell);
        Query.AddIgnoredActor(Root);
        if (Root->GetConstructionInstigator()) Query.AddIgnoredActor(Root->GetConstructionInstigator());
        const FVector Directions[] = {FVector::UpVector, Pose.GetUnitAxis(EAxis::X)};
        for (const FVector& Direction : Directions)
        {
            TArray<FHitResult> Hits;
            Root->GetWorld()->LineTraceMultiByChannel(Hits, Aim + Direction * 100,
                Aim - Direction * 100, ECC_WorldStatic, Query);
            for (FHitResult Hit : Hits)
            {
                SFNativeHit::ResolveForPlacement(Hit);
                if (!IsValid(Cast<AFGBuildablePipeHyper>(Hit.GetActor())) || !Cell->IsValidHitResult(Hit)) continue;
                Cell->SetActorTransform(Pose);
                Cell->SetScrollRotateValue(Root->GetScrollRotateValue());
                Cell->ResetConstructDisqualifiers();
                // Resolve as the ordinary independent native recipe, then restore request ownership.
                Parent->SetObjectPropertyValue_InContainer(Cell, nullptr);
                Cell->UpdateHologramPlacement(Hit);
                Parent->SetObjectPropertyValue_InContainer(Cell, Root);
                UFGPipeConnectionComponentBase* Binding = GetEntranceBinding(Cell);
                const FTransform Resolved = Cell->GetActorTransform();
                // This bound is a snap correction, not extra placement permission. The native
                // resolver selects the peer, orientation and pose; never reposition its result.
                if (!IsValid(Binding) || FVector::Dist(Binding->GetComponentLocation(), Aim) > 1.0
                    || FVector::Dist(Resolved.GetLocation(), Pose.GetLocation()) > 1.0
                    || !Resolved.GetRotation().Equals(Pose.GetRotation(), 0.001)
                    || !Resolved.GetScale3D().Equals(Pose.GetScale3D(), 0.001)) continue;
                if (Cell->GetUpgradedActor()) return EEntranceResolution::Rejected;
                if (Root->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
                AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator());
                Cell->SetActorHiddenInGame(false);
                Cell->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                return Cell->CanConstruct() ? EEntranceResolution::Accepted : EEntranceResolution::Rejected;
            }
        }
        return EEntranceResolution::Unattached;
    }
}

bool SFHypertubeEntranceState::IsHandled(const AFGHologram* Root)
{
    return IsValid(Root) && Root->IsA<AFGPipePartHologram>() && Root->GetBuildClass()
        && Root->GetBuildClass()->GetFName() == TEXT("Build_PipeHyperStart_C")
        && !Root->ActorHasTag(TEXT("SF_EntranceConstructionCell"));
}
bool SFHypertubeEntranceState::HasProxyGrid(AFGHologram* Root)
{
    if (!IsHandled(Root) || Root->GetParentHologram()) return false;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFBuildableChildHologram>()) return true;
    return false;
}
bool SFHypertubeEntranceState::Prepare(AFGHologram* Root, const FSFScalingSpec* Spec)
{
    if (!bEntranceRequestActive || !IsHandled(Root) || Root->GetParentHologram() || !Root->HasAuthority()) return false;
    if (const bool* Existing = EntrancePreparedRoots.Find(Root)) return *Existing;
    EntrancePreparedRoots.Add(Root, false);
    // The selected state addressed here is an ordinary native endpoint snap. Existing
    // freestanding grids have no peer to inherit and keep their generic construction path.
    if (!GetEntranceBinding(Root)) { EntrancePreparedRoots[Root] = true; return true; }
    TArray<AFGHologram*> Previews;
    TArray<FTransform> Poses;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFBuildableChildHologram>())
        { Previews.Add(Child); Poses.Add(Child->GetActorTransform()); }
    if (Spec && Spec->CellCount() > 1 && Previews.IsEmpty())
    {
        if (!Spec->bValid || Spec->BuildClass != Root->GetBuildClass()) return false;
        const FSFCounterState& C = Spec->Counters;
        FSFPositionCalculator Calculator;
        int32 Index = 0;
        for (int32 Z = 0; Z < FMath::Abs(C.GridCounters.Z); ++Z)
            for (int32 Y = 0; Y < FMath::Abs(C.GridCounters.Y); ++Y)
                for (int32 X = 0; X < FMath::Abs(C.GridCounters.X); ++X)
                {
                    const int32 GX = X * FMath::Sign(C.GridCounters.X), GY = Y * FMath::Sign(C.GridCounters.Y), GZ = Z * FMath::Sign(C.GridCounters.Z);
                    const FVector Location = Calculator.CalculateChildPosition(GX, GY, GZ, Root->GetActorLocation(),
                        Root->GetActorRotation(), Spec->ItemSize, C, Index++, FVector::ZeroVector);
                    if (!X && !Y && !Z) continue;
                    FRotator Rotation = Root->GetActorRotation();
                    Rotation.Yaw += (C.RotationAxis == ESFScaleAxis::Y ? -GY : GX) * C.RotationZ;
                    Poses.Emplace(Rotation, Location);
                }
    }
    TSet<UFGPipeConnectionComponentBase*> UsedEndpoints;
    UsedEndpoints.Add(GetEntranceBinding(Root));
    TArray<AFGHologram*> Accepted;
    for (const FTransform& Pose : Poses)
    {
        AFGHologram* Cell = SpawnEntranceCell(Root, Pose);
        if (!Cell) return false;
        const EEntranceResolution Resolution = ResolveEntranceCell(Cell, Root, Pose);
        if (Resolution == EEntranceResolution::Rejected) return false;
        if (Resolution == EEntranceResolution::Accepted)
        {
            UFGPipeConnectionComponentBase* Endpoint = GetEntranceBinding(Cell);
            if (UsedEndpoints.Contains(Endpoint)) return false;
            UsedEndpoints.Add(Endpoint);
            Accepted.Add(Cell);
        }
        else Accepted.Add(nullptr);
    }
    for (int32 Index = 0; Index < Accepted.Num(); ++Index)
    {
        if (!Previews.IsEmpty())
        {
            if (Accepted[Index]) EntrancePreparedCells.Add(Previews[Index], Accepted[Index]);
        }
        else
        {
            AFGHologram* Cell = Accepted[Index];
            const FName Name(*FString::Printf(TEXT("EntranceGrid_%d"), Index));
            if (Cell) Root->AddChild(Cell, Name);
            else
            {
                Cell = ASFBuildableChildHologram::SpawnPreview(Root, Poses[Index], ESFChildHologramType::ScalingGrid, Name);
                if (!Cell) return false;
                EntranceRequestCells.Add(Cell);
            }
            Cell->Tags.AddUnique(TEXT("SF_GridChild"));
            Cell->SetActorHiddenInGame(false);
        }
    }
    EntrancePreparedRoots[Root] = true;
    return true;
}
AFGHologram* SFHypertubeEntranceState::FindPreparedCell(AFGHologram* Preview)
{
    const auto* Cell = EntrancePreparedCells.Find(Preview);
    return bEntranceRequestActive && Cell ? Cell->Get() : nullptr;
}
bool SFHypertubeEntranceState::IsRequestActive() { return bEntranceRequestActive; }
SFHypertubeEntranceState::FRequestScope::FRequestScope()
{
    bOwnsRequest = !bEntranceRequestActive;
    bEntranceRequestActive = true;
}
SFHypertubeEntranceState::FRequestScope::~FRequestScope()
{
    if (!bOwnsRequest) return;
    for (const auto& Cell : EntranceRequestCells) if (Cell.IsValid()) Cell->Destroy();
    EntranceRequestCells.Reset();
    EntrancePreparedRoots.Reset();
    EntrancePreparedCells.Reset();
    bEntranceRequestActive = false;
}
