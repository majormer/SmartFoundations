#include "SFPassthroughAdapter.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Buildables/FGBuildableFoundation.h"

FSFPassthroughAdapter::FSFPassthroughAdapter(AFGHologram* InHologram)
	: FSFHologramAdapterBase(InHologram)
	, PassthroughHologram(Cast<AFGPassthroughHologram>(InHologram))
{
	check(PassthroughHologram);
}

FBoxSphereBounds FSFPassthroughAdapter::GetBuildingBounds() const
{
	// TODO: Bounds should reflect actual mesh after RebuildMeshesAndUpdateClearance()
	// For now, return default floor hole size (8m x 8m x 4m foundation height)
	// 
	// IMPLEMENTATION NOTES:
	// - Should query mSnappedBuildingThickness from PassthroughHologram
	// - Size may vary based on snapped foundation
	// - Consider mMaxPassthroughLength for multi-foundation spans
	FVector Size(800.0f, 800.0f, 400.0f); // Default 8m foundation size
	return FBoxSphereBounds(FVector::ZeroVector, Size * 0.5f, Size.Size() * 0.5f);
}

FTransform FSFPassthroughAdapter::GetBaseTransform() const
{
	return PassthroughHologram->GetActorTransform();
}

void FSFPassthroughAdapter::ApplyTransformOffset(const FVector& Offset)
{
	// TODO: Implement if needed for preview offsetting
}

TWeakObjectPtr<AFGHologram> FSFPassthroughAdapter::GetHologram() const
{
	return PassthroughHologram;
}

bool FSFPassthroughAdapter::IsValid() const
{
	return PassthroughHologram != nullptr && ::IsValid(PassthroughHologram);
}

bool FSFPassthroughAdapter::SupportsFeature(ESFFeature Feature) const
{
	// Issue #187: Enable scaling for floor holes (grid placement on foundations)
	// Children snap individually to foundations via AFGPassthroughHologram::TrySnapToActor()
	switch (Feature)
	{
	case ESFFeature::ScaleX:
	case ESFFeature::ScaleY:
	case ESFFeature::ScaleZ:
		return true;  // Grid placement - children snap to foundations independently
	case ESFFeature::Spacing:
		return true;  // Custom spacing between floor holes
	default:
		return false;  // Nudge, Steps, Stagger, Rotation - disabled (snapping controls position)
	}
}

FString FSFPassthroughAdapter::GetAdapterTypeName() const
{
	return TEXT("Passthrough");
}

// IMPLEMENTATION ROADMAP:
// 
// Phase 1 - Basic Support (Current):
// ✅ Adapter registration and type detection
// ✅ Default size reporting
// ⬜ All features disabled for safety
// 
// Phase 2 - Foundation Snapping Integration:
// ⬜ Implement foundation detection at child positions
// ⬜ Call TrySnapToActor() for each child during positioning
// ⬜ Verify mSnappedFoundations populated correctly
// ⬜ Handle snap failure gracefully (mark child invalid)
// 
// Phase 3 - Clearance Management:
// ⬜ Ensure GetIgnoredClearanceActors() includes snapped foundations
// ⬜ Test clearance validation with adjacent buildings
// ⬜ Handle clearance for multi-foundation spans
// 
// Phase 4 - Mesh Rebuild Coordination:
// ⬜ Trigger RebuildMeshesAndUpdateClearance() after child positioning
// ⬜ Verify mesh components generated correctly for children
// ⬜ Handle thickness variations across grid
// 
// Phase 5 - Feature Enablement (if safe):
// ⬜ Evaluate X/Y scaling (probably NO due to snapping)
// ⬜ Evaluate Z-scaling (probably NO due to foundation thickness)
// ⬜ Evaluate Steps (probably NO due to foundation level requirement)
// ⬜ Document why each feature is disabled
// 
// TESTING CHECKLIST:
// ⬜ Single floor hole placement
// ⬜ 2x1 grid of floor holes on same foundation
// ⬜ Floor holes across multiple foundations (if supported)
// ⬜ Clearance validation with nearby buildings
// ⬜ Different foundation types (normal, frame, inverted ramp)
// ⬜ Multiplayer replication of floor hole grids
