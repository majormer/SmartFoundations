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
