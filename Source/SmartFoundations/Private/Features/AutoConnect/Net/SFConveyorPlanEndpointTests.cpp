// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Misc/AutomationTest.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Features/Scaling/SFScalingSpec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorPlanEndpointRolesTest, "SmartFoundations.AutoConnect.ConveyorPlanEndpointRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorPlanEndpointRolesTest::RunTest(const FString& Parameters)
{
    FSFConduitPlanEntry Entry;
    Entry.Kind = ESFConduitPlanKind::StackableBelt;
    Entry.bExactConveyorEndpoints = true;
    Entry.BuildClass = AFGBuildableConveyorBelt::StaticClass();
    Entry.Recipe = UFGRecipe::StaticClass();
    Entry.SplinePoints.SetNum(2);
    Entry.SplinePoints[1].Location = FVector(500, 0, 0);
    FItemAmount Cost; Cost.ItemClass = UFGItemDescriptor::StaticClass(); Cost.Amount = 3;
    Entry.Cost.Add(Cost);
    Entry.ConveyorFrom.bPresent = true;
    Entry.ConveyorFrom.BuildClass = AFGBuildableConveyorBelt::StaticClass();
    Entry.ConveyorFrom.PortName = TEXT("SnapOnly0");
    Entry.ConveyorFrom.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_SNAP_ONLY);
    Entry.ConveyorTo = Entry.ConveyorFrom;
    Entry.ConveyorTo.OwnerPose.SetLocation(FVector(500, 0, 0));
    TestTrue(TEXT("Support route preserves bounded native positioning anchors"), Entry.HasBoundedConveyorIntent());
    Entry.Kind = ESFConduitPlanKind::Belt;
    TestFalse(TEXT("Positioning anchors cannot stand in for ordinary factory transfer peers"), Entry.HasBoundedConveyorIntent());
    Entry.Kind = ESFConduitPlanKind::StackableBelt;
    Entry.ConveyorFrom.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_ANY);
    TestFalse(TEXT("Unresolved flow is not a positioning anchor"), Entry.HasBoundedConveyorIntent());
    Entry.ConveyorFrom.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_SNAP_ONLY);
    Entry.ConveyorFrom.BuildClass = nullptr;
    TestFalse(TEXT("A selected anchor still requires an exact owner identity"), Entry.HasBoundedConveyorIntent());
    return true;
}
#endif
