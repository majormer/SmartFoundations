// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFBeamHologramState.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFBeamPreviewHologram.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGBeamHologram.h"
#include "FGBuildableBeam.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

bool SFBeamHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
	// [MP-AUTH] Preview actors can have ROLE_Authority on clients, so check world mode.
	if (!IsValid(Parent) || !Parent->IsA<AFGBeamHologram>() || Parent->GetParentHologram()
		|| !Parent->GetWorld() || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		if (IsValid(Child) && Child->IsA<ASFBeamPreviewHologram>() && !Child->IsDisabled()) return true;
	}
	return false;
}

void SFBeamHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
	if (!IsValid(Parent) || !IsValid(Child) || !Parent->IsA<AFGBeamHologram>()) return;
	if (ASFBeamPreviewHologram* Preview = Cast<ASFBeamPreviewHologram>(Child))
	{
		Preview->SyncGeometry(Cast<AFGBeamHologram>(Parent));
		return;
	}
	if (!Child->IsA<AFGBeamHologram>()) return;
	// Copy supported modes without invoking placement, which would overwrite the Smart transform.
	for (const TCHAR* Name : {TEXT("mDefaultBuildMode"),
		TEXT("mBuildModeDiagonal"), TEXT("mBuildModeFreeForm")})
	{
		if (FProperty* Property = FindFProperty<FProperty>(Parent->GetClass(), Name))
		{
			Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Child),
				Property->ContainerPtrToValuePtr<void>(Parent));
		}
	}
	// An active player's mode can come from the build gun rather than an override.
	// Children/templates are not the gun's active hologram and need an explicit mode.
	TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
	Parent->GetSupportedBuildModes(Modes);
	const FClassProperty* Override = FindFProperty<FClassProperty>(AFGHologram::StaticClass(), TEXT("mBuildModeOverride"));
	TSubclassOf<UFGHologramBuildModeDescriptor> SelectedMode = Override
		? Cast<UClass>(Override->GetObjectPropertyValue_InContainer(Parent)) : nullptr;
	for (const auto& Mode : Modes)
	{
		if (Mode && Mode->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
		{
			TSubclassOf<UFGHologramBuildModeDescriptor> HologramMode = Mode.Get();
			if (Parent->IsCurrentBuildMode(HologramMode))
			{
				SelectedMode = HologramMode;
				break;
			}
		}
	}
	if (Override && Override->GetObjectPropertyValue_InContainer(Child) != SelectedMode.Get())
	{
		Child->SetBuildModeOverride(SelectedMode);
	}
	// A mode change may reset the native placement step. Restore the parent's step last.
	if (FProperty* Step = FindFProperty<FProperty>(AFGBeamHologram::StaticClass(), TEXT("mBuildStep")))
	{
		Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Child),
			Step->ContainerPtrToValuePtr<void>(Parent));
	}
	if (FFloatProperty* Length = FindFProperty<FFloatProperty>(AFGBeamHologram::StaticClass(), TEXT("mCurrentLength")))
	{
		const float Value = Length->GetPropertyValue_InContainer(Parent);
		if (FMath::IsFinite(Value) && Value > 0.0f
			&& Length->GetPropertyValue_InContainer(Child) != Value)
		{
			Length->SetPropertyValue_InContainer(Child, Value);
			if (UFunction* Refresh = Child->FindFunction(TEXT("OnRep_CurrentLength")))
			{
				Child->ProcessEvent(Refresh, nullptr);
			}
		}
	}
}

bool SFBeamHologramState::TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions)
{
	if (!IsValid(Hologram) || !Hologram->GetBuildClass()) return false;
	FVector LocalDimensions;
	if (Hologram->IsA<AFGBeamHologram>())
	{
		const AFGBuildableBeam* Beam = Cast<AFGBuildableBeam>(Hologram->GetBuildClass()->GetDefaultObject());
		const FFloatProperty* Length = FindFProperty<FFloatProperty>(AFGBeamHologram::StaticClass(), TEXT("mCurrentLength"));
		if (!Beam || !Length) return false;
		const float Value = Length->GetPropertyValue_InContainer(Hologram);
		const float Width = Beam->GetSize();
		if (!FMath::IsFinite(Value) || Value <= 0.0f || !FMath::IsFinite(Width) || Width <= 0.0f) return false;
		LocalDimensions = FVector(Value, Width, Width);
	}
	else
	{
		const FName BuildName = Hologram->GetBuildClass()->GetFName();
		static const FName ConnectorName(TEXT("Build_Beam_Connector_C"));
		static const FName DoubleConnectorName(TEXT("Build_Beam_Connector_Double_C"));
		static const FName SupportName(TEXT("Build_Beam_Support_C"));
		if (BuildName != ConnectorName && BuildName != DoubleConnectorName && BuildName != SupportName) return false;
		LocalDimensions = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass()).DefaultSize;
	}
	if (Hologram->GetActorRotation().ContainsNaN()) return false;
	OutDimensions = SFScalingFootprint::FromLocalDimensions(LocalDimensions, Hologram->GetActorRotation());
	return true;
}
