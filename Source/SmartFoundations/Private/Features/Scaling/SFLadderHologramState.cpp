// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/SFLadderHologramState.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Features/Scaling/Holograms/SFLadderPreviewHologram.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGLadderHologram.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

bool SFLadderHologramState::IsFinalStep(AFGHologram* Hologram)
{
	if (!IsValid(Hologram) || !Hologram->IsA<AFGLadderHologram>()) return false;
	const FProperty* Step = FindFProperty<FProperty>(AFGLadderHologram::StaticClass(), TEXT("mBuildStep"));
	ELadderHologramBuildStep Value = ELadderHologramBuildStep::LHBS_SnapToActor;
	if (Step) Step->CopyCompleteValue(&Value, Step->ContainerPtrToValuePtr<void>(Hologram));
	return Value == ELadderHologramBuildStep::LHBS_AdjustHeight;
}

bool SFLadderHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
	// [MP-AUTH] Client preview actors can have authority role, so also check net mode.
	if (!IsValid(Parent) || !Parent->IsA<AFGLadderHologram>() || Parent->GetParentHologram()
		|| !Parent->GetWorld() || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		if (IsValid(Child) && Child->IsA<ASFLadderPreviewHologram>() && !Child->IsDisabled()) return true;
	}
	return false;
}

void SFLadderHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
	if (!IsValid(Parent) || !IsValid(Child) || !Parent->IsA<AFGLadderHologram>()) return;
	if (ASFLadderPreviewHologram* Preview = Cast<ASFLadderPreviewHologram>(Child))
	{
		Preview->SyncGeometry(Cast<AFGLadderHologram>(Parent));
		return;
	}
	if (!Child->IsA<AFGLadderHologram>()) return;
	const FTransform Pose = Child->GetActorTransform();
	if (FProperty* Step = FindFProperty<FProperty>(AFGLadderHologram::StaticClass(), TEXT("mBuildStep")))
	{
		Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Child), Step->ContainerPtrToValuePtr<void>(Parent));
	}
	if (FIntProperty* Height = FindFProperty<FIntProperty>(AFGLadderHologram::StaticClass(), TEXT("mTargetSegmentHeight")))
	{
		const int32 Value = Height->GetPropertyValue_InContainer(Parent);
		if (Value >= 0 && Height->GetPropertyValue_InContainer(Child) != Value)
		{
			Height->SetPropertyValue_InContainer(Child, Value);
			if (UFunction* Refresh = Child->FindFunction(TEXT("OnRep_TargetSegmentHeight"))) Child->ProcessEvent(Refresh, nullptr);
		}
	}
	Child->SetActorTransform(Pose);
}

bool SFLadderHologramState::TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions)
{
	if (!IsValid(Hologram) || !Hologram->IsA<AFGLadderHologram>() || !Hologram->GetBuildClass()
		|| Hologram->GetActorRotation().ContainsNaN()) return false;
	const FIntProperty* Height = FindFProperty<FIntProperty>(AFGLadderHologram::StaticClass(), TEXT("mTargetSegmentHeight"));
	const FFloatProperty* Segment = FindFProperty<FFloatProperty>(AFGLadderHologram::StaticClass(), TEXT("mMeshHeight"));
	if (!Height || !Segment) return false;
	const int32 Target = Height->GetPropertyValue_InContainer(Hologram);
	const float SegmentHeight = Segment->GetPropertyValue_InContainer(Hologram);
	if (Target < 0 || !FMath::IsFinite(SegmentHeight) || SegmentHeight <= 0.0f) return false;
	FVector LocalDimensions = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass()).DefaultSize;
	// Native ladders snap by functional segment height; their upper handrails overlap
	// the following segment. Counting the decorative overhang would introduce a climb gap.
	LocalDimensions.Z = (static_cast<double>(Target) + 1.0) * SegmentHeight;
	OutDimensions = SFScalingFootprint::FromLocalDimensions(LocalDimensions, Hologram->GetActorRotation());
	return !OutDimensions.ContainsNaN();
}
