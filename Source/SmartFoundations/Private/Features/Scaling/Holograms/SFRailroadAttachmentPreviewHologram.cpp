// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFRailroadAttachmentPreviewHologram.h"
#include "Features/Scaling/SFRailroadAttachmentHologramState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "FGConstructDisqualifier.h"

void ASFRailroadAttachmentPreviewHologram::SyncGeometry(AFGHologram* Parent)
{
    bGeometryReady = false;
    if (!SFRailroadAttachmentHologramState::IsHandled(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
    TArray<UStaticMeshComponent*> Existing;
    GetComponents(Existing);
    for (UStaticMeshComponent* Mesh : Existing)
        if (!GeometryPreviews.Contains(Mesh)) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); }
    TArray<UStaticMeshComponent*> Sources;
    Parent->GetComponents(Sources);
    int32 Index = 0;
    for (UStaticMeshComponent* Source : Sources)
    {
        if (!Source->GetStaticMesh()) continue;
        const FString Name = Source->GetStaticMesh()->GetName();
        if (!Name.StartsWith(TEXT("SM_TrainSignal")) && !Name.StartsWith(TEXT("SM_RailwayEndStop"))
            && !Name.StartsWith(TEXT("SM_Elevator")) && !Name.StartsWith(TEXT("SM_PowerPoleWall"))) continue;
        if (GeometryPreviews.Num() <= Index)
        {
            UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
            Mesh->SetupAttachment(GetRootComponent());
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetComponentTickEnabled(false);
            Mesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
            AddInstanceComponent(Mesh);
            Mesh->RegisterComponent();
            GeometryPreviews.Add(Mesh);
        }
        UStaticMeshComponent* Mesh = GeometryPreviews[Index++];
        Mesh->SetStaticMesh(Source->GetStaticMesh());
        Mesh->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform()));
        for (int32 Material = 0; Material < Source->GetNumMaterials(); ++Material)
            Mesh->SetMaterial(Material, Source->GetMaterial(Material));
        Mesh->SetVisibility(Source->IsVisible());
        Mesh->SetHiddenInGame(Source->bHiddenInGame);
    }
    for (int32 Extra = Index; Extra < GeometryPreviews.Num(); ++Extra)
    { GeometryPreviews[Extra]->SetVisibility(false); GeometryPreviews[Extra]->SetHiddenInGame(true); }
    bGeometryReady = Index > 0;
    SetPlacementMaterialState(GetHologramMaterialState());
}

void ASFRailroadAttachmentPreviewHologram::RememberRequestedPose()
{
    if (!bHasResolvedPose || !GetActorTransform().Equals(LastResolvedPose, 0.01)) RequestedPose = GetActorTransform();
}
void ASFRailroadAttachmentPreviewHologram::ApplyResolvedPose(const FTransform& Pose)
{
    SetActorTransform(Pose);
    LastResolvedPose = Pose;
    bHasResolvedPose = true;
}
void ASFRailroadAttachmentPreviewHologram::CheckValidPlacement()
{
    SyncGeometry(GetParentHologram());
    RememberRequestedPose();
    ResetConstructDisqualifiers();
    if (!bGeometryReady || !SFRailroadAttachmentHologramState::ValidatePreview(this))
    {
        // A rejected request may later return to the previous successful pose.
        // Capture that new request even when it equals LastResolvedPose.
        bHasResolvedPose = false;
        AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
    }
}
void ASFRailroadAttachmentPreviewHologram::SetPlacementMaterialState(EHologramMaterialState State)
{
    Super::SetPlacementMaterialState(State);
    const uint8 Stencil = GetStencilForHologramMaterialState(State);
    for (UStaticMeshComponent* Mesh : GeometryPreviews)
        if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil))
        { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Stencil); Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); }
}
AActor* ASFRailroadAttachmentPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID Id)
{
    AFGHologram* Cell = SFRailroadAttachmentHologramState::FindPreparedCell(this);
    return IsValid(Cell) ? Cell->Construct(OutChildren, Id) : nullptr;
}
void ASFRailroadAttachmentPreviewHologram::Destroyed()
{
    SFRailroadAttachmentHologramState::ReleasePreview(this);
    Super::Destroyed();
}
