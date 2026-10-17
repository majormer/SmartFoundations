// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFConveyorLiftPreviewHologram.h"
#include "Features/Scaling/SFConveyorLiftHologramState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "FGFactoryConnectionComponent.h"
#include "UObject/UnrealType.h"
#include "FGConstructDisqualifier.h"

void ASFConveyorLiftPreviewHologram::SyncGeometry(AFGHologram* Parent)
{
    bGeometryReady = false;
    if (!IsValid(Parent) || !Parent->IsA<AFGConveyorLiftHologram>() || Parent->GetBuildClass() != GetBuildClass()) return;
    // Preview ports follow selected native geometry, but keep their own identities and no live peers.
    TInlineComponentArray<UFGFactoryConnectionComponent*> SourcesPorts, TargetPorts;
    Parent->GetComponents(SourcesPorts); GetComponents(TargetPorts);
    for (auto* Source : SourcesPorts) for (auto* Target : TargetPorts)
        if (Source->GetFName() == Target->GetFName())
        {
            const FTransform PortPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform()) * GetActorTransform();
            if (!Target->GetComponentTransform().Equals(PortPose, 0.01)) Target->SetWorldTransform(PortPose);
            if (Target->GetDirection() != Source->GetDirection()) Target->SetDirection(Source->GetDirection());
        }
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
        bool bLiftMesh = false;
        for (const TCHAR* Name : {TEXT("mBottomMesh"), TEXT("mMidMesh"), TEXT("mHalfMidMesh"), TEXT("mTopMesh"), TEXT("mJointMesh"), TEXT("mPassthroughBottomMesh"), TEXT("mPassthroughTopMesh")})
            if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), Name))
                bLiftMesh |= Property->GetObjectPropertyValue_InContainer(Parent) == Source->GetStaticMesh();
        if (!bLiftMesh) continue;
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
        if (Mesh->GetStaticMesh() != Source->GetStaticMesh()) Mesh->SetStaticMesh(Source->GetStaticMesh());
        const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
        if (!Mesh->GetRelativeTransform().Equals(LocalPose, 0.01)) Mesh->SetRelativeTransform(LocalPose);
        for (int32 Material = 0; Material < Source->GetNumMaterials(); ++Material)
            if (Mesh->GetMaterial(Material) != Source->GetMaterial(Material)) Mesh->SetMaterial(Material, Source->GetMaterial(Material));
        Mesh->SetVisibility(Source->IsVisible());
        Mesh->SetHiddenInGame(Source->bHiddenInGame);
    }
    for (int32 Extra = Index; Extra < GeometryPreviews.Num(); ++Extra)
    { GeometryPreviews[Extra]->SetVisibility(false); GeometryPreviews[Extra]->SetHiddenInGame(true); }
    NativeCostMultiplier = Parent->GetBaseCostMultiplier();
    bGeometryReady = Index > 0 && NativeCostMultiplier > 0;
    SetPlacementMaterialState(GetHologramMaterialState());
}

void ASFConveyorLiftPreviewHologram::RememberRequestedPose()
{
    if (!bHasResolvedPose || !GetActorTransform().Equals(LastResolvedPose, 0.01)) RequestedPose = GetActorTransform();
}
void ASFConveyorLiftPreviewHologram::ApplyResolvedPose(const FTransform& Pose)
{
    SetActorTransform(Pose);
    LastResolvedPose = Pose;
    bHasResolvedPose = true;
}
void ASFConveyorLiftPreviewHologram::CheckValidPlacement()
{
    SyncGeometry(GetParentHologram());
    RememberRequestedPose();
    ResetConstructDisqualifiers();
    if (!bGeometryReady || !SFConveyorLiftHologramState::ValidatePreview(this))
    {
        // A rejected request may later return to the previous successful pose.
        // Capture that new request even when it equals LastResolvedPose.
        bHasResolvedPose = false;
        AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
    }
}
void ASFConveyorLiftPreviewHologram::SetPlacementMaterialState(EHologramMaterialState State)
{
    Super::SetPlacementMaterialState(State);
    const uint8 Stencil = GetStencilForHologramMaterialState(State);
    for (UStaticMeshComponent* Mesh : GeometryPreviews)
        if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil))
        { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Stencil); Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); }
}
AActor* ASFConveyorLiftPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID Id)
{
    AFGHologram* Cell = SFConveyorLiftHologramState::FindPreparedCell(this);
    return IsValid(Cell) ? Cell->Construct(OutChildren, Id) : nullptr;
}
void ASFConveyorLiftPreviewHologram::Destroyed()
{
    SFConveyorLiftHologramState::ReleasePreview(this);
    Super::Destroyed();
}

int32 ASFConveyorLiftPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }
