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
