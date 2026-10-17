// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Holograms/Logistics/SFHypertubeSupportPreviewHologram.h"
#include "Holograms/Logistics/SFSelectedPoleState.h"
#include "Components/StaticMeshComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGConstructDisqualifier.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Hologram/FGConveyorPoleHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — Selected conveyor/pipeline/hypertube floor poles
//  1. Local authority: Construct briefly creates a native selected-height/angle hologram.
//  2. Remote client: compact spec expansion copies serialized root state on authority.
void ASFHypertubeSupportPreviewHologram::SyncGeometry(AFGPoleHologram* Parent)
{
	bGeometryReady = false;
	if (!SFSelectedPoleState::IsSelectedPole(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
	TArray<UStaticMeshComponent*> Sources;
	if (!SFSelectedPoleState::GetSelectedMeshes(Parent, Sources)) return;
	NativeCostMultiplier = Parent->GetBaseCostMultiplier();
	if (NativeCostMultiplier <= 0) return;
	if (Parent->IsA<AFGPipelinePoleHologram>())
	{
		// Auto-Connect routes from the preview's own native snap component. Its recipe
		// pose is insufficient after the root selects a different pole height or tilt.
		UFGPipeConnectionComponentBase* SourceSnap = CastChecked<AFGPipelinePoleHologram>(Parent)->GetSnapConnection();
		if (!IsValid(SourceSnap)) return;
		TInlineComponentArray<UFGPipeConnectionComponentBase*> Connections;
		GetComponents(Connections);
		UFGPipeConnectionComponentBase* PreviewSnap = nullptr;
		for (UFGPipeConnectionComponentBase* Connection : Connections)
		{
			if (Connection->GetFName() == SourceSnap->GetFName() && Connection->GetClass() == SourceSnap->GetClass())
			{
				PreviewSnap = Connection;
				break;
			}
		}
		if (!PreviewSnap) return;
		const FTransform LocalSnap = SourceSnap->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
		const FTransform PreviewPose = LocalSnap * GetActorTransform();
		if (!PreviewSnap->GetComponentTransform().Equals(PreviewPose, 0.01f)) PreviewSnap->SetWorldTransform(PreviewPose);
		PreviewSnap->SetPipeConnectionType(SourceSnap->GetPipeConnectionType());
		PreviewSnap->SetConnectorClearance(SourceSnap->GetConnectorClearance());
		PreviewSnap->SetDisallowSnappingTo(SourceSnap->IsSnappingToDisallowed());
		// Copy routing geometry only; native peer links remain owned by their actors.
	}
	else if (AFGConveyorPoleHologram* Conveyor = Cast<AFGConveyorPoleHologram>(Parent))
	{
		UFGFactoryConnectionComponent* SourceSnap = Conveyor->GetSnapConnection();
		if (!IsValid(SourceSnap)) return;
		TInlineComponentArray<UFGFactoryConnectionComponent*> Connections;
		GetComponents(Connections);
		UFGFactoryConnectionComponent* PreviewSnap = nullptr;
		for (UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (Connection->GetFName() == SourceSnap->GetFName() && Connection->GetClass() == SourceSnap->GetClass())
			{
				PreviewSnap = Connection;
				break;
			}
		}
		if (!PreviewSnap) return;
		const FTransform LocalSnap = SourceSnap->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
		const FTransform PreviewPose = LocalSnap * GetActorTransform();
		if (!PreviewSnap->GetComponentTransform().Equals(PreviewPose, 0.01f)) PreviewSnap->SetWorldTransform(PreviewPose);
		PreviewSnap->SetDirection(SourceSnap->GetDirection());
		PreviewSnap->SetConnectorClearance(SourceSnap->GetConnectorClearance());
		// Snap points do not acquire the parent's inventory or live peer links.
	}
	if (!bDefaultMeshesHidden)
	{
		TArray<UStaticMeshComponent*> Defaults;
		GetComponents(Defaults);
		for (UStaticMeshComponent* Mesh : Defaults)
		{
			Mesh->SetVisibility(false);
			Mesh->SetHiddenInGame(true);
		}
		// Reuse recipe geometry before allocating another component per grid cell.
		for (UStaticMeshComponent* Source : Sources)
		{
			UStaticMeshComponent* Reusable = nullptr;
			for (UStaticMeshComponent* Mesh : Defaults)
			{
				if (Mesh->GetClass() == UStaticMeshComponent::StaticClass() && !GeometryPreviews.Contains(Mesh)
					&& Mesh->GetStaticMesh() == Source->GetStaticMesh()) { Reusable = Mesh; break; }
			}
			GeometryPreviews.Add(Reusable);
		}
		bDefaultMeshesHidden = true;
	}
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		UStaticMeshComponent* Source = Sources[Index];
		if (!GeometryPreviews[Index])
		{
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
			Mesh->SetupAttachment(GetRootComponent());
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetComponentTickEnabled(false);
			Mesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
			AddInstanceComponent(Mesh);
			Mesh->RegisterComponent();
			GeometryPreviews[Index] = Mesh;
		}
		UStaticMeshComponent* Mesh = GeometryPreviews[Index];
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

int32 ASFHypertubeSupportPreviewHologram::GetBaseCostMultiplier() const { return NativeCostMultiplier; }

void ASFHypertubeSupportPreviewHologram::CheckValidPlacement()
{
	Super::CheckValidPlacement();
	if (!bGeometryReady) AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
}

void ASFHypertubeSupportPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
	Super::SetPlacementMaterialState(MaterialState);
	ApplyGeometryMaterialState(MaterialState);
}

void ASFHypertubeSupportPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
	const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
	for (UStaticMeshComponent* Mesh : GeometryPreviews)
	{
		if (Mesh && (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil
			|| Mesh->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default))
		{
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(Stencil);
			Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		}
	}
}

AActor* ASFHypertubeSupportPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
	AFGPoleHologram* Parent = Cast<AFGPoleHologram>(GetParentHologram());
	if ((!RecordedState.bCaptured && !SFSelectedPoleState::IsSelectedPole(Parent)) || !bGeometryReady) return nullptr;
	const FTransform Pose = GetActorTransform();
	const auto* ParentProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
	if (!ParentProperty) return nullptr;
	AFGHologram* Native = AFGHologram::SpawnHologramFromRecipe(GetRecipe(), GetOwner(), Pose.GetLocation(), GetConstructionInstigator(), [this, &Pose, ParentProperty](AFGHologram* Cell)
		{
			Cell->SetActorTransform(Pose);
			ParentProperty->SetObjectPropertyValue_InContainer(Cell, this);
			Cell->Tags.AddUnique(FName(RecordedState.bCaptured ? TEXT("SF_ExtendChild") : TEXT("SF_GridChild")));
		});
	if (!IsValid(Native)) return nullptr;
	Native->SetConstructionInstigator(GetConstructionInstigator());
	USFHologramDataService::MarkAsChild(Native, GetParentHologram(),
		RecordedState.bCaptured ? ESFChildHologramType::ExtendClone : ESFChildHologramType::ScalingGrid);
	if (RecordedState.bCaptured)
	{
		if (!SFSelectedPoleState::Apply(Native, RecordedState)) { Native->Destroy(); return nullptr; }
	}
	else SFSelectedPoleState::SyncNative(Parent, Native);
	Native->SetActorTransform(Pose);
	AFGBuildableBlueprintDesigner* Designer = Parent ? Parent->GetBlueprintDesigner() : GetBlueprintDesigner();
	if (Designer) Native->SetInsideBlueprintDesigner(Designer);
	AActor* Built = Native->Construct(OutChildren, ConstructionID);
	for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
	if (IsValid(Native)) Native->Destroy();
	return Built;
}

bool ASFHypertubeSupportPreviewHologram::InitializeRecordedState(const FSFSelectedPoleState& State)
{
	if (!State.bCaptured || !GetRecipe()) return false;
	const FTransform Pose = GetActorTransform();
	const auto* ParentProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
	if (!ParentProperty) return false;
	AFGHologram* Native = AFGHologram::SpawnHologramFromRecipe(GetRecipe(), GetOwner(), Pose.GetLocation(), GetConstructionInstigator(), [this, &Pose, ParentProperty](AFGHologram* Cell)
		{
			Cell->SetActorTransform(Pose);
			ParentProperty->SetObjectPropertyValue_InContainer(Cell, this);
			Cell->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
		});
	if (!IsValid(Native)) return false;
	const bool bApplied = SFSelectedPoleState::Apply(Native, State);
	if (bApplied) SyncGeometry(CastChecked<AFGPoleHologram>(Native));
	for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
	Native->Destroy();
	if (!bApplied || !bGeometryReady) return false;
	RecordedState = State;
	return true;
}
