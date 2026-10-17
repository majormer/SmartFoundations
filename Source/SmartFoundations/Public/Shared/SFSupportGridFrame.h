// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Class.h"

class AFGHologram;

// Only native rotatable conduit wall supports use a tilted grid. Conveyors and
// ordinary factory grids retain their existing yaw/world-height contract.
namespace SFSupportGridFrame
{
 namespace Private
 {
  inline thread_local const AFGHologram* NativeRotationHologram = nullptr;
 }
 // A Panel choice is an explicit native rotation request, not a scaling wheel event.
 // The guard exists only during that call and only for this exact hologram.
 class FScopedNativeRotation
 {
 public:
  explicit FScopedNativeRotation(const AFGHologram* Hologram)
   : Guard(Private::NativeRotationHologram, Hologram) {}
 private:
  TGuardValue<const AFGHologram*> Guard;
 };
 inline bool IsNativeRotationRequest(const AFGHologram* Hologram)
 {
  return Hologram && Private::NativeRotationHologram == Hologram;
 }
 inline bool UsesParentRotation(const UClass* BuildClass)
 {
  if (!BuildClass) return false;
  const FName Name = BuildClass->GetFName();
  return Name == TEXT("Build_PipelineSupportWall_C") || Name == TEXT("Build_HyperTubeWallSupport_C");
 }
 inline FVector RotateYawOffset(const FVector& Offset, const FRotator& ParentRotation)
 {
  const FQuat Yaw = FRotator(0, ParentRotation.Yaw, 0).Quaternion();
  return ParentRotation.Quaternion().RotateVector(Yaw.UnrotateVector(Offset));
 }
 // Resolve view-relative up and the remaining in-wall horizontal axis. At a
 // diagonal tie keep local Z as up, avoiding a facing-dependent axis switch.
 inline void VerticalAxis(const FRotator& Rotation, int32& Axis, int32& Sign)
 {
  const FVector Y = -Rotation.RotateVector(FVector::RightVector);
  const FVector Z = Rotation.RotateVector(FVector::UpVector);
  Axis = FMath::Abs(Y.Z) > FMath::Abs(Z.Z) + KINDA_SMALL_NUMBER ? 1 : 2;
  Sign = (Axis == 1 ? Y.Z : Z.Z) < 0 ? -1 : 1;
 }
 inline void HorizontalAxis(const FRotator& Rotation, int32& Axis, int32& Sign)
 {
  if (Axis != 1) return;
  int32 UpAxis, UpSign;
  VerticalAxis(Rotation, UpAxis, UpSign);
  Axis = UpAxis == 1 ? 2 : 1;
  const FVector Basis = Axis == 1 ? -Rotation.RotateVector(FVector::RightVector)
                                 : Rotation.RotateVector(FVector::UpVector);
  const FVector FlatY = -FRotator(0, Rotation.Yaw, 0).RotateVector(FVector::RightVector);
  Sign *= FVector::DotProduct(Basis, FlatY) < 0 ? -1 : 1;
 }
 inline FVector RunNormal(const FRotator& Rotation, const FVector& Toward)
 {
  FVector Normal = Rotation.RotateVector(FVector::RightVector).GetSafeNormal();
  return FVector::DotProduct(Normal, Toward) < 0 ? -Normal : Normal;
 }
}
