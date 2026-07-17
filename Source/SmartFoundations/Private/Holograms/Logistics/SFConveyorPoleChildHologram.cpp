// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFConveyorPoleChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"

ASFConveyorPoleChildHologram::ASFConveyorPoleChildHologram()
{
	// Minimal constructor - configuration happens post-spawn via deferred construction.
}

void ASFConveyorPoleChildHologram::CheckValidPlacement()
{
	// #354: Always valid - children are positioned by Smart! grid calculations. The vanilla conveyor-pole
	// CheckValidPlacement runs snap/clearance checks children can't satisfy. Skip it entirely.
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFConveyorPoleChildHologram::CheckClearance()
{
	// #354: Skip clearance checks - children don't need independent clearance validation.
}

void ASFConveyorPoleChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// #354: Block parent from repositioning - Smart! handles positioning via SetActorLocation.
}

bool ASFConveyorPoleChildHologram::DoMultiStepPlacement(bool isInputFromARelease)
{
	// #354: Children are single-step - Smart! places them at the correct position and syncs the parent's
	// chosen height (mPoleVariationIndex) via SyncMultiStepHologramProperties. Return true = placement done.
	return true;
}

void ASFConveyorPoleChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}

void ASFConveyorPoleChildHologram::SetHologramNudgeLocation()
{
	// [#497] Vanilla's locked-parent placement path (UFGBuildGunStateBuild::TickState ->
	// AFGHologram::UpdateHologramPlacement (FGHologram.cpp:440) -> SetHologramNudgeLocation
	// (FGHologram.cpp:2120)) cascades through mChildren with a PLAIN SetActorLocation of
	// lock-location + nudge offset - bypassing the SetHologramLocationAndRotation no-op entirely.
	// Extend locks its parent, children never capture a lock location (ZeroVector), so the cascade
	// dragged every child to world origin every tick (caught by the #497 origin-trap stack dump).
	// Smart owns this child's transform; parent nudges are propagated by Smart's own
	// transform-change follow. No-op, mirroring the #418 drift contract.
}
