// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendWiringPlan.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendWiringPlanTest, "SmartFoundations.Extend.NativeWiringPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendWiringPlanTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USceneComponent> ParentObject(NewObject<USceneComponent>()), BeltObject(NewObject<USceneComponent>());
    TStrongObjectPtr<USceneComponent> NearbyObject(NewObject<USceneComponent>());
    const FObjectKey Parent(ParentObject.Get()), Belt(BeltObject.Get()), Nearby(NearbyObject.Get());
    FSFCloneTopology Preview;
    FSFCloneHologram Child;
    Child.HologramId = TEXT("belt_0"); Child.Role = TEXT("belt_segment"); Child.bConstructible = true;
    Child.CloneConnections.ConveyorAny0 = FSFConnectionRef(TEXT("parent"), TEXT("Output0"));
    Child.CloneConnections.ConveyorAny1 = FSFConnectionRef(TEXT("source:ExistingSplitter"), TEXT("Input1"));
    Preview.ChildHolograms.Add(Child);
    FSFExtendWiringPlan Plan(Preview);
    Preview.ChildHolograms[0].CloneConnections.ConveyorAny1.Target = TEXT("source:DifferentPreview");
    TestEqual(TEXT("Later preview edits cannot replace captured edge intent"),
        Plan.GetTopology().ChildHolograms[0].CloneConnections.ConveyorAny1.Target, FString(TEXT("source:ExistingSplitter")));
    TestTrue(TEXT("External source is a required exact binding, not a clone output"), Plan.GetRequiredSources().Contains(TEXT("ExistingSplitter")));
    TestFalse(TEXT("Source name does not become permission for a constructed role"), Plan.GetRequiredRoles().Contains(TEXT("source:ExistingSplitter")));
    Plan.RegisterRole(TEXT("parent"), Parent);
    TestFalse(TEXT("Missing child role cannot be silently skipped"), Plan.Seal(TSet<FObjectKey>{Parent, Belt}));
    Plan.RegisterRole(TEXT("belt_0"), Belt);
    TestFalse(TEXT("A nearby object cannot substitute for an unreturned construction output"), Plan.Seal(TSet<FObjectKey>{Parent, Nearby}));
    TestTrue(TEXT("Complete exact returned roles can seal"), Plan.Seal(TSet<FObjectKey>{Parent, Belt}));
    TestFalse(TEXT("Sealed role bindings cannot be overwritten by a later preview callback"), Plan.RegisterRole(TEXT("belt_0"), Nearby));
    TestTrue(TEXT("Original actor generation remains bound"), Plan.GetBindings().FindChecked(TEXT("belt_0")) == Belt);

    FSFExtendWiringPlan Ambiguous(Preview);
    Ambiguous.RegisterRole(TEXT("parent"), Parent); Ambiguous.RegisterRole(TEXT("belt_0"), Belt);
    TestFalse(TEXT("Conflicting bindings invalidate the plan instead of last-write-wins"), Ambiguous.RegisterRole(TEXT("belt_0"), Nearby));
    TestFalse(TEXT("An ambiguous plan cannot authorize wiring"), Ambiguous.Seal(TSet<FObjectKey>{Parent, Belt, Nearby}));
    FSFExtendWiringPlan Aliased(Preview);
    Aliased.RegisterRole(TEXT("parent"), Parent); Aliased.RegisterRole(TEXT("belt_0"), Parent);
    TestFalse(TEXT("Two required roles cannot claim the same returned actor"), Aliased.Seal(TSet<FObjectKey>{Parent}));
    Preview.ChildHolograms[0].CloneConnections.ConveyorAny0.Connector.Empty();
    FSFExtendWiringPlan MissingPort(Preview);
    TestFalse(TEXT("A named target without an exact port cannot become an accepted plan"), MissingPort.IsValid());
    FSFCloneTopology ConveyorOnly;
    ConveyorOnly.ChildHolograms.Add(Child);
    TestTrue(TEXT("Ordinary conveyor intent can request the conveyor-only settlement adapter"), FSFExtendWiringPlan::HasOnlyConveyorEffects(ConveyorOnly));
    FSFCloneHologram WireCost;
    WireCost.Role = TEXT("wire_cost"); WireCost.bConstructible = false; WireCost.bPreviewOnly = true;
    ConveyorOnly.ChildHolograms.Add(WireCost);
    TestFalse(TEXT("A nonconstructed cost wire still requires its power materialization receipt"), FSFExtendWiringPlan::HasOnlyConveyorEffects(ConveyorOnly));
    TestFalse(TEXT("A cost marker without exact endpoints does not qualify its power adapter"), FSFExtendWiringPlan::HasOnlyConveyorAndPricedPowerEffects(ConveyorOnly));
    ConveyorOnly.ChildHolograms.Last().HologramId = TEXT("wire_factory");
    ConveyorOnly.ChildHolograms.Last().PowerFrom = FSFConnectionRef(TEXT("source:ExistingFactory"), TEXT("PowerInput"));
    ConveyorOnly.ChildHolograms.Last().PowerTo = FSFConnectionRef(TEXT("parent"), TEXT("PowerInput"));
    TestTrue(TEXT("An exact priced power edge is distinct from an unobserved sibling effect"), FSFExtendWiringPlan::HasOnlyConveyorAndPricedPowerEffects(ConveyorOnly));
    ConveyorOnly.ChildHolograms.Pop();
    ConveyorOnly.ChildHolograms[0].LiftData.PassthroughCloneIds.Add(TEXT("floor_hole"));
    TestFalse(TEXT("Floor-hole state requires its exact passthrough adapter"), FSFExtendWiringPlan::HasOnlyConveyorEffects(ConveyorOnly));
    ConveyorOnly.ChildHolograms[0].LiftData.PassthroughCloneIds.Reset();
    ConveyorOnly.ChildHolograms[0].Role = TEXT("lane_segment"); ConveyorOnly.ChildHolograms[0].LaneSegmentType = TEXT("pipe");
    TestFalse(TEXT("A pipe lane cannot masquerade as conveyor-only work"), FSFExtendWiringPlan::HasOnlyConveyorEffects(ConveyorOnly));
    FSFCloneTopology FloorHole;
    FSFCloneHologram Lift;
    Lift.HologramId = TEXT("lift"); Lift.Role = TEXT("lift_segment"); Lift.BuildClass = TEXT("Build_ConveyorLiftMk1_C");
    Lift.bConstructible = true; Lift.bHasLiftData = true; Lift.LiftData.PassthroughCloneIds = {TEXT("hole"), TEXT("")};
    FloorHole.ChildHolograms.Add(Lift);
    FSFCloneHologram Hole;
    Hole.HologramId = TEXT("hole"); Hole.Role = TEXT("passthrough"); Hole.BuildClass = TEXT("Build_FoundationPassthrough_Lift_C");
    Hole.bConstructible = true; Hole.Thickness = 400; Hole.bHasPassthroughLinks = true;
    Hole.PassthroughTop = FSFConnectionRef(TEXT("lift"), TEXT("ConveyorAny0"));
    FloorHole.ChildHolograms.Add(Hole);
    TestTrue(TEXT("Exact reciprocal native lift/hole roles qualify value admission"), FSFExtendWiringPlan::HasExactLiftHoleRoles(FloorHole));
    TestTrue(TEXT("Floor-hole effects require the dedicated receipt, independently of the conveyor-only adapter"), FSFExtendWiringPlan::HasSupportedEffects(FloorHole));
    FloorHole.ChildHolograms.Last().bHasPassthroughLinks = false;
    TestFalse(TEXT("An old preset missing captured face bindings cannot infer them from nearby actors"), FSFExtendWiringPlan::HasExactLiftHoleRoles(FloorHole));
    FloorHole.ChildHolograms.Last().bHasPassthroughLinks = true;
    FloorHole.ChildHolograms.Last().PassthroughTop.Target = TEXT("source:old_lift");
    TestFalse(TEXT("A historical source actor cannot satisfy a copied floor-hole face"), FSFExtendWiringPlan::HasExactLiftHoleRoles(FloorHole));
    FloorHole.ChildHolograms.Last().PassthroughTop.Target = TEXT("lift");
    FloorHole.ChildHolograms[0].LiftData.PassthroughCloneIds[1] = TEXT("second_hole");
    Hole.HologramId = TEXT("second_hole"); FloorHole.ChildHolograms.Add(Hole);
    TestFalse(TEXT("Two copied holes cannot claim one native lift mouth"), FSFExtendWiringPlan::HasExactLiftHoleRoles(FloorHole));
    return true;
}
#endif
