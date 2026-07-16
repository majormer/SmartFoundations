// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "SFBuildableHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "SFFactoryHologram.generated.h"

/**
 * Base class for all Smart factory holograms.
 * Implements recipe copying functionality for production buildings.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFFactoryHologram : public AFGFactoryHologram
{
    GENERATED_BODY()

public:
    ASFFactoryHologram();

    // Factory-specific functionality
    virtual void BeginPlay() override;
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;
    
    // CRITICAL: Override to block repositioning when EXTEND/scaling is active
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
    
    // Override CheckValidPlacement to skip clearance checks during EXTEND mode.
    // Without this, vanilla's clearance detection adds encroachment disqualifiers
    // that prevent building, especially with rotation.
    virtual void CheckValidPlacement() override;

    /** Depot-aware affordability during Extend: vanilla CheckCanAfford ignores the Dimensional
     *  Depot, which would flag an extend buildable only from the depot as unaffordable (red). */
    virtual void CheckCanAfford(class UFGInventoryComponent* inventory) override;

    /** Propagate parent placement material changes to Smart-owned child previews. */
    virtual void SetPlacementMaterialState(EHologramMaterialState materialState) override;
    
    /**
     * Initialize this hologram with the build class from another hologram.
     * Must be called BEFORE BeginPlay (e.g., after SpawnActorDeferred, before FinishSpawning).
     * @param SourceHologram The hologram to copy the build class from
     */
    void InitializeFromHologram(AFGHologram* SourceHologram);

    // (The MP spec-based construction machinery that briefly lived on this class - spec UPROPERTY,
    // Pre/SerializeConstructMessage overrides, GetCost scaling, Construct expansion - moved to the
    // class-agnostic hook path in USFGameInstanceModule::RegisterSpecConstructionHooks + the
    // hologram data registry, so it covers every scalable buildable without hologram swaps.)

protected:
    // Recipe copying implementation
    virtual void ApplyStoredRecipe(AActor* Building) const;
    virtual bool IsProductionBuilding(AActor* Building) const;

    // Common helper functions (inherited from buildable holograms)
    virtual void LogSmartActivity(const FString& Activity) const;
    virtual void SetSmartMetadata(int32 GroupIndex, int32 ChildIndex);

private:
    /** #497 set-once guard: last state fanned out to Smart children. RefreshExtension re-asserts the
     *  parent state several times per frame; re-cascading an unchanged state repainted every child's
     *  spline meshes every frame. Children spawned later are painted at spawn by their spawner. */
    EHologramMaterialState LastCascadedChildMaterialState = EHologramMaterialState::HMS_OK;
    bool bChildMaterialStateCascaded = false;
};
