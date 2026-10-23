// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintVehicleSeams.h"
#include "SFBlueprintVehicleNativeFrame.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintVehicleNativeFrameTest, "SmartFoundations.AutoConnect.BlueprintVehicleNativeFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintVehicleNativeFrameTest::RunTest(const FString& Parameters)
{
    auto* Parent = NewObject<USceneComponent>();
    auto* Visual = NewObject<USceneComponent>();
    auto* BuiltRoot = NewObject<USceneComponent>();
    Visual->SetupAttachment(Parent);
    const auto Pose = Visual->GetComponentTransform();
    TestTrue(TEXT("The native copied visual retains its attachment frame"), SFBlueprintVehicleNativeFrame::Matches(Visual, Pose));
    TestFalse(TEXT("An unattached built root cannot substitute for that visual"), SFBlueprintVehicleNativeFrame::Matches(BuiltRoot, BuiltRoot->GetComponentTransform()));
    FTransform Shifted = Pose; Shifted.AddToTranslation(FVector(100,0,0));
    TestFalse(TEXT("A stale copied frame rejects before native candidate lookup"), SFBlueprintVehicleNativeFrame::Matches(Visual, Shifted));
    TestFalse(TEXT("An absent copied visual rejects"), SFBlueprintVehicleNativeFrame::Matches(nullptr, Pose));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintVehiclePlanBoundsTest, "SmartFoundations.AutoConnect.BlueprintVehiclePlanBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintVehiclePlanBoundsTest::RunTest(const FString& Parameters)
{
    FSFBlueprintVehiclePlanEntry E;
    E.Tail.BuildClass = E.Head.BuildClass = AFGVehiclePathNode::StaticClass();
    E.Incoming.BuildClass = E.Outgoing.BuildClass = AFGVehiclePathSegment::StaticClass();
    E.Head.Cell = E.Outgoing.Cell = FIntVector(1, 0, 0);
    FSplinePointData P; P.Location = FVector::ZeroVector; P.ArriveTangent = P.LeaveTangent = FVector(100, 0, 0);
    E.Incoming.Spline = E.Outgoing.Spline = {P, P};
    TestTrue(TEXT("Direct adjacent native paths require no invented bridge or cost"), FSFBlueprintVehicleSeams::IsBounded({E}));
    TestFalse(TEXT("A physical endpoint cannot be claimed twice"), FSFBlueprintVehicleSeams::IsBounded({E,E}));
    auto Bad = E; Bad.Outgoing.Cell = FIntVector::ZeroValue;
    TestFalse(TEXT("The outgoing segment must belong to the target copy"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    Bad = E; Bad.Tail.Cell.X = std::numeric_limits<int32>::min();
    TestFalse(TEXT("Extreme cell coordinates reject before subtraction"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    Bad = E; Bad.Head.BuildClass = AFGVehiclePathSegment::StaticClass();
    TestFalse(TEXT("A segment cannot impersonate a node"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    Bad = E; Bad.Incoming.Spline.SetNum(257);
    TestFalse(TEXT("Oversized path geometry rejects"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    Bad = E; Bad.Outgoing.Spline[0].Location.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite geometry rejects"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    Bad = E; Bad.Tail.Original.SetTranslation(FVector(std::numeric_limits<double>::quiet_NaN(),0,0));
    TestFalse(TEXT("Nonfinite original identity rejects"), FSFBlueprintVehicleSeams::IsBounded({Bad}));
    return true;
}
#endif
