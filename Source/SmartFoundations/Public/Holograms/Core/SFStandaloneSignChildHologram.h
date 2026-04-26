#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFStandaloneSignChildHologram.generated.h"

/**
 * Smart! child hologram for standalone signs and billboards.
 * Extends AFGStandaloneSignHologram so the multi-step property sync
 * (mBuildStep) works via Cast<AFGStandaloneSignHologram>.
 *
 * Overrides SpawnChildren to skip pole creation (children don't need poles —
 * Smart! handles positioning via grid calculations).
 *
 * Overrides CheckValidPlacement/CheckValidFloor to skip snap checks that
 * children can't satisfy (they're positioned by Smart!, not vanilla snapping).
 *
 * Issue #192: Created to support scalable signs/billboards with multi-step
 * floor/ceiling placement.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFStandaloneSignChildHologram : public AFGStandaloneSignHologram
{
	GENERATED_BODY()

public:
	ASFStandaloneSignChildHologram();

	/** Always-valid placement — skip snap checks that children can't satisfy */
	virtual void CheckValidPlacement() override;

	/** Skip floor checks for grid children */
	virtual void CheckValidFloor() override;

	/** Skip clearance checks for grid children */
	virtual void CheckClearance() override;

	/** Block parent from repositioning — Smart! handles positioning via SetActorLocation */
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

	/** Skip pole child creation — known limitation (Issue #192, see SFHologramHelperService.cpp) */
	virtual void SpawnChildren(AActor* hologramOwner, FVector spawnLocation, APawn* hologramInstigator) override;

	/** Skip multi-step — children are placed in one step by Smart! */
	virtual bool DoMultiStepPlacement(bool isInputFromARelease) override;

	/** Cleanup data structure on destruction */
	virtual void Destroyed() override;

	/** Public setter for build class (mBuildClass is protected) */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }

	/** Public setter for build step so parent can sync it to children */
	void SetBuildStep(ESignHologramBuildStep InStep) { mBuildStep = InStep; }
};
