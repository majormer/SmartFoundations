// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFFloodlightChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"

ASFFloodlightChildHologram::ASFFloodlightChildHologram()
{
	// Minimal constructor — configuration happens post-spawn via deferred construction
}

void ASFFloodlightChildHologram::CheckValidPlacement()
{
	// Issue #200: Always valid — children are positioned by Smart! grid calculations.
	// Wall-mounted floodlights' vanilla CheckValidPlacement checks wall snapping,
	// which children can't satisfy. Skip it entirely.
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFFloodlightChildHologram::CheckClearance()
{
	// Issue #200: Skip clearance checks — children don't need independent clearance validation.
}

void ASFFloodlightChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// Block parent from repositioning — Smart! handles positioning via SetActorLocation.
}

void ASFFloodlightChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}

void ASFFloodlightChildHologram::SetHologramNudgeLocation()
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
