// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/SFFoundationPassthroughState.h"
#include "Features/Scaling/Holograms/SFFoundationPassthroughPreviewHologram.h"
#include "Features/Scaling/Holograms/SFHypertubePassthroughConstructionHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

bool SFFoundationPassthroughState::IsHandled(const AFGHologram* Hologram)
{
    if (!IsValid(Hologram) || !Hologram->IsA<AFGPassthroughHologram>() || !Hologram->GetBuildClass()) return false;
    const FName BuildClass = Hologram->GetBuildClass()->GetFName();
    if (BuildClass == TEXT("Build_FoundationPassthrough_Hypertube_C"))
        return Hologram->IsA<AFGPassthroughPipeHyperHologram>();
    // The vanilla lift and pipeline floor holes use the generic passthrough contract;
    // unlike a hypertube floor hole they do not expose connectors of their own.
    return !Hologram->IsA<AFGPassthroughPipeBaseHologram>()
        && (BuildClass == TEXT("Build_FoundationPassthrough_Pipe_C")
            || BuildClass == TEXT("Build_FoundationPassthrough_Lift_C"));
}
bool SFFoundationPassthroughState::ReadThickness(const AFGHologram* Hologram, float& OutThickness)
{
    if (!IsHandled(Hologram)) return false;
    const FFloatProperty* Property = FindFProperty<FFloatProperty>(AFGPassthroughHologram::StaticClass(), TEXT("mSnappedBuildingThickness"));
    if (!Property) return false;
    OutThickness = Property->GetPropertyValue_InContainer(Hologram);
    return FMath::IsFinite(OutThickness) && OutThickness > 0;
}
bool SFFoundationPassthroughState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
    OutMeshes.Reset();
    if (!IsHandled(Hologram)) return false;
    const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGPassthroughHologram::StaticClass(), TEXT("mMeshComponents"));
    const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!Inner) return false;
    FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Hologram));
    for (int32 Index = 0; Index < Array.Num(); ++Index)
    {
        UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)));
        if (!IsValid(Mesh) || !Mesh->GetStaticMesh()) return false;
        OutMeshes.Add(Mesh);
    }
    return !OutMeshes.IsEmpty();
}
void SFFoundationPassthroughState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (IsHandled(Parent) && IsValid(Child) && Parent->GetBuildClass() == Child->GetBuildClass())
        if (ASFFoundationPassthroughPreviewHologram* Preview = Cast<ASFFoundationPassthroughPreviewHologram>(Child))
            Preview->SyncGeometry(Parent);
}
bool SFFoundationPassthroughState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
    // [MP-AUTH] A remote client's preview role alone does not establish local construction authority.
    if (!IsHandled(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
        || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
        if (IsValid(Child) && Child->IsA<ASFFoundationPassthroughPreviewHologram>() && !Child->IsDisabled()) return true;
    return false;
}
AFGHologram* SFFoundationPassthroughState::SpawnConstructionCell(AFGHologram* Seed,
    AFGHologram* OwnerParent, FName ChildName, const FTransform& Pose)
{
    float Thickness = 0;
    if (!ReadThickness(Seed, Thickness) || !IsValid(OwnerParent) || !Seed->GetWorld() || !Seed->GetRecipe()) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = Seed->GetOwner();
    Params.Instigator = Seed->GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    AFGPassthroughHologram* Cell = nullptr;
    if (Seed->IsA<AFGPassthroughPipeHyperHologram>())
    {
        auto* HyperCell = Seed->GetWorld()->SpawnActor<ASFHypertubePassthroughConstructionHologram>(
            ASFHypertubePassthroughConstructionHologram::StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
        if (HyperCell) HyperCell->SetCellBuildClass(Seed->GetBuildClass());
        Cell = HyperCell;
    }
    else
    {
        auto* GenericCell = Seed->GetWorld()->SpawnActor<ASFPassthroughChildHologram>(
            ASFPassthroughChildHologram::StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
        if (GenericCell) GenericCell->SetBuildClass(Seed->GetBuildClass());
        Cell = GenericCell;
    }
    if (!Cell) return nullptr;
    Cell->SetRecipe(Seed->GetRecipe());
    Cell->SetConstructionInstigator(Seed->GetConstructionInstigator());
    Cell->FinishSpawning(Pose);
    if (FProperty* Permission = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mCanBePlacedInBlueprintDesigner")))
        Permission->CopyCompleteValue(Permission->ContainerPtrToValuePtr<void>(Cell), Permission->ContainerPtrToValuePtr<void>(Seed));
    OwnerParent->AddChild(Cell, ChildName);
    USFHologramDataService::MarkAsChild(Cell, Seed, ESFChildHologramType::ScalingGrid);
    Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
    if (auto* HyperCell = Cast<ASFHypertubePassthroughConstructionHologram>(Cell))
        HyperCell->SetSelectedThickness(Thickness);
    else if (auto* GenericCell = Cast<ASFPassthroughChildHologram>(Cell))
        GenericCell->SetSnappedThickness(Thickness);
    Cell->SetActorTransform(Pose);
    if (Seed->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    return Cell;
}
