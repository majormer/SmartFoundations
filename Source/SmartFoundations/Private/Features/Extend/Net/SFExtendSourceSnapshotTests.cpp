// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Features/Extend/Net/SFExtendSourceSnapshot.h"
#include "Buildables/FGBuildable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendSourceSnapshotNetworkTest,
    "SmartFoundations.Extend.TopologyReply.NativeNetworkMaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendSourceSnapshotNetworkTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    FSFSourceDistributor& Distributor = Source.BeltInputChains.AddDefaulted_GetRef().Distributor;
    Distributor.Id = TEXT("source_distributor");
    Distributor.ConnectorWorldPositions.Add(TEXT("Output1"), FVector(302100, -215500, 6000));
    Distributor.ConnectorWorldPositions.Add(TEXT("Input1"), FVector(302100, -215300, 6000));
    FSFSourcePowerPole& Pole = Source.PowerPoles.AddDefaulted_GetRef();
    Pole.PortCapacities.Add(TEXT("PowerConnection1"), 0);
    Pole.PortCapacities.Add(TEXT("PowerConnection2"), 3);
    UScriptStruct::ICppStructOps* Ops = FSFSourceTopology::StaticStruct()->GetCppStructOps();
    if (!TestTrue(TEXT("Native RPC uses explicit snapshot serializer"), Ops && Ops->HasNetSerializer())) return false;
    FNetBitWriter Writer(nullptr, 64 * 1024 * 8);
    bool bSuccess = false;
    Ops->NetSerialize(Writer, nullptr, bSuccess, &Source);
    if (!TestTrue(TEXT("Native network writer succeeds"), bSuccess && !Writer.IsError())) return false;
    FSFSourceTopology Received;
    FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
    Ops->NetSerialize(Reader, nullptr, bSuccess, &Received);
    if (!TestTrue(TEXT("Native network reader succeeds"), bSuccess && !Reader.IsError())) return false;
    TestEqual(TEXT("Both exact socket positions survive RPC"), Received.BeltInputChains[0].Distributor.ConnectorWorldPositions.Num(), 2);
    TestTrue(TEXT("Socket geometry is unchanged"), Received.BeltInputChains[0].Distributor.ConnectorWorldPositions.FindRef(TEXT("Output1")) == FVector(302100, -215500, 6000));
    TestEqual(TEXT("Independent power faces survive RPC"), Received.PowerPoles[0].PortCapacities.Num(), 2);
    TestEqual(TEXT("Full face stays full"), Received.PowerPoles[0].PortCapacities.FindRef(TEXT("PowerConnection1")), 0);
    TestEqual(TEXT("Other face retains its capacity"), Received.PowerPoles[0].PortCapacities.FindRef(TEXT("PowerConnection2")), 3);
    FNetBitWriter Oversized(nullptr, 64);
    uint32 TooManyBytes = 32 * 1024 + 1;
    Oversized.SerializeIntPacked(TooManyBytes);
    FNetBitReader BoundedReader(nullptr, Oversized.GetData(), Oversized.GetNumBits());
    Ops->NetSerialize(BoundedReader, nullptr, bSuccess, &Received);
    TestTrue(TEXT("Oversized network reply is rejected before value allocation"), !bSuccess && BoundedReader.IsError());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendSourceSnapshotIdentityTest,
    "SmartFoundations.Extend.TopologyReply.ExactOwnerIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendSourceSnapshotIdentityTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("server_factory");
    FSFSourceChain& Chain = Source.BeltInputChains.AddDefaulted_GetRef();
    Chain.FactoryConnector = TEXT("Input1");
    Chain.Distributor.Id = TEXT("server_splitter");
    Chain.Distributor.ConnectorUsed = TEXT("Output3");
    FSFSourceSegment& Belt = Chain.Segments.AddDefaulted_GetRef();
    Belt.Id = TEXT("unreplicated_belt");
    Belt.Connections.ConveyorAny0 = FSFConnectionRef(TEXT("server_splitter"), TEXT("Output3"));
    Belt.Connections.ConveyorAny1 = FSFConnectionRef(TEXT("server_factory"), TEXT("Input1"));
    FSFSourceSegment& Hole = Source.PipePassthroughs.AddDefaulted_GetRef();
    Hole.Id = TEXT("server_hole");
    Hole.PassthroughTop = FSFConnectionRef(TEXT("unreplicated_belt"), TEXT("ConveyorAny0"));
    Hole.RelatedSourceIds.Add(TEXT("unreplicated_belt"));
    Belt.LiftData.PassthroughCloneIds.Add(TEXT("server_hole"));
    Belt.ConnectedPowerPoleSourceId = TEXT("server_pole");
    Source.PowerPoles.AddDefaulted_GetRef().Id = TEXT("server_pole");
    TMap<FString, FString> Owners;
    Owners.Add(TEXT("server_factory"), TEXT("client_factory"));
    Owners.Add(TEXT("server_splitter"), TEXT("client_splitter"));
    Owners.Add(TEXT("server_pole"), TEXT("client_pole"));
    Owners.Add(TEXT("server_hole"), TEXT("client_hole"));
    SFExtendSourceSnapshot::RemapOwners(Source, Owners);
    TestEqual(TEXT("Factory identity adapts"), Source.Factory.Id, FString(TEXT("client_factory")));
    TestEqual(TEXT("Distributor identity adapts"), Chain.Distributor.Id, FString(TEXT("client_splitter")));
    TestEqual(TEXT("Missing replicated belt retains value identity"), Belt.Id, FString(TEXT("unreplicated_belt")));
    TestEqual(TEXT("Connection targets adapt"), Belt.Connections.ConveyorAny1.Target, Source.Factory.Id);
    TestEqual(TEXT("Exact socket survives"), Belt.Connections.ConveyorAny0.Connector, FString(TEXT("Output3")));
    TestEqual(TEXT("Pump power owner adapts"), Belt.ConnectedPowerPoleSourceId, Source.PowerPoles[0].Id);
    TestEqual(TEXT("Lift hole owner adapts"), Belt.LiftData.PassthroughCloneIds[0], Hole.Id);
    TestEqual(TEXT("Hole related conduit stays internally consistent"), Hole.RelatedSourceIds[0], Belt.Id);
    TestEqual(TEXT("Hole endpoint retains exact conduit"), Hole.PassthroughTop.Target, Belt.Id);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendSourceSnapshotBudgetTest,
    "SmartFoundations.Extend.TopologyReply.BoundedValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendSourceSnapshotBudgetTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    FSFSourceChain& Chain = Source.BeltInputChains.AddDefaulted_GetRef();
    Chain.Segments.SetNum(3);
    for (FSFSourceSegment& Belt : Chain.Segments)
    {
        Belt.Type = TEXT("belt");
        Belt.RecipeClass = TEXT("Recipe_ConveyorBeltMk5_C");
        Belt.SplineData.Points.SetNum(2);
    }
    TestTrue(TEXT("Ordinary native three-belt source fits"), SFExtendSourceSnapshot::FitsReply(Source));
    FSFSourceTopology Painted = Source;
    Painted.BeltInputChains[0].Segments.SetNum(12);
    for (FSFSourceSegment& Part : Painted.BeltInputChains[0].Segments)
    {
        Part.Customization.bCaptured = true;
        // Real source replies include long descriptor paths for each appearance field.
        const FString Path = TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/Desc_Swatch_Default.Desc_Swatch_Default_C");
        Part.Customization.SwatchClass = Path;
        Part.Customization.PatternClass = Path;
        Part.Customization.MaterialClass = Path;
        Part.Customization.SkinClass = Path;
        Part.Customization.PaintFinishClass = Path;
        Part.SplineData.Points.SetNum(2);
    }
    TestTrue(TEXT("Complete painted manifold fits without save-archive field metadata"), SFExtendSourceSnapshot::FitsReply(Painted));
    Chain.Segments[0].SplineData.Points.SetNum(65);
    TestFalse(TEXT("Single excessive spline rejected"), SFExtendSourceSnapshot::FitsReply(Source));
    Chain.Segments[0].SplineData.Points.SetNum(2);
    Chain.Segments.SetNum(65);
    TestFalse(TEXT("Excessive part count rejected"), SFExtendSourceSnapshot::FitsReply(Source));
    Chain.Segments.SetNum(3);
    Chain.Segments[0].Customization.SwatchClass = FString::ChrN(40000, TEXT('x'));
    TestFalse(TEXT("Large descriptor strings cannot bypass reply budget"), SFExtendSourceSnapshot::FitsReply(Source));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendSourceSnapshotCaptureTest,
    "SmartFoundations.Extend.TopologyReply.UnreplicatedConveyors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendSourceSnapshotCaptureTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine"), GEngine)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    FActorSpawnParameters Spawn;
    Spawn.bDeferConstruction = true;
    UClass* FactoryClass = LoadClass<AFGBuildable>(nullptr,
        TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1.Build_SmelterMk1_C"));
    if (!TestNotNull(TEXT("Concrete deferred owner class"), FactoryClass)) return false;
    AFGBuildable* Factory = World->SpawnActor<AFGBuildable>(FactoryClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Owner fixture"), Factory)) return false;
    FSFExtendTopology Reply;
    Reply.SourceBuilding = Factory;
    Reply.bIsValid = true;
    Reply.bHasSourceSnapshot = true;
    Reply.SourceSnapshot.Factory.Id = TEXT("authority_factory");
    FSFExtendSourceOwner& Owner = Reply.SourceOwners.AddDefaulted_GetRef();
    Owner.Id = Reply.SourceSnapshot.Factory.Id;
    Owner.Actor = Factory;
    Reply.InputChains.AddDefaulted_GetRef().Conveyors.Add(nullptr);
    FSFSourceChain& Chain = Reply.SourceSnapshot.BeltInputChains.AddDefaulted_GetRef();
    FSFSourceSegment& Belt = Chain.Segments.AddDefaulted_GetRef();
    Belt.Id = TEXT("authority_only_belt");
    Belt.Type = TEXT("belt");
    Belt.RecipeClass = TEXT("Recipe_ConveyorBeltMk5_C");
    Belt.Connections.ConveyorAny1 = FSFConnectionRef(Owner.Id, TEXT("Input1"));
    Belt.bHasSplineData = true;
    Belt.SplineData.Length = 500;
    Belt.SplineData.Points.SetNum(2);
    Belt.Customization.bCaptured = true;
    Belt.Customization.OverridePrimary = FLinearColor::Red;
    TestTrue(TEXT("Reply resolves using the exact replicated owner"), SFExtendSourceSnapshot::ResolveReply(Reply));
    const FSFSourceTopology Captured = FSFSourceTopology::CaptureFromTopology(Reply);
    TestEqual(TEXT("Null conveyor reference cannot remove quoted segment"), Captured.BeltInputChains[0].Segments.Num(), 1);
    const FSFSourceSegment& CapturedBelt = Captured.BeltInputChains[0].Segments[0];
    TestEqual(TEXT("Recipe retained for cost"), CapturedBelt.RecipeClass, Belt.RecipeClass);
    TestEqual(TEXT("Spline retained for native preview"), CapturedBelt.SplineData.Length, 500.f);
    TestEqual(TEXT("Endpoint resolves to client factory"), CapturedBelt.Connections.ConveyorAny1.Target, Factory->GetName());
    TestTrue(TEXT("Captured appearance survives without source belt"), CapturedBelt.Customization.OverridePrimary == FLinearColor::Red);
    Reply.SourceSnapshot.Factory.Id = TEXT("different_source");
    TestFalse(TEXT("Reply cannot apply values from another source"), SFExtendSourceSnapshot::ResolveReply(Reply));
    TestEqual(TEXT("Mismatched values cannot fall back to incomplete local geometry"),
        FSFSourceTopology::CaptureFromTopology(Reply).BeltInputChains.Num(), 0);
    Reply.Reset();
    TestFalse(TEXT("Reset clears snapshot presence"), Reply.bHasSourceSnapshot);
    TestEqual(TEXT("Reset clears owner aliases"), Reply.SourceOwners.Num(), 0);
    return true;
}
#endif
