// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFBeamHologramState.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Features/Scaling/Holograms/SFBeamChildHologram.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFScalingTiltFootprintTest, "SmartFoundations.Scaling.TiltFootprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFScalingTiltFootprintTest::RunTest(const FString& Parameters)
{
	const FVector Support(200.0, 200.0, 60.0);
	TestTrue(TEXT("Floor preserves local dimensions"), SFScalingFootprint::FromLocalDimensions(Support, FRotator::ZeroRotator).Equals(Support));
	TestTrue(TEXT("Yaw is applied by grid positioning only"), SFScalingFootprint::FromLocalDimensions(Support, FRotator(0, 90, 0)).Equals(Support));
	TestTrue(TEXT("Wall support is 200 cm high"), SFScalingFootprint::FromLocalDimensions(Support, FRotator(90, 90, 0)).Equals(FVector(60, 200, 200), 0.001));
	TestTrue(TEXT("Roll swaps depth and height"), SFScalingFootprint::FromLocalDimensions(Support, FRotator(0, 0, 90)).Equals(FVector(200, 60, 200), 0.001));
	TestTrue(TEXT("Wall double connector is 210 cm high"), SFScalingFootprint::FromLocalDimensions(FVector(210, 111, 111), FRotator(90, 15, 0)).Equals(FVector(111, 111, 210), 0.001));
	TestTrue(TEXT("Ceiling keeps dimensions"), SFScalingFootprint::FromLocalDimensions(Support, FRotator(180, 30, 0)).Equals(Support, 0.001));
	const double Diagonal = 1300.0 / FMath::Sqrt(2.0);
	TestTrue(TEXT("Diagonal beam covers its full projected height"), SFScalingFootprint::FromLocalDimensions(FVector(1200, 100, 100), FRotator(45, 70, 0)).Equals(FVector(Diagonal, 100, Diagonal), 0.001));
	TestTrue(TEXT("Vertical beam uses its selected length as height"), SFScalingFootprint::FromLocalDimensions(FVector(1200, 100, 100), FRotator(90, -20, 0)).Equals(FVector(100, 100, 1200), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNativeGridSpacingTest, "SmartFoundations.Scaling.NativeGridSpacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFNativeGridSpacingTest::RunTest(const FString& Parameters)
{
    const FVector Tilted = SFScalingFootprint::FromLocalDimensions(FVector(300, 200, 111), FRotator(90, 0, 0));
    const FVector Result = SFScalingFootprint::RoundHorizontalToGrid(Tilted, 100);
    TestTrue(TEXT("Tilted connector stays on the horizontal native grid"), Result.Equals(FVector(200, 200, 300), 0.001));
    const FVector Raw(209.6659, 110.6918, 59.62427);
    TestTrue(TEXT("Only horizontal dimensions round up"), SFScalingFootprint::RoundHorizontalToGrid(Raw, 100).Equals(FVector(300, 200, Raw.Z), 0.0001));
    TestTrue(TEXT("Floating-point noise does not add a whole cell"), SFScalingFootprint::RoundHorizontalToGrid(FVector(200.000001, 300.000001, 111), 100).Equals(FVector(200, 300, 111), 0.0001));
    TestTrue(TEXT("Native grid interval is respected"), SFScalingFootprint::RoundHorizontalToGrid(Raw, 50).Equals(FVector(250, 150, Raw.Z), 0.0001));
    TestTrue(TEXT("Absent grid interval preserves the input"), SFScalingFootprint::RoundHorizontalToGrid(Raw, 0).Equals(Raw));
    TestEqual(TEXT("Floor Fan fits three native metre nudges"), SFScalingFootprint::RoundHeightToNudge(241.5285, 100), 300.0);
    TestEqual(TEXT("Rotated Valve fits three native metre nudges"), SFScalingFootprint::RoundHeightToNudge(236.6754, 100), 300.0);
    TestEqual(TEXT("Exact native step does not add another step"), SFScalingFootprint::RoundHeightToNudge(200.000001, 100), 200.0);
    TestEqual(TEXT("Family native half-metre step is respected"), SFScalingFootprint::RoundHeightToNudge(241.5285, 50), 250.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBeamChildStateTest, "SmartFoundations.Scaling.BeamChildState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBeamChildStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;
	AFGBeamHologram* Parent = World->SpawnActor<AFGBeamHologram>();
	ASFBeamChildHologram* First = World->SpawnActor<ASFBeamChildHologram>();
	FFloatProperty* Length = FindFProperty<FFloatProperty>(AFGBeamHologram::StaticClass(), TEXT("mCurrentLength"));
	FProperty* Step = FindFProperty<FProperty>(AFGBeamHologram::StaticClass(), TEXT("mBuildStep"));
	if (!TestNotNull(TEXT("Parent"), Parent) || !TestNotNull(TEXT("First child"), First)
		|| !TestNotNull(TEXT("Native length contract"), Length) || !TestNotNull(TEXT("Native step contract"), Step))
	{
		World->DestroyWorld(false);
		return false;
	}
	const EBeamHologramBuildStep ChosenStep = EBeamHologramBuildStep::BHBS_Zoop;
	Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Parent), &ChosenStep);
	Length->SetPropertyValue_InContainer(Parent, 1200.0f);
	SFBeamHologramState::Sync(Parent, First);
	TestEqual(TEXT("Existing child takes chosen length"), Length->GetPropertyValue_InContainer(First), 1200.0f);
	TestTrue(TEXT("Existing child takes chosen step"), Step->Identical_InContainer(Parent, First));

	// A new grid cell must take the current state without any change to its parent.
	ASFBeamChildHologram* Late = World->SpawnActor<ASFBeamChildHologram>();
	SFBeamHologramState::Sync(Parent, Late);
	TestEqual(TEXT("Late child takes unchanged parent length"), Length->GetPropertyValue_InContainer(Late), 1200.0f);
	TestTrue(TEXT("Late child takes unchanged parent step"), Step->Identical_InContainer(Parent, Late));
	Length->SetPropertyValue_InContainer(Parent, 3200.0f);
	SFBeamHologramState::Sync(Parent, First);
	SFBeamHologramState::Sync(Parent, Late);
	TestEqual(TEXT("Existing cell updates length"), Length->GetPropertyValue_InContainer(First), 3200.0f);
	TestEqual(TEXT("Late cell updates length"), Length->GetPropertyValue_InContainer(Late), 3200.0f);

	USceneComponent* Root = NewObject<USceneComponent>(First);
	First->SetRootComponent(Root);
	Root->RegisterComponent();
	const FTransform Intended(FRotator(35, 70, 0), FVector(14000, -8000, 4200));
	First->SetActorTransform(Intended);
	First->SetHologramLocationAndRotation(FHitResult());
	First->SetHologramNudgeLocation();
	SFBeamHologramState::Sync(Parent, First);
	TestTrue(TEXT("Placement, nudge and state copying retain authored transform"), First->GetActorTransform().Equals(Intended));
	TestEqual(TEXT("Sync does not alter parent geometry"), Length->GetPropertyValue_InContainer(Parent), 3200.0f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStructuralProfileTest, "SmartFoundations.Scaling.StructuralProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStructuralProfileTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Material : {TEXT("Orange"), TEXT("Tar"), TEXT("Window")})
	{
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			const FString Name = FString::Printf(TEXT("Build_Roof_%s_0%d_C"), Material, Index);
			const auto Profile = USFBuildableSizeRegistry::GetProfileByName(Name);
			TestTrue(*Name, Profile.bSupportsScaling);
			TestEqual(TEXT("Native roof footprint"), Profile.DefaultSize.X, 800.0);
			TestEqual(TEXT("Native roof depth"), Profile.DefaultSize.Y, 800.0);
		}
		for (const TCHAR* Corner : {TEXT("InCorner"), TEXT("OutCorner")})
		{
			for (int32 Index = 1; Index <= 3; ++Index)
			{
				const auto Profile = USFBuildableSizeRegistry::GetProfileByName(
					FString::Printf(TEXT("Build_Roof_%s_%s_0%d_C"), Material, Corner, Index));
				TestTrue(TEXT("Corner enabled"), Profile.bSupportsScaling);
				TestEqual(TEXT("Corner uses native width, not placeholder"), Profile.DefaultSize.X, 800.0);
			}
		}
	}
	for (const TCHAR* Name : {TEXT("Build_Beam_C"), TEXT("Build_Beam_Painted_C"), TEXT("Build_Beam_Concrete_C")})
	{
		const auto Profile = USFBuildableSizeRegistry::GetProfileByName(Name);
		TestTrue(TEXT("Beam scalable"), Profile.bSupportsScaling);
		TestEqual(TEXT("Beam local width"), Profile.DefaultSize.Y, 100.0);
	}
	TestFalse(TEXT("Ore extractor remains excluded"), USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_MinerMk1_C")).bSupportsScaling);
	return true;
}
#endif
