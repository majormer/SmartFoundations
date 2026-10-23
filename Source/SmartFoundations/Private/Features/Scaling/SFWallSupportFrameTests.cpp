// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFPositionCalculator.h"
#include "Shared/SFSupportGridFrame.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWallSupportFrameTest,
 "SmartFoundations.WallSupports.RotatedRunFrame",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWallSupportFrameTest::RunTest(const FString& Parameters)
{
 UPackage* Outer = CreatePackage(TEXT("/Temp/SmartWallSupportFrameTest"));
 UClass* Pipe = NewObject<UClass>(Outer, TEXT("Build_PipelineSupportWall_C"));
 UClass* Hyper = NewObject<UClass>(Outer, TEXT("Build_HyperTubeWallSupport_C"));
 UClass* Belt = NewObject<UClass>(Outer, TEXT("Build_ConveyorPoleWall_C"));
 TestTrue(TEXT("Pipe wall frame"), SFSupportGridFrame::UsesParentRotation(Pipe));
 TestTrue(TEXT("Hyper wall frame"), SFSupportGridFrame::UsesParentRotation(Hyper));
 TestFalse(TEXT("Conveyor unchanged"), SFSupportGridFrame::UsesParentRotation(Belt));
 FSFPositionCalculator Calc;
 FSFCounterState State;
 State.SpacingY = 5400;
 const FVector Origin(1234, -2345, 3456), Size(200,200,200);
 for (double Yaw : {0.0, 90.0, 180.0, -90.0})
 for (double Roll : {0.0, 45.0, 90.0, -45.0, -90.0})
 for (int32 Sign : {-1, 1})
 {
  const FRotator Rotation(0,Yaw,Roll);
  const FVector Expected = Origin + Rotation.RotateVector(FVector(0, -Sign*5600.0, 0));
  for (const UClass* Family : {Pipe, Hyper})
  {
   const FVector Actual = Calc.CalculateChildPosition(0,Sign,0,Origin,Rotation,Size,State,0,FVector::ZeroVector,Family);
   TestTrue(TEXT("Y positions follow selected native rotation"), Actual.Equals(Expected,0.01));
   const FVector Normal = SFSupportGridFrame::RunNormal(Rotation,(Expected-Origin).GetSafeNormal());
   TestTrue(TEXT("Exit normal follows the run, including vertical"), FVector::DotProduct(Normal,(Expected-Origin).GetSafeNormal()) > 0.99999);
  }
  int32 UpAxis, UpSign;
  SFSupportGridFrame::VerticalAxis(Rotation, UpAxis, UpSign);
  const FVector Up = UpAxis == 1 ? -Rotation.RotateVector(FVector::RightVector) : Rotation.RotateVector(FVector::UpVector);
  TestTrue(TEXT("Player relative vertical points upward"), (Up * UpSign).Z > 0.7);
  int32 SideAxis = 1, SideSign = 1;
  SFSupportGridFrame::HorizontalAxis(Rotation, SideAxis, SideSign);
  TestTrue(TEXT("Player relative horizontal and vertical axes are distinct"), SideAxis != UpAxis);
  const FVector Side = SideAxis == 1 ? -Rotation.RotateVector(FVector::RightVector) : Rotation.RotateVector(FVector::UpVector);
  const FVector FlatY = -FRotator(0,Yaw,0).RotateVector(FVector::RightVector);
  TestTrue(TEXT("Player relative side follows horizontal intent"), FVector::DotProduct(Side * SideSign, FlatY) > 0.7);
  const FVector Legacy = Calc.CalculateChildPosition(0,Sign,0,Origin,Rotation,Size,State);
  TestTrue(TEXT("Unrelated conveyor positions retain legacy calculation"), Calc.CalculateChildPosition(0,Sign,0,Origin,Rotation,Size,State,0,FVector::ZeroVector,Belt).Equals(Legacy,0.01));
 }
 return true;
}
#endif
