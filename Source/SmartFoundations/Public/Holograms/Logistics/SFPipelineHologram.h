#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGPipelineHologram.h"
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

	void SetRoutingMode(int32 InRoutingMode) { RoutingMode = InRoutingMode; }
    
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
