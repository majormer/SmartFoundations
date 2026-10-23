// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SFBlueprintRailSourceIdentity.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintRailCanonicalFrameTest,
    "SmartFoundations.AutoConnect.BlueprintRailCanonicalFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintRailCanonicalFrameTest::RunTest(const FString&)
{
    const FTransform ExpectedOwner(FRotator(0, 90, 0), FVector(179200, -112800, 120302.65));
    const FTransform ActualOwner(FRotator::ZeroRotator, ExpectedOwner.GetLocation());
    TArray<FSplinePointData> Local; Local.SetNum(2);
    Local[1].Location = FVector(1600, 0, 0);
    for (auto& Point : Local) Point.ArriveTangent = Point.LeaveTangent = FVector(1600, 0, 0);
    const auto World = SFBlueprintRailSourceIdentity::InWorld(Local, ExpectedOwner);
    auto Baked = Local;
    for (auto& Point : Baked)
    {
        Point.Location = ExpectedOwner.TransformVector(Point.Location);
        Point.ArriveTangent = ExpectedOwner.TransformVector(Point.ArriveTangent);
        Point.LeaveTangent = ExpectedOwner.TransformVector(Point.LeaveTangent);
    }
    TestTrue(TEXT("Native spline rotation baking preserves exact world geometry"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, ActualOwner, Baked));
    TestFalse(TEXT("Matching origins must not accept a differently directed track"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, ActualOwner, Local));
    auto WrongSpline = Baked; WrongSpline[1].Location.X += .2;
    TestFalse(TEXT("A nearby endpoint must not substitute for the captured geometry"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, ActualOwner, WrongSpline));
    WrongSpline = Baked; WrongSpline[0].LeaveTangent *= -1;
    TestFalse(TEXT("Matching endpoints do not accept different curvature"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, ActualOwner, WrongSpline));
    FTransform Wrong = ActualOwner; Wrong.AddToTranslation(FVector(.2, 0, 0));
    TestFalse(TEXT("The exact source actor origin is retained"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, Wrong, Baked));
    Wrong = ActualOwner; Wrong.SetScale3D(FVector(1, 1, 2));
    TestFalse(TEXT("Different selected source scale is rejected"),
        SFBlueprintRailSourceIdentity::Matches(ExpectedOwner, World, Wrong, Baked));
    return true;
}
#endif
