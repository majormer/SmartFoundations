// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "SFUpgradePipelineHologram.generated.h"

/** Preserves native indicator placement while the source pipe is still alive. */
UCLASS()
class SMARTFOUNDATIONS_API ASFUpgradePipelineHologram : public ASFPipelineHologram
{
	GENERATED_BODY()
public:
	void SetUpgradeSource(class AFGBuildablePipeline* Source);
	virtual void ConfigureActor(class AFGBuildable* Buildable) const override;

private:
	TWeakObjectPtr<class AFGBuildablePipeline> UpgradeSource;
};
