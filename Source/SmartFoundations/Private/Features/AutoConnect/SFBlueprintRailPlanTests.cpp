// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintRailSeams.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintRailPortIdentityTest, "SmartFoundations.AutoConnect.BlueprintRailPortIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintRailPortIdentityTest::RunTest(const FString& Parameters)
{
    FSFBlueprintRailPort A;
    A.BuildClass = AFGBuildableRailroadTrack::StaticClass(); A.ComponentName = TEXT("TrackConnection0");
    A.OriginalOwner = FTransform(FRotator(0, -0.000001, 0), FVector(-799.974976, 0, 102.646156));
    FSFBlueprintRailPort B = A;
    B.OriginalOwner = FTransform(FRotator(0, 0, -0.000001), FVector(-799.974975, 0, 102.646156));
    TestTrue(TEXT("Native transform roundoff and Euler signed zero do not change content identity"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    const FQuat Q = A.OriginalOwner.GetRotation();
    B.OriginalOwner = A.OriginalOwner;
    B.OriginalOwner.SetRotation(FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W));
    TestTrue(TEXT("Quaternion sign aliases identify the same authored port"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    B = A; B.OriginalOwner.AddToTranslation(FVector(1, 0, 0));
    TestFalse(TEXT("A different authored position is not a nearest-port substitute"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    B = A; B.Cell = FIntVector(1, 0, 0);
    TestFalse(TEXT("Different grid copies remain distinct"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    B = A; B.ComponentName = TEXT("TrackConnection1");
    TestFalse(TEXT("The opposite rail mouth remains distinct"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    B = A; B.BuildClass = AFGBuildable::StaticClass();
    TestFalse(TEXT("Content class remains part of identity"), FSFBlueprintRailSeams::SamePortIdentity(A, B));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintRailPlanBoundsTest, "SmartFoundations.AutoConnect.BlueprintRailPlanBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintRailPlanBoundsTest::RunTest(const FString& Parameters)
{
    FSFBlueprintRailPlanEntry E;
    FSFBlueprintRailPort From;
    From.BuildClass = AFGBuildableRailroadTrack::StaticClass(); From.ComponentName = TEXT("TrackConnection1");
    E.From.Add(From); E.To = From; E.To.Cell = FIntVector(1, 0, 0); E.To.ComponentName = TEXT("TrackConnection0"); E.bDirect = true;
    TestTrue(TEXT("Direct seams need no invented spline"), FSFBlueprintRailSeams::IsBounded({E}));
    E.To.Cell = E.From[0].Cell;
    TestFalse(TEXT("A seam cannot target another port within the same copy"), FSFBlueprintRailSeams::IsBounded({E}));
    E.To.Cell = FIntVector(2, 0, 0);
    TestFalse(TEXT("A seam cannot skip an intervening copy"), FSFBlueprintRailSeams::IsBounded({E}));
    E.To.Cell = FIntVector(1, 0, 0);
    E.From.Add(From);
    TestFalse(TEXT("Duplicate source identities are ambiguous"), FSFBlueprintRailSeams::IsBounded({E}));
    E.From.SetNum(1);
    TestFalse(TEXT("The same seam cannot be charged or constructed twice"), FSFBlueprintRailSeams::IsBounded({E,E}));
    E.bDirect = false;
    TestFalse(TEXT("A bridge cannot omit its priced geometry"), FSFBlueprintRailSeams::IsBounded({E}));
    E.Spline.SetNum(2); E.Spline[1].Location = FVector(1600, 0, 0);
    TestTrue(TEXT("Native two-point routed bridge accepted structurally"), FSFBlueprintRailSeams::IsBounded({E}));
    E.Spline[1].LeaveTangent.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite geometry rejected before reconstruction"), FSFBlueprintRailSeams::IsBounded({E}));
    E.Spline[1].LeaveTangent = FVector::ZeroVector; E.To.BuildClass = AFGBuildable::StaticClass();
    TestFalse(TEXT("Unrelated buildables cannot impersonate rail endpoints"), FSFBlueprintRailSeams::IsBounded({E}));
    E.To.BuildClass = AFGBuildableRailroadTrack::StaticClass(); E.To.ComponentName = NAME_None;
    TestFalse(TEXT("Missing exact component identity rejected"), FSFBlueprintRailSeams::IsBounded({E}));
    E.To.ComponentName = TEXT("TrackConnection0"); E.From.Reset();
    TestFalse(TEXT("Orphan target rejected"), FSFBlueprintRailSeams::IsBounded({E}));
    return true;
}
#endif
