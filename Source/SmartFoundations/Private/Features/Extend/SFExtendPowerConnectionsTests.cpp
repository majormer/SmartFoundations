// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFExtendFactoryPower.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFRestoreGrid.h"
#include "Features/Restore/SFRestoreService.h"
#include "Holograms/Power/SFWireHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPowerIdentityTest, "SmartFoundations.Extend.Power.Identity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPowerIdentityTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    AActor* Outlet = World->SpawnActor<AActor>();
    AActor* Neighbor = World->SpawnActor<AActor>();
    auto AddPort = [](AActor* Actor, const TCHAR* Name)
    {
        auto* Port = NewObject<UFGPowerConnectionComponent>(Actor, FName(Name));
        Actor->AddInstanceComponent(Port);
        return Port;
    };
    UFGPowerConnectionComponent* Face1 = AddPort(Outlet, TEXT("PowerConnection1"));
    TestTrue(TEXT("Legacy single port resolves"), SFExtendPowerConnections::Resolve(Outlet, FString()) == Face1);
    UFGPowerConnectionComponent* Face2 = AddPort(Outlet, TEXT("PowerConnection2"));
    AddPort(Neighbor, TEXT("Missing"));
    TestTrue(TEXT("Selected back face survives actor rotation"), SFExtendPowerConnections::Resolve(Outlet, TEXT("PowerConnection2")) == Face2);
    Outlet->SetActorRotation(FRotator(0, 90, 0));
    TestTrue(TEXT("Rotation does not select another face"), SFExtendPowerConnections::Resolve(Outlet, TEXT("PowerConnection2")) == Face2);
    TestNull(TEXT("Legacy double outlet is ambiguous"), SFExtendPowerConnections::Resolve(Outlet, FString()));
    TestNull(TEXT("Missing named face does not fall back or select neighbor"), SFExtendPowerConnections::Resolve(Outlet, TEXT("Missing")));
    TestNull(TEXT("Missing owner"), SFExtendPowerConnections::Resolve(nullptr, TEXT("PowerConnection1")));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPowerPersistenceTest, "SmartFoundations.Extend.Power.Persistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPowerPersistenceTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("factory-source");
    Source.Factory.Class = TEXT("Build_ConstructorMk1_C");
    Source.Factory.Power.Connector = TEXT("PowerConnection");
    Source.Factory.Power.LocalPosition = FSFVec3(FVector(0, -600, 800));
    Source.Factory.Power.Capacity = 2;
    Source.Factory.Power.SourceFreeConnections = 1;
    Source.Factory.Power.MaxWireLength = 10000;
    Source.Factory.Power.bHasWire = true;
    Source.Factory.Power.bContinuesChain = true;
    Source.Factory.Power.bRequested = false; // Preserve metadata even when this build opts out.
    FSFSourcePowerPole Pole;
    Pole.Id = TEXT("pole-source"); Pole.Class = TEXT("Build_PowerPoleWallDouble_C");
    Pole.PoleConnectorName = TEXT("PowerConnection2");
    Pole.FactoryConnectorName = TEXT("PowerConnection");
    Pole.PortCapacities.Add(TEXT("PowerConnection1"), 4);
    Pole.PortCapacities.Add(TEXT("PowerConnection2"), 4);
    Pole.bHasConnectorWorld = true;
    Pole.PoleConnectorWorld = FSFVec3(FVector(-70, 0, 0));
    Pole.FactoryConnectorWorld = FSFVec3(FVector(300, 400, 0));
    Pole.bSourceHasFreeConnections = false;
    Source.PowerPoles.Add(Pole);
    FSFCloneTopology Clone = FSFCloneTopology::FromSource(Source, FVector(1000, 0, 0));
    TestNull(TEXT("Full source face emits no source cable or charge"), SFExtendPowerConnections::Find(&Clone, TEXT("wire_source_clone_0")));
    const FSFCloneHologram* Wire = SFExtendPowerConnections::Find(&Clone, TEXT("wire_factory_pole_0"));
    if (!TestNotNull(TEXT("Internal factory cable"), Wire)) return false;
    TestEqual(TEXT("Exact pole face"), Wire->PowerTo.Connector, Pole.PoleConnectorName);
    TestEqual(TEXT("Exact factory socket"), Wire->PowerFrom.Connector, Pole.FactoryConnectorName);
    TestTrue(TEXT("Preview endpoint is back face"), Wire->SplineData.Points.Last().World.ToFVector().Equals(FVector(930, 0, 0)));

    USFRestoreService* Service = NewObject<USFRestoreService>();
    FSFRestorePreset Preset;
    Preset.Name = TEXT("Power identity regression");
    Preset.bHasExtendTopology = true;
    Preset.BuildingClassName = Source.Factory.Class;
    Preset.ExtendCloneTopology = Clone;
    FSFCloneHologram Lane;
    Lane.HologramId = TEXT("lane_persistence");
    Lane.bIsLaneSegment = true;
    Lane.LaneSegmentType = TEXT("pipe");
    Lane.bLaneStartNormalVerified = true;
    Lane.bLaneEndNormalVerified = false;
    Lane.LaneStartNormal = FSFVec3(FVector::RightVector);
    Lane.LaneEndNormal = FSFVec3(-FVector::RightVector);
    Lane.bHasPassthroughLinks = true;
    Lane.PowerMaxLength = 10000;
    Lane.PassthroughTop = FSFConnectionRef(TEXT("pipe_segment_3"), TEXT("PipeConnection1"));
    Preset.ExtendCloneTopology.ChildHolograms.Add(Lane);
    FSFRestorePreset Reloaded;
    TestTrue(TEXT("Actual preset JSON round trip"), Service->JsonToPreset(Service->PresetToJson(Preset), Reloaded));
    const FSFCloneHologram* SavedLane = SFExtendPowerConnections::Find(&Reloaded.ExtendCloneTopology, TEXT("lane_persistence"));
    if (TestNotNull(TEXT("Saved lane"), SavedLane))
    {
        TestTrue(TEXT("Verified socket survives actual preset persistence"), SavedLane->bLaneStartNormalVerified);
        TestFalse(TEXT("Unverified socket is not promoted by persistence"), SavedLane->bLaneEndNormalVerified);
        TestTrue(TEXT("Socket direction preserved"), SavedLane->LaneStartNormal.ToFVector().Equals(FVector::RightVector));
        TestTrue(TEXT("Captured floor-hole identity marker survives persistence"), SavedLane->bHasPassthroughLinks);
        TestEqual(TEXT("Floor-hole pipe owner survives persistence"), SavedLane->PassthroughTop.Target, Lane.PassthroughTop.Target);
        TestEqual(TEXT("Floor-hole named endpoint survives persistence"), SavedLane->PassthroughTop.Connector, Lane.PassthroughTop.Connector);
        TestTrue(TEXT("Unattached floor-hole face remains empty"), SavedLane->PassthroughBottom.Target.IsEmpty());
        TestEqual(TEXT("Native wire limit survives preset persistence"), SavedLane->PowerMaxLength, Lane.PowerMaxLength);
    }
    const FSFCloneHologram* SavedWire = SFExtendPowerConnections::Find(&Reloaded.ExtendCloneTopology, TEXT("wire_factory_pole_0"));
    const FSFCloneHologram* SavedPole = SFExtendPowerConnections::Find(&Reloaded.ExtendCloneTopology, TEXT("power_pole_0"));
    const FSFExtendFactoryPower& SavedFactoryPower = Reloaded.ExtendCloneTopology.FactoryPower;
    TestEqual(TEXT("Saved factory power socket"), SavedFactoryPower.Connector, Source.Factory.Power.Connector);
    TestTrue(TEXT("Saved factory local socket"), SavedFactoryPower.LocalPosition.ToFVector().Equals(FVector(0, -600, 800)));
    TestEqual(TEXT("Saved factory native capacity"), SavedFactoryPower.Capacity, 2);
    TestEqual(TEXT("Saved factory source remaining slots"), SavedFactoryPower.SourceFreeConnections, 1);
    TestEqual(TEXT("Saved factory native cable range"), SavedFactoryPower.MaxWireLength, 10000.f);
    TestTrue(TEXT("Saved factory continuation eligibility"), SavedFactoryPower.bHasWire && SavedFactoryPower.bContinuesChain);
    TestFalse(TEXT("Saved factory opt out"), SavedFactoryPower.bRequested);
    if (TestNotNull(TEXT("Saved wire"), SavedWire))
    {
        TestEqual(TEXT("Saved owner"), SavedWire->PowerTo.Target, Wire->PowerTo.Target);
        TestEqual(TEXT("Saved face"), SavedWire->PowerTo.Connector, Wire->PowerTo.Connector);
        TestEqual(TEXT("Saved factory socket"), SavedWire->PowerFrom.Connector, Wire->PowerFrom.Connector);
    }
    if (TestNotNull(TEXT("Saved pole"), SavedPole))
    {
        TestEqual(TEXT("Two independent saved capacities"), SavedPole->PowerPortCapacities.Num(), 2);
        TestEqual(TEXT("Saved selected face"), SavedPole->PowerConnectorName, Pole.PoleConnectorName);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendSourceWireRotationTest, "SmartFoundations.Extend.Power.SourceWireRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendSourceWireRotationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ASFWireHologram* Preview = World->SpawnActor<ASFWireHologram>();
    if (!TestNotNull(TEXT("Actual wire preview"), Preview)) { World->DestroyWorld(false); return false; }
    for (const float Yaw : {90.f, -90.f, 180.f})
    for (const FVector Offset : {FVector(1000, 0, 0), FVector(-1000, 0, 0), FVector(0, 0, 1000)})
    {
        FSFCloneTopology Topology;
        FSFCloneHologram Wire;
        Wire.Role = TEXT("wire_cost");
        Wire.bIsSourceToCloneWire = true;
        Wire.bHasSplineData = true;
        Wire.PowerFrom = FSFConnectionRef(TEXT("source:pole"), TEXT("Face2"));
        Wire.PowerTo = FSFConnectionRef(TEXT("power_pole_0"), TEXT("Face2"));
        FSFSplinePoint Start, End;
        Start.World = FSFVec3(FVector::ZeroVector);
        End.World = FSFVec3(Offset);
        Wire.SplineData.Points = {Start, End};
        Topology.ChildHolograms.Add(Wire);
        const FVector Center = Offset + FVector(0, 500, 0);
        const FRotator Rotation(0, Yaw, 0);
        Topology.ParentTransform = FSFTransform(Center, FRotator::ZeroRotator);
        Topology.ApplyRigidYawRotation(Rotation, Center, Offset);
        TestTrue(TEXT("Rotated parent pose remains coherent with its children"),
            Topology.ParentTransform.Location.ToFVector().Equals(Center)
            && Topology.ParentTransform.Rotation.ToFRotator().Equals(Rotation));
        const FSFCloneHologram& Result = Topology.ChildHolograms[0];
        const FVector ExpectedEnd = Center + Rotation.RotateVector(Offset - Center);
        TestTrue(TEXT("Existing source remains fixed"), Result.SplineData.Points[0].World.ToFVector().Equals(FVector::ZeroVector));
        TestTrue(TEXT("Clone follows its own factory rotation"), Result.SplineData.Points.Last().World.ToFVector().Equals(ExpectedEnd));
        TestTrue(TEXT("Topology length follows actual endpoints"), FMath::IsNearlyEqual(Result.SplineData.Length, static_cast<float>(ExpectedEnd.Size()), 0.01f));
        SFExtendPowerConnections::RefreshPreview(Preview, Result);
        TestTrue(TEXT("Actual preview cost-length cache refreshes"), FMath::IsNearlyEqual(Preview->GetWireLength(), static_cast<float>(ExpectedEnd.Size()), 0.01f));
        TestEqual(TEXT("Rotation preserves endpoint identity"), Result.PowerFrom.Target, Wire.PowerFrom.Target);
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendMultiFacePoleTest, "SmartFoundations.Extend.Power.MultipleFactoryWires",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendMultiFacePoleTest::RunTest(const FString& Parameters)
{
    FSFSourceTopology Source;
    Source.Factory.Id = TEXT("factory");
    Source.Factory.Class = TEXT("Build_ConstructorMk1_C");
    FSFSourcePowerPole Pole;
    Pole.Id = TEXT("one-outlet");
    Pole.Class = TEXT("Build_PowerPoleWallDouble_C");
    Pole.PoleConnectorName = TEXT("Face1");
    Pole.FactoryConnectorName = TEXT("Power");
    Pole.MaxConnections = 4;
    Pole.FactoryMaxConnections = 2;
    Pole.SourceFreeConnections = 3;
    Pole.bSourceHasFreeConnections = true;
    Source.PowerPoles.Add(Pole);
    Pole.PoleConnectorName = TEXT("Face2");
    Source.PowerPoles.Add(Pole);
    const FSFCloneTopology Clone = FSFCloneTopology::FromSource(Source, FVector(1000, 0, 0));
    int32 Poles = 0, FactoryWires = 0, SourceWires = 0;
    for (const FSFCloneHologram& Holo : Clone.ChildHolograms)
    {
        if (Holo.Role == TEXT("power_pole")) ++Poles;
        if (Holo.Role != TEXT("wire_cost")) continue;
        TestEqual(TEXT("Both faces belong to one cloned outlet"), Holo.PowerTo.Target, FString(TEXT("power_pole_0")));
        if (Holo.bIsSourceToCloneWire) ++SourceWires;
        else ++FactoryWires;
    }
    TestEqual(TEXT("Multiple wires do not duplicate the physical outlet"), Poles, 1);
    TestEqual(TEXT("Both original factory connections preserved"), FactoryWires, 2);
    TestEqual(TEXT("Independent face backbone connections preserved"), SourceWires, 2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPowerReservationTest, "SmartFoundations.Extend.Power.Reservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPowerReservationTest::RunTest(const FString& Parameters)
{
    FSFCloneTopology Topology;
    auto Add = [&](const TCHAR* Owner, const TCHAR* Face, int32 Limit)
    {
        FSFCloneHologram Wire;
        Wire.Role = TEXT("wire_cost");
        Wire.PowerTo = FSFConnectionRef(Owner, Face);
        Wire.PowerToCapacity = Limit;
        Topology.ChildHolograms.Add(Wire);
    };
    FString Reason;
    // Middle cell: factory + preceding edge + following edge + pump, all one face.
    for (int32 Index = 0; Index < 4; ++Index) Add(TEXT("middle"), TEXT("Face1"), 4);
    for (int32 Index = 0; Index < 3; ++Index) Add(TEXT("middle"), TEXT("Face2"), 4);
    TestTrue(TEXT("Independent 4/4 and 3/4 faces are valid"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Add(TEXT("middle"), TEXT("Face1"), 4);
    TestFalse(TEXT("Fifth cable cannot borrow from other face"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Topology.ChildHolograms.Pop();
    Add(TEXT("source:pole"), TEXT("Face2"), 1);
    TestTrue(TEXT("Last free source slot reserved"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Add(TEXT("source:pole"), TEXT("Face2"), 1);
    TestFalse(TEXT("Second source reservation cannot reuse slot"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Topology.ChildHolograms.Reset();
    Add(TEXT("factory"), TEXT("Power"), 1);
    Add(TEXT("factory"), TEXT("Power"), 1);
    TestFalse(TEXT("No MAM research: two cables rejected"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    for (FSFCloneHologram& Wire : Topology.ChildHolograms) Wire.PowerToCapacity = 2;
    TestTrue(TEXT("With MAM research: two cables valid"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Topology.ChildHolograms.Reset();
    FSFCloneHologram Wire;
    Wire.Role = TEXT("wire_cost");
    Wire.PowerMaxLength = 1000;
    FSFSplinePoint Start, End;
    Start.World = FSFVec3(FVector::ZeroVector);
    End.World = FSFVec3(FVector(1000, 0, 0));
    Wire.SplineData.Points = {Start, End};
    Topology.ChildHolograms.Add(Wire);
    TestTrue(TEXT("Wire at native range boundary is valid"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    Topology.ChildHolograms[0].SplineData.Points.Last().World.Y = 500;
    TestFalse(TEXT("Moved connector beyond range invalidates preview plan"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestorePowerChainTest, "SmartFoundations.Restore.Power.ChainPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRestorePowerChainTest::RunTest(const FString& Parameters)
{
    FSFCloneTopology Topology;
    for (int32 Z = 0; Z < 2; ++Z)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 X = 0; X < 3; ++X)
    {
        const FString Prefix = (X == 0 && Y == 0 && Z == 0) ? FString() : SFRestoreGrid::Prefix(X, Y, Z);
        FSFCloneHologram Wire;
        Wire.HologramId = Prefix + TEXT("wire_factory_pole_0");
        Wire.Role = TEXT("wire_cost");
        Wire.PowerFrom = FSFConnectionRef(Prefix.IsEmpty() ? TEXT("parent") : Prefix + TEXT("factory"), TEXT("Power"));
        Wire.PowerTo = FSFConnectionRef(Prefix + TEXT("power_pole_0"), TEXT("Face2"));
        Wire.PowerFromCapacity = 1;
        Wire.PowerToCapacity = 4;
        FSFSplinePoint Start, End;
        Start.World = FSFVec3(FVector(X * 1000, Y * 2000, Z * 3000));
        End.World = FSFVec3(Start.World.ToFVector() + FVector(-70, 100, 700));
        Wire.SplineData.Points = {Start, End}; Wire.bHasSplineData = true;
        Topology.ChildHolograms.Add(Wire);
    }
    SFExtendPowerConnections::AddRestoredChainWires(Topology);
    TestEqual(TEXT("Twelve internal cables plus eight adjacent X seams"), Topology.ChildHolograms.Num(), 20);
    SFExtendPowerConnections::AddRestoredChainWires(Topology);
    TestEqual(TEXT("Replanning cannot duplicate priced cables"), Topology.ChildHolograms.Num(), 20);
    for (const FSFCloneHologram& Wire : Topology.ChildHolograms)
    {
        if (!Wire.HologramId.Contains(TEXT("wire_chain_"))) continue;
        const FVector Delta = Wire.SplineData.Points.Last().World.ToFVector() - Wire.SplineData.Points[0].World.ToFVector();
        TestTrue(TEXT("No cross-row or cross-floor cable"), Delta.Equals(FVector(1000, 0, 0)));
        TestEqual(TEXT("Start face preserved"), Wire.PowerFrom.Connector, FString(TEXT("Face2")));
        TestEqual(TEXT("End face preserved"), Wire.PowerTo.Connector, FString(TEXT("Face2")));
    }
    FString Reason;
    TestTrue(TEXT("All planned wires fit independent endpoint budgets"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    TArray<FSFCloneHologram> OtherFace;
    for (const FSFCloneHologram& Wire : Topology.ChildHolograms)
    {
        if (Wire.HologramId.Contains(TEXT("wire_chain_"))) continue;
        FSFCloneHologram Copy = Wire;
        Copy.HologramId.ReplaceInline(TEXT("wire_factory_pole_0"), TEXT("wire_factory_pole_1"));
        Copy.PowerTo.Connector = TEXT("Face1");
        Copy.PowerFromCapacity = 2;
        OtherFace.Add(Copy);
    }
    for (FSFCloneHologram& Wire : Topology.ChildHolograms) Wire.PowerFromCapacity = 2;
    Topology.ChildHolograms.Append(OtherFace);
    SFExtendPowerConnections::AddRestoredChainWires(Topology);
    TestEqual(TEXT("Two faces retain independent chains without duplicate physical poles"), Topology.ChildHolograms.Num(), 40);
    TestTrue(TEXT("Both face chains fit native capacity"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestoreLegacyPowerTest, "SmartFoundations.Restore.Power.LegacyPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFRestoreLegacyPowerTest::RunTest(const FString& Parameters)
{
    FSFCloneTopology Topology;
    FSFCloneHologram Pole;
    Pole.HologramId = TEXT("power_pole_0");
    Pole.Role = TEXT("power_pole");
    Pole.BuildClass = TEXT("Build_PowerPoleMk1_C");
    Pole.PowerPoleMaxConnections = 4;
    Topology.ChildHolograms.Add(Pole);
    FSFCloneHologram Wire;
    Wire.HologramId = TEXT("wire_factory_pole_0");
    Wire.Role = TEXT("wire_cost");
    FSFSplinePoint Start, End;
    End.World = FSFVec3(FVector(100, 0, 700));
    Wire.SplineData.Points = {Start, End};
    Topology.ChildHolograms.Add(Wire);
    Wire.HologramId = TEXT("wire_source_clone_0");
    Topology.ChildHolograms.Add(Wire);
    TestTrue(TEXT("Old single-port saved plan is promoted"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    TestEqual(TEXT("Restore has no live source cable"), Topology.ChildHolograms.Num(), 2);
    const FSFCloneHologram& Promoted = Topology.ChildHolograms[1];
    TestEqual(TEXT("Factory owner recovered from old cable intent"), Promoted.PowerFrom.Target, FString(TEXT("parent")));
    TestEqual(TEXT("Pole owner recovered from old cable intent"), Promoted.PowerTo.Target, Pole.HologramId);
    TestTrue(TEXT("No invented component name"), Promoted.PowerTo.Connector.IsEmpty());
    TestEqual(TEXT("Old per-pole limit retained"), Promoted.PowerToCapacity, 4);
    TestTrue(TEXT("Promotion is idempotent"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    Topology.ChildHolograms[0].BuildClass = TEXT("Build_PowerPoleWallDouble_C");
    TestFalse(TEXT("Old ambiguous two-face capture is refused"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    FString Reason;
    TestFalse(TEXT("Ambiguity blocks placement before charging"), SFExtendPowerConnections::ValidateCapacity(Topology, Reason));
    TestFalse(TEXT("Ambiguity has an actionable reason"), Reason.IsEmpty());
    Topology.ChildHolograms[1].PowerTo.Connector = TEXT("PowerConnection2");
    TestTrue(TEXT("Explicit face resolves ambiguity"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    FSFCloneHologram Pump;
    Pump.HologramId = TEXT("pipe_attachment_0");
    Pump.Role = TEXT("pipe_attachment");
    Pump.ConnectedPowerPoleHologramId = Pole.HologramId;
    Pump.ConnectedPowerPoleConnectorName = TEXT("PowerConnection1");
    Topology.ChildHolograms.Add(Pump);
    Wire.HologramId = TEXT("wire_pump_pipe_attachment_0");
    Topology.ChildHolograms.Add(Wire);
    TestTrue(TEXT("Mixed modern and legacy cable records are promoted per edge"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    TestEqual(TEXT("Pump retains named opposite face"), Topology.ChildHolograms.Last().PowerTo.Connector, FString(TEXT("PowerConnection1")));
    Topology.ChildHolograms.Last().PowerFrom.Target.Reset();
    TestFalse(TEXT("Partial endpoint metadata is not misclassified as legacy"), SFExtendPowerConnections::PromoteLegacyRestoreWires(Topology));
    return true;
}
#endif
