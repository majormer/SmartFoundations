// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFHypertubeSupportHologramState.h"
#include "SmartFoundations.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Holograms/Logistics/SFSelectedPoleState.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Holograms/Logistics/SFHypertubeSupportPreviewHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Hologram/FGConveyorPoleHologram.h"
#include "UObject/UnrealType.h"

bool SFHypertubeSupportHologramState::IsSelectedPole(AFGHologram* Hologram)
{
 return SFSelectedPoleState::IsSelectedPole(Hologram);
}

bool SFHypertubeSupportHologramState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
 return SFSelectedPoleState::GetSelectedMeshes(Hologram, OutMeshes);
}

bool SFHypertubeSupportHologramState::IsFinalStep(AFGHologram* Hologram)
{
	if (!IsSelectedPole(Hologram)) return false;
	const FProperty* Step = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep"));
	EPoleHologramBuildStep Value = EPoleHologramBuildStep::PHBS_PlacementAndRotation;
	if (Step) Step->CopyCompleteValue(&Value, Step->ContainerPtrToValuePtr<void>(Hologram));
	return Value == EPoleHologramBuildStep::PHBS_AdjustHeight;
}

bool SFHypertubeSupportHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
	// [MP-AUTH] Preview actors on a remote client can have authority role.
	if (!IsSelectedPole(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
		|| Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		if (IsValid(Child) && Child->IsA<ASFHypertubeSupportPreviewHologram>() && !Child->IsDisabled()) return true;
	}
	return false;
}

void SFHypertubeSupportHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
	if (!IsSelectedPole(Parent) || !IsValid(Child) || Parent->GetBuildClass() != Child->GetBuildClass()) return;
	if (ASFHypertubeSupportPreviewHologram* Preview = Cast<ASFHypertubeSupportPreviewHologram>(Child))
	{
		Preview->SyncGeometry(CastChecked<AFGPoleHologram>(Parent));
		return;
	}
	SFSelectedPoleState::SyncNative(Parent, Child);
}

FVector SFHypertubeSupportHologramState::GridDimensions(const FVector& SelectedMeshDimensions,
    const FVector& ProfileDimensions, const FRotator& Rotation)
{
    // Default support spacing subtracts this registered horizontal footprint.
    // Using the narrower mesh here would shorten every repeated interval.
    return SFScalingFootprint::FromLocalDimensions(
        FVector(ProfileDimensions.X, ProfileDimensions.Y, SelectedMeshDimensions.Z), Rotation);
}

bool SFHypertubeSupportHologramState::TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions)
{
	if (!IsSelectedPole(Hologram) || Hologram->GetActorRotation().ContainsNaN()) return false;
	TArray<UStaticMeshComponent*> Meshes;
	if (!GetSelectedMeshes(Hologram, Meshes)) return false;
	FBox Bounds(ForceInit);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !Mesh->GetStaticMesh()) return false;
		const FTransform LocalPose = Mesh->GetComponentTransform().GetRelativeTransform(Hologram->GetActorTransform());
		Bounds += Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(LocalPose);
	}
	if (!Bounds.IsValid) return false;
    OutDimensions = GridDimensions(Bounds.GetSize(),
        USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass()).DefaultSize,
        Hologram->GetActorRotation());
	return !OutDimensions.ContainsNaN() && OutDimensions.GetMin() > 0.0;
}
