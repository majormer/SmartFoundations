#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGConveyorLiftHologram.h"
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
    
    /**
     * Set the snapped connection components for this lift.
     * Used by EXTEND to establish connections before building.
     * @param Connection0 The connection at the bottom of the lift (can be nullptr)
     * @param Connection1 The connection at the top of the lift (can be nullptr)
     */
    void SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1);
};
