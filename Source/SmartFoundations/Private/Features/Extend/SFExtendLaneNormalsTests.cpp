// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendLaneNormals.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Shared/Conduits/SFDistributorTopology.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestoreLaneNormalsTest, "SmartFoundations.Restore.Lanes.SocketNormals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRestoreLaneNormalsTest::RunTest(const FString& Parameters)
{
    const FString Cross = TEXT("Build_PipelineJunction_Cross_C");
    const FString Tee = TEXT("Build_PipelineJunction_T_C");
    FVector Direction;
    TestTrue(TEXT("T branch exists"), FSFDistributorTopologyResolver::GetLocalPortDirection(Tee, TEXT("Connection2"), Direction));
    TestTrue(TEXT("T branch is negative Y"), Direction.Equals(-FVector::RightVector));
    TestFalse(TEXT("T has no fourth port"), FSFDistributorTopologyResolver::GetLocalPortDirection(Tee, TEXT("Connection3"), Direction));
    TestTrue(TEXT("Cross third port exists"), FSFDistributorTopologyResolver::GetLocalPortDirection(Cross, TEXT("Connection2"), Direction));
    TestTrue(TEXT("Cross third port is positive Y"), Direction.Equals(FVector::RightVector));
    TestFalse(TEXT("Unknown classes have no invented map"), FSFDistributorTopologyResolver::GetLocalPortDirection(TEXT("ModdedJunction"), TEXT("Connection0"), Direction));

    for (const FString& Type : {FString(TEXT("belt")), FString(TEXT("pipe"))})
    for (const float Yaw : {0.f, 90.f, -90.f, 180.f})
    {
        const FRotator Rotation(0, Yaw, 0);
        FSFCloneHologram Lane;
        Lane.bIsLaneSegment = true;
        Lane.LaneSegmentType = Type;
        Lane.LaneFromConnector = Type == TEXT("pipe") ? TEXT("Connection1") : TEXT("Output1");
        Lane.LaneToConnector = Type == TEXT("pipe") ? TEXT("Connection0") : TEXT("Input1");
        Lane.LaneStartNormal = FSFVec3(Rotation.RotateVector(FVector::ForwardVector));
        Lane.LaneEndNormal = FSFVec3(Rotation.RotateVector(-FVector::ForwardVector));
        SFExtendLaneNormals::VerifyCapture(Lane, Type == TEXT("pipe") ? Cross : TEXT("Build_ConveyorAttachmentMerger_C"), Rotation);
        TestTrue(TEXT("Named start verified"), Lane.bLaneStartNormalVerified);
        TestTrue(TEXT("Named end verified"), Lane.bLaneEndNormalVerified);
        // The chord is deliberately neither socket axis: spacing, steps and stagger.
        const FVector Start(900, -400, 1200);
        const FVector End = Start + Rotation.RotateVector(FVector(800, 250, 400));
        const FVector ExpectedStart = Lane.LaneStartNormal.ToFVector();
        const FVector ExpectedEnd = Lane.LaneEndNormal.ToFVector();
        SFExtendLaneNormals::RepairUnverified(Lane, Start, End);
        TestTrue(TEXT("Bent lane retains start socket normal"), Lane.LaneStartNormal.ToFVector().Equals(ExpectedStart));
        TestTrue(TEXT("Stepped lane retains end socket normal"), Lane.LaneEndNormal.ToFVector().Equals(ExpectedEnd));
        Lane.bLaneEndNormalVerified = false;
        Lane.LaneEndNormal = FSFVec3(FVector::UpVector);
        SFExtendLaneNormals::RepairUnverified(Lane, Start, End);
        TestTrue(TEXT("One legacy endpoint does not poison verified endpoint"), Lane.LaneStartNormal.ToFVector().Equals(ExpectedStart));
        TestTrue(TEXT("Unproven legacy end retains chord recovery"), Lane.LaneEndNormal.ToFVector().Equals((Start - End).GetSafeNormal()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestoreLegacyLaneNormalsTest, "SmartFoundations.Restore.Lanes.LegacyRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRestoreLegacyLaneNormalsTest::RunTest(const FString& Parameters)
{
    FSFCloneTopology Topology;
    Topology.WorldOffset = FSFVec3(FVector(1000, 200, 400));
    FSFCloneHologram Junction;
    Junction.HologramId = TEXT("junction_0");
    Junction.SourceId = TEXT("source-junction");
    Junction.Role = TEXT("pipe_junction");
    Junction.BuildClass = TEXT("Build_PipelineJunction_T_C");
    Junction.Transform = FSFTransform(FVector(2000, 1000, 300), FRotator(0, 90, 0));
    Topology.ChildHolograms.Add(Junction);
    FSFCloneHologram Lane;
    Lane.bIsLaneSegment = true;
    Lane.LaneSegmentType = TEXT("pipe");
    Lane.CloneConnections.ConveyorAny0 = FSFConnectionRef(TEXT("source:source-junction"), TEXT("Connection1"));
    Lane.CloneConnections.ConveyorAny1 = FSFConnectionRef(TEXT("junction_0"), TEXT("Connection0"));
    FSFSplinePoint Start, End;
    Start.World = FSFVec3(FVector(1000, 900, -100));
    End.World = FSFVec3(FVector(2000, 900, 300));
    Lane.SplineData.Points = {Start, End};
    Lane.LaneStartNormal = FSFVec3(FVector::UpVector);
    Lane.LaneEndNormal = FSFVec3(FVector::UpVector);
    SFExtendLaneNormals::RecoverLegacy(Topology, Lane);
    TestTrue(TEXT("Legacy source pose recovered through source identity and offset"), Lane.bLaneStartNormalVerified);
    TestTrue(TEXT("Legacy clone pose recovered through clone identity"), Lane.bLaneEndNormalVerified);
    TestTrue(TEXT("Poisoned start corrected from named socket"), Lane.LaneStartNormal.ToFVector().Equals(FVector::RightVector));
    TestTrue(TEXT("Poisoned end corrected from named socket"), Lane.LaneEndNormal.ToFVector().Equals(-FVector::RightVector));
    Lane.bLaneStartNormalVerified = false;
    Lane.bLaneEndNormalVerified = false;
    Lane.SplineData.Points[0].World.Z += 100;
    Lane.CloneConnections.ConveyorAny1.Connector = TEXT("Connection3");
    SFExtendLaneNormals::RecoverLegacy(Topology, Lane);
    TestFalse(TEXT("Wrong endpoint geometry is not proof"), Lane.bLaneStartNormalVerified);
    TestFalse(TEXT("Missing T socket is not invented"), Lane.bLaneEndNormalVerified);
    Lane.SplineData.Points[0] = Start;
    Topology.ChildHolograms.Add(Junction);
    SFExtendLaneNormals::RecoverLegacy(Topology, Lane);
    TestFalse(TEXT("Ambiguous owner cannot establish direction"), Lane.bLaneStartNormalVerified);
    return true;
}
#endif
