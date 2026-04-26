#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFFloodlightChildHologram.generated.h"

/**
 * Smart! child hologram for wall-mounted floodlights.
 * Extends AFGFloodlightHologram so the existing multi-step property sync
 * (mFixtureAngle, mBuildStep, OnRep_FixtureAngle) works via Cast<AFGFloodlightHologram>.
 *
 * Overrides CheckValidPlacement to skip the wall snap check that children can't satisfy
 * (children are positioned by Smart! grid calculations, not vanilla wall snapping).
 *
 * Issue #200: Created because ASFBuildableChildHologram (AFGBuildableHologram base)
 * doesn't have mFixtureAngle/mBuildStep, so floodlight angle sync fails with it.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFFloodlightChildHologram : public AFGFloodlightHologram
{
	GENERATED_BODY()

public:
	ASFFloodlightChildHologram();

	/** Always-valid placement — skip wall snap check that children can't satisfy */
	virtual void CheckValidPlacement() override;

	/** Skip clearance checks for grid children */
	virtual void CheckClearance() override;

	/** Block parent from repositioning — Smart! handles positioning via SetActorLocation */
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

	/** Cleanup data structure on destruction */
	virtual void Destroyed() override;

	/** Public setter for build class (mBuildClass is protected) */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
};
