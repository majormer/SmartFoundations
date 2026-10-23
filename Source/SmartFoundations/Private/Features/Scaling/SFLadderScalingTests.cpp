// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFLadderHologramState.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGLadderHologram.h"
#include "Buildables/FGBuildableLadder.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLadderSelectedHeightTest, "SmartFoundations.Scaling.LadderSelectedHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFLadderSelectedHeightTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;
	AFGLadderHologram* Parent = World->SpawnActor<AFGLadderHologram>();
	AFGLadderHologram* Child = World->SpawnActor<AFGLadderHologram>();
	FIntProperty* Height = FindFProperty<FIntProperty>(AFGLadderHologram::StaticClass(), TEXT("mTargetSegmentHeight"));
	FProperty* Step = FindFProperty<FProperty>(AFGLadderHologram::StaticClass(), TEXT("mBuildStep"));
	if (!TestNotNull(TEXT("Parent"), Parent) || !TestNotNull(TEXT("Child"), Child)
		|| !TestNotNull(TEXT("Native selected-height field"), Height) || !TestNotNull(TEXT("Native step field"), Step))
	{
		World->DestroyWorld(false); return false;
	}
	// Native CustomSerialization is a UHT extension, not ordinary retained metadata.
	// Replication flags are reflected here; construction-message parity is tested in game.
	TestTrue(TEXT("Selected height remains a replicated native field"), Height->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("Placement step remains a replicated native field"), Step->HasAnyPropertyFlags(CPF_Net));
	const ELadderHologramBuildStep InitialStep = ELadderHologramBuildStep::LHBS_SnapToActor;
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &InitialStep);
	TestFalse(TEXT("The first click must not stage or suppress the preview tree"), SFLadderHologramState::IsFinalStep(Parent));
	const ELadderHologramBuildStep FinalStep = ELadderHologramBuildStep::LHBS_AdjustHeight;
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &FinalStep);
	const FTransform Authored(FRotator(0, 35, 0), FVector(1200, -700, 90));
	Child->SetActorTransform(Authored);
	Height->SetPropertyValue_InContainer(Parent, 3);
	SFLadderHologramState::Sync(Parent, Child);
	TestTrue(TEXT("Height selection reaches the construction step"), SFLadderHologramState::IsFinalStep(Parent));
	TestEqual(TEXT("Existing cell retains four selected segments"), Height->GetPropertyValue_InContainer(Child), 3);
	TestTrue(TEXT("Child receives the construction step"), SFLadderHologramState::IsFinalStep(Child));
	TestTrue(TEXT("Native refresh preserves Smart's authored pose"), Child->GetActorTransform().Equals(Authored));
	Height->SetPropertyValue_InContainer(Parent, 1);
	AFGLadderHologram* Added = World->SpawnActor<AFGLadderHologram>();
	SFLadderHologramState::Sync(Parent, Child);
	SFLadderHologramState::Sync(Parent, Added);
	TestEqual(TEXT("Shrink updates an existing cell"), Height->GetPropertyValue_InContainer(Child), 1);
	TestEqual(TEXT("Grid growth inherits the current height rather than defaults"), Height->GetPropertyValue_InContainer(Added), 1);
	TestEqual(TEXT("Synchronization does not alter the parent"), Height->GetPropertyValue_InContainer(Parent), 1);
	TestTrue(TEXT("Ladder scaling is enabled"), USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_Ladder_C")).bSupportsScaling);
	World->DestroyWorld(false);
	return true;
}
#endif
