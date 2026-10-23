// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Shared/Conduits/SFConveyanceLength.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSupportNativeLengthTest,
	"SmartFoundations.AutoConnect.NativeSupportLength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSupportNativeLengthTest::RunTest(const FString& Parameters)
{
	const ASFConveyorBeltHologram& Belt = *GetDefault<ASFConveyorBeltHologram>();
	const ASFPipelineHologram& Pipe = *GetDefault<ASFPipelineHologram>();
	const FSFPositionCalculator Calculator;
	FSFCounterState State;
	State.SpacingX = 5500;
	const FVector Origin(159600, -100400, 120200);
	const FVector Size(100, 200, 200);
	int32 LegacyRejections = 0;
	for (double Yaw : {0.0, 45.0, 90.0, -45.0})
	{
		FVector Previous = Origin;
		for (int32 X = 1; X <= 16; ++X)
		{
			const FVector Current = Calculator.CalculateChildPosition(X, 0, 0, Origin, FRotator(0, Yaw, 0), Size, State);
			const float Distance = FVector::Distance(Previous, Current);
			LegacyRejections += Distance > 5600.0f ? 1 : 0;
			TestFalse(TEXT("Native belt limit accepts every 56m grid span"), SFConveyanceLength::ExceedsNativeLimit(Distance, Belt));
			TestFalse(TEXT("Native pipe limit accepts every 56m grid span"), SFConveyanceLength::ExceedsNativeLimit(Distance, Pipe));
			Previous = Current;
		}
	}
	TestTrue(TEXT("The exact old cutoff rejects some computed diagonal spans"), LegacyRejections > 0);
	TestFalse(TEXT("Belt accepts its native boundary"), SFConveyanceLength::ExceedsNativeLimit(Belt.GetMaxSplineLength(), Belt));
	TestFalse(TEXT("Pipe accepts its native boundary"), SFConveyanceLength::ExceedsNativeLimit(Pipe.GetMaxSplineLength(), Pipe));
	TestTrue(TEXT("Belt rejects beyond its native boundary"), SFConveyanceLength::ExceedsNativeLimit(Belt.GetMaxSplineLength() + 0.001, Belt));
	TestTrue(TEXT("Pipe rejects beyond its native boundary"), SFConveyanceLength::ExceedsNativeLimit(Pipe.GetMaxSplineLength() + 0.001, Pipe));

	// A configured native limit must control the result; no fixed 56m fallback or added tolerance.
	struct FConfiguredHologram
	{
		float Limit;
		float GetMaxSplineLength() const { return Limit; }
	};
	TestTrue(TEXT("Lower native configuration is preserved"), SFConveyanceLength::ExceedsNativeLimit(5600.0, FConfiguredHologram{5500.0f}));
	TestFalse(TEXT("Higher native configuration is preserved"), SFConveyanceLength::ExceedsNativeLimit(5650.0, FConfiguredHologram{5700.0f}));
	TestTrue(TEXT("No tolerance is added above a configured limit"), SFConveyanceLength::ExceedsNativeLimit(5700.001, FConfiguredHologram{5700.0f}));
	return true;
}
#endif
