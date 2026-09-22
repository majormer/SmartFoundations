// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendFactoryPower.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFRestoreGrid.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendFactoryDaisyPlanTest, "SmartFoundations.Extend.Power.FactoryDaisyPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendFactoryDaisyPlanTest::RunTest(const FString& Parameters)
{
    FSFSourceFactory Source;
    Source.Id = TEXT("source_factory");
    Source.Transform = FSFTransform(FVector(100, 200, 300), FRotator(0, 90, 0));
    auto& Power = Source.Power;
    Power.Connector = TEXT("PowerConnection");
    Power.LocalPosition = FSFVec3(FVector(0, -600, 800));
    Power.Capacity = 2; Power.SourceFreeConnections = 1; Power.MaxWireLength = 10000;
    TestTrue(TEXT("Poleless opt-in starts a chain"), SFExtendFactoryPower::IsRequested(Power, true, true, true));
    TestFalse(TEXT("Poleless option disabled"), SFExtendFactoryPower::IsRequested(Power, true, false, true));
    Power.bHasWire = true;
    TestFalse(TEXT("An unrelated existing wire does not enable daisy chaining"), SFExtendFactoryPower::IsRequested(Power, true, true, true));
    Power.bContinuesChain = true;
    TestTrue(TEXT("Continue existing factory chain without poleless option"), SFExtendFactoryPower::IsRequested(Power, true, false, true));
    TestFalse(TEXT("Master switch stands down"), SFExtendFactoryPower::IsRequested(Power, false, true, true));
    TestFalse(TEXT("Research is mandatory"), SFExtendFactoryPower::IsRequested(Power, true, true, false));
    Power.Capacity = 1;
    TestFalse(TEXT("No assumed second slot"), SFExtendFactoryPower::IsRequested(Power, true, true, true));
    Power.Capacity = 2; Power.bRequested = true;
    FSFCloneTopology Clone;
    Clone.ParentTransform = FSFTransform(FVector(1100, 200, 700), FRotator(0, 180, 0));
    SFExtendFactoryPower::AddSourceWire(Clone, Source);
    TestEqual(TEXT("One priced cable"), Clone.ChildHolograms.Num(), 1);
    if (Clone.ChildHolograms.Num() != 1) return false;
    const auto& Wire = Clone.ChildHolograms[0];
    TestEqual(TEXT("Cable uses the priced preview role"), Wire.Role, FString(TEXT("wire_cost")));
    TestTrue(TEXT("Source stays explicit for adaptive transforms"), Wire.bIsSourceToCloneWire);
    TestEqual(TEXT("Exact source owner"), Wire.PowerFrom.Target, FString(TEXT("source:source_factory")));
    TestEqual(TEXT("Exact clone owner"), Wire.PowerTo.Target, FString(TEXT("parent")));
    TestEqual(TEXT("Source uses remaining capacity"), Wire.PowerFromCapacity, 1);
    TestEqual(TEXT("New factory uses full native capacity"), Wire.PowerToCapacity, 2);
    TestTrue(TEXT("Source socket is transformed from local position"), Wire.SplineData.Points[0].World.ToFVector().Equals(FVector(700, 200, 1100), 0.01));
    TestTrue(TEXT("Clone socket follows its own rotation and height"), Wire.SplineData.Points.Last().World.ToFVector().Equals(FVector(1100, 800, 1500), 0.01));
    SFExtendFactoryPower::AddSourceWire(Clone, Source);
    TestEqual(TEXT("Replanning does not duplicate cost"), Clone.ChildHolograms.Num(), 1);
    FSFCloneHologram FirstAdditional = Clone.ChildHolograms[0];
    SFExtendPowerConnections::RemapCellTargets(FirstAdditional, TEXT("sc_2_0_0_"), FString());
    TestEqual(TEXT("First additional factory wires to held parent"), FirstAdditional.PowerFrom.Target, FString(TEXT("parent")));
    TestEqual(TEXT("Additional factory destination is exact cell"), FirstAdditional.PowerTo.Target, FString(TEXT("sc_2_0_0_factory")));
    FSFCloneHologram NextAdditional = Clone.ChildHolograms[0];
    SFExtendPowerConnections::RemapCellTargets(NextAdditional, TEXT("sc_3_1_1_"), TEXT("sc_2_1_1_"));
    TestEqual(TEXT("Later source uses preceding factory, never prefix-parent"), NextAdditional.PowerFrom.Target, FString(TEXT("sc_2_1_1_factory")));
    FSFCloneHologram OutletWire = Clone.ChildHolograms[0];
    OutletWire.PowerTo = FSFConnectionRef(TEXT("power_pole_0"), TEXT("PowerConnection2"));
    OutletWire.PowerFrom.Connector = TEXT("PowerConnection2");
    SFExtendPowerConnections::RemapCellTargets(OutletWire, TEXT("sc_3_1_1_"), TEXT("sc_2_1_1_"));
    TestEqual(TEXT("Pole source keeps its own owner ID"), OutletWire.PowerFrom.Target, FString(TEXT("sc_2_1_1_power_pole_0")));
    TestEqual(TEXT("Pole source keeps selected face"), OutletWire.PowerFrom.Connector, FString(TEXT("PowerConnection2")));
    Power.SourceFreeConnections = 0;
    SFExtendFactoryPower::AddSourceWire(Clone, Source);
    TestEqual(TEXT("Full source emits no cable or cost"), Clone.ChildHolograms.Num(), 0);

    TMap<FIntVector, FTransform> Cells;
    for (int32 Z = 0; Z < 2; ++Z)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 X = 0; X < 3; ++X)
        Cells.Add(FIntVector(X, Y, Z), FTransform(FRotator(0, X * 30, 0), FVector(X * 1500, Y * 2000, Z * 1000 + X * 200)));
    SFExtendFactoryPower::AddRestoreWires(Clone, Cells);
    TestEqual(TEXT("3x2x2 has two edges in each of four independent rows"), Clone.ChildHolograms.Num(), 8);
    FString Reason;
    TestTrue(TEXT("Middle cells reserve exactly two native slots"), SFExtendPowerConnections::ValidateCapacity(Clone, Reason));
    for (const FSFCloneHologram& RestoredWire : Clone.ChildHolograms)
    {
        TestFalse(TEXT("Restored cable never points at an original world source"), RestoredWire.PowerFrom.Target.StartsWith(TEXT("source:")));
        TestFalse(TEXT("Restored internal cable is not a live source edge"), RestoredWire.bIsSourceToCloneWire);
    }
    SFExtendFactoryPower::AddRestoreWires(Clone, Cells);
    TestEqual(TEXT("Restore regeneration is idempotent"), Clone.ChildHolograms.Num(), 8);
    Cells.Remove(FIntVector(1, 1, 1));
    SFExtendFactoryPower::AddRestoreWires(Clone, Cells);
    TestEqual(TEXT("Missing middle cell removes both edges, not one long bridge"), Clone.ChildHolograms.Num(), 6);
    Clone.FactoryPower.bRequested = false;
    SFExtendFactoryPower::AddRestoreWires(Clone, Cells);
    TestEqual(TEXT("Disabling removes preview and charge"), Clone.ChildHolograms.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPoleFedDaisyPlanTest, "SmartFoundations.Extend.Power.PoleFedDaisyPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPoleFedDaisyPlanTest::RunTest(const FString& Parameters)
{
    // A row of four two-slot factories, each with its own cloned pole cable, plus daisy links
    // between neighbours: pole + in + out would need three slots on the middle factories.
    FSFCloneTopology Clone;
    Clone.FactoryPower.Connector = TEXT("PowerConnection");
    Clone.FactoryPower.Capacity = 2;
    Clone.FactoryPower.MaxWireLength = 100000;
    Clone.FactoryPower.bRequested = true;
    TMap<FIntVector, FTransform> Cells;
    for (int32 X = 0; X < 4; ++X)
    {
        Cells.Add(FIntVector(X, 0, 0), FTransform(FVector(X * 1500, 0, 0)));
        const FString Factory = X == 0 ? FString(TEXT("parent")) : SFRestoreGrid::Prefix(X, 0, 0) + TEXT("factory");
        FSFCloneHologram PoleCable;
        PoleCable.HologramId = FString::Printf(TEXT("wire_factory_pole_%d"), X);
        PoleCable.Role = TEXT("wire_cost");
        PoleCable.PowerFrom = FSFConnectionRef(Factory, TEXT("PowerConnection"));
        PoleCable.PowerTo = FSFConnectionRef(FString::Printf(TEXT("pole_%d"), X), TEXT("PowerConnection"));
        PoleCable.PowerFromCapacity = 2;
        PoleCable.PowerToCapacity = 4;
        Clone.ChildHolograms.Add(PoleCable);
    }
    SFExtendFactoryPower::AddRestoreWires(Clone, Cells);
    const int32 PoleCables = 4;
    TestEqual(TEXT("Restore plans a daisy link between every neighbour"), Clone.ChildHolograms.Num(), PoleCables + 3);
    FString Reason;
    TestFalse(TEXT("Unpruned pole-fed row overflows its middle sockets (the regression)"),
        SFExtendPowerConnections::ValidateCapacity(Clone, Reason));

    SFExtendPowerConnections::FSocketBudget Budget;
    const int32 Dropped = SFExtendPowerConnections::PruneDaisyOverCapacity(Clone, Budget);
    TestTrue(TEXT("Pruned plan fits every socket"), SFExtendPowerConnections::ValidateCapacity(Clone, Reason));
    TestEqual(TEXT("Only the links that would overflow are dropped"), Dropped, 1);
    int32 PoleCablesKept = 0;
    for (const FSFCloneHologram& Holo : Clone.ChildHolograms)
    {
        if (Holo.HologramId.StartsWith(TEXT("wire_factory_pole_"))) ++PoleCablesKept;
    }
    TestEqual(TEXT("Every factory keeps its pole cable"), PoleCablesKept, PoleCables);

    // Unlimited room: nothing is dropped when each factory has a spare slot for in + out.
    FSFCloneTopology Roomy = Clone;
    Roomy.ChildHolograms.RemoveAll([](const FSFCloneHologram& Holo) { return Holo.HologramId.StartsWith(TEXT("wire_factory_pole_")); });
    SFExtendFactoryPower::AddRestoreWires(Roomy, Cells);
    SFExtendPowerConnections::FSocketBudget RoomyBudget;
    TestEqual(TEXT("Poleless row keeps its full chain"), SFExtendPowerConnections::PruneDaisyOverCapacity(Roomy, RoomyBudget), 0);
    TestEqual(TEXT("Poleless row has three links"), Roomy.ChildHolograms.Num(), 3);
    return true;
}
#endif
