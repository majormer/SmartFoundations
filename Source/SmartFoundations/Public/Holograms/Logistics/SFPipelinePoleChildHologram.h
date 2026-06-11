// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Hologram/FGPoleHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFPipelinePoleChildHologram.generated.h"

/**
 * Smart! child hologram for the STANDARD pipeline support (Build_PipelineSupport_C). #364
 *
 * Extends AFGPipelinePoleHologram so the parent's two-step placement (base, then HEIGHT) AND its
 * vertical ANGLE (mVerticalAngle - the tilt of the top piece + pipe connection for sloped runs,
 * which conveyor poles do not have) can be synced to children. Height is carried by
 * mPoleVariationIndex (private, reflection) and the step by mBuildStep (protected); the angle has
 * public Get/SetVerticalAngle. Smart! positions children via the grid system, so vanilla
 * snap/clearance/multi-step placement is bypassed.
 *
 * Mirrors ASFConveyorPoleChildHologram (#354). See SFSubsystem_HologramLifecycle.cpp
 * SyncMultiStepHologramProperties() for the per-tick sync.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPipelinePoleChildHologram : public AFGPipelinePoleHologram
{
	GENERATED_BODY()

public:
	ASFPipelinePoleChildHologram();

	/** Always-valid placement - children are positioned by Smart!, not vanilla snapping. */
	virtual void CheckValidPlacement() override;

	/** Skip clearance checks for grid children. */
	virtual void CheckClearance() override;

	/** Block parent from repositioning - Smart! handles positioning via SetActorLocation. */
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

	/** Children are single-step - Smart! places them in one step. */
	virtual bool DoMultiStepPlacement(bool isInputFromARelease) override;

	/** Cleanup data registry entry on destruction. */
	virtual void Destroyed() override;

	/** Public setter for build class (mBuildClass is protected). */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }

	/** Public setter for the build step so the parent can sync it to children (mBuildStep is protected). */
	void SetChildBuildStep(EPoleHologramBuildStep InStep) { mBuildStep = InStep; }

	/** Public shim to force a mesh/height refresh after the parent syncs mPoleVariationIndex via
	 *  reflection (fallback if OnRep_PoleVariationIndex does not refresh). UpdatePoleMesh() is
	 *  protected on the base. */
	void RefreshPoleMesh() { UpdatePoleMesh(); }
};
