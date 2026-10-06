// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Features/Extend/SFRestoreGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestoreGridTest,
    "SmartFoundations.Restore.Grid3D", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFRestoreGridTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Base layer keeps legacy ids"), SFRestoreGrid::Prefix(2, 1, 0), FString(TEXT("rr_2_1_")));
    TestEqual(TEXT("Upper layer has a distinct id"), SFRestoreGrid::Prefix(0, 0, 1), FString(TEXT("rr_0_0_1_")));
    FIntVector Cell;
    TestTrue(TEXT("Legacy prefix parses"), SFRestoreGrid::ParsePrefix(TEXT("rr_2_1_"), Cell));
    TestEqual(TEXT("Legacy prefix is on base layer"), Cell, FIntVector(2, 1, 0));
    TestTrue(TEXT("Layer prefix parses"), SFRestoreGrid::ParsePrefix(TEXT("rr_2_1_3_"), Cell));
    TestEqual(TEXT("All coordinates survive parsing"), Cell, FIntVector(2, 1, 3));
    TestFalse(TEXT("Malformed coordinates are rejected"), SFRestoreGrid::ParsePrefix(TEXT("rr_1_bad_3_"), Cell));
    TestFalse(TEXT("Negative cell indices are rejected"), SFRestoreGrid::ParsePrefix(TEXT("rr_1_2_-3_"), Cell));
    TestFalse(TEXT("Numeric prefixes with junk do not alias a cell"), SFRestoreGrid::ParsePrefix(TEXT("rr_1_2x_3_"), Cell));
    TestFalse(TEXT("Empty coordinates are rejected"), SFRestoreGrid::ParsePrefix(TEXT("rr_1__3_"), Cell));
    TestFalse(TEXT("Overflow cannot wrap into a valid index"), SFRestoreGrid::ParsePrefix(TEXT("rr_4294967296_2_3_"), Cell));
    TestTrue(TEXT("Largest representable index parses"), SFRestoreGrid::ParsePrefix(TEXT("rr_2147483647_2_3_"), Cell));
    TestEqual(TEXT("Largest index is unchanged"), Cell.X, MAX_int32);

    FSFCounterState State;
    State.SpacingZ = 2000;
    State.StepsX = 100;
    const FVector Size(800, 600, 400);
    const auto Upper = SFRestoreGrid::Placement(FRotator::ZeroRotator, Size, 600, State, 0, 0, 1);
    TestEqual(TEXT("Z-only copy includes height and spacing"), Upper.WorldOffset, FVector(0, 0, 2400));
    const auto Diagonal = SFRestoreGrid::Placement(FRotator::ZeroRotator, Size, 600, State, 2, 1, 2);
    TestEqual(TEXT("XYZ composes with existing steps"), Diagonal.WorldOffset, FVector(1600, 600, 5000));
    State.GridCounters.Z = -3;
    const auto Lower = SFRestoreGrid::Placement(FRotator(0, 90, 0), Size, 600, State, 0, 0, 2);
    TestEqual(TEXT("Negative grid Z descends independently of yaw"), Lower.WorldOffset, FVector(0, 0, -4800));
    State.StaggerZX = 100;
    State.StaggerZY = 50;
    const auto Lean = SFRestoreGrid::Placement(FRotator(0, 90, 0), Size, 600, State, 0, 0, 2);
    TestTrue(TEXT("Stack stagger follows signed layer and parent yaw"),
        Lean.WorldOffset.Equals(FVector(100, -200, -4800), 0.001));
    const FString FactoryId = SFRestoreGrid::Prefix(1, 2, 3) + TEXT("factory");
    TestTrue(TEXT("Wiring strips only factory suffix, retaining separator"),
        SFRestoreGrid::ParsePrefix(FactoryId.LeftChop(7), Cell));
    TestEqual(TEXT("Wiring keeps upper-layer identity"), Cell, FIntVector(1, 2, 3));

    TSet<FString> Ids;
    for (int32 Z = 0; Z < 3; ++Z)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 X = 0; X < 4; ++X)
    {
        const FString Prefix = SFRestoreGrid::Prefix(X, Y, Z);
        Ids.Add(Prefix + TEXT("factory"));
        TestTrue(TEXT("Every generated cell round trips"), SFRestoreGrid::ParsePrefix(Prefix, Cell));
        TestEqual(TEXT("Cell identity is collision-free"), Cell, FIntVector(X, Y, Z));
    }
    TestEqual(TEXT("4x2x3 has 24 distinct factory ids"), Ids.Num(), 24);
    return true;
}
#endif
