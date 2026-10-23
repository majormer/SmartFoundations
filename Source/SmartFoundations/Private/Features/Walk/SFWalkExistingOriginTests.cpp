// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkExistingOrigin.h"
#include "Features/Walk/SFWalkBusLayout.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Hologram/FGHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkExistingOriginContractTest,
    "SmartFoundations.Walking.ExistingOriginContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkExistingOriginContractTest::RunTest(const FString& Parameters)
{
    FSFWalkCommitSpec Spec;
    Spec.OriginFrame = FTransform(FRotator(0, 225, 0), FVector(1000, 2000, 3000));
    Spec.LaneSpacing = 300; Spec.StackSpacing = 400;
    Spec.ExistingLanes = 3; Spec.ExistingStacks = 3;
    FSFWalkCommitSegment Segment;
    Segment.NumLanes = 3; Segment.NumStacks = 3; Segment.Advance = 2400;
    Spec.Segments = {Segment, Segment};
    const FVector Offset(0, 0, 175);
    FSFPositionCalculator Calculator; FSFCounterState Counters;
    for (int32 Cell = 0; Cell < 9; ++Cell)
    {
        FSFWalkExistingEndpoint Endpoint;
        Endpoint.Position = Calculator.CalculateChildPosition(0, Cell / 3, Cell % 3,
            Spec.OriginFrame.GetLocation(), Spec.OriginFrame.Rotator(), FVector(0, 300, 400), Counters)
            + Spec.OriginFrame.TransformVectorNoScale(Offset);
        Endpoint.Outward = -Spec.OriginFrame.GetUnitAxis(EAxis::X);
        Endpoint.bReverseFlow = Cell % 2 != 0;
        Spec.ExistingEndpoints.Add(Endpoint);
    }
    TestTrue(TEXT("Rotated manual bus with mixed flow and native connector offset"), SFWalkExistingOrigin::ValidateGeometry(Spec, Offset));
    auto Bad = Spec; Bad.ExistingEndpoints.Swap(0, 1);
    TestFalse(TEXT("Swapped lane identities rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.LaneSpacing = 200;
    TestFalse(TEXT("Forged grid spacing rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.OriginFrame.AddToTranslation(FVector(10, 0, 0));
    TestFalse(TEXT("Moved origin rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.ExistingEndpoints[0].Outward *= -1;
    TestFalse(TEXT("Reversed native connector rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.Segments[1].NumLanes = 2;
    TestFalse(TEXT("Changing existing lane mapping rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.Segments[0].Rise = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite segment rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.Segments[0].Advance = 0;
    TestFalse(TEXT("Zero advance rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    Bad = Spec; Bad.Segments.SetNum(456);
    for (auto& Item : Bad.Segments) { Item = Segment; }
    TestFalse(TEXT("Excessive materialization rejected"), SFWalkExistingOrigin::ValidateGeometry(Bad, Offset));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkBusExpansionTest,
    "SmartFoundations.Walking.BusExpansion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkBusExpansionTest::RunTest(const FString& Parameters)
{
    // Four existing lanes, two existing stacks: arbitrary growth on either side
    // must map each original exactly once, with only added cells charged at origin.
    for (int32 Lanes : {4, 6, -6, 17, -17})
    {
        for (int32 Stacks : {2, 5, -5})
        {
            TSet<int32> Existing;
            int32 NewOrigins = 0;
            for (int32 Cell = 0; Cell < FMath::Abs(Lanes) * FMath::Abs(Stacks); ++Cell)
            {
                const int32 Index = SFWalkBusLayout::ExistingIndex(Cell, Lanes, Stacks, 4, 2);
                if (Index == INDEX_NONE) { ++NewOrigins; continue; }
                TestFalse(TEXT("No original is duplicated"), Existing.Contains(Index));
                Existing.Add(Index);
                const int32 Lane = SFWalkBusLayout::Coordinate(Cell / FMath::Abs(Stacks), Lanes, 4);
                const int32 Stack = SFWalkBusLayout::Coordinate(Cell % FMath::Abs(Stacks), Stacks, 2);
                TestEqual(TEXT("Original geometry and flow identity stay fixed"), Index, Lane * 2 + Stack);
            }
            TestEqual(TEXT("All originals preserved"), Existing.Num(), 8);
            TestEqual(TEXT("Only added origins need construction"), NewOrigins, FMath::Abs(Lanes) * FMath::Abs(Stacks) - 8);
        }
    }
    TestEqual(TEXT("Grow beyond detected width"), SFWalkBusLayout::Step(4, 2, 4), 6);
    TestEqual(TEXT("Return to detected width"), SFWalkBusLayout::Step(6, -2, 4), 4);
    TestEqual(TEXT("Crossing origin adds on opposite side"), SFWalkBusLayout::Step(4, -1, 4), -5);
    TestEqual(TEXT("Opposite expansion returns without removing originals"), SFWalkBusLayout::Step(-5, 1, 4), 4);
    TestFalse(TEXT("Shrinking original cross-section refused"), SFWalkBusLayout::IsValid(3, 2, 4, 2));
    TestFalse(TEXT("Ambiguous negative original-only counter refused"), SFWalkBusLayout::IsValid(-4, 2, 4, 2));
    TestFalse(TEXT("Oversized cross-section refused"), SFWalkBusLayout::IsValid(256, 256, 4, 2));
    TestFalse(TEXT("Integer-min counter refused before absolute value"), SFWalkBusLayout::IsValid(MIN_int32, 2, 4, 2));
    // Authority binds original poses even when the signed expanded rectangle changes.
    FSFWalkCommitSpec Spec;
    Spec.ExistingLanes = 4; Spec.ExistingStacks = 1;
    Spec.OriginFrame = FTransform(FRotator(0, 225, 0), FVector(1000, 2000, 3000));
    FSFWalkCommitSegment Segment; Segment.Advance = 1200; Segment.NumLanes = -7;
    Spec.Segments = {Segment};
    FSFPositionCalculator Calculator; FSFCounterState Counters;
    const FVector Offset(0, 0, 300);
    for (int32 Lane = 0; Lane < 4; ++Lane)
    {
        FSFWalkExistingEndpoint Endpoint;
        Endpoint.Position = Calculator.CalculateChildPosition(0, Lane, 0, Spec.OriginFrame.GetLocation(),
            Spec.OriginFrame.Rotator(), FVector(0, 200, 200), Counters) + Spec.OriginFrame.TransformVectorNoScale(Offset);
        Endpoint.Outward = -Spec.OriginFrame.GetUnitAxis(EAxis::X);
        Endpoint.bReverseFlow = Lane % 2 != 0;
        Spec.ExistingEndpoints.Add(Endpoint);
    }
    TestTrue(TEXT("Expanded rotated bus authority geometry accepted"), SFWalkExistingOrigin::ValidateGeometry(Spec, Offset));
    Spec.ExistingLanes = 3;
    TestFalse(TEXT("Forged original dimensions refused"), SFWalkExistingOrigin::ValidateGeometry(Spec, Offset));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkExistingAnchorTest,
    "SmartFoundations.Walking.ExistingAnchorState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkExistingAnchorTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Preview world"), World)) { return false; }
    AFGHologram* Seed = World->SpawnActor<AFGHologram>();
    USceneComponent* Root = NewObject<USceneComponent>(Seed, TEXT("AnchorRoot"));
    Seed->SetRootComponent(Root); Root->RegisterComponent();
    UFGPipeConnectionComponent* Snap = NewObject<UFGPipeConnectionComponent>(Seed, TEXT("SnapOnly0"));
    Snap->SetupAttachment(Root); Snap->RegisterComponent();
    Snap->SetRelativeLocation(FVector(0, 0, 375));
    const FTransform Pose(FRotator(0, 45, 0), FVector(1234, -5678, 900));
    Seed->SetActorTransform(Pose);
    FVector Offset;
    TestTrue(TEXT("Generic pipe anchor accepted for Hypertube support"),
        SFWalkExistingOrigin::TryConnectorOffset(Seed, ESFWalkConveyanceType::Hypertube, Offset));
    TestTrue(TEXT("Native 375 cm offset retained under parent rotation"), Offset.Equals(FVector(0, 0, 375), 0.001));
    TestFalse(TEXT("Pipe anchor cannot become a conveyor anchor"),
        SFWalkExistingOrigin::TryConnectorOffset(Seed, ESFWalkConveyanceType::Belt, Offset));
    Snap->DestroyComponent();
    UFGPipeConnectionComponentHyper* HyperSnap = NewObject<UFGPipeConnectionComponentHyper>(Seed, TEXT("Snap Only"));
    HyperSnap->SetupAttachment(Root); HyperSnap->RegisterComponent();
    HyperSnap->SetRelativeLocation(FVector(0, 0, 375));
    TestTrue(TEXT("Converted Hyper anchor retains its native spaced name"),
        SFWalkExistingOrigin::TryConnectorOffset(Seed, ESFWalkConveyanceType::Hypertube, Offset));
    TestTrue(TEXT("Converted Hyper offset retained"), Offset.Equals(FVector(0, 0, 375), 0.001));
    HyperSnap->DestroyComponent();
    TestFalse(TEXT("Missing native anchor does not silently become zero"),
        SFWalkExistingOrigin::TryConnectorOffset(Seed, ESFWalkConveyanceType::Hypertube, Offset));
    AFGHologram* BeltSeed = World->SpawnActor<AFGHologram>();
    USceneComponent* BeltRoot = NewObject<USceneComponent>(BeltSeed, TEXT("AnchorRoot"));
    BeltSeed->SetRootComponent(BeltRoot); BeltRoot->RegisterComponent();
    UFGFactoryConnectionComponent* BeltSnap = NewObject<UFGFactoryConnectionComponent>(BeltSeed, TEXT("SnapOnly0"));
    BeltSnap->SetupAttachment(BeltRoot); BeltSnap->RegisterComponent();
    BeltSnap->SetRelativeLocation(FVector(0, 0, 300));
    TestTrue(TEXT("Conveyor native anchor remains supported"),
        SFWalkExistingOrigin::TryConnectorOffset(BeltSeed, ESFWalkConveyanceType::Belt, Offset));
    TestTrue(TEXT("Conveyor offset retained"), Offset.Equals(FVector(0, 0, 300), 0.001));
    // Native SetActorHiddenInGame and lock methods are SDK stubs. Visibility and
    // lock rebasing therefore require Shipping runtime acceptance; test only pose here.
    Seed->SetActorHiddenInGame(true);
    const FTransform NewPose(FRotator(0, 90, 0), FVector(2345, -6789, 1000));
    SFWalkExistingOrigin::SetAnchorPose(Seed, NewPose);
    TestTrue(TEXT("Visibility repair retains canonical pose"), Seed->GetActorTransform().Equals(NewPose));
    World->DestroyWorld(false);
    return true;
}
#endif
