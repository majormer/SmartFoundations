// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGConveyorBeltHologram.h"
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
     * #341: drain the list of freshly-built stackable belts (each registers here after it builds and
     * wires its connectors by coincidence). Called from the parent pole hologram's Construct hook -
     * in-frame and BEFORE the factory tick - to register the run so vanilla builds one chain per
     * series-run. Empties the list. THESIS Belts/ChainActors 6.16.
     */
    static void DrainStackBuiltConveyors(TArray<class AFGBuildableConveyorBase*>& OutBelts);

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

    /**
     * #497: Preview belt children frequently carry a null mRecipe (their real cost comes from spline
     * length in GetCost, not a base recipe). Vanilla AFGHologram::GetBaseCost would then call
     * UFGRecipe::GetIngredients(nullptr) and log a warning every frame per child — filling the log with
     * synchronous disk writes (Sentry breadcrumbs) and stuttering the game. A null recipe has no base
     * cost, so short-circuit to empty in that case; otherwise defer to vanilla.
     */
    virtual TArray<FItemAmount> GetBaseCost() const override;

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
	 * [#380] Route this belt as an Extend "lane" segment, honoring the player's configured belt
	 * routing mode (Default / Curve / Straight from the auto-connect settings) - falling back to
	 * AutoRouteSplineWithNormals if build-mode routing is unavailable. Lane belts previously called
	 * AutoRouteSplineWithNormals directly and so always came out "Default", ignoring the setting.
	 * (Factory-internal belts are exact clones of the source and intentionally do NOT use this.)
	 */
	void RouteLaneWithConfiguredMode(const FVector& StartPos, const FVector& StartNormal,
	                                 const FVector& EndPos, const FVector& EndNormal);

	/**
	 * [#380] Route this belt using the VANILLA build-mode descriptors. Maps Smart's belt routing mode
	 * (0=Default, 1=Curve, 2=Straight) to the game's own mBuildModeCurve / mBuildModeStraight and lets
	 * AutoRouteSpline insert real bends (mBendRadius) - the same spline the build gun produces. Falls
	 * back to AutoRouteSplineWithNormals only when the descriptor isn't available. Shared by auto-connect
	 * (SP + MP) and Extend lane belts so "Curve"/"Straight" actually take effect.
	 * @return true if vanilla build-mode routing produced a valid spline; false if it fell back.
	 */
	bool ApplyBeltBuildModeRouting(int32 BeltRoutingMode, const FVector& StartPos, const FVector& StartNormal,
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
	 * #497: Unconditionally sweep every spline mesh with the stencil/visibility setup for the given
	 * state, bypassing the set-once guard in SetPlacementMaterialState. Only for the two moments the
	 * mesh SET changes (TriggerMeshGeneration) or a caller explicitly forces (ForceApplyHologramMaterial);
	 * everything else goes through SetPlacementMaterialState, which skips the sweep when the state is
	 * already applied — repainting per frame was the render-proxy churn behind the Extend GPU lag.
	 */
	void ApplySplineMeshMaterialState(EHologramMaterialState materialState);

	/** #497: invalidate the cached GetCost result — call after any spline/route/build-class change. */
	void InvalidateCostCache() { bSelfCostCacheValid = false; }
	
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

	/** [#466] Which vanilla verdict rejected the last placement check - lets auto-connect report
	 *  the REAL reason (and retry another pairing) instead of inferring from chord angles. */
	bool WasLastRejectTooSteep() const { return bLastRejectTooSteep; }
	bool WasLastRejectInvalidShape() const { return bLastRejectInvalidShape; }

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

	/** [#466] Set by CheckValidPlacement from the vanilla disqualifier list */
	bool bLastRejectTooSteep = false;
	bool bLastRejectInvalidShape = false;

	/** [#414] The belt's DEFAULT build-mode descriptor, lazily copied from the real hologram's CDO
	 *  (like mBuildModeCurve/Straight). Mode-0 routing sets this so Default-mode lanes stop routing
	 *  with NO build mode - the null state that degraded pipe routes until #437. */
	TSubclassOf<class UFGHologramBuildModeDescriptor> CachedDefaultBuildMode;
	
	// Track if PostHologramPlacement has already been called for Extend children
	// Vanilla parent hologram calls this repeatedly during tick, but we only want to run it once
	// to establish snapped connections - subsequent calls crash due to garbage collected spline data
	bool bPostHologramPlacementCalled = false;

	/** #497 set-once guard: the state last swept onto the spline meshes (and whether any sweep ran).
	 *  Meshes created after a sweep are painted by TriggerMeshGeneration's trailing apply. */
	EHologramMaterialState LastAppliedSplineMaterialState = EHologramMaterialState::HMS_OK;
	bool bSplineMaterialStateApplied = false;

	/** #497 (77×1 = 3 fps profile): cached self cost. The build gun's cost panel, vanilla
	 *  CheckCanAfford, and the extend affordability check EACH walk every preview child's GetCost
	 *  per frame; the belt/pipe fallback (recipe lookups + spline math + allocations) measured as
	 *  the dominant game-thread cost at 77 modules. The result only changes when the spline/route/
	 *  class changes — invalidated at every mutation point via InvalidateCostCache(). Consumed only
	 *  for SF_ExtendChild previews with a healthy spline, so the #357 zero-spline restoration inside
	 *  GetCost (load-bearing: it repairs previews vanilla resets) still runs when needed. */
	mutable TArray<FItemAmount> CachedSelfCost;
	mutable bool bSelfCostCacheValid = false;
};
