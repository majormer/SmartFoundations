// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Misc/AutomationTest.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftPortStateTest, "SmartFoundations.AutoConnect.BlueprintLiftPortState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftPortStateTest::RunTest(const FString& Parameters)
{
    FSFBlueprintLiftPort A; A.BuildClass = AFGBuildableConveyorLift::StaticClass(); A.Name = TEXT("ConveyorAny1"); A.Index = 1;
    A.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_ANY);
    TestTrue(TEXT("Same authored mouth and priced pose"), FSFBlueprintLiftSeams::SamePort(A, A));
    auto B = A; B.Cell.Z = 1;
    TestFalse(TEXT("Different repeated copy cannot alias"), FSFBlueprintLiftSeams::SamePort(A, B));
    B = A; B.PortLocalPose.SetLocation(FVector(0, 0, 100));
    TestFalse(TEXT("Changed selected lift height rejects"), FSFBlueprintLiftSeams::SamePort(A, B));
    B = A; B.PortLocalPose.SetRotation(FRotator(0, 45, 0).Quaternion());
    TestFalse(TEXT("Changed mouth direction rejects"), FSFBlueprintLiftSeams::SamePort(A, B));
    B = A; B.OwnerPose.SetLocation(FVector(100, 0, 0));
    TestFalse(TEXT("Moved copy rejects"), FSFBlueprintLiftSeams::SamePort(A, B));
    B = A; B.OriginalOwner.SetLocation(FVector(100, 0, 0));
    TestFalse(TEXT("Different authored lift cannot alias"), FSFBlueprintLiftSeams::SamePort(A, B));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftPlanBoundsTest, "SmartFoundations.AutoConnect.BlueprintLiftPlanBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftPlanBoundsTest::RunTest(const FString& Parameters)
{
    FSFBlueprintLiftPlanEntry E;
    E.Tier = 2; E.BuildClass = AFGBuildableConveyorBelt::StaticClass(); E.Recipe = UFGRecipe::StaticClass();
    E.From.BuildClass = E.To.BuildClass = AFGBuildableConveyorLift::StaticClass();
    E.From.Name = TEXT("ConveyorAny1"); E.To.Name = TEXT("ConveyorAny0");
    E.From.Index = 1; E.To.Index = 0; E.To.Cell.Z = 1;
    E.From.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_OUTPUT);
    E.To.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_INPUT);
    E.Spline.SetNum(2); E.Spline[1].Location = FVector(1000, 0, 0);
    FItemAmount Cost; Cost.ItemClass = UFGItemDescriptor::StaticClass(); Cost.Amount = 4; E.Cost.Add(Cost);
    TestTrue(TEXT("Bounded directed adjacent-copy plan"), FSFBlueprintLiftSeams::IsBounded({E}));
    TestFalse(TEXT("Repeated claims on one physical mouth reject"), FSFBlueprintLiftSeams::IsBounded({E, E}));
    auto Bad = E; Bad.From.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_ANY);
    TestFalse(TEXT("Unresolved flow cannot become a directed seam"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    Bad = E; Bad.To.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_OUTPUT);
    TestFalse(TEXT("Two output mouths cannot become a directed seam"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    Bad = E; Bad.To.Cell.Z = 3;
    TestFalse(TEXT("Skipped copy cannot become an adjacent seam"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    Bad = E; Bad.To.Direction = static_cast<int32>(EFactoryConnectionDirection::FCD_SNAP_ONLY);
    TestFalse(TEXT("Snap-only is not a conveyor mouth"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    Bad = E; Bad.Cost[0].Amount = -1;
    TestFalse(TEXT("Negative payment rejected"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    Bad = E; Bad.Spline.SetNum(65);
    TestFalse(TEXT("Oversized spline payload rejected"), FSFBlueprintLiftSeams::IsBounded({Bad}));
    return true;
}

#endif
