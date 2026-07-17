// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFStandaloneSignChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"

ASFStandaloneSignChildHologram::ASFStandaloneSignChildHologram()
{
	// Minimal constructor — configuration happens post-spawn via deferred construction
}

void ASFStandaloneSignChildHologram::CheckValidPlacement()
{
	// Issue #192: Always valid — children are positioned by Smart! grid calculations.
	// Standalone signs' vanilla CheckValidPlacement checks wall/floor snapping,
	// which children can't satisfy. Skip it entirely.
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFStandaloneSignChildHologram::CheckValidFloor()
{
	// Issue #192: Skip floor checks — children don't need independent floor validation.
}

void ASFStandaloneSignChildHologram::CheckClearance()
{
	// Issue #192: Skip clearance checks — children don't need independent clearance validation.
}

void ASFStandaloneSignChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// Block parent from repositioning — Smart! handles positioning via SetActorLocation.
}

void ASFStandaloneSignChildHologram::SpawnChildren(AActor* hologramOwner, FVector spawnLocation, APawn* hologramInstigator)
{
	// Issue #192: Skip pole creation — known limitation.
	// See SFHologramHelperService.cpp sign child spawner block for full documentation
	// of approaches attempted and why they failed.
}

bool ASFStandaloneSignChildHologram::DoMultiStepPlacement(bool isInputFromARelease)
{
	// Issue #192: Children are single-step — Smart! places them at the correct position.
	// Return true to indicate placement is complete (no height adjustment step needed).
	return true;
}

void ASFStandaloneSignChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}

void ASFStandaloneSignChildHologram::SetHologramNudgeLocation()
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
