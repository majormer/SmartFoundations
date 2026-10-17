// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFLadderPreviewHologram.h"
#include "Features/Scaling/SFLadderHologramState.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Hologram/FGLadderHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — ladder preview materialization
//  1. Local authority: Construct briefly creates a native selected-height hologram.
//  2. Remote client: compact spec expansion copies the serialized root height on authority.
void ASFLadderPreviewHologram::SyncGeometry(AFGLadderHologram* Parent)
{
	bGeometryReady = false;
	if (!IsValid(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
	const FObjectPropertyBase* Body = FindFProperty<FObjectPropertyBase>(AFGLadderHologram::StaticClass(), TEXT("mInstancedMeshComponent"));
	UInstancedStaticMeshComponent* Source = Body ? Cast<UInstancedStaticMeshComponent>(Body->GetObjectPropertyValue_InContainer(Parent)) : nullptr;
	if (!IsValid(Source) || !Source->GetStaticMesh() || Source->GetInstanceCount() <= 0) return;
	NativeCostMultiplier = Parent->GetBaseCostMultiplier();
	if (NativeCostMultiplier <= 0) return;
	if (!GeometryPreview)
	{
		// Recipe geometry is a default single segment, not the selected native body.
		TArray<UStaticMeshComponent*> Defaults;
		GetComponents(Defaults);
		for (UStaticMeshComponent* Mesh : Defaults)
		{
			Mesh->SetVisibility(false);
			Mesh->SetHiddenInGame(true);
		}
		GeometryPreview = NewObject<UInstancedStaticMeshComponent>(this);
		GeometryPreview->SetupAttachment(GetRootComponent());
		GeometryPreview->SetMobility(EComponentMobility::Movable);
		GeometryPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GeometryPreview->SetComponentTickEnabled(false);
		GeometryPreview->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
		AddInstanceComponent(GeometryPreview);
		GeometryPreview->RegisterComponent();
	}
	if (GeometryPreview->GetStaticMesh() != Source->GetStaticMesh()) GeometryPreview->SetStaticMesh(Source->GetStaticMesh());
	const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
	if (!GeometryPreview->GetRelativeTransform().Equals(LocalPose, 0.01f)) GeometryPreview->SetRelativeTransform(LocalPose);
	bool bInstancesChanged = InstancePoses.Num() != Source->GetInstanceCount();
	TArray<FTransform> SelectedPoses;
	SelectedPoses.Reserve(Source->GetInstanceCount());
	for (int32 Index = 0; Index < Source->GetInstanceCount(); ++Index)
	{
		FTransform Pose;
		if (!Source->GetInstanceTransform(Index, Pose, false)) return;
		SelectedPoses.Add(Pose);
		bInstancesChanged |= !InstancePoses.IsValidIndex(Index) || !InstancePoses[Index].Equals(Pose, 0.01f);
	}
	if (bInstancesChanged)
	{
		GeometryPreview->ClearInstances();
		GeometryPreview->AddInstances(SelectedPoses, false, false);
		InstancePoses = MoveTemp(SelectedPoses);
	}
	for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
	{
		if (GeometryPreview->GetMaterial(Slot) != Source->GetMaterial(Slot)) GeometryPreview->SetMaterial(Slot, Source->GetMaterial(Slot));
	}
	GeometryPreview->SetVisibility(Source->IsVisible());
	GeometryPreview->SetHiddenInGame(Source->bHiddenInGame);
	ApplyGeometryMaterialState(GetHologramMaterialState());
	bGeometryReady = true;
}

int32 ASFLadderPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }

void ASFLadderPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
	Super::SetPlacementMaterialState(MaterialState);
	ApplyGeometryMaterialState(MaterialState);
}

void ASFLadderPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
	if (!GeometryPreview) return;
	const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
	if (!GeometryPreview->bRenderCustomDepth || GeometryPreview->CustomDepthStencilValue != Stencil
		|| GeometryPreview->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default)
	{
		GeometryPreview->SetRenderCustomDepth(true);
		GeometryPreview->SetCustomDepthStencilValue(Stencil);
		GeometryPreview->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
	}
}

AActor* ASFLadderPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
	AFGLadderHologram* Parent = Cast<AFGLadderHologram>(GetParentHologram());
	if (!IsValid(Parent) || !bGeometryReady) return nullptr;
	const FTransform Pose = GetActorTransform();
	AFGHologram* Native = AFGHologram::SpawnChildHologramFromRecipe(this,
		TEXT("NativeLadderConstruction"), GetRecipe(), GetOwner(), Pose.GetLocation(), [&Pose](AFGHologram* Cell)
		{
			Cell->SetActorTransform(Pose);
			Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
		});
	if (!IsValid(Native)) return nullptr;
	Native->SetConstructionInstigator(GetConstructionInstigator());
	USFHologramDataService::MarkAsChild(Native, Parent, ESFChildHologramType::ScalingGrid);
	SFLadderHologramState::Sync(Parent, Native);
	Native->SetActorTransform(Pose);
	if (Parent->GetBlueprintDesigner()) Native->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
	AActor* Built = Native->Construct(OutChildren, ConstructionID);
	for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
	if (IsValid(Native)) Native->Destroy();
	return Built;
}
