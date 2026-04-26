#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGConveyorBeltHologram.h"
#include "SFConveyorBeltHologram.generated.h"

/**
 * Smart Conveyor Belt hologram with Auto-Connect support.
 * Always validates as valid to prevent blocking parent hologram placement.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorBeltHologram : public AFGConveyorBeltHologram
{
    GENERATED_BODY()

public:
    // Constructor ensures ticking is enabled for runtime position corrections
    ASFConveyorBeltHologram();

    // BeginPlay to log NetMode and world context for debugging
    virtual void BeginPlay() override;

    // Per-frame update hook allows us to correct actor position if engine resets it
    virtual void Tick(float DeltaSeconds) override;

    /** Override placement validation to always succeed for child hologram preview */
    virtual void CheckValidPlacement() override;

    /** Override to push hologram materials onto spline meshes (engine won't do this for dynamically created spline meshes) */
    virtual void SetPlacementMaterialState(EHologramMaterialState materialState) override;
    
    /** 
     * Override Construct to prevent building when used as EXTEND preview.
     * When used as Auto-Connect child, this will build the belt normally.
     * When used as EXTEND preview child, this returns nullptr to prevent building.
     */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    /**
     * Override PostHologramPlacement to skip vanilla spline update for child holograms.
     * Vanilla's GenerateAndUpdateSpline() crashes when called on our child belt holograms
     * because we've already set up the spline manually via TriggerMeshGeneration().
     */
    virtual void PostHologramPlacement(const FHitResult& hitResult, bool callForChildren) override;
    
    /**
     * Override to prevent parent hologram from moving this belt child.
     * When the parent distributor moves, it calls SetHologramLocationAndRotation on children,
     * which resets the belt to origin. We block this to maintain our configured world position.
     */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
    
    /**
     * Override GetCost to calculate belt material cost based on spline length.
     * This enables automatic cost aggregation when belt holograms are registered as children.
     * @param includeChildren Whether to include child hologram costs (passed to parent)
     * @return Belt material cost based on length and tier
     */
    virtual TArray<FItemAmount> GetCost(bool includeChildren) const override;
    
    // Custom method to set up belt spline between two connectors
	void SetupBeltSpline(UFGFactoryConnectionComponent* StartConnector, UFGFactoryConnectionComponent* EndConnector);
	
	// Public wrapper to trigger mesh generation after FinishSpawning() completes
	// This allows the subsystem to call the protected UpdateSplineComponent() method
	void TriggerMeshGeneration();
	
	// Public wrapper to access protected spline component for dynamic updates
	// This allows BeltPreviewHelper to access mSplineComponent without AccessTransformers
	USplineComponent* GetSplineComponent() { return mSplineComponent; }
	const TArray<FSplinePointData>& GetSplineData() const { return mSplineData; }
	const TArray<FSplinePointData>& GetSplinePointData() const { return mSplineData; }  // Alias for compatibility
	
	// Set spline data directly and update the spline component
	void SetSplineDataAndUpdate(const TArray<FSplinePointData>& InSplineData);
	
	// Public wrapper to set build class before spawning
	void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
	
	/** 
	 * Call TryUpgrade with a hit result pointing at the target belt.
	 * This sets mUpgradedConveyorBelt internally and triggers vanilla upgrade flow.
	 */
	bool SetupUpgradeTarget(class AFGBuildableConveyorBelt* InUpgradeTarget);
	
	// Public wrapper to call protected GenerateAndUpdateSpline for upgrade operations
	void CallGenerateAndUpdateSpline(const FHitResult& HitResult) { GenerateAndUpdateSpline(HitResult); }
	
	// Route spline between two connectors using their positions and normals
	void AutoRouteSplineWithNormals(const FVector& StartPos, const FVector& StartNormal, 
	                                 const FVector& EndPos, const FVector& EndNormal);
	
	/**
	 * Use engine's automatic spline routing via build modes.
	 * This is a public wrapper that accesses the private mBuildModeCurve and calls
	 * the protected GenerateAndUpdateSpline() method.
	 * @return True if build mode routing succeeded, false if not available (fallback to AutoRouteSplineWithNormals)
	 */
	
	
	// Nuclear option: Start continuous position correction via recurring timer
	void StartContinuousPositionCorrection(UFGFactoryConnectionComponent* Output, UFGFactoryConnectionComponent* Input);
	void StopContinuousPositionCorrection();
	
	// Force hologram material to be applied to all mesh components
	// Call this after TriggerMeshGeneration() to ensure spline meshes have correct hologram material
	void ForceApplyHologramMaterial();
	
	/**
	 * Set the snapped connection components for this belt hologram.
	 * This tells the vanilla system that the belt is already connected at both ends,
	 * preventing it from spawning child pole holograms.
	 * 
	 * @param Connection0 The connection at the start of the belt (can be nullptr)
	 * @param Connection1 The connection at the end of the belt (can be nullptr)
	 */
	void SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1);

	bool TryUseBuildModeRouting(
		const FVector& StartPos,
		const FVector& StartNormal,
		const FVector& EndPos,
		const FVector& EndNormal);

	void SetRoutingMode(int32 InRoutingMode) { RoutingMode = InRoutingMode; }
	bool GetLastVanillaPlacementValid() const { return bLastVanillaPlacementValid; }

protected:
	// Override ConfigureActor to initialize mesh assets from build class
	virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;
	
	/**
	 * Override ConfigureComponents to rebuild chain actors after connections are established.
	 * This runs AFTER ConfigureActor but BEFORE the buildable is fully registered with the subsystem.
	 * This is the same timing AutoLink uses for their Remove→Connect→Add pattern.
	 */
	virtual void ConfigureComponents(class AFGBuildable* inBuildable) const override;
	
private:
	// Timer handle for continuous position correction (nuclear option)
	FTimerHandle ContinuousPositionCorrectionTimer;
	
	// Weak pointers to connectors for continuous correction
	TWeakObjectPtr<UFGFactoryConnectionComponent> CachedOutputConnector;
	TWeakObjectPtr<UFGFactoryConnectionComponent> CachedInputConnector;

	// Tracks last known actor location so we can detect sudden snaps to origin
	FVector PreviousLocationSample = FVector::ZeroVector;

	int32 RoutingMode = 0;
	bool bLastVanillaPlacementValid = true;
	
	// Track if PostHologramPlacement has already been called for Extend children
	// Vanilla parent hologram calls this repeatedly during tick, but we only want to run it once
	// to establish snapped connections - subsequent calls crash due to garbage collected spline data
	bool bPostHologramPlacementCalled = false;
};
