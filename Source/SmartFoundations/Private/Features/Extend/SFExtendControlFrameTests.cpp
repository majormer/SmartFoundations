// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Features/Extend/SFExtendControlFrame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSFExtendControlFrameTest,
	"SmartFoundations.Extend.ControlFrame.SignedPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendControlFrameTest::RunTest(const FString& Parameters)
{
	FSFCounterState State;
	State.GridCounters.X = -3;
	State.GridCounters.Y = -2;
	State.SpacingX = 100;
	State.SpacingY = 200;
	State.StepsX = 50;
	State.StepsY = 25;
	const FSFExtendCellPlacement Placement = CalculateExtendCellPlacement(
		FRotator::ZeroRotator,
		FVector(1000.0f, 600.0f, 400.0f),
		800.0f,
		State,
		2,
		1);

	TestEqual(TEXT("Negative Chain uses signed X state"), Placement.WorldOffset.X, -2200.0);
	TestEqual(TEXT("Negative Rows uses signed Y state"), Placement.WorldOffset.Y, -1000.0);
	TestEqual(TEXT("Chain and Rows steps compose"), Placement.WorldOffset.Z, 125.0);

	const FSFExtendCellPlacement RestoredRelative = CalculateExtendCellPlacement(
		FRotator::ZeroRotator,
		FVector(1000.0f, 600.0f, 400.0f),
		800.0f,
		State,
		3,
		1,
		1,
		0);
	TestEqual(TEXT("Restore origin preserves Chain delta"), RestoredRelative.WorldOffset.X, -2200.0);
	TestEqual(TEXT("Restore origin preserves Rows delta"), RestoredRelative.WorldOffset.Y, -1000.0);
	TestEqual(TEXT("Restore origin preserves step delta"), RestoredRelative.WorldOffset.Z, 125.0);

	FSFCounterState RotatedState;
	RotatedState.GridCounters.X = -2;
	const FSFExtendCellPlacement RotatedPlacement = CalculateExtendCellPlacement(
		FRotator(0.0f, 90.0f, 0.0f),
		FVector(800.0f, 1000.0f, 400.0f),
		1000.0f,
		RotatedState,
		1,
		0);
	// RotateVector uses the engine's SIMD trigonometry; its residual at 90 degrees is
	// larger than the default double near-zero epsilon. One micron is well below gameplay
	// placement precision while still rejecting an incorrect axis or a visible offset.
	TestTrue(FString::Printf(TEXT("Yaw 90 negative Chain has no world X drift (X=%.12f cm)"),
		RotatedPlacement.WorldOffset.X), FMath::IsNearlyZero(RotatedPlacement.WorldOffset.X, 0.0001));
	TestEqual(TEXT("Yaw 90 negative Chain maps to world -Y"), RotatedPlacement.WorldOffset.Y, -800.0);

	return true;
}

#endif
