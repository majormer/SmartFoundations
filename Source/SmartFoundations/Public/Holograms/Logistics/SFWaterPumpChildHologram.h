// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Water Pump Child Hologram

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "SFWaterPumpChildHologram.generated.h"

class AFGWaterVolume;

/**
 * Child hologram for scaled water extractors
 * 
 * Inherits from AFGResourceExtractorHologram (not AFGWaterPumpHologram, which has a
 * private constructor). Replaces vanilla CheckMinimumDepth() with Smart!'s own water
 * volume validation. Vanilla validation fails for children positioned via
 * SetActorLocation (even over valid water). This class implements a direct
 * AFGWaterVolume::EncompassesPoint() check instead.
 * 
 * IMPORTANT: Unlike ceiling lights or passthroughs, water extractors MUST validate
 * that they are over water. An unvalidated extractor over land would generate free
 * water resources, breaking game balance. We do NOT skip resource validation.
 * 
 * Pattern matches ASFConveyorAttachmentChildHologram, ASFPipelineJunctionChildHologram, etc.
 * for spawn/lifecycle, but adds its own resource validation.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFWaterPumpChildHologram : public AFGResourceExtractorHologram
{
	GENERATED_BODY()

public:
	ASFWaterPumpChildHologram();

	// Begin AFGHologram Interface
	virtual void CheckValidPlacement() override;
	/** #418: no-op — blocks vanilla parent-propagation resets; Smart! positions via SetActorLocation. */
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
	// End AFGHologram Interface

	// Begin AFGBuildableHologram Interface
	virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;
	// End AFGBuildableHologram Interface

	/** Public setter for build class (mBuildClass is protected on AFGHologram) */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }

	/**
	 * Smart!'s own water validation — replaces vanilla CheckMinimumDepth() for children.
	 * Checks if this hologram's position is inside any AFGWaterVolume using EncompassesPoint().
	 * Public so the MP fire-gate (#428) can refuse a client build whose grid cells fall on land,
	 * before the spec is staged and the previews destroyed.
	 * @return true if over valid water, false if over land
	 */
	bool ValidateWaterPosition() const;

public:
	/** [#497] Block vanilla's locked-parent nudge cascade — it bypasses SetHologramLocationAndRotation
	 *  and dragged every extend child to world origin each tick (see the .cpp override). */
	virtual void SetHologramNudgeLocation() override;

};
