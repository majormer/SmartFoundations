// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Shared/SFPillarGridFrame.h"
#include "Shared/SFSupportGridFrame.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Hologram/FGPillarHologram.h"
#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPillarFrameTest, "SmartFoundations.Scaling.NativePillarFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPillarFrameTest::RunTest(const FString& Parameters)
{
	UPackage* Outer = CreatePackage(TEXT("/Temp/SmartNativePillarFrame"));
	UClass* Pillar = NewObject<UClass>(Outer, TEXT("Build_Pillar_Small_Concrete_C"));
	Pillar->SetSuperStruct(AFGBuildable::StaticClass());
	UClass* Blueprint = NewObject<UClass>(Outer, TEXT("Build_Blueprint_C"));
	UClass* Modded = NewObject<UClass>(Outer, TEXT("Build_ModdedPillar_C"));
	TestTrue(TEXT("Native pillar selected"), SFPillarGridFrame::Applies(Pillar));
	TestFalse(TEXT("Blueprints excluded"), SFPillarGridFrame::Applies(Blueprint));
	TestFalse(TEXT("Unknown modded pillars excluded"), SFPillarGridFrame::Applies(Modded));
	TestFalse(TEXT("Pillars do not gain conduit wall capabilities"), SFSupportGridFrame::UsesParentRotation(Pillar));

	FSFPositionCalculator Calculator;
	FSFCounterState State;
	State.SpacingX = 100; State.SpacingY = 200; State.SpacingZ = 300;
	const FVector Size(200, 200, 400), Origin(159600, -100400, 120200);
	for (const FRotator Rotation : {FRotator(45, 0, 0), FRotator(-30, 90, 0), FRotator(0, 30, 90), FRotator(90, 180, 0), FRotator(0, 0, 180)})
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		for (int32 Sign : {-1, 1})
		{
			FIntVector Cell = FIntVector::ZeroValue; Cell[Axis] = Sign;
			const double Pitch = Size[Axis] + (Axis == 0 ? State.SpacingX : Axis == 1 ? State.SpacingY : State.SpacingZ);
			const FVector Direction = Rotation.RotateVector(SFPillarGridFrame::LocalAxis(Axis) * Sign);
			const FVector Actual = Calculator.CalculateChildPosition(Cell.X, Cell.Y, Cell.Z, Origin, Rotation, Size, State, 0, FVector::ZeroVector, Pillar);
			TestTrue(TEXT("Signed local spacing follows the full native rotation"), Actual.Equals(Origin + Direction * Pitch, 0.01));
			const FVector Arrow = Rotation.RotateVector(SFPillarGridFrame::ArrowRotation(Axis, Sign).RotateVector(FVector::UpVector));
			TestTrue(TEXT("Arrow shaft agrees with actual grid displacement"), Arrow.Equals(Direction, 0.0001));
		}
		for (float ViewYaw : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f})
		{
			const auto Axes = SFPillarGridFrame::Resolve(Rotation, ViewYaw);
			TestTrue(TEXT("Player Relative roles use three distinct axes"), Axes.Forward != Axes.Side && Axes.Forward != Axes.Vertical && Axes.Side != Axes.Vertical);
			const FVector Up = Rotation.RotateVector(SFPillarGridFrame::LocalAxis(Axes.Vertical) * Axes.VerticalSign);
			TestTrue(TEXT("Vertical gesture selects an upward native axis"), Up.Z > 0.5);
			TestTrue(TEXT("Forward gesture follows view"), FVector::DotProduct(Rotation.RotateVector(SFPillarGridFrame::LocalAxis(Axes.Forward) * Axes.ForwardSign), FRotator(0, ViewYaw, 0).Vector()) >= -0.0001);
			TestTrue(TEXT("Side gesture follows view right"), FVector::DotProduct(Rotation.RotateVector(SFPillarGridFrame::LocalAxis(Axes.Side) * Axes.SideSign), FRotator(0, ViewYaw, 0).RotateVector(FVector::RightVector)) >= -0.0001);
		}
		TestEqual(TEXT("Blueprint displacement remains the original grid"),
			Calculator.CalculateChildPosition(1, 1, 1, Origin, Rotation, Size, State, 0, FVector::ZeroVector, Blueprint),
			Calculator.CalculateChildPosition(1, 1, 1, Origin, Rotation, Size, State));
		FSFCounterState Stepped = State; Stepped.StepsX = 100;
		TestTrue(TEXT("Steps remains world vertical"),
			(Calculator.CalculateChildPosition(1, 0, 0, Origin, Rotation, Size, Stepped, 0, FVector::ZeroVector, Pillar)
			- Calculator.CalculateChildPosition(1, 0, 0, Origin, Rotation, Size, State, 0, FVector::ZeroVector, Pillar)).Equals(FVector(0, 0, 100), 0.01));
	}
	const auto Upright = SFPillarGridFrame::Resolve(FRotator::ZeroRotator, 0);
	TestTrue(TEXT("Upright controls retain shipped signs"), Upright.Forward == 0 && Upright.ForwardSign == -1 && Upright.Side == 1 && Upright.SideSign == -1 && Upright.Vertical == 2 && Upright.VerticalSign == 1);
	TestEqual(TEXT("45-degree tie keeps local Z as up"), SFPillarGridFrame::Resolve(FRotator(45, 0, 0), 0).Vertical, 2);

	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;
	AFGPillarHologram* Hologram = World->SpawnActor<AFGPillarHologram>();
	FObjectPropertyBase* BuildClass = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mBuildClass"));
	if (TestNotNull(TEXT("Pillar preview"), Hologram) && TestNotNull(TEXT("Native build class property"), BuildClass))
	{
		BuildClass->SetObjectPropertyValue_InContainer(Hologram, Pillar);
		Hologram->SetActorRotation(FRotator(45, 30, 0));
		FVector Dimensions;
		TestTrue(TEXT("Pillar dimensions resolved"), SFScalingFootprint::TryGetDimensions(Hologram, Dimensions));
		TestTrue(TEXT("Tilt does not expand local dimensions a second time"), Dimensions.Equals(Size, 0.0001));
		BuildClass->SetObjectPropertyValue_InContainer(Hologram, nullptr);
	}
	World->DestroyWorld(false);
	return true;
}
#endif
