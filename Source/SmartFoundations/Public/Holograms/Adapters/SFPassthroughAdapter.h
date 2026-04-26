#pragma once

#include "CoreMinimal.h"
#include "ISFHologramAdapter.h"

/**
 * Adapter for foundation passthrough holes (floor holes for lifts)
 * Handles: Conveyor Lift Floor Holes, Pipe Floor Holes
 * 
 * CRITICAL CHARACTERISTICS (from AFGPassthroughHologram research):
 * 
 * Inheritance: AFGPassthroughHologram -> AFGFactoryHologram -> AFGBuildableHologram
 * 
 * SPECIAL SNAPPING BEHAVIOR:
 * - MUST snap to foundations via TrySnapToActor() to be valid
 * - Tracks snapped foundations in mSnappedFoundations array
 * - Adapts to foundation thickness (mSnappedBuildingThickness)
 * 
 * CLEARANCE SYSTEM:
 * - GetIgnoredClearanceActors() - Ignores snapped foundations
 * - GetClearanceData() - Custom clearance rules
 * - Foundations it snaps to are exempt from clearance checks
 * 
 * DYNAMIC MESH SYSTEM:
 * - RebuildMeshesAndUpdateClearance() - Rebuilds mesh based on snap
 * - mMeshComponents - Generated mesh components that adapt to foundation thickness
 * 
 * MULTI-FOUNDATION SUPPORT:
 * - mAllowMultiFoundationPassThrough - Can span multiple foundations
 * - mMaxPassthroughLength - Maximum span length
 * - TryExtendInWorldDirection() - Extends across foundations
 * 
 * SMART! INTEGRATION CHALLENGES:
 * 1. Children must individually snap to foundations
 * 2. Grid positioning alone is insufficient - snapping determines final position
 * 3. Clearance exemptions must be maintained for each child
 * 4. Foundation thickness data must propagate to children
 * 5. RebuildMeshesAndUpdateClearance() must be called after positioning
 * 
 * IMPLEMENTATION NOTES:
 * - Cannot use simple grid math positioning like factories
 * - Each child needs TrySnapToActor() called with foundation underneath
 * - May need to detect foundation at each grid position
 * - Validate that snapping succeeded before considering child valid
 * 
 * TESTING CONSIDERATIONS:
 * - Test with single foundation (basic case)
 * - Test with multi-foundation spanning (if supported)
 * - Test with different foundation thicknesses
 * - Test clearance validation with adjacent buildings
 * 
 * Research Date: 2025-10-10
 * Source: FGPassthroughHologram.h in Satisfactory 1.1 SML
 */
class SMARTFOUNDATIONS_API FSFPassthroughAdapter : public FSFHologramAdapterBase
{
public:
	FSFPassthroughAdapter(class AFGHologram* InHologram);

	// ISFHologramAdapter interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual FTransform GetBaseTransform() const override;
	virtual void ApplyTransformOffset(const FVector& Offset) override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual TWeakObjectPtr<AFGHologram> GetHologram() const override;
	virtual bool IsValid() const override;
	virtual FString GetAdapterTypeName() const override;

	// TODO: Add passthrough-specific methods for:
	// - Foundation snapping coordination
	// - Thickness data propagation
	// - Clearance exemption management
	// - Mesh rebuild triggering

private:
	/**
	 * Cached reference to passthrough hologram for specialized operations
	 * Cast from base hologram - guaranteed valid in constructor
	 */
	class AFGPassthroughHologram* PassthroughHologram;
};
