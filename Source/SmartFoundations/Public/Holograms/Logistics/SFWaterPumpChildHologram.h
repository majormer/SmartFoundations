// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
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
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
	// End AFGHologram Interface

	// Begin AFGBuildableHologram Interface
	virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;
	// End AFGBuildableHologram Interface

	/** Public setter for build class (mBuildClass is protected on AFGHologram) */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }

protected:
	/**
	 * Smart!'s own water validation — replaces vanilla CheckMinimumDepth() for children.
	 * Checks if this hologram's position is inside any AFGWaterVolume using EncompassesPoint().
	 * Adds UFGCDNeedsWaterVolume disqualifier if not over water.
	 * @return true if over valid water, false if over land
	 */
	bool ValidateWaterPosition() const;
};
