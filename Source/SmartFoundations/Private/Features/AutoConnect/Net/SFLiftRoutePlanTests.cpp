// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Net/SFLiftRoutePlan.h"
#include "Buildables/FGBuildable.h"
#include "FGRecipe.h"
#include "FGItemDescriptor.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftRoutePlanBoundsTest, "SmartFoundations.AutoConnect.LiftRoutePlanBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftRoutePlanBoundsTest::RunTest(const FString& Parameters)
{
    FSFLiftRoutePlanEntry Lift;
    Lift.bLift = true;
    Lift.BuildClass = AFGBuildable::StaticClass();
    Lift.Recipe = UFGRecipe::StaticClass();
    Lift.From.BuildClass = AFGBuildable::StaticClass();
    Lift.From.PortName = TEXT("Output0");
    Lift.From.Direction = 1;
    Lift.Cost.Add(FItemAmount(UFGItemDescriptor::StaticClass(), 1));
    TArray<FSFLiftRoutePlanEntry> Entries{Lift};
    TestTrue(TEXT("A bounded standalone lift plan is accepted"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0].From.RouteIndex = 0;
    TestFalse(TEXT("Self references cannot enter native preparation"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0] = Lift;
    Entries[0].From.RouteIndex = 1;
    TestFalse(TEXT("Forward references cannot enter native preparation"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0] = Lift;
    Entries[0].Cost[0].Amount = -1;
    TestFalse(TEXT("Negative prices are rejected"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0] = Lift;
    Entries[0].From.PortName = NAME_None;
    TestFalse(TEXT("A missing anchor is rejected"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0] = Lift;
    Entries[0].bLift = false;
    TestFalse(TEXT("A belt without a spline is rejected"), SFLiftRoutePlan::IsBounded(Entries));
    Entries[0] = Lift;
    Entries[0].Spline.Add(FSplinePointData());
    Entries[0].Spline[0].Location.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite route geometry is rejected before native code"), SFLiftRoutePlan::IsBounded(Entries));
    Entries.Init(Lift, 4097);
    TestFalse(TEXT("Oversized arrays are rejected"), SFLiftRoutePlan::IsBounded(Entries));
    return true;
}
#endif
