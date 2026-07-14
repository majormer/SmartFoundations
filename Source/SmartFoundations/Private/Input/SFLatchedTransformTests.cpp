// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Input/SFLatchedTransform.h"

// [#482] Exhaustive tests for the tap-to-toggle activation-policy state machine. Everything the
// latch DECIDES is covered here; USFSubsystem only applies decisions (booleans/lock/HUD), so a
// green run pins the whole policy without needing a controller or a game session.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSFLatchedTransformTransitionTest,
	"Smart.Input.LatchedTransform.Transition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFLatchedTransformTransitionTest::RunTest(const FString& Parameters)
{
	using namespace SFLatchedTransform;
	using EMode = ESFLatchedTransformMode;

	const EMode AllModes[] = { EMode::Spacing, EMode::Steps, EMode::Stagger, EMode::Rotation, EMode::Recipe };

	// From None, tapping any mode latches it.
	for (EMode Tapped : AllModes)
	{
		TestEqual(FString::Printf(TEXT("None + tap %s latches it"), ToString(Tapped)),
			Transition(EMode::None, Tapped), Tapped);
	}

	// Tapping the latched mode again unlatches.
	for (EMode Current : AllModes)
	{
		TestEqual(FString::Printf(TEXT("%s + same tap unlatches"), ToString(Current)),
			Transition(Current, Current), EMode::None);
	}

	// Tapping a different mode switches directly (never through None).
	for (EMode Current : AllModes)
	{
		for (EMode Tapped : AllModes)
		{
			if (Current == Tapped)
			{
				continue;
			}
			TestEqual(FString::Printf(TEXT("%s + tap %s switches"), ToString(Current), ToString(Tapped)),
				Transition(Current, Tapped), Tapped);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSFLatchedTransformDecideInputTest,
	"Smart.Input.LatchedTransform.DecideInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFLatchedTransformDecideInputTest::RunTest(const FString& Parameters)
{
	using namespace SFLatchedTransform;
	using EMode = ESFLatchedTransformMode;

	const EMode AllModes[] = { EMode::Spacing, EMode::Steps, EMode::Stagger, EMode::Rotation, EMode::Recipe };

	// ---- Setting OFF: never consume; the hold path must run unchanged. ----
	for (EMode Tapped : AllModes)
	{
		for (bool bPressed : { true, false })
		{
			const FSFLatchInputDecision D = DecideInput(false, bPressed, EMode::None, Tapped);
			TestFalse(TEXT("OFF never consumes"), D.bConsume);
			TestFalse(TEXT("OFF with no latch flags no stale clear"), D.bClearStaleLatch);
		}
	}

	// Setting OFF while a latch exists (flipped mid-latch): still not consumed, but the stale
	// latch must be flagged for clearing so no mode is ever stuck.
	{
		const FSFLatchInputDecision D = DecideInput(false, true, EMode::Spacing, EMode::Steps);
		TestFalse(TEXT("OFF mid-latch never consumes"), D.bConsume);
		TestTrue(TEXT("OFF mid-latch flags the stale latch"), D.bClearStaleLatch);
	}

	// ---- Setting ON: releases are consumed and change nothing (no release timestamps, so the
	// hold path's re-tap-to-cycle gesture can never fire from latch traffic). ----
	for (EMode Current : { EMode::None, EMode::Spacing, EMode::Recipe })
	{
		for (EMode Tapped : AllModes)
		{
			const FSFLatchInputDecision D = DecideInput(true, false, Current, Tapped);
			TestTrue(TEXT("ON release consumed"), D.bConsume);
			TestEqual(TEXT("ON release changes nothing"), D.NewMode, Current);
		}
	}

	// ---- Setting ON: presses toggle/switch per Transition. ----
	{
		FSFLatchInputDecision D = DecideInput(true, true, EMode::None, EMode::Spacing);
		TestTrue(TEXT("ON press consumed"), D.bConsume);
		TestEqual(TEXT("None -> Spacing"), D.NewMode, EMode::Spacing);

		D = DecideInput(true, true, EMode::Spacing, EMode::Spacing);
		TestEqual(TEXT("Spacing + Spacing -> None (toggle off)"), D.NewMode, EMode::None);

		D = DecideInput(true, true, EMode::Spacing, EMode::Rotation);
		TestEqual(TEXT("Spacing + Rotation -> Rotation (direct switch)"), D.NewMode, EMode::Rotation);

		D = DecideInput(true, true, EMode::Rotation, EMode::Recipe);
		TestEqual(TEXT("Rotation + Recipe -> Recipe"), D.NewMode, EMode::Recipe);

		D = DecideInput(true, true, EMode::Recipe, EMode::Recipe);
		TestEqual(TEXT("Recipe + Recipe -> None"), D.NewMode, EMode::None);
	}

	// ---- Full tap sequence simulating a Steam Input radial session: each entry sends
	// press+release pairs; the latch must land exactly where the taps say. ----
	{
		EMode Mode = EMode::None;
		auto Tap = [&Mode](EMode Tapped)
		{
			FSFLatchInputDecision Press = DecideInput(true, true, Mode, Tapped);
			Mode = Press.NewMode;
			FSFLatchInputDecision Release = DecideInput(true, false, Mode, Tapped);
			Mode = Release.NewMode;
		};

		Tap(EMode::Spacing);   // on
		Tap(EMode::Steps);     // switch
		Tap(EMode::Rotation);  // switch
		Tap(EMode::Rotation);  // off
		TestEqual(TEXT("radial session ends unlatched"), Mode, EMode::None);

		Tap(EMode::Recipe);    // on
		Tap(EMode::Stagger);   // switch away from recipe
		TestEqual(TEXT("recipe -> stagger switch"), Mode, EMode::Stagger);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
