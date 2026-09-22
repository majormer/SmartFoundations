// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "SFPipeColorSnapshot.generated.h"

class AFGBuildableHologram;
class ASFPipelineHologram;
class UFGPipeConnectionComponent;

/** Paint intent only: never copy a building's material, skin, pattern, or runtime shader data. */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFPipeColorSnapshot
{
	GENERATED_BODY()
	UPROPERTY() bool bCaptured = false;
	UPROPERTY() TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch;
	UPROPERTY() FFactoryCustomizationColorSlot Colors;

	static FSFPipeColorSnapshot Capture(const FFactoryCustomizationData& Data);
	FFactoryCustomizationData ToCustomization() const;
	void Apply(AFGBuildableHologram* Hologram) const;
	/** Direct pipe endpoints win. A fitting may inherit only unanimous paint from its attached pipes. */
	static void Inherit(ASFPipelineHologram* Hologram, UFGPipeConnectionComponent* Start, UFGPipeConnectionComponent* End);
};
