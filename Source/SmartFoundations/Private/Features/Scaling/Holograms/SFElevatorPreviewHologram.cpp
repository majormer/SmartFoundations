// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Holograms/SFElevatorPreviewHologram.h"
#include "SmartFoundations.h"
#include "Features/Scaling/SFElevatorHologramState.h"
#include "Components/InstancedStaticMeshComponent.h"

// SP/MP DIVERGENCE MAP — elevator assembly proxy scaling
//  1. Local authority: a brief native cell constructs the shaft and its two owned stops.
//  2. Remote client: authority expands the serialized native root into the same native cells.
void ASFElevatorPreviewHologram::SyncGeometry(AFGHologram* Parent)
{
    bGeometryReady = false;
    if (!SFElevatorHologramState::IsHandled(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
    TArray<UStaticMeshComponent*> Sources;
    if (!SFElevatorHologramState::GetSelectedMeshes(Parent, Sources)
        || !SFElevatorHologramState::GetStopCost(Parent, StopCost)) return;
    NativeCostMultiplier = Parent->GetBaseCostMultiplier();
    if (NativeCostMultiplier <= 0) return;
    if (!bDefaultMeshesHidden)
    {
        TArray<UStaticMeshComponent*> Defaults;
        GetComponents(Defaults);
        for (UStaticMeshComponent* Mesh : Defaults) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); }
        bDefaultMeshesHidden = true;
    }
    for (int32 Index = 0; Index < Sources.Num(); ++Index)
    {
        UStaticMeshComponent* Source = Sources[Index];
        UInstancedStaticMeshComponent* SourceInstances = Cast<UInstancedStaticMeshComponent>(Source);
        if (!GeometryPreviews.IsValidIndex(Index))
        {
            UStaticMeshComponent* Mesh = SourceInstances
                ? NewObject<UInstancedStaticMeshComponent>(this) : NewObject<UStaticMeshComponent>(this);
            Mesh->SetupAttachment(GetRootComponent());
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetComponentTickEnabled(false);
            Mesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
            AddInstanceComponent(Mesh);
            Mesh->RegisterComponent();
            GeometryPreviews.Add(Mesh);
        }
        UStaticMeshComponent* Mesh = GeometryPreviews[Index];
        if (Mesh->GetStaticMesh() != Source->GetStaticMesh()) Mesh->SetStaticMesh(Source->GetStaticMesh());
        const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
        if (!Mesh->GetRelativeTransform().Equals(LocalPose, 0.01f)) Mesh->SetRelativeTransform(LocalPose);
        if (SourceInstances)
        {
            UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Mesh);
            if (!Instances) return;
            const int32 Count = SourceInstances->GetInstanceCount();
            if (Instances->GetInstanceCount() != Count)
            {
                Instances->ClearInstances();
                for (int32 InstanceIndex = 0; InstanceIndex < Count; ++InstanceIndex)
                {
                    FTransform Instance;
                    SourceInstances->GetInstanceTransform(InstanceIndex, Instance);
                    Instances->AddInstance(Instance);
                }
            }
            else for (int32 InstanceIndex = 0; InstanceIndex < Count; ++InstanceIndex)
            {
                FTransform SourcePose, ExistingPose;
                SourceInstances->GetInstanceTransform(InstanceIndex, SourcePose);
                Instances->GetInstanceTransform(InstanceIndex, ExistingPose);
                if (!ExistingPose.Equals(SourcePose, 0.01f))
                    Instances->UpdateInstanceTransform(InstanceIndex, SourcePose, false, true);
            }
        }
        for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
            if (Mesh->GetMaterial(Slot) != Source->GetMaterial(Slot)) Mesh->SetMaterial(Slot, Source->GetMaterial(Slot));
        const AFGHologram* Assembly = Cast<AFGHologram>(Source->GetOwner());
        const bool bAssemblyVisible = !Assembly || (!Assembly->IsDisabled() && !Assembly->IsHidden());
        Mesh->SetVisibility(Source->IsVisible() && bAssemblyVisible);
        Mesh->SetHiddenInGame(Source->bHiddenInGame || !bAssemblyVisible);
    }
    for (int32 Index = Sources.Num(); Index < GeometryPreviews.Num(); ++Index)
    { GeometryPreviews[Index]->SetVisibility(false); GeometryPreviews[Index]->SetHiddenInGame(true); }
    ApplyGeometryMaterialState(GetHologramMaterialState());
    bGeometryReady = true;
}

int32 ASFElevatorPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }
TArray<FItemAmount> ASFElevatorPreviewHologram::GetCost(bool bIncludeChildren) const
{
    TArray<FItemAmount> Cost = Super::GetCost(bIncludeChildren);
    if (bIncludeChildren) for (const FItemAmount& Item : StopCost)
    {
        FItemAmount* Existing = Cost.FindByPredicate(
            [&Item](const FItemAmount& Entry) { return Entry.ItemClass == Item.ItemClass; });
        if (Existing) Existing->Amount += Item.Amount;
        else Cost.Add(Item);
    }
    return Cost;
}

void ASFElevatorPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
    Super::SetPlacementMaterialState(MaterialState);
    ApplyGeometryMaterialState(MaterialState);
}
void ASFElevatorPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
    const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
    for (UStaticMeshComponent* Mesh : GeometryPreviews)
        if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil))
        { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Stencil); Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); }
}

AActor* ASFElevatorPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
    AFGHologram* Parent = GetParentHologram();
    if (!bGeometryReady || !SFElevatorHologramState::IsHandled(Parent)) return nullptr;
    AFGHologram* Native = SFElevatorHologramState::SpawnConstructionCell(Parent, this,
        TEXT("NativeElevatorConstruction"), GetActorTransform());
    if (!Native) return nullptr;
    AActor* Built = Native->Construct(OutChildren, ConstructionID);
    for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
    if (IsValid(Native)) Native->Destroy();
    return Built;
}
