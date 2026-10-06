// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendPassthroughLinks.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPassthroughLinksTest, "SmartFoundations.Extend.Passthrough.ScopedIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPassthroughLinksTest::RunTest(const FString& Parameters)
{
    for (const FString& Prefix : {FString(), FString(TEXT("sc_2_1_3_")), FString(TEXT("rr_1_2_3_")), FString(TEXT("sc0_"))})
    for (const float Thickness : {100.f, 200.f, 400.f})
    for (const FRotator Rotation : {FRotator::ZeroRotator, FRotator(90, 0, 0)})
    {
        const FTransform Transform(Rotation, FVector(10000, -4000, 2000));
        FSFPassthroughFace Top, Bottom;
        Top.HoleId = Prefix + TEXT("passthrough_0");
        Top.bTop = true;
        Top.bCaptured = true;
        Top.Location = Transform.TransformPosition(FVector(0, 0, Thickness * 0.5));
        Top.CapturedEndpoint = FSFConnectionRef(Prefix + TEXT("pipe_segment_0"), TEXT("PipeConnection1"));
        Bottom = Top;
        Bottom.bTop = false;
        Bottom.Location = Transform.TransformPosition(FVector(0, 0, Thickness * -0.5));
        Bottom.CapturedEndpoint = FSFConnectionRef(Prefix + TEXT("pipe_segment_1"), TEXT("PipeConnection0"));
        FSFPassthroughEndpoint A, B, Other;
        A.Identity = Top.CapturedEndpoint; A.Location = Top.Location;
        B.Identity = Bottom.CapturedEndpoint; B.Location = Bottom.Location;
        Other.Identity = FSFConnectionRef(TEXT("sc_999_0_0_pipe_segment_0"), TEXT("PipeConnection1"));
        Other.Location = Top.Location;
        TArray<FSFPassthroughFace> Faces = {Top, Bottom};
        TArray<FSFPassthroughEndpoint> Endpoints = {Other, B, A};
        auto Plan = SFExtendPassthroughLinks::Plan(Faces, Endpoints);
        TestEqual(TEXT("Independent faces at actual thickness"), Plan.Num(), 2);
        if (Plan.Num() == 2)
        {
            TestEqual(TEXT("Top retains exact named pipe endpoint despite enumeration order"), Plan[0].Value, 2);
            TestEqual(TEXT("Bottom retains exact named pipe endpoint"), Plan[1].Value, 1);
        }
        Faces[0].bOccupied = true;
        TestEqual(TEXT("Occupied face cannot be overwritten"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
        Faces[0].bOccupied = false;
        // Native floor-hole construction can inset a snapped pipe endpoint into the
        // foundation rather than leaving it on the geometric outer face.
        Endpoints[2].Location.Z -= Thickness * 0.5f;
        TestEqual(TEXT("Captured identity retains a native inset endpoint"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 2);
        Endpoints[2] = A;
        Faces[0].CapturedEndpoint.Connector = TEXT("Missing");
        TestEqual(TEXT("Wrong named endpoint never falls back to proximity"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
        Faces[0].CapturedEndpoint = FSFConnectionRef();
        TestEqual(TEXT("Captured empty face stays empty"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
        Faces[0].bCaptured = false;
        TestEqual(TEXT("Legacy face recovers unique same-cell endpoint"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 2);
        Endpoints[2].Location.Z += 400;
        TestEqual(TEXT("Same XY on unrelated floor is rejected"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
        Endpoints[2] = A;
        Endpoints.Add(A);
        TestEqual(TEXT("Ambiguous legacy endpoint is rejected"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
        Endpoints.Pop();
        FSFPassthroughFace Coincident = Faces[0];
        Coincident.HoleId = Prefix + TEXT("passthrough_1");
        Faces.Add(Coincident);
        TestEqual(TEXT("One endpoint cannot be assigned to two holes"), SFExtendPassthroughLinks::Plan(Faces, Endpoints).Num(), 1);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPassthroughCapturePlanTest, "SmartFoundations.Extend.Passthrough.CapturedPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPassthroughCapturePlanTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("source-factory");
    Source.Factory.Class = TEXT("Build_OilRefinery_C");
    FSFSourceChain Chain;
    Chain.ChainId = TEXT("pipe_input_0");
    Chain.Distributor.Id = TEXT("source-junction");
    Chain.Distributor.Class = TEXT("Build_PipelineJunction_Cross_C");
    Chain.Distributor.ConnectorUsed = TEXT("Connection2");
    FSFSourceSegment Pipe;
    Pipe.Id = TEXT("source-pipe");
    Pipe.Type = TEXT("pipe");
    Pipe.Class = TEXT("Build_Pipeline_C");
    Chain.Segments.Add(Pipe);
    FSFSourceSegment Hole;
    Hole.Id = TEXT("source-hole");
    Hole.Type = TEXT("passthrough");
    Hole.Class = TEXT("Build_FoundationPassthrough_Pipe_C");
    Hole.Thickness = 400;
    Hole.bHasPassthroughLinks = true;
    Hole.PassthroughTop = FSFConnectionRef(Pipe.Id, TEXT("PipeConnection1"));
    Hole.PassthroughBottom = FSFConnectionRef(TEXT("unrelated-pipe"), TEXT("PipeConnection0"));
    Hole.RelatedSourceIds = {Pipe.Id};
    Chain.Segments.Add(Hole);
    Source.PipeInputChains.Add(Chain);
    Source.PipePassthroughs.Add(Hole); // Duplicate discovery must not emit a second actor.
    Hole.Id = TEXT("second-hole");
    Source.PipePassthroughs.Add(Hole);
    const FSFCloneTopology Clone = FSFCloneTopology::FromSource(Source, FVector(1000, 200, 400));
    FString PipeClone;
    for (const FSFCloneHologram& Holo : Clone.ChildHolograms)
        if (Holo.SourceId == Pipe.Id) PipeClone = Holo.HologramId;
    TestFalse(TEXT("Captured pipe survives clone planning"), PipeClone.IsEmpty());
    TSet<FString> HoleIds;
    int32 HoleCount = 0;
    for (const FSFCloneHologram& Holo : Clone.ChildHolograms)
    {
        if (Holo.Role != TEXT("passthrough")) continue;
        ++HoleCount;
        HoleIds.Add(Holo.HologramId);
        TestTrue(TEXT("Exact link marker retained"), Holo.bHasPassthroughLinks);
        TestEqual(TEXT("Exact pipe remapped into this clone"), Holo.PassthroughTop.Target, PipeClone);
        TestEqual(TEXT("Pipe component identity retained"), Holo.PassthroughTop.Connector, FString(TEXT("PipeConnection1")));
        TestTrue(TEXT("External owner does not become parent or a nearby pipe"), Holo.PassthroughBottom.Target.IsEmpty());
        TestEqual(TEXT("Native captured thickness retained"), Holo.Thickness, 400.f);
    }
    TestEqual(TEXT("Chain and spatial captures share one physical hole"), HoleCount, 2);
    TestEqual(TEXT("Distinct holes have distinct clone IDs"), HoleIds.Num(), 2);
    return true;
}
#endif
