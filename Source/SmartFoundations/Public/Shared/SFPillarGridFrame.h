// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

/** Native pillar placement frame, independent of conduit-wall capabilities. */
namespace SFPillarGridFrame
{
	inline bool Applies(const UClass* BuildClass)
	{
		if (!BuildClass) return false;
		static const TSet<FName> Classes = {
			TEXT("Build_PillarBase_C"), TEXT("Build_PillarMiddle_C"),
			TEXT("Build_PillarMiddle_Concrete_C"), TEXT("Build_PillarMiddle_Frame_C"),
			TEXT("Build_PillarBase_Small_C"), TEXT("Build_Pillar_Small_Metal_C"),
			TEXT("Build_Pillar_Small_Concrete_C"), TEXT("Build_Pillar_Small_Frame_C")};
		return Classes.Contains(BuildClass->GetFName());
	}

	inline FVector LocalAxis(int32 Axis)
	{
		return Axis == 0 ? -FVector::ForwardVector : Axis == 1 ? -FVector::RightVector : FVector::UpVector;
	}

	inline FVector RotateYawOffset(const FVector& Offset, const FRotator& Rotation)
	{
		return Rotation.Quaternion().RotateVector(FRotator(0, Rotation.Yaw, 0).UnrotateVector(Offset));
	}

	struct FAxes
	{
		int32 Forward = 0, ForwardSign = -1;
		int32 Side = 1, SideSign = -1;
		int32 Vertical = 2, VerticalSign = 1;
	};

	inline FAxes Resolve(const FRotator& Rotation, float ViewYaw)
	{
		FAxes Result;
		FVector Axes[3];
		for (int32 Axis = 0; Axis < 3; ++Axis) Axes[Axis] = Rotation.RotateVector(LocalAxis(Axis));
		// Prefer local Z at a diagonal tie, preserving upright controls.
		for (int32 Axis = 0; Axis < 2; ++Axis)
			if (FMath::Abs(Axes[Axis].Z) > FMath::Abs(Axes[Result.Vertical].Z) + KINDA_SMALL_NUMBER)
				Result.Vertical = Axis;
		Result.VerticalSign = Axes[Result.Vertical].Z < 0 ? -1 : 1;
		const FRotator View(0, ViewYaw, 0);
		const FVector Forward = View.Vector();
		const FVector Right = View.RotateVector(FVector::RightVector);
		Result.Forward = Result.Vertical == 0 ? 1 : 0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
			if (Axis != Result.Vertical && FMath::Abs(FVector::DotProduct(Axes[Axis], Forward))
				> FMath::Abs(FVector::DotProduct(Axes[Result.Forward], Forward)) + KINDA_SMALL_NUMBER)
				Result.Forward = Axis;
		Result.Side = 3 - Result.Vertical - Result.Forward;
		Result.ForwardSign = FVector::DotProduct(Axes[Result.Forward], Forward) < 0 ? -1 : 1;
		Result.SideSign = FVector::DotProduct(Axes[Result.Side], Right) < 0 ? -1 : 1;
		return Result;
	}

	inline FRotator ArrowRotation(int32 Axis, int32 Sign)
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, LocalAxis(Axis) * Sign).Rotator();
	}
}
