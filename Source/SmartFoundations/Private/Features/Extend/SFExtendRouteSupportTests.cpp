// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/Net/SFExtendSourceSnapshot.h"
#include "Features/Extend/SFExtendRouteSupports.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendRouteSupportPlanTest,
    "SmartFoundations.Extend.RouteSupports.SurvivingRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendRouteSupportPlanTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("factory");
    Source.Factory.Class = TEXT("Build_ConstructorMk1_C");
    FSFSourceChain& Chain = Source.BeltInputChains.AddDefaulted_GetRef();
    Chain.ChainId = TEXT("belt_input_0");
    Chain.Distributor.Id = TEXT("splitter");
    Chain.Distributor.Class = TEXT("Build_ConveyorAttachmentSplitter_C");
    Chain.Distributor.ConnectorUsed = TEXT("Output3");
    FSFSourceSegment& Belt = Chain.Segments.AddDefaulted_GetRef();
    Belt.Id = TEXT("route"); Belt.Type = TEXT("belt"); Belt.Class = TEXT("Build_ConveyorBeltMk1_C");
    FSFSourceSegment Support;
    Support.Id = TEXT("support"); Support.Class = TEXT("Build_ConveyorPole_C");
    Support.Transform = FSFTransform(FVector(100, 200, 300), FRotator::ZeroRotator);
    Support.RelatedSourceIds = {TEXT("route")};
    Support.SelectedPole.bCaptured = true; Support.SelectedPole.Height = 300; Support.SelectedPole.Variation = 1;
    Source.RouteSupports = {Support, Support}; // A pole shared by two discovered chains emits once.
    Support.Id = TEXT("unrelated"); Support.RelatedSourceIds = {TEXT("other-route")};
    Source.RouteSupports.Add(Support);
    const FVector Offset(1000, 2000, 400);
    auto Supports = [](const FSFCloneTopology& Clone)
    {
        return Clone.ChildHolograms.FilterByPredicate([](const FSFCloneHologram& Holo) { return Holo.Role == TEXT("route_support"); });
    };
    const auto Kept = Supports(FSFCloneTopology::FromSource(Source, Offset));
    TestEqual(TEXT("Shared support emits once; unrelated support is excluded"), Kept.Num(), 1);
    if (Kept.Num() == 1)
    {
        TestEqual(TEXT("Captured native variation survives"), Kept[0].SelectedPole.Variation, 1);
        TestEqual(TEXT("Captured height survives"), Kept[0].SelectedPole.Height, 300.f);
        TestTrue(TEXT("Support moves with the factory module"), Kept[0].Transform.Location.ToFVector().Equals(FVector(1100, 2200, 700)));
    }
    for (const FString Class : {FString(TEXT("Build_ConveyorPoleStackable_C")), FString(TEXT("Build_PipeSupportStackable_C")),
        FString(TEXT("Build_ConveyorPoleWall_C")), FString(TEXT("Build_ConveyorCeilingAttachment_C")), FString(TEXT("Build_PipelineSupportWall_C"))})
    {
        Support.Id = TEXT("fixed-support"); Support.Class = Class;
        Support.RelatedSourceIds = {TEXT("route")}; Support.SelectedPole = FSFSelectedPoleState();
        Source.RouteSupports = {Support, Support};
        const auto Fixed = Supports(FSFCloneTopology::FromSource(Source, Offset));
        TestEqual(TEXT("Fixed support emits once without invented height selection"), Fixed.Num(), 1);
        TestTrue(TEXT("Fixed companion uses an explicit native family"), SFExtendRouteSupports::IsFixedSupport(FName(*Class)));
        if (Fixed.Num() == 1)
        {
            TestEqual(TEXT("Fixed recipe/build class survives"), Fixed[0].BuildClass, Class);
            TestFalse(TEXT("No adjustable-pole state fabricated"), Fixed[0].SelectedPole.bCaptured);
            TestTrue(TEXT("Copied pose survives without fresh snapping"), Fixed[0].Transform.Location.ToFVector().Equals(FVector(1100, 2200, 700)));
        }
    }
    Support.Class = TEXT("Build_ConveyorPole_C"); Source.RouteSupports = {Support};
    TestEqual(TEXT("Adjustable pole with missing selected state is rejected"), Supports(FSFCloneTopology::FromSource(Source, Offset)).Num(), 0);
    Support.Class = TEXT("Build_ConveyorPoleStackable_C"); Source.RouteSupports = {Support};
    TestFalse(TEXT("Unproven wall mount cannot use fixed stackable reconstruction"), SFExtendRouteSupports::IsFixedStackable(TEXT("Build_ConveyorPoleWall_C")));
    Chain.Distributor.ConnectedConnectors = {TEXT("Input1"), TEXT("Output1")};
    TestEqual(TEXT("Excluded branch cannot leave an orphan support"), Supports(FSFCloneTopology::FromSource(Source, Offset)).Num(), 0);
    Source.RouteSupports.SetNum(65);
    TestFalse(TEXT("Support records count toward the bounded network reply"), SFExtendSourceSnapshot::FitsReply(Source));
    return true;
}
#endif
