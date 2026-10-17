// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;

namespace SFScalingFootprint
{
	/** Project local geometry into Smart's yaw-relative horizontal axes and world vertical axis. */
	FVector FromLocalDimensions(const FVector& LocalDimensions, const FRotator& ParentRotation);
	/** Round projected horizontal pitches upward without changing physical/functional height. */
	FVector RoundHorizontalToGrid(const FVector& Dimensions, double GridStep);
	/** Fit fixed geometry on whole native nudge steps without overlapping it. */
	double RoundHeightToNudge(double Height, double NudgeStep);
	/** Selected native geometry and fixed profiles; horizontal snap pitches stay separate from physical vertical bounds. */
	bool TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions);
}
