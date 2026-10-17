// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFSignPreviewHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Buildables/FGBuildablePole.h"
#include "Hologram/FGSignPoleHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Holograms/Core/SFSignHologramState.h"
#include "Subsystem/SFHologramDataService.h"

namespace
{
const FName InvalidSupport(TEXT("SF_SignGridSupportInvalid"));
}

// SP/MP DIVERGENCE MAP — sign preview materialization
//  1. Local authority: proxy Construct materializes a standalone native cell;
//     its cached support choice survives suppression of the parent's own pole.
//  2. Staged client: existing authority expansion materializes native recipe cells.
//  3. Legacy client: dispatch retains native children and existing serialization.
void ASFSignPreviewHologram::SyncSupport(AFGSignPoleHologram* Template, bool bWanted,
	const FTransform& PolePose, double Height)
{
	bSupportWanted = bWanted;
	SupportPose = PolePose;
	SupportHeight = Height;
	bSupportStateReady = true;
	SupportCost = bWanted && IsValid(Template) ? Template->GetCost(false) : TArray<FItemAmount>();
	bSupportValid = !bWanted;
	const FPoleVariation* Variation = nullptr;
	if (bWanted && IsValid(Template) && FMath::IsFinite(Height) && Height > 0.0)
	{
		const AFGBuildablePole* Defaults = Template->GetBuildClass()
			? Cast<AFGBuildablePole>(Template->GetBuildClass()->GetDefaultObject()) : nullptr;
		Variation = Defaults ? Defaults->GetBestPoleVariationForHeight(static_cast<float>(Height)) : nullptr;
		bSupportValid = Variation && Variation->StaticMesh
			&& FMath::IsNearlyEqual(static_cast<double>(Variation->Height), Height, 0.1);
	}
	if (bSupportValid) Tags.Remove(InvalidSupport);
	else Tags.AddUnique(InvalidSupport);

	UStaticMeshComponent* TemplateMesh = IsValid(Template) ? Template->GetPoleMesh() : nullptr;
	if (bWanted && bSupportValid && TemplateMesh)
	{
		if (!SupportPreview)
		{
			SupportPreview = NewObject<UStaticMeshComponent>(this, TEXT("SignSupportPreview"));
			SupportPreview->SetupAttachment(GetRootComponent());
			SupportPreview->SetMobility(EComponentMobility::Movable);
			SupportPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SupportPreview->SetComponentTickEnabled(false);
			SupportPreview->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
			AddInstanceComponent(SupportPreview);
			SupportPreview->RegisterComponent();
		}
		if (SupportPreview->GetStaticMesh() != Variation->StaticMesh)
			SupportPreview->SetStaticMesh(Variation->StaticMesh);
		const FTransform MeshPose = TemplateMesh->GetRelativeTransform() * PolePose;
		if (!SupportPreview->GetComponentTransform().Equals(MeshPose, 0.01f))
			SupportPreview->SetWorldTransform(MeshPose);
		for (int32 Index = 0; Index < TemplateMesh->GetNumMaterials(); ++Index)
		{
			if (SupportPreview->GetMaterial(Index) != TemplateMesh->GetMaterial(Index))
				SupportPreview->SetMaterial(Index, TemplateMesh->GetMaterial(Index));
		}
		SupportPreview->SetVisibility(true);
		SupportPreview->SetHiddenInGame(false);
		ApplySupportMaterialState(GetHologramMaterialState());
	}
	else if (SupportPreview)
	{
		SupportPreview->SetVisibility(false);
		SupportPreview->SetHiddenInGame(true);
	}
}

void ASFSignPreviewHologram::SetPlacementMaterialState(EHologramMaterialState MaterialState)
{
	Super::SetPlacementMaterialState(MaterialState);
	ApplySupportMaterialState(MaterialState);
}

void ASFSignPreviewHologram::ApplySupportMaterialState(EHologramMaterialState MaterialState)
{
	if (!SupportPreview) return;
	// Dynamic meshes need the same stencil treatment as native hologram meshes.
	// Keep the pole's own material and let the hologram post-process tint it.
	const uint8 Stencil = GetStencilForHologramMaterialState(MaterialState);
	if (!SupportPreview->bRenderCustomDepth
		|| SupportPreview->CustomDepthStencilValue != Stencil
		|| SupportPreview->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default)
	{
		SupportPreview->SetRenderCustomDepth(true);
		SupportPreview->SetCustomDepthStencilValue(Stencil);
		SupportPreview->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		SupportPreview->MarkRenderStateDirty();
	}
}

TArray<FItemAmount> ASFSignPreviewHologram::GetCost(bool bIncludeChildren) const
{
	TArray<FItemAmount> Cost = Super::GetCost(bIncludeChildren);
	if (bIncludeChildren)
	{
		for (const FItemAmount& Item : SupportCost)
		{
			FItemAmount* Existing = Cost.FindByPredicate(
				[&Item](const FItemAmount& Entry) { return Entry.ItemClass == Item.ItemClass; });
			if (Existing) Existing->Amount += Item.Amount;
			else Cost.Add(Item);
		}
	}
	return Cost;
}

AActor* ASFSignPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
	// [MP-AUTH] Local construction needs native ConfigureChildActor to assign poles.
	// Attach the materialized cell to this proxy, not the native root being iterated.
	// Its parent also prevents it from looking up another root's staged request.
	AFGStandaloneSignHologram* Parent = Cast<AFGStandaloneSignHologram>(GetParentHologram());
	if (!IsValid(Parent) || !bSupportStateReady || !bSupportValid) return nullptr;
	const FTransform Pose = GetActorTransform();
	AFGHologram* Materialized = AFGHologram::SpawnChildHologramFromRecipe(this,
		TEXT("NativeSignConstruction"), GetRecipe(), GetOwner(), Pose.GetLocation(), [&Pose](AFGHologram* Cell)
		{
			Cell->SetActorTransform(Pose);
			Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
		});
	if (!IsValid(Materialized)) return nullptr;
	Materialized->SetConstructionInstigator(GetConstructionInstigator());
	USFHologramDataService::MarkAsChild(Materialized, Parent, ESFChildHologramType::ScalingGrid);
	Materialized->SetActorTransform(Pose);
	if (Parent->GetBlueprintDesigner()) Materialized->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
	AActor* Built = nullptr;
	if (SFSignHologramState::ApplyProxyConstruction(Parent, Materialized,
		bSupportWanted, SupportHeight, SupportPose))
	{
		Built = Materialized->Construct(OutChildren, ConstructionID);
	}
	const TArray<AFGHologram*> NativeChildren = Materialized->GetHologramChildren();
	for (AFGHologram* Child : NativeChildren) if (IsValid(Child)) Child->Destroy();
	if (IsValid(Materialized)) Materialized->Destroy();
	return Built;
}
