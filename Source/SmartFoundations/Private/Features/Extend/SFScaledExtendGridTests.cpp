// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SFScaledExtendGrid.h"
#include "Features/Extend/SFExtendControlFrame.h"
#include "Features/Extend/SFExtendCloneTopology.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFScaledExtendGridTest, "SmartFoundations.Extend.Grid3D",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFScaledExtendGridTest::RunTest(const FString& Parameters)
{
    FSFCounterState State;
    State.GridCounters = FIntVector(2, 3, 2);
    State.SpacingZ = 200;
    const FVector Size(800, 1000, 1200);
    FSFCloneTopology Base;
    for (float Y : {-1000.0f, 1100.0f})
    {
        FSFCloneHologram H;
        H.Role = TEXT("distributor");
        H.Transform.Location = FSFVec3(FVector(0, Y, 0));
        Base.ChildHolograms.Add(H);
    }
    const float Pitch = CalculateExtendEffectiveRowHeight(Size, &Base);
    TestEqual(TEXT("Original module row pitch, #534"), Pitch, 2500.0f);
    TSet<FString> Ids;
    int32 Seeds = 0;
    SFScaledExtendGrid::ForEachAdditionalCell(State, [&](FIntVector Cell)
    {
        Ids.Add(SFScaledExtendGrid::Prefix(Cell.X, Cell.Y, Cell.Z));
        Seeds += Cell.X == 0 ? 1 : 0;
        TestFalse(TEXT("Source and held parent excluded"), Cell.Y == 0 && Cell.Z == 0 && Cell.X <= 1);
        const auto P = CalculateExtendCellPlacement(FRotator::ZeroRotator, Size, Pitch, State,
            Cell.X, Cell.Y, 0, 0, Cell.Z);
        TestTrue(TEXT("Uniform row pitch on every layer"), FMath::IsNearlyEqual(P.WorldOffset.Y, Cell.Y * 2500.0));
        TestTrue(TEXT("Layer includes factory height and spacing"), FMath::IsNearlyEqual(P.WorldOffset.Z, Cell.Z * 1400.0));
    });
    TestEqual(TEXT("Additional cells plus parent plus existing source fill 3x3x2"), Ids.Num(), 16);
    TestEqual(TEXT("One seed per new row/layer"), Seeds, 5);
    State.GridCounters = FIntVector(-3, -4, -3);
    TSet<FString> GrownIds;
    SFScaledExtendGrid::ForEachAdditionalCell(State, [&](FIntVector C) { GrownIds.Add(SFScaledExtendGrid::Prefix(C.X,C.Y,C.Z)); });
    for (const FString& Id : Ids) TestTrue(TEXT("Identity survives XYZ growth"), GrownIds.Contains(Id));
    const auto Below = CalculateExtendCellPlacement(FRotator(0,90,0), Size, Pitch, State, 2, 2, 0, 0, 2);
    TestTrue(TEXT("Negative layers remain world vertical under yaw"), FMath::IsNearlyEqual(Below.WorldOffset.Z, -2800.0));
    State.GridCounters = FIntVector(1,1,2);
    int32 ZOnly = 0;
    SFScaledExtendGrid::ForEachAdditionalCell(State, [&](FIntVector) { ++ZOnly; });
    TestEqual(TEXT("Z-only growth creates seed and clone"), ZOnly, 2);
    State.GridCounters.Z = 1;
    int32 Single = 0;
    SFScaledExtendGrid::ForEachAdditionalCell(State, [&](FIntVector) { ++Single; });
    TestEqual(TEXT("Shrink to single held clone"), Single, 0);
    return true;
}
#endif
