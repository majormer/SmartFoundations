// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Extend/SFExtendRunFrame.h"
#include "Features/Extend/SFRestoreGrid.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendRunFrameTest, "SmartFoundations.Camera.ExtendRestoreRunFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendRunFrameTest::RunTest(const FString& Parameters)
{
	const FVector Source(159600, -100400, 120200);
	FTransform Frame;
	TestFalse(TEXT("No run keeps the caller's fallback"), SFExtendRunFrame::TryMake(Source, Source, Frame));
	TestFalse(TEXT("Rebuild with no clones keeps the fallback"), SFExtendRunFrame::TryFromClones(Source, Source, {}, Frame));
	for (int32 Sign : {-1, 1})
	{
		const FVector First = Source + FVector(Sign * 800, 0, 0);
		TestTrue(TEXT("Single Extend frame"), SFExtendRunFrame::TryMake(First, Source, Frame));
		TestTrue(TEXT("Single Extend follows selected side"), Frame.GetUnitAxis(EAxis::X).Equals(FVector(Sign, 0, 0), 0.0001));
		TArray<FSFScaledExtendClone> Clones;
		for (const FIntVector Cell : {FIntVector(3, 1, 0), FIntVector(2, 0, 0), FIntVector(3, 0, 0), FIntVector(2, 1, 0)})
		{
			FSFScaledExtendClone Clone;
			Clone.GridX = Cell.X; Clone.GridY = Cell.Y; Clone.GridZ = Cell.Z;
			Clone.WorldOffset = FVector(Sign * (Cell.X == 3 ? 2500 : 1800), (Cell.X == 3 ? 1200 : 400) + Cell.Y * 10000, Cell.X == 3 ? 400 : 200);
			Clones.Add(Clone);
		}
		const FVector ExpectedFocus = Source + Clones[0].WorldOffset;
		const FVector ExpectedForward = FVector(Sign * 700, 800, 200).GetSafeNormal();
		TestTrue(TEXT("Curved scaled Extend frame"), SFExtendRunFrame::TryFromClones(Source, First, Clones, Frame));
		TestTrue(TEXT("Existing farthest focus preserved"), Frame.GetLocation().Equals(ExpectedFocus, 0.0001));
		TestTrue(TEXT("Heading uses previous cell on the same row, not the source or array neighbor"), Frame.GetUnitAxis(EAxis::X).Equals(ExpectedForward, 0.0001));
		TestTrue(TEXT("Stepped frame is orthogonal"), FMath::Abs(FVector::DotProduct(Frame.GetUnitAxis(EAxis::X), Frame.GetUnitAxis(EAxis::Z))) < 0.0001);
		Clones.SetNum(0);
		FSFScaledExtendClone Second;
		Second.GridX = 2; Second.WorldOffset = FVector(Sign * 1600, 0, 0); Clones.Add(Second);
		TestTrue(TEXT("Held first copy supplies omitted predecessor"), SFExtendRunFrame::TryFromClones(Source, First, Clones, Frame));
		TestTrue(TEXT("Held first heading has correct sign"), Frame.GetUnitAxis(EAxis::X).Equals(FVector(Sign, 0, 0), 0.0001));
	}

	for (int32 Sign : {-1, 1})
	for (float Yaw : {0.0f, 90.0f, 215.0f})
	for (float Turn : {0.0f, 15.0f, -15.0f})
	{
		FSFCounterState State;
		State.GridCounters = FIntVector(Sign * 5, -3, 2);
		State.SpacingX = 200; State.SpacingY = 300; State.StepsX = 100;
		State.RotationZ = Turn;
		auto Location = [&](const FIntVector& Cell)
		{
			return Source + SFRestoreGrid::Placement(FRotator(0, Yaw, 0), FVector(800, 600, 400), 600, State, Cell.X, Cell.Y, Cell.Z).WorldOffset;
		};
		const FIntVector Last(4, 2, 1);
		TestTrue(TEXT("Signed/curved Restore frame"), SFExtendRunFrame::TryFromRestoredGrid(Last, Location, Frame));
		TestTrue(TEXT("Restore keeps its actual final-cell focus"), Frame.GetLocation().Equals(Location(Last), 0.0001));
		TestTrue(TEXT("Restore uses the final step, including reverse sign and curve"), Frame.GetUnitAxis(EAxis::X).Equals((Location(Last) - Location(FIntVector(3, 2, 1))).GetSafeNormal(), 0.0001));
		TestTrue(TEXT("Row-only Restore frame"), SFExtendRunFrame::TryFromRestoredGrid(FIntVector(0, 2, 0), Location, Frame));
		TestTrue(TEXT("Row-only Restore follows row direction"), Frame.GetUnitAxis(EAxis::X).Equals((Location(FIntVector(0, 2, 0)) - Location(FIntVector(0, 1, 0))).GetSafeNormal(), 0.0001));
	}
	TestTrue(TEXT("Vertical run has a finite orthogonal frame"), SFExtendRunFrame::TryMake(Source + FVector(0, 0, 400), Source, Frame));
	TestTrue(TEXT("Vertical forward follows run"), Frame.GetUnitAxis(EAxis::X).Equals(FVector::UpVector, 0.0001));
	TestTrue(TEXT("Vertical up remains orthogonal"), FMath::Abs(FVector::DotProduct(Frame.GetUnitAxis(EAxis::X), Frame.GetUnitAxis(EAxis::Z))) < 0.0001);
	return true;
}
#endif
