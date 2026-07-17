// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "SFConveyorLiftHologram.generated.h"

/**
 * Smart Conveyor Lift hologram with EXTEND support.
 * Allows lifts to be built as children of parent holograms.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorLiftHologram : public AFGConveyorLiftHologram
{
    GENERATED_BODY()

public:
    ASFConveyorLiftHologram();

    virtual void BeginPlay() override;
    virtual void Destroyed() override;
    
    /** Override to update lift height on HUD */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
    
    /** Override placement validation to always succeed for child hologram preview */
    virtual void CheckValidPlacement() override;

    /**
     * #497: Preview lift children can carry a null mRecipe. Vanilla AFGHologram::GetBaseCost would then
     * call UFGRecipe::GetIngredients(nullptr) and log a warning every frame per child — synchronous disk
     * writes (Sentry breadcrumbs) that stutter the game. A null recipe has no base cost, so return empty
     * in that case; otherwise defer to vanilla.
     */
    virtual TArray<FItemAmount> GetBaseCost() const override;

    /** 
     * Override Construct to build lift when used as EXTEND child.
     * When tagged with SF_ExtendChild, this builds the lift via vanilla mechanism.
     */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    /**
     * Override ConfigureComponents to establish connections and rebuild chain actors.
     * This runs DURING construction (pre-tick) and is the safe place to do Add/Remove/Add.
     */
    virtual void ConfigureComponents(AFGBuildable* inBuildable) const override;
    
    /** Set the top transform (relative to actor) for lift height configuration */
    void SetTopTransform(const FTransform& InTopTransform);
    
    /** Get the configured top transform */
    FTransform GetTopTransform() const;
    
    /** Force apply hologram material to all mesh components */
    void ForceApplyHologramMaterial();
    
    /** Public wrapper to set build class before spawning */
    void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
    
    /** 
     * Call TryUpgrade with a hit result pointing at the target lift.
     * This sets mUpgradedConveyorLift internally and triggers vanilla upgrade flow.
     */
    bool SetupUpgradeTarget(class AFGBuildableConveyorLift* InUpgradeTarget);

private:
    /** Cached lift height for change detection */
    float CachedLiftHeight = 0.0f;

    /** #497 set-once guard: last material state force-applied to the lift meshes. */
    EHologramMaterialState LastAppliedLiftMaterialState = EHologramMaterialState::HMS_OK;
    bool bLiftMaterialStateApplied = false;
    
    /**
     * Set the snapped connection components for this lift.
     * Used by EXTEND to establish connections before building.
     * @param Connection0 The connection at the bottom of the lift (can be nullptr)
     * @param Connection1 The connection at the top of the lift (can be nullptr)
     */
    void SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1);

public:
	/** [#497] Block vanilla's locked-parent nudge cascade — it bypasses SetHologramLocationAndRotation
	 *  and dragged every extend child to world origin each tick (see the .cpp override). */
	virtual void SetHologramNudgeLocation() override;

};
