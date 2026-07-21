// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGConveyorPoleHologram.h"
#include "Hologram/FGPoleHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFConveyorPoleChildHologram.generated.h"

/**
 * Smart! child hologram for the STANDARD conveyor pole (Build_ConveyorPole_C). #354
 *
 * Extends AFGConveyorPoleHologram so the parent's two-step placement (base, then HEIGHT) can be synced to
 * children: the height is carried by mPoleVariationIndex (private) and the step by mBuildStep (protected).
 * Smart! positions children via the grid system, so vanilla snap/clearance/multi-step placement is bypassed.
 *
 * Mirrors ASFStandaloneSignChildHologram (the other multi-step grid child). See
 * SFSubsystem_HologramLifecycle.cpp SyncMultiStepHologramProperties() for the per-tick height sync.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorPoleChildHologram : public AFGConveyorPoleHologram
{
	GENERATED_BODY()

public:
	ASFConveyorPoleChildHologram();

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

	/** Public shim to force a mesh/height refresh after the parent syncs mPoleVariationIndex via reflection
	 *  (fallback if OnRep_PoleVariationIndex does not refresh). UpdatePoleMesh() is protected on the base. */
	void RefreshPoleMesh() { UpdatePoleMesh(); }

public:
	/** [#497] Block vanilla's locked-parent nudge cascade — it bypasses SetHologramLocationAndRotation
	 *  and dragged every extend child to world origin each tick (see the .cpp override). */
	virtual void SetHologramNudgeLocation() override;

};
