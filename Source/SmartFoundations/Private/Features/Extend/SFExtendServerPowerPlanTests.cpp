// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendScaledService.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendServerPowerPlanTest, "SmartFoundations.Extend.Power.ServerMergedPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendServerPowerPlanTest::RunTest(const FString& Parameters)
{
    USFExtendService* Owner = NewObject<USFExtendService>();
    USFExtendScaledService* Service = NewObject<USFExtendScaledService>();
    Service->Initialize(Owner);
    FSFCloneTopology Base;
    auto AddWire = [](FSFCloneTopology& Plan, const FString& Id, const FString& From, const FString& To)
    {
        FSFCloneHologram Wire;
        Wire.HologramId = Id;
        Wire.Role = TEXT("wire_cost");
        Wire.PowerFrom = FSFConnectionRef(From, TEXT("PowerConnection"));
        Wire.PowerTo = FSFConnectionRef(To, TEXT("PowerConnection2"));
        Wire.PowerFromCapacity = 4;
        Wire.PowerToCapacity = 4;
        Plan.ChildHolograms.Add(Wire);
    };
    AddWire(Base, TEXT("factory_wire"), TEXT("parent"), TEXT("pole"));
    AddWire(Base, TEXT("source_wire"), TEXT("source:pole"), TEXT("pole"));
    for (int32 Z = 0; Z < 2; ++Z)
    for (int32 Y = 0; Y < 2; ++Y)
    for (int32 X = 0; X < 3; ++X)
    {
        if (Y == 0 && Z == 0 && X < 2) continue;
        FSFScaledExtendClone& Cell = Owner->ScaledExtendClones.AddDefaulted_GetRef();
        Cell.GridX = X; Cell.GridY = Y; Cell.GridZ = Z;
        Cell.CloneTopology = MakeShared<FSFCloneTopology>();
        const FString Prefix = FString::Printf(TEXT("sc_%d_%d_%d_"), X, Y, Z);
        AddWire(*Cell.CloneTopology, Prefix + TEXT("factory_wire"), Prefix + TEXT("factory"), Prefix + TEXT("pole"));
        if (X > 0)
        {
            const FString Previous = (Y == 0 && Z == 0 && X == 2) ? FString()
                : FString::Printf(TEXT("sc_%d_%d_%d_"), X - 1, Y, Z);
            AddWire(*Cell.CloneTopology, Prefix + TEXT("seam"), Previous + TEXT("pole"), Prefix + TEXT("pole"));
        }
    }
    // Exercise the real server entry with already-generated cell plans. No native world or
    // hologram spawn is simulated: this isolates the missing post-spawn merge, not gameplay.
    for (int32 Request = 0; Request < 2; ++Request)
    {
        Owner->SetStoredCloneTopologyForServerCommit(Base);
        Service->SpawnCloneSetsForServerCommit();
        TestEqual(TEXT("Server plan contains eleven factory wires and eight X seams"),
            Owner->StoredCloneTopology->ChildHolograms.Num(), 19);
        TestTrue(TEXT("Server preserves its unexpanded base for the next request"),
            Owner->ScaledExtendBaseTopology.IsValid() && Owner->ScaledExtendBaseTopology->ChildHolograms.Num() == 2);
        TSet<FString> Ids;
        for (const FSFCloneHologram& Wire : Owner->StoredCloneTopology->ChildHolograms) Ids.Add(Wire.HologramId);
        TestEqual(TEXT("No duplicate plan records across requests"), Ids.Num(), 19);
        const FSFCloneHologram* Upper = SFExtendPowerConnections::Find(Owner->StoredCloneTopology.Get(), TEXT("sc_2_1_1_factory_wire"));
        if (TestNotNull(TEXT("Last upper cell reaches materializer plan"), Upper))
            TestEqual(TEXT("Double outlet face is preserved"), Upper->PowerTo.Connector, FString(TEXT("PowerConnection2")));
        TestTrue(TEXT("Full plan fits native-style port budgets"), Service->ValidatePowerCapacity());
    }
    // Validation must see an overbooked port in an additional cell, not just the parent.
    FSFCloneTopology& Last = *Owner->ScaledExtendClones.Last().CloneTopology;
    for (int32 Index = 0; Index < 5; ++Index)
        AddWire(Last, FString::Printf(TEXT("overbook_%d"), Index), TEXT("extraFactory"), TEXT("extraPole"));
    Owner->SetStoredCloneTopologyForServerCommit(Base);
    Service->SpawnCloneSetsForServerCommit();
    TestFalse(TEXT("Additional-cell overbooking rejects construction"), Service->ValidatePowerCapacity());
    Owner->ScaledExtendClones.Reset();
    Owner->SetStoredCloneTopologyForServerCommit(Base);
    Service->SpawnCloneSetsForServerCommit();
    TestEqual(TEXT("Next single-copy request has no stale scaled cables"), Owner->StoredCloneTopology->ChildHolograms.Num(), 2);
    Service->Shutdown();
    return true;
}
#endif
