// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "Shared/Conduits/ConduitPreviewHelperTypes.h"
#include "FGPipeConnectionComponentHyper.h"
#include "Holograms/Logistics/SFPipelineHologram.h"

/** Normally priced native tube between genuine Hyper network ports on Blueprint copies. */
class SMARTFOUNDATIONS_API FSFHypertubePreviewHelper
	: public TConduitPreviewHelper<UFGPipeConnectionComponentHyper, ASFPipelineHologram>
{
	using Super = TConduitPreviewHelper<UFGPipeConnectionComponentHyper, ASFPipelineHologram>;
public:
	FSFHypertubePreviewHelper(UWorld* InWorld, AFGHologram* Parent);
	void UpdatePreview(UFGPipeConnectionComponentHyper* Start, UFGPipeConnectionComponentHyper* End);
protected:
	virtual FString GetConnectorType() const override;
	virtual TSubclassOf<AFGSplineHologram> GetHologramClass() const override;
	virtual TSubclassOf<AFGBuildable> GetBuildClass(USFSubsystem* Subsystem) const override;
	virtual void ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem) override;
	virtual void SetupSplineRouting(AFGSplineHologram* SpawnedHologram) override;
};
