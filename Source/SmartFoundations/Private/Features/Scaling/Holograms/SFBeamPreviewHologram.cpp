// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFBeamPreviewHologram.h"
#include "Features/Scaling/SFBeamHologramState.h"
#include "Components/StaticMeshComponent.h"
#include "Hologram/FGBeamHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — beam preview materialization
//  1. Local authority: Construct briefly materializes a native recipe hologram.
//  2. Remote client: existing compact spec expansion constructs native cells on authority.
//  3. Developer-only legacy client: dispatch retains the original native child tree.
void ASFBeamPreviewHologram::SyncGeometry(AFGBeamHologram* Parent)
{
	bGeometryReady = false;
	if (!IsValid(Parent) || Parent->GetBuildClass() != GetBuildClass()) return;
	const FFloatProperty* Length = FindFProperty<FFloatProperty>(AFGBeamHologram::StaticClass(), TEXT("mCurrentLength"));
	const FObjectPropertyBase* Main = FindFProperty<FObjectPropertyBase>(AFGBeamHologram::StaticClass(), TEXT("mBeamMesh"));
	const float CurrentLength = Length ? Length->GetPropertyValue_InContainer(Parent) : 0.0f;
	UStaticMeshComponent* MainMesh = Main ? Cast<UStaticMeshComponent>(Main->GetObjectPropertyValue_InContainer(Parent)) : nullptr;
	if (!FMath::IsFinite(CurrentLength) || CurrentLength <= 0.0f || !IsValid(MainMesh) || !MainMesh->GetStaticMesh()) return;
	NativeCostMultiplier = Parent->GetBaseCostMultiplier();
	if (NativeCostMultiplier <= 0) return;

	// Copy only the native length geometry. Arrows, clearance, guidelines and the native
	// Zoop visualization are not beam geometry and must not be duplicated on every cell.
	TArray<UStaticMeshComponent*> Sources{MainMesh};
	if (const FArrayProperty* Generated = FindFProperty<FArrayProperty>(AFGBeamHologram::StaticClass(), TEXT("mGeneratedComponents")))
	{
		if (const FObjectPropertyBase* Item = CastField<FObjectPropertyBase>(Generated->Inner))
		{
			FScriptArrayHelper Array(Generated, Generated->ContainerPtrToValuePtr<void>(Parent));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Item->GetObjectPropertyValue(Array.GetRawPtr(Index)));
				if (IsValid(Mesh) && Mesh->GetStaticMesh()) Sources.AddUnique(Mesh);
			}
		}
	}
	if (!bDefaultMeshesHidden)
	{
		TArray<UStaticMeshComponent*> Defaults;
		GetComponents(Defaults);
		UStaticMeshComponent* Reusable = nullptr;
		for (UStaticMeshComponent* Mesh : Defaults)
		{
			// The recipe already supplied the main mesh. Reuse it instead of retaining
			// a hidden copy and allocating another component for every preview cell.
			if (!Reusable && Mesh->GetClass() == UStaticMeshComponent::StaticClass()
				&& Mesh->GetStaticMesh() == MainMesh->GetStaticMesh()) Reusable = Mesh;
			Mesh->SetVisibility(false);
			Mesh->SetHiddenInGame(true);
		}
		if (Reusable) GeometryPreviews.Add(Reusable);
		bDefaultMeshesHidden = true;
	}
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		UStaticMeshComponent* Source = Sources[Index];
		if (!GeometryPreviews.IsValidIndex(Index))
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
		UStaticMeshComponent* Mesh = GeometryPreviews[Index];
		if (Mesh->GetStaticMesh() != Source->GetStaticMesh()) Mesh->SetStaticMesh(Source->GetStaticMesh());
		const FTransform LocalPose = Source->GetComponentTransform().GetRelativeTransform(Parent->GetActorTransform());
		if (!Mesh->GetRelativeTransform().Equals(LocalPose, 0.01f)) Mesh->SetRelativeTransform(LocalPose);
		for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
		{
			if (Mesh->GetMaterial(Slot) != Source->GetMaterial(Slot)) Mesh->SetMaterial(Slot, Source->GetMaterial(Slot));
		}
		Mesh->SetVisibility(Source->IsVisible());
		Mesh->SetHiddenInGame(Source->bHiddenInGame);
	}
	for (int32 Index = Sources.Num(); Index < GeometryPreviews.Num(); ++Index)
	{
		GeometryPreviews[Index]->SetVisibility(false);
		GeometryPreviews[Index]->SetHiddenInGame(true);
	}
	ApplyGeometryMaterialState(GetHologramMaterialState());
	bGeometryReady = true;
}

int32 ASFBeamPreviewHologram::GetBaseCostMultiplier() const
{
	return NativeCostMultiplier;
}

void ASFBeamPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
	Super::SetPlacementMaterialState(MaterialState);
	ApplyGeometryMaterialState(MaterialState);
}

void ASFBeamPreviewHologram::ApplyGeometryMaterialState(EHologramMaterialState MaterialState)
{
	const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
	for (UStaticMeshComponent* Mesh : GeometryPreviews)
	{
		if (!Mesh->bRenderCustomDepth || Mesh->CustomDepthStencilValue != Stencil
			|| Mesh->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default)
		{
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(Stencil);
			Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		}
	}
}

AActor* ASFBeamPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
	AFGBeamHologram* Parent = Cast<AFGBeamHologram>(GetParentHologram());
	if (!IsValid(Parent) || !bGeometryReady) return nullptr;
	const FTransform Pose = GetActorTransform();
	AFGHologram* Native = AFGHologram::SpawnChildHologramFromRecipe(this,
		TEXT("NativeBeamConstruction"), GetRecipe(), GetOwner(), Pose.GetLocation(), [&Pose](AFGHologram* Cell)
		{
			Cell->SetActorTransform(Pose);
			Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
		});
	if (!IsValid(Native)) return nullptr;
	Native->SetConstructionInstigator(GetConstructionInstigator());
	USFHologramDataService::MarkAsChild(Native, Parent, ESFChildHologramType::ScalingGrid);
	SFBeamHologramState::Sync(Parent, Native);
	Native->SetActorTransform(Pose);
	if (Parent->GetBlueprintDesigner()) Native->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
	AActor* Built = Native->Construct(OutChildren, ConstructionID);
	for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
	if (IsValid(Native)) Native->Destroy();
	return Built;
}
