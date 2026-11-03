// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Holograms/SFFoundationPassthroughPreviewHologram.h"
#include "SmartFoundations.h"
#include "Features/Scaling/SFFoundationPassthroughState.h"
#include "Features/Scaling/SFLiftFloorHoleGridRequest.h"
#include "Components/StaticMeshComponent.h"
#include "FGPipeConnectionComponent.h"

// SP/MP DIVERGENCE MAP — Foundation floor-hole proxy scaling
//  1. Local authority: Construct creates one brief family-correct native commit cell.
//  2. Remote client: authority expands the existing serialized native root into commit cells.
void ASFFoundationPassthroughPreviewHologram::SyncGeometry(AFGHologram* Parent)
{
    bGeometryReady = false;
    if (!SFFoundationPassthroughState::IsHandled(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
    TArray<UStaticMeshComponent*> Sources;
    if (!SFFoundationPassthroughState::GetSelectedMeshes(Parent, Sources)) return;
    NativeCostMultiplier = Parent->GetBaseCostMultiplier();
    if (NativeCostMultiplier <= 0) return;

    TInlineComponentArray<UFGPipeConnectionComponentBase*> NativeConnections;
    TInlineComponentArray<UFGPipeConnectionComponentBase*> PreviewConnections;
    Parent->GetComponents(NativeConnections);
    GetComponents(PreviewConnections);
    if (NativeConnections.Num() != PreviewConnections.Num()) return;
    for (UFGPipeConnectionComponentBase* Source : NativeConnections)
    {
        UFGPipeConnectionComponentBase* Target = nullptr;
        for (UFGPipeConnectionComponentBase* Candidate : PreviewConnections)
            if (Candidate->GetFName() == Source->GetFName() && Candidate->GetClass() == Source->GetClass())
                { Target = Candidate; break; }
        if (!Target) return;
        const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
        const FTransform WorldPose = LocalPose * GetActorTransform();
        if (!Target->GetComponentTransform().Equals(WorldPose, 0.01f)) Target->SetWorldTransform(WorldPose);
        Target->SetPipeConnectionType(Source->GetPipeConnectionType());
        Target->SetConnectorClearance(Source->GetConnectorClearance());
        Target->SetDisallowSnappingTo(Source->IsSnappingToDisallowed());
        // Geometry only: each preview keeps its own connection identity and acquires no live peer.
    }
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

int32 ASFFoundationPassthroughPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }
void ASFFoundationPassthroughPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
    Super::SetPlacementMaterialState(MaterialState);
    ApplyGeometryMaterialState(MaterialState);
}
void ASFFoundationPassthroughPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
    const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
    for (UStaticMeshComponent* Mesh : GeometryPreviews)
        if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil))
            { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Stencil); Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); }
}

AActor* ASFFoundationPassthroughPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
    AFGHologram* Parent = GetParentHologram();
    if (!bGeometryReady || !SFFoundationPassthroughState::IsHandled(Parent)) return nullptr;
    if (FSFLiftFloorHoleGridRequest::IsHandled(Parent))
    {
        auto* Prepared = FSFLiftFloorHoleGridRequest::FindCell(this);
        return IsValid(Prepared) ? Prepared->Construct(OutChildren, ConstructionID) : nullptr;
    }
    AFGHologram* Native = SFFoundationPassthroughState::SpawnConstructionCell(Parent, this,
        TEXT("NativeFoundationPassthroughConstruction"), GetActorTransform());
    if (!Native) return nullptr;
    AActor* Built = Native->Construct(OutChildren, ConstructionID);
    if (IsValid(Native)) Native->Destroy();
    return Built;
}
