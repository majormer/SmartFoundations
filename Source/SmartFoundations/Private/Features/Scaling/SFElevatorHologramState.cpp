// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFElevatorHologramState.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Features/Scaling/Holograms/SFElevatorPreviewHologram.h"
#include "Features/Scaling/Holograms/SFElevatorConstructionHologram.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Hologram/FGElevatorFloorStopHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — Elevator construction
//  1. Existing shaft — [MP-SEAM] client retains native owner/height serialization,
//                      bypassing scaling-spec teardown in the build-input hook.
//  2. Replacement    — [MP-AUTH] Construct retains the exact original Smart group.
//  3. New grid      — native authority materialization recreates owned floor stops.

namespace
{
bool GetStops(AFGHologram* Parent, TArray<AFGHologram*>& OutStops)
{
    OutStops.Reset();
    const FObjectPropertyBase* Stops = FindFProperty<FObjectPropertyBase>(
        AFGElevatorHologram::StaticClass(), TEXT("mChildFloorStops"));
    if (!Stops || Stops->ArrayDim != 2) return false;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        AFGHologram* Stop = Cast<AFGElevatorFloorStopHologram>(
            Stops->GetObjectPropertyValue(Stops->ContainerPtrToValuePtr<void>(Parent, Index)));
        if (!IsValid(Stop) || Stop->GetParentHologram() != Parent) return false;
        OutStops.Add(Stop);
    }
    return true;
}
}

bool SFElevatorHologramState::IsHandled(const AFGHologram* Hologram)
{
    return IsValid(Hologram) && Hologram->IsA<AFGElevatorHologram>()
        && Hologram->GetBuildClass() && Hologram->GetBuildClass()->GetFName() == TEXT("Build_Elevator_C");
}

AFGBuildable* SFElevatorHologramState::GetSnappedElevator(const AFGHologram* Hologram)
{
    if (!IsHandled(Hologram)) return nullptr;
    const FObjectPropertyBase* Snapped = FindFProperty<FObjectPropertyBase>(
        AFGElevatorHologram::StaticClass(), TEXT("mSnappedToElevator"));
    AFGBuildable* Owner = Snapped ? Cast<AFGBuildable>(Snapped->GetObjectPropertyValue_InContainer(Hologram)) : nullptr;
    return IsValid(Owner) && Owner->GetClass() == Hologram->GetBuildClass() ? Owner : nullptr;
}

bool SFElevatorHologramState::GetSelectedMeshes(AFGHologram* Parent, TArray<UStaticMeshComponent*>& OutMeshes)
{
    OutMeshes.Reset();
    TArray<AFGHologram*> Assemblies;
    if (!IsHandled(Parent) || !GetStops(Parent, Assemblies)) return false;
    Assemblies.Insert(Parent, 0);
    for (AFGHologram* Assembly : Assemblies)
    {
        TArray<UStaticMeshComponent*> Meshes;
        Assembly->GetComponents(Meshes);
        for (UStaticMeshComponent* Mesh : Meshes)
        {
            if (!IsValid(Mesh) || !Mesh->GetStaticMesh()) continue;
            const FString Name = Mesh->GetStaticMesh()->GetName();
            // Guidelines and power-wire previews are native interaction aids.
            if (Name.StartsWith(TEXT("SM_Elevator")) || Name.StartsWith(TEXT("SM_PowerPoleWall")))
                OutMeshes.Add(Mesh);
        }
    }
    return OutMeshes.Num() >= 4;
}

bool SFElevatorHologramState::TryGetDimensions(AFGHologram* Parent, FVector& OutDimensions)
{
    TArray<UStaticMeshComponent*> Meshes;
    if (!GetSelectedMeshes(Parent, Meshes)) return false;
    FBox Bounds(ForceInit);
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        if (!Mesh->IsVisible() || Mesh->bHiddenInGame) continue;
        const FTransform LocalPose = Mesh->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
        const FBox MeshBox = Mesh->GetStaticMesh()->GetBoundingBox();
        if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Mesh))
        {
            for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
            {
                FTransform Instance;
                if (Instances->GetInstanceTransform(Index, Instance)) Bounds += MeshBox.TransformBy(Instance * LocalPose);
            }
        }
        else Bounds += MeshBox.TransformBy(LocalPose);
    }
    if (!Bounds.IsValid || Bounds.GetSize().ContainsNaN()) return false;
    OutDimensions = SFScalingFootprint::FromLocalDimensions(Bounds.GetSize(), Parent->GetActorRotation());
    return OutDimensions.GetMin() > 0.0;
}

bool SFElevatorHologramState::GetStopCost(AFGHologram* Parent, TArray<FItemAmount>& OutCost)
{
    OutCost.Reset();
    TArray<AFGHologram*> Stops;
    if (!IsHandled(Parent) || !GetStops(Parent, Stops)) return false;
    for (AFGHologram* Stop : Stops)
    {
        if (Stop->IsDisabled()) continue;
        for (const FItemAmount& Item : Stop->GetCost(true))
        {
            FItemAmount* Existing = OutCost.FindByPredicate(
                [&Item](const FItemAmount& Entry) { return Entry.ItemClass == Item.ItemClass; });
            if (Existing) Existing->Amount += Item.Amount;
            else OutCost.Add(Item);
        }
    }
    return true;
}

void SFElevatorHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (IsHandled(Parent) && IsValid(Child) && Parent->GetBuildClass() == Child->GetBuildClass())
        if (ASFElevatorPreviewHologram* Preview = Cast<ASFElevatorPreviewHologram>(Child)) Preview->SyncGeometry(Parent);
}

bool SFElevatorHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
    // [MP-AUTH] Remote previews do not establish local construction authority.
    if (!IsHandled(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
        || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
        if (IsValid(Child) && Child->IsA<ASFElevatorPreviewHologram>() && !Child->IsDisabled()) return true;
    return false;
}

AFGHologram* SFElevatorHologramState::SpawnConstructionCell(AFGHologram* Seed,
    AFGHologram* OwnerParent, FName ChildName, const FTransform& Pose)
{
    if (!IsHandled(Seed) || !IsValid(OwnerParent) || !Seed->GetWorld() || !Seed->GetRecipe()) return nullptr;
    TArray<AFGHologram*> SourceStops;
    if (!GetStops(Seed, SourceStops)) return nullptr;
    // Extending an existing shaft is a different native ownership contract.
    const FObjectPropertyBase* Snapped = FindFProperty<FObjectPropertyBase>(
        AFGElevatorHologram::StaticClass(), TEXT("mSnappedToElevator"));
    if (!Snapped || Snapped->GetObjectPropertyValue_InContainer(Seed)) return nullptr;
    const TCHAR* Names[] = {TEXT("mFloorStopHologramRecipe"), TEXT("mHeightMesh1m"), TEXT("mHeightMesh4m"),
        TEXT("mBottomMesh"), TEXT("mTopMesh"), TEXT("mActivePointIdx"), TEXT("mTopTransform"),
        TEXT("mHeight"), TEXT("mIsReversed")};
    TArray<FProperty*> Properties;
    for (const TCHAR* Name : Names)
    {
        FProperty* Property = AFGElevatorHologram::StaticClass()->FindPropertyByName(Name);
        if (!Property) return nullptr;
        Properties.Add(Property);
    }
    FStructProperty* Placed = FindFProperty<FStructProperty>(AFGElevatorHologram::StaticClass(), TEXT("mPlacedTransform"));
    if (!Placed) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = Seed->GetOwner();
    Params.Instigator = Seed->GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    ASFElevatorConstructionHologram* Cell = Seed->GetWorld()->SpawnActor<ASFElevatorConstructionHologram>(
        ASFElevatorConstructionHologram::StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
    if (!Cell) return nullptr;
    Cell->SetChildBuildClass(Seed->GetBuildClass());
    Cell->SetRecipe(Seed->GetRecipe());
    Cell->SetConstructionInstigator(Seed->GetConstructionInstigator());
    for (FProperty* Property : Properties)
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Cell), Property->ContainerPtrToValuePtr<void>(Seed));
    *Placed->ContainerPtrToValuePtr<FTransform>(Cell) = Pose;
    Cell->FinishSpawning(Pose);
    TArray<AFGHologram*> CreatedStops;
    if (!GetStops(Cell, CreatedStops))
        Cell->SpawnChildren(Seed->GetOwner(), Pose.GetLocation(), Seed->GetInstigator());
    if (FProperty* Permission = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mCanBePlacedInBlueprintDesigner")))
        Permission->CopyCompleteValue(Permission->ContainerPtrToValuePtr<void>(Cell), Permission->ContainerPtrToValuePtr<void>(Seed));
    OwnerParent->AddChild(Cell, ChildName);
    USFHologramDataService::MarkAsChild(Cell, Seed, ESFChildHologramType::ScalingGrid);
    Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
    // Native refresh positions the two owned stops and regenerates the selected shaft.
    Cell->BuildFloorStopInfos();
    if (UFunction* Rep = Cell->FindFunction(TEXT("OnRep_TopTransform"))) Cell->ProcessEvent(Rep, nullptr);
    Cell->SetActorTransform(Pose);
    if (Seed->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    TArray<AFGHologram*> Stops;
    if (!GetStops(Cell, Stops)) { Cell->Destroy(); return nullptr; }
    for (int32 Index = 0; Index < Stops.Num(); ++Index)
    {
        // SpawnChildren starts a fresh native placement, whose upper stop is disabled
        // until the player selects the height. Copy that selection explicitly: the
        // top-transform refresh positions it but does not enable it for construction.
        Stops[Index]->SetDisabled(SourceStops[Index]->IsDisabled());
        Stops[Index]->SetActorTransform(Cell->GetTransformForChildStop(Index));
    }
    return Cell;
}
