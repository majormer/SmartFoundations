// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Holograms/Logistics/SFSelectedPoleState.h"
#include "Components/StaticMeshComponent.h"
#include "Buildables/FGBuildablePole.h"
#include "FGBuildablePolePipe.h"
#include "Hologram/FGConveyorPoleHologram.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "UObject/UnrealType.h"

bool SFSelectedPoleState::IsSelectedPole(AFGHologram* Hologram)
{
	if (!IsValid(Hologram) || !Hologram->GetBuildClass()) return false;
	const FName Name = Hologram->GetBuildClass()->GetFName();
	if (Name == TEXT("Build_ConveyorPole_C")) return Hologram->IsA<AFGConveyorPoleHologram>();
	return Hologram->IsA<AFGPipelinePoleHologram>()
		&& (Name == TEXT("Build_PipelineSupport_C") || Name == TEXT("Build_PipeHyperSupport_C"));
}

bool SFSelectedPoleState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
	OutMeshes.Reset();
	if (!IsSelectedPole(Hologram)) return false;
	OutMeshes.Add(CastChecked<AFGPoleHologram>(Hologram)->GetPoleMesh());
	if (Hologram->IsA<AFGPipelinePoleHologram>())
	{
		const FObjectPropertyBase* Top = FindFProperty<FObjectPropertyBase>(AFGPipelinePoleHologram::StaticClass(), TEXT("mSupportTopPartMeshComponent"));
		OutMeshes.Add(Top ? Cast<UStaticMeshComponent>(Top->GetObjectPropertyValue_InContainer(Hologram)) : nullptr);
	}
	for (UStaticMeshComponent* Mesh : OutMeshes) if (!IsValid(Mesh) || !Mesh->GetStaticMesh()) return false;
	return true;
}

void SFSelectedPoleState::SyncNative(AFGHologram* Parent, AFGHologram* Child)
{
	if (!IsSelectedPole(Parent) || !IsSelectedPole(Child) || Parent->GetBuildClass() != Child->GetBuildClass()) return;
	const FTransform Pose = Child->GetActorTransform();
	for (const TCHAR* Name : {TEXT("mBuildStep"), TEXT("mPoleVariationIndex")})
		if (FProperty* Property = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), Name))
			Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Child), Property->ContainerPtrToValuePtr<void>(Parent));
	if (Parent->IsA<AFGPipelinePoleHologram>() && Child->IsA<AFGPipelinePoleHologram>())
		if (FProperty* Angle = FindFProperty<FProperty>(AFGPipelinePoleHologram::StaticClass(), TEXT("mVerticalAngle")))
			Angle->CopyCompleteValue(Angle->ContainerPtrToValuePtr<void>(Child), Angle->ContainerPtrToValuePtr<void>(Parent));
	for (const TCHAR* Name : {TEXT("OnRep_PoleVariationIndex"), TEXT("OnRep_VerticalAngle")})
		if (UFunction* Refresh = Child->FindFunction(Name)) Child->ProcessEvent(Refresh, nullptr);
	Child->SetActorTransform(Pose);
}

bool SFSelectedPoleState::Capture(AFGBuildable* Buildable, FSFSelectedPoleState& OutState)
{
	OutState = FSFSelectedPoleState();
	AFGBuildablePole* Pole = Cast<AFGBuildablePole>(Buildable);
	if (!IsValid(Pole)) return false;
	const FName Name = Pole->GetClass()->GetFName();
	if (Name != TEXT("Build_ConveyorPole_C") && Name != TEXT("Build_PipelineSupport_C") && Name != TEXT("Build_PipeHyperSupport_C")) return false;
	if (!Pole->GetPoleVariations().IsValidIndex(Pole->mSelectedPoleVersion) || !FMath::IsFinite(Pole->mHeight) || Pole->mHeight <= 0) return false;
	OutState.Height = Pole->mHeight;
	OutState.Variation = Pole->mSelectedPoleVersion;
	if (AFGBuildablePolePipe* Pipe = Cast<AFGBuildablePolePipe>(Pole)) OutState.VerticalAngle = Pipe->mVerticalAngle;
	OutState.bCaptured = FMath::IsFinite(OutState.VerticalAngle);
	return OutState.bCaptured;
}

bool SFSelectedPoleState::Apply(AFGHologram* Hologram, const FSFSelectedPoleState& State)
{
	if (!State.bCaptured || !IsSelectedPole(Hologram) || !FMath::IsFinite(State.Height)
		|| !FMath::IsFinite(State.VerticalAngle) || State.Height <= 0) return false;
	const AFGBuildablePole* CDO = Cast<AFGBuildablePole>(Hologram->GetBuildClass()->GetDefaultObject());
	if (!CDO || !CDO->GetPoleVariations().IsValidIndex(State.Variation)) return false;
	FIntProperty* Variation = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex"));
	UFunction* Refresh = Hologram->FindFunction(TEXT("OnRep_PoleVariationIndex"));
	if (!Variation || !Refresh) return false;
	// Use an existing native variation, never synthesize a mesh or an arbitrary height.
	Variation->SetPropertyValue_InContainer(Hologram, State.Variation);
	Hologram->ProcessEvent(Refresh, nullptr);
	if (!FMath::IsNearlyEqual(CastChecked<AFGPoleHologram>(Hologram)->GetPoleHeight(), State.Height, 0.1f)) return false;
	if (AFGPipelinePoleHologram* Pipe = Cast<AFGPipelinePoleHologram>(Hologram))
	{
		Pipe->SetVerticalAngle(State.VerticalAngle);
		if (!FMath::IsNearlyEqual(Pipe->GetVerticalAngle(), State.VerticalAngle, 0.01f)) return false;
	}
	else if (!FMath::IsNearlyZero(State.VerticalAngle)) return false;
	return true;
}
