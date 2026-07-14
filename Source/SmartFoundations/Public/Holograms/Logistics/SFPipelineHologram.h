// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGPipelineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "SFPipelineHologram.generated.h"

/**
 * Smart Pipeline hologram with Auto-Connect support.
 * v2: Added manual search fallback for junction connector wiring.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPipelineHologram : public AFGPipelineHologram
{
    GENERATED_BODY()

public:
    ASFPipelineHologram();

    virtual void BeginPlay() override;
    virtual void CheckValidPlacement() override;
    virtual void SetPlacementMaterialState(EHologramMaterialState materialState) override;
    
    /** 
     * Override Construct to build pipe when used as EXTEND child.
     * When tagged with SF_ExtendChild, this builds the pipe via vanilla mechanism.
     */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    /**
     * Override SpawnChildren to prevent vanilla from spawning child pole holograms for EXTEND pipes.
     * EXTEND pipes don't need support poles - they're cloned from existing pipes that already have poles.
     */
    virtual void SpawnChildren(AActor* hologramOwner, FVector spawnLocation, APawn* hologramInstigator) override;
    
    /**
     * Override SetHologramLocationAndRotation to prevent vanilla from spawning child poles during updates.
     * For EXTEND pipes, we don't want any automatic child pole creation.
     */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
    
    /**
     * Override PostHologramPlacement to prevent vanilla from calling UpdateSplineComponent on EXTEND children.
     * The vanilla implementation tries to access spline mesh components that may not exist or have been
     * replaced by our TriggerMeshGeneration(), causing a crash.
     */
    virtual void PostHologramPlacement(const FHitResult& hitResult, bool callForChildren) override;

    void SetupPipeSpline(UFGPipeConnectionComponentBase* StartConnector, UFGPipeConnectionComponentBase* EndConnector);
    void TriggerMeshGeneration();
    
    /**
     * Override GetCost to calculate pipe material costs based on spline length and tier.
     * This ensures pipe costs are properly aggregated into parent hologram for affordability checks.
     * @param includeChildren Whether to include child hologram costs (passed to parent)
     * @return Pipe material cost based on length and tier
     */
    virtual TArray<FItemAmount> GetCost(bool includeChildren) const override;

    USplineComponent* GetSplineComponent() const { return mSplineComponent; }

    void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
    
    /**
     * Set the snapped connection components for this pipe hologram.
     * This tells the vanilla system that the pipe is already connected at both ends,
     * preventing it from spawning child pole holograms.
     * 
     * @param Connection0 The connection at the start of the pipe (can be nullptr)
     * @param Connection1 The connection at the end of the pipe (can be nullptr)
     */
    void SetSnappedConnections(UFGPipeConnectionComponentBase* Connection0, UFGPipeConnectionComponentBase* Connection1);

	bool TryUseBuildModeRouting(
		const FVector& StartPos,
		const FVector& StartNormal,
		const FVector& EndPos,
		const FVector& EndNormal);

	/**
	 * [#383] Route this pipe using the VANILLA build-mode descriptors. Maps the pipe routing mode
	 * (0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical) to the game's own
	 * mBuildMode* descriptors and lets AutoRouteSpline route it - so all six modes (incl. Noodle / H2V,
	 * which Smart's hand-rolled routing never supported) take effect. Falls back to the existing
	 * TryUseBuildModeRouting when a descriptor isn't available. Mirrors the belt ApplyBeltBuildModeRouting.
	 * @return true if a valid spline was produced (vanilla or fallback).
	 */
	bool ApplyPipeBuildModeRouting(int32 PipeRoutingMode, const FVector& StartPos, const FVector& StartNormal,
	                               const FVector& EndPos, const FVector& EndNormal);

	/** [#383] Route this pipe as an Extend "lane" segment: read the configured pipe routing mode and
	 *  drive the vanilla descriptors (mirrors the belt RouteLaneWithConfiguredMode). Previously the
	 *  Extend pipe lanes called TryUseBuildModeRouting without setting the mode, so they always routed Auto. */
	void RoutePipeLaneWithConfiguredMode(const FVector& StartPos, const FVector& StartNormal,
	                                     const FVector& EndPos, const FVector& EndNormal);

	void SetRoutingMode(int32 InRoutingMode) { RoutingMode = InRoutingMode; }

	/** [#437] Floor-hole routing: route from the passthrough face with the exit vector seeding the
	 *  router's start tangent. NO forced stub is added (round 2): the correct vanilla routers build
	 *  their own straight riser out of the face and their own connector stub at the far end
	 *  (ground truth: live capture of a hand-built H2V route). The first param is retired and
	 *  ignored. After routing, the produced shape is validated against vanilla's minimum bend
	 *  radius (mMinBendRadius); on violation the child is flagged (IsRoutedShapeInvalid) and
	 *  CheckValidPlacement paints it invalid with vanilla's own "Invalid Pipe Shape" disqualifier
	 *  instead of force-rendering a shape vanilla would reject.
	 *  @return true if a spline was produced (valid OR flagged-invalid); false if routing failed
	 *          entirely (caller falls back to its hand-rolled spline). */
	bool RouteWithStraightExit(float ExitStubCm, const FVector& StartPos, const FVector& ExitNormal,
	                           const FVector& EndPos, const FVector& EndNormal);

	/** [#437] Did the last RouteWithStraightExit produce a shape vanilla would reject? */
	bool IsRoutedShapeInvalid() const { return bRoutedShapeInvalid; }

	/** Validate the currently generated spline against the shared length cap and vanilla bend radius. */
	bool ValidateCurrentSpline(float MaxSplineLengthCm, bool& OutTooLong);
    
    // Get the 6-point build spline (not the 2-point preview spline)
    const TArray<FSplinePointData>& GetBuildSplineData() const { return mBuildSplineData; }
    
    // Get the current spline data
    const TArray<FSplinePointData>& GetSplineData() const { return mSplineData; }
    
    // Set spline data directly and update the spline component (for EXTEND cloning)
    void SetSplineDataAndUpdate(const TArray<FSplinePointData>& InSplineData);
    
    // Force apply hologram material to all mesh components
    void ForceApplyHologramMaterial();

protected:
    virtual void ConfigureComponents(class AFGBuildable* inBuildable) const override;
    
    /**
     * Override ConfigureActor to restore backup spline data if vanilla cleared it.
     * Same pattern as ASFConveyorBeltHologram - spline data can be cleared during
     * construction, so we restore from backup before the buildable is configured.
     */
    virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;

private:
    float PreviewLengthCm = 0.0f;
	int32 RoutingMode = 0;

	/** [#437] Set by RouteWithStraightExit when the routed shape violates vanilla's minimum bend
	 *  radius; CheckValidPlacement turns it into UFGCDPipeInvalidShape + HMS_ERROR. */
	bool bRoutedShapeInvalid = false;

	/** [#437] Minimum bend radius of the current routed spline (cm), via discrete circumradius
	 *  sampling. Max float when effectively straight/too short to bend. */
	float ComputeMinRoutedBendRadiusCm() const;
    
    // Store the perfect 6-point build spline separately
    // Preview uses simple 2-point, build uses this 6-point
    TArray<FSplinePointData> mBuildSplineData;
    
    // Prevent double-construction (vanilla child system + manual build code)
    // When Construct() is called, this flag is set to prevent duplicate builds
    bool bHasBeenConstructed = false;
    
    // Store the built actor so we can return it on duplicate Construct() calls
    // Returning nullptr crashes vanilla's InternalConstructHologram
    UPROPERTY()
    TWeakObjectPtr<AActor> ConstructedActor;
};
