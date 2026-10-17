// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFFloodlightPreviewHologram.h"
#include "Features/Scaling/SFFloodlightHologramState.h"
#include "Components/StaticMeshComponent.h"

// SP/MP DIVERGENCE MAP — Floodlight proxy scaling
//  1. Local authority: a brief typed cell applies the selected native angle at commit.
//  2. Remote client: authority expands the serialized native root into the same typed cells.
void ASFFloodlightPreviewHologram::SyncGeometry(AFGHologram* Parent)
{
    bGeometryReady = false;
    if (!SFFloodlightHologramState::IsHandled(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
    TArray<UStaticMeshComponent*> Sources;
    if (!SFFloodlightHologramState::GetSelectedMeshes(Parent, Sources)) return;
    NativeCostMultiplier = Parent->GetBaseCostMultiplier();
    if (NativeCostMultiplier <= 0) return;
    if (!bDefaultMeshesHidden)
    {
        TArray<UStaticMeshComponent*> Defaults;
        GetComponents(Defaults);
        for (UStaticMeshComponent* Mesh : Defaults) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); }
        bDefaultMeshesHidden = true;
    }
    while (GeometryPreviews.Num() < Sources.Num())
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetComponentTickEnabled(false);
        Mesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
        AddInstanceComponent(Mesh);
        Mesh->RegisterComponent();
        GeometryPreviews.Add(Mesh);
    }
    for (int32 Index = 0; Index < GeometryPreviews.Num(); ++Index)
    {
        UStaticMeshComponent* Mesh = GeometryPreviews[Index];
        if (Index >= Sources.Num()) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); continue; }
        UStaticMeshComponent* Source = Sources[Index];
        if (Mesh->GetStaticMesh() != Source->GetStaticMesh()) Mesh->SetStaticMesh(Source->GetStaticMesh());
        const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
        if (!Mesh->GetRelativeTransform().Equals(LocalPose, 0.01f)) Mesh->SetRelativeTransform(LocalPose);
        for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
            if (Mesh->GetMaterial(Slot) != Source->GetMaterial(Slot)) Mesh->SetMaterial(Slot, Source->GetMaterial(Slot));
        Mesh->SetVisibility(Source->IsVisible());
        Mesh->SetHiddenInGame(Source->bHiddenInGame);
    }
    ApplyGeometryMaterialState(GetHologramMaterialState());
    bGeometryReady = true;
}

int32 ASFFloodlightPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }
void ASFFloodlightPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
    Super::SetPlacementMaterialState(MaterialState);
    ApplyGeometryMaterialState(MaterialState);
}
void ASFFloodlightPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
    const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
    for (UStaticMeshComponent* Mesh : GeometryPreviews)
        if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil))
        { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Stencil); Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); }
}

AActor* ASFFloodlightPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
    AFGHologram* Parent = GetParentHologram();
    if (!bGeometryReady || !SFFloodlightHologramState::IsHandled(Parent)) return nullptr;
    AFGHologram* Native = SFFloodlightHologramState::SpawnConstructionCell(Parent, this,
        TEXT("NativeFloodlightConstruction"), GetActorTransform());
    if (!Native) return nullptr;
    AActor* Built = Native->Construct(OutChildren, ConstructionID);
    if (IsValid(Native)) Native->Destroy();
    return Built;
}
