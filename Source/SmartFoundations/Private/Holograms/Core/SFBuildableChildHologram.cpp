// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "SmartFoundations.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FGRecipe.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"
#include "Features/Scaling/SFHypertubeEntranceState.h"

ASFBuildableChildHologram::ASFBuildableChildHologram()
{
	// Minimal constructor — configuration happens post-spawn via deferred construction
}

ASFBuildableChildHologram* ASFBuildableChildHologram::SpawnPreview(
	AFGHologram* Seed, const FTransform& Pose, ESFChildHologramType ChildType, FName ChildName,
	TSubclassOf<ASFBuildableChildHologram> PreviewClass)
{
	if (!IsValid(Seed) || !Seed->GetWorld() || !Seed->GetRecipe() || !Seed->GetBuildClass())
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Name = ChildName;
	Params.Owner = Seed->GetOwner();
	Params.Instigator = Seed->GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASFBuildableChildHologram* Preview = Seed->GetWorld()->SpawnActor<ASFBuildableChildHologram>(
		PreviewClass ? PreviewClass.Get() : StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
	if (!Preview)
	{
		return nullptr;
	}

	Preview->SetChildBuildClass(Seed->GetBuildClass());
	Preview->SetRecipe(Seed->GetRecipe());
	Preview->SetConstructionInstigator(Seed->GetConstructionInstigator());
	Preview->FinishSpawning(Pose);
	if (!ChildName.IsNone())
	{
		Seed->AddChild(Preview, ChildName);
	}
	USFHologramDataService::DisableValidation(Preview);
	USFHologramDataService::MarkAsChild(Preview, Seed, ChildType);
	Preview->SetActorHiddenInGame(false);
	Preview->SetActorEnableCollision(false);
	TArray<UPrimitiveComponent*> Primitives;
	Preview->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	TArray<UStaticMeshComponent*> Meshes;
	Preview->GetComponents(Meshes);
	for (UStaticMeshComponent* MeshComponent : Meshes)
	{
		const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
		if (Mesh && Mesh->GetName().Equals(TEXT("ClearanceBox")))
		{
			MeshComponent->SetVisibility(false);
			MeshComponent->SetHiddenInGame(true);
		}
	}
	Preview->SetActorTickEnabled(false);
	Preview->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	Preview->UpdateComponentTransforms();
	return Preview;
}

void ASFBuildableChildHologram::CheckValidPlacement()
{
	// Issue #200: Always valid — children are positioned by Smart! grid calculations.
	// Parent's CheckValidPlacement iterates mChildren and calls this on each child.
	// Without this override, children of ceiling lights would fail "Must be built on a ceiling"
	// and children of wall floodlights would fail "Must snap to a Wall or similar".
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFBuildableChildHologram::CheckClearance()
{
	// Issue #200: Skip clearance checks — children don't need independent clearance validation.
	// Smart! positions children via SetActorLocation; vanilla clearance detection would
	// add false "Encroaching another object's clearance" disqualifiers.
}

void ASFBuildableChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// Block parent from repositioning this child — Smart! handles positioning via SetActorLocation.
	// Without this, the parent's tick would reset children to the hit result position.
}

AActor* ASFBuildableChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	UE_LOG(LogSmartHologram, Verbose, TEXT("SFBuildableChildHologram::Construct: Building %s from %s"),
		mBuildClass ? *mBuildClass->GetName() : TEXT("NULL"), *GetName());

	// [MP-SLICE0] TEMP multiplayer instrumentation — remove before release.
	// Server-side per-child signal: if this fires with HasAuthority=1 on a CLIENT-initiated
	// build, the previewed child round-tripped to the server. NetMode: 0=Standalone
	// 1=DedicatedServer 2=ListenServer 3=Client.
	{
		const int32 NetMode = GetWorld() ? (int32)GetWorld()->GetNetMode() : -1;
		UE_LOG(LogSmartHologram, Verbose,
			TEXT("[MP-SLICE0] BuildableChild::Construct: %s build=%s NetMode=%d HasAuthority=%d"),
			*GetName(), mBuildClass ? *mBuildClass->GetName() : TEXT("NULL"), NetMode, HasAuthority() ? 1 : 0);
	}

	// A request-validated native entrance owns its ConfigureComponents snap handoff.
	// Unattached cells and every other generic buildable retain their ordinary path.
	AFGHologram* NativeCell = SFHypertubeEntranceState::FindPreparedCell(this);
	AActor* BuiltActor = IsValid(NativeCell)
		? NativeCell->Construct(out_children, constructionID)
		: Super::Construct(out_children, constructionID);

	if (BuiltActor)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("SFBuildableChildHologram::Construct: Successfully built %s"),
			*BuiltActor->GetName());
	}
	else
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("SFBuildableChildHologram::Construct: Construct returned nullptr for %s"),
			*GetName());
	}

	return BuiltActor;
}

void ASFBuildableChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}

void ASFBuildableChildHologram::SetHologramNudgeLocation()
{
	// [#497] Vanilla's locked-parent placement path (UFGBuildGunStateBuild::TickState ->
	// AFGHologram::UpdateHologramPlacement (FGHologram.cpp:440) -> SetHologramNudgeLocation
	// (FGHologram.cpp:2120)) cascades through mChildren with a PLAIN SetActorLocation of
	// lock-location + nudge offset - bypassing the SetHologramLocationAndRotation no-op entirely.
	// Extend locks its parent, children never capture a lock location (ZeroVector), so the cascade
	// dragged every child to world origin every tick (caught by the #497 origin-trap stack dump).
	// Smart owns this child's transform; parent nudges are propagated by Smart's own
	// transform-change follow. No-op, mirroring the #418 drift contract.
}
