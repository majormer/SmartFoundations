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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFScaledExtendPipeAxisTest, "SmartFoundations.Extend.Pipes.RowIndependentAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFScaledExtendPipeAxisTest::RunTest(const FString& Parameters)
{
    // Runtime regression: 5-degree arc, 10m forward pitch, 34m row pitch, 30m layers.
    const FVector SourceLocation(315800, -136400, 8500);
    const FVector FirstOffset(43.6054566, 998.7312017, 0);
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("source-factory");
    Source.Factory.Class = TEXT("Build_OilRefinery_C");
    Source.Factory.Transform = FSFTransform(SourceLocation, FRotator(0, -90, 0));
    FSFSourceChain Chain;
    Chain.ChainId = TEXT("pipe-input");
    Chain.FactoryConnector = TEXT("PipeInputFactory");
    Chain.Distributor.Id = TEXT("source-junction");
    Chain.Distributor.Class = TEXT("Build_PipelineJunction_Cross_C");
    Chain.Distributor.ConnectorUsed = TEXT("Connection1");
    const FVector JunctionLocation = SourceLocation + FVector(1500, 200, 175);
    Chain.Distributor.Transform = FSFTransform(JunctionLocation, FRotator(0, 180, 0));
    Chain.Distributor.ConnectorWorldPositions.Add(TEXT("Connection0"), JunctionLocation + FVector(100, 0, 0));
    Chain.Distributor.ConnectorWorldPositions.Add(TEXT("Connection1"), JunctionLocation + FVector(-100, 0, 0));
    Chain.Distributor.ConnectorWorldPositions.Add(TEXT("Connection2"), JunctionLocation + FVector(0, -100, 0));
    Chain.Distributor.ConnectorWorldPositions.Add(TEXT("Connection3"), JunctionLocation + FVector(0, 100, 0));
    Chain.Distributor.ConnectedConnectors = {TEXT("Connection1"), TEXT("Connection2")};
    Source.PipeInputChains.Add(Chain);
    auto CountLanes = [](const FSFCloneTopology& Plan)
    {
        int32 Count = 0;
        for (const FSFCloneHologram& H : Plan.ChildHolograms)
            Count += H.bIsLaneSegment && H.LaneSegmentType == TEXT("pipe") ? 1 : 0;
        return Count;
    };
    const FVector BadAxis = (FirstOffset + FVector(3400, 0, 0)).GetSafeNormal2D();
    TestTrue(TEXT("Old row-biased axis falls below native-port selection threshold"), BadAxis.Y < 0.30);
    TestEqual(TEXT("Reproduce omitted pipe plan before routing"),
        CountLanes(FSFCloneTopology::FromSource(Source, FirstOffset * 2, BadAxis)), 0);

    for (const double Direction : {-1.0, 1.0})
    for (const double RowSign : {-1.0, 1.0})
    {
        Source.PipeInputChains[0].Distributor.ConnectedConnectors =
            {TEXT("Connection1"), Direction > 0 ? TEXT("Connection2") : TEXT("Connection3")};
        const FVector Axis = SFScaledExtendGrid::PrincipalAxis(SourceLocation, SourceLocation + FirstOffset * Direction);
        TestTrue(TEXT("Forward follows held clone, including negative extend"),
            Axis.Equals((FirstOffset * Direction).GetSafeNormal2D()));
        TestTrue(TEXT("Parent vertical steps cannot tilt the planar port-selection axis"),
            Axis.Equals(SFScaledExtendGrid::PrincipalAxis(SourceLocation,
                SourceLocation + FirstOffset * Direction + FVector(0, 0, 9000))));
        int32 Count = 0;
        for (int32 Z = 0; Z < 2; ++Z)
        for (int32 Y = 0; Y < 2; ++Y)
        for (int32 X = 1; X <= 2; ++X)
        {
            const FVector Offset = FirstOffset * (Direction * X) + FVector(RowSign * Y * 3400, 0, Z * 3000);
            const FSFCloneTopology Plan = FSFCloneTopology::FromSource(Source, Offset, Axis);
            Count += CountLanes(Plan);
            for (const FSFCloneHologram& H : Plan.ChildHolograms)
            {
                if (!H.bIsLaneSegment) continue;
                TestEqual(TEXT("Stable named source port across cells"), H.CloneConnections.ConveyorAny0.Connector,
                    FString(Direction > 0 ? TEXT("Connection3") : TEXT("Connection2")));
                TestTrue(TEXT("Both lane normals retain verified named-port provenance"),
                    H.bLaneStartNormalVerified && H.bLaneEndNormalVerified);
            }
        }
        TestEqual(TEXT("All eight rotated-grid pipe lanes retained on either side and level"), Count, 8);
        // Later arced clones must reuse the first-clone axis, not their own direction.
        TestEqual(TEXT("A distant clone cannot change the source port"),
            CountLanes(FSFCloneTopology::FromSource(Source, FVector(20000, 100, 3000), Axis)), 1);
        Source.PipeInputChains[0].Distributor.ConnectedConnectors.Add(
            Direction > 0 ? TEXT("Connection3") : TEXT("Connection2"));
        TestEqual(TEXT("Occupied real source port is still excluded"),
            CountLanes(FSFCloneTopology::FromSource(Source, FirstOffset * Direction, Axis)), 0);
    }
    return true;
}
#endif
