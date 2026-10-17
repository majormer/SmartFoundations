// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFHypertubeSupportHologramState.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Engine/World.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Hologram/FGConveyorPoleHologram.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHypertubeSupportStateTest, "SmartFoundations.Scaling.HypertubeSupportSelectedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFHypertubeSupportStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;
	AFGPipelinePoleHologram* Parent = World->SpawnActor<AFGPipelinePoleHologram>();
	AFGPipelinePoleHologram* Child = World->SpawnActor<AFGPipelinePoleHologram>();
	FClassProperty* Build = FindFProperty<FClassProperty>(AFGHologram::StaticClass(), TEXT("mBuildClass"));
	FIntProperty* Height = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex"));
	FFloatProperty* Angle = FindFProperty<FFloatProperty>(AFGPipelinePoleHologram::StaticClass(), TEXT("mVerticalAngle"));
	FProperty* Step = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep"));
	if (!TestNotNull(TEXT("Parent"), Parent) || !TestNotNull(TEXT("Child"), Child)
		|| !TestNotNull(TEXT("Build identity"), Build) || !TestNotNull(TEXT("Height"), Height)
		|| !TestNotNull(TEXT("Angle"), Angle) || !TestNotNull(TEXT("Step"), Step))
	{
		World->DestroyWorld(false); return false;
	}
	// These named classes are identity tokens only; no CDO/construction behavior is
	// inferred from SDK stubs. Real meshes, charging and network decoding are tested in game.
	UPackage* IdentityOuter = CreatePackage(TEXT("/Temp/SFHypertubeSupportSelectedState"));
	UClass* Floor = NewObject<UClass>(IdentityOuter, TEXT("Build_PipeHyperSupport_C"));
	UClass* Wall = NewObject<UClass>(IdentityOuter, TEXT("Build_HyperTubeWallSupport_C"));
	Floor->SetSuperStruct(AActor::StaticClass());
	Wall->SetSuperStruct(AActor::StaticClass());
	Build->SetObjectPropertyValue_InContainer(Parent, Floor);
	Build->SetObjectPropertyValue_InContainer(Child, Floor);
	TestTrue(TEXT("Floor support receives the dynamic contract"), SFHypertubeSupportHologramState::IsSelectedPole(Parent));
	TestTrue(TEXT("Height remains a replicated native field"), Height->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("Angle remains a replicated native field"), Angle->HasAnyPropertyFlags(CPF_Net));
	const EPoleHologramBuildStep Initial = EPoleHologramBuildStep::PHBS_PlacementAndRotation;
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &Initial);
	TestFalse(TEXT("First click must preserve previews for height selection"), SFHypertubeSupportHologramState::IsFinalStep(Parent));
	const EPoleHologramBuildStep Final = EPoleHologramBuildStep::PHBS_AdjustHeight;
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &Final);
	Height->SetPropertyValue_InContainer(Parent, 2);
	Angle->SetPropertyValue_InContainer(Parent, 45.0f);
	const FTransform Pose(FRotator(0, 35, 0), FVector(1200, -700, 90));
	Child->SetActorTransform(Pose);
	SFHypertubeSupportHologramState::Sync(Parent, Child);
	TestTrue(TEXT("Final height step is copied"), SFHypertubeSupportHologramState::IsFinalStep(Child));
	TestEqual(TEXT("Selected native variation is copied"), Height->GetPropertyValue_InContainer(Child), 2);
	TestEqual(TEXT("Tilted connection state is copied"), Angle->GetPropertyValue_InContainer(Child), 45.0f);
	TestTrue(TEXT("Native refresh retains authored grid pose"), Child->GetActorTransform().Equals(Pose));
	Height->SetPropertyValue_InContainer(Parent, 0);
	Angle->SetPropertyValue_InContainer(Parent, -22.5f);
	SFHypertubeSupportHologramState::Sync(Parent, Child);
	TestEqual(TEXT("Existing cell shrinks"), Height->GetPropertyValue_InContainer(Child), 0);
	TestEqual(TEXT("Existing cell follows opposite tilt"), Angle->GetPropertyValue_InContainer(Child), -22.5f);
	Build->SetObjectPropertyValue_InContainer(Parent, Wall);
	TestFalse(TEXT("Static wall supports do not enter the multi-step path"), SFHypertubeSupportHologramState::IsSelectedPole(Parent));
	Height->SetPropertyValue_InContainer(Parent, 4);
	SFHypertubeSupportHologramState::Sync(Parent, Child);
	TestEqual(TEXT("Unrelated wall root cannot overwrite a floor cell"), Height->GetPropertyValue_InContainer(Child), 0);
	TestTrue(TEXT("Floor support scaling profile enabled"), USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_PipeHyperSupport_C")).bSupportsScaling);
	UClass* Pipeline = NewObject<UClass>(IdentityOuter, TEXT("Build_PipelineSupport_C"));
	Pipeline->SetSuperStruct(AActor::StaticClass());
	Build->SetObjectPropertyValue_InContainer(Parent, Pipeline);
	TestTrue(TEXT("Ordinary pipeline support shares the selected-state contract"), SFHypertubeSupportHologramState::IsSelectedPole(Parent));
	SFHypertubeSupportHologramState::Sync(Parent, Child);
	TestEqual(TEXT("Different support recipes cannot exchange selected state"), Height->GetPropertyValue_InContainer(Child), 0);
	Build->SetObjectPropertyValue_InContainer(Child, Pipeline);
	SFHypertubeSupportHologramState::Sync(Parent, Child);
	TestEqual(TEXT("Pipeline materialization receives the selected height"), Height->GetPropertyValue_InContainer(Child), 4);
	TestEqual(TEXT("Pipeline materialization receives the selected angle"), Angle->GetPropertyValue_InContainer(Child), -22.5f);
	TestTrue(TEXT("Pipeline materialization retains grid pose"), Child->GetActorTransform().Equals(Pose));
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &Initial);
	TestFalse(TEXT("Pipeline first click selects height instead of constructing"), SFHypertubeSupportHologramState::IsFinalStep(Parent));
	AFGConveyorPoleHologram* ConveyorParent = World->SpawnActor<AFGConveyorPoleHologram>();
	AFGConveyorPoleHologram* ConveyorChild = World->SpawnActor<AFGConveyorPoleHologram>();
	if (!TestNotNull(TEXT("Conveyor parent"), ConveyorParent) || !TestNotNull(TEXT("Conveyor child"), ConveyorChild))
	{
		World->DestroyWorld(false); return false;
	}
	UClass* Conveyor = NewObject<UClass>(IdentityOuter, TEXT("Build_ConveyorPole_C"));
	UClass* Stackable = NewObject<UClass>(IdentityOuter, TEXT("Build_ConveyorPoleStackable_C"));
	Conveyor->SetSuperStruct(AActor::StaticClass());
	Stackable->SetSuperStruct(AActor::StaticClass());
	Build->SetObjectPropertyValue_InContainer(ConveyorParent, Conveyor);
	Build->SetObjectPropertyValue_InContainer(ConveyorChild, Conveyor);
	TestTrue(TEXT("Regular conveyor pole uses selected-state previews"), SFHypertubeSupportHologramState::IsSelectedPole(ConveyorParent));
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(ConveyorParent), &Initial);
	TestFalse(TEXT("Conveyor first input must only select height"), SFHypertubeSupportHologramState::IsFinalStep(ConveyorParent));
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(ConveyorParent), &Final);
	Height->SetPropertyValue_InContainer(ConveyorParent, 2);
	ConveyorChild->SetActorTransform(Pose);
	SFHypertubeSupportHologramState::Sync(ConveyorParent, ConveyorChild);
	TestTrue(TEXT("Conveyor materialization retains the final native step"), SFHypertubeSupportHologramState::IsFinalStep(ConveyorChild));
	TestEqual(TEXT("Conveyor materialization retains selected height"), Height->GetPropertyValue_InContainer(ConveyorChild), 2);
	TestTrue(TEXT("Conveyor refresh retains transformed grid pose"), ConveyorChild->GetActorTransform().Equals(Pose));
	Height->SetPropertyValue_InContainer(ConveyorParent, 0);
	SFHypertubeSupportHologramState::Sync(ConveyorParent, ConveyorChild);
	TestEqual(TEXT("Conveyor existing cell follows a shorter selection"), Height->GetPropertyValue_InContainer(ConveyorChild), 0);
	Build->SetObjectPropertyValue_InContainer(ConveyorParent, Stackable);
	TestFalse(TEXT("Stackable poles retain their separate construction contract"), SFHypertubeSupportHologramState::IsSelectedPole(ConveyorParent));
	Height->SetPropertyValue_InContainer(ConveyorParent, 4);
	SFHypertubeSupportHologramState::Sync(ConveyorParent, ConveyorChild);
	TestEqual(TEXT("Stackable root cannot overwrite a regular pole cell"), Height->GetPropertyValue_InContainer(ConveyorChild), 0);
	Build->SetObjectPropertyValue_InContainer(Parent, Conveyor);
	TestFalse(TEXT("Build identity alone cannot turn a pipeline hologram into a conveyor pole"), SFHypertubeSupportHologramState::IsSelectedPole(Parent));
	World->DestroyWorld(false);
	return true;
}
#endif
