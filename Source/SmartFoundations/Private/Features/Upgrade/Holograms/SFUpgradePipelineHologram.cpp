// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Upgrade/Holograms/SFUpgradePipelineHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildablePipeline.h"

void ASFUpgradePipelineHologram::SetUpgradeSource(AFGBuildablePipeline* Source)
{
	UpgradeSource = Source;
}

void ASFUpgradePipelineHologram::ConfigureActor(AFGBuildable* Buildable) const
{
	// Native indicator placement happens in BeginPlay, after ConfigureActor. The old
	// pipe must survive until its connections are transferred, but must not obstruct
	// the replacement's indicator search in the meantime.
	if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable))
	{
		if (AFGBuildablePipeline* Source = UpgradeSource.Get())
		{
			Pipe->AddIgnoredActorForIndicator(Source);
			TArray<AActor*> SourceChildren;
			Source->GetChildDismantleActors_Implementation(SourceChildren);
			for (AActor* Child : SourceChildren)
			{
				if (IsValid(Child)) Pipe->AddIgnoredActorForIndicator(Child);
			}
		}
	}
	Super::ConfigureActor(Buildable);
}
