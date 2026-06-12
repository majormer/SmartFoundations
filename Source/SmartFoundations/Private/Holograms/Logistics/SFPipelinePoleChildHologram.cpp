// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFPipelinePoleChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"

ASFPipelinePoleChildHologram::ASFPipelinePoleChildHologram()
{
	// Minimal constructor - configuration happens post-spawn via deferred construction.
}

void ASFPipelinePoleChildHologram::CheckValidPlacement()
{
	// #364: Always valid - children are positioned by Smart! grid calculations. The vanilla
	// pipeline-pole CheckValidPlacement runs snap/clearance checks children can't satisfy.
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFPipelinePoleChildHologram::CheckClearance()
{
	// #364: Skip clearance checks - children don't need independent clearance validation.
}

void ASFPipelinePoleChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// #364: Block parent from repositioning - Smart! handles positioning via SetActorLocation.
}

bool ASFPipelinePoleChildHologram::DoMultiStepPlacement(bool isInputFromARelease)
{
	// #364: Children are single-step - Smart! places them at the correct position and syncs the
	// parent's chosen height (mPoleVariationIndex) + vertical angle via
	// SyncMultiStepHologramProperties. Return true = placement done.
	return true;
}

void ASFPipelinePoleChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}
