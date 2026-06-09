// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "SFBuildableHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Features/Scaling/SFScalingSpec.h"  // FSFScalingSpec (MP spec-based construction)
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

    // ── Multiplayer spec-based construction (docs/Features/Multiplayer/DESIGN_MP_ConstructionModel.md)
    // The client populates a compact FSFScalingSpec describing the whole grid; on a remote client
    // fire the grid children are stripped from the wire (PreConstructMessageSerialization) so the
    // construct message stays O(1); the server regenerates the children from the spec
    // (PostConstructMessageDeserialization) before vanilla cost-aggregation + Construct run, so both
    // reuse proven machinery. All gated behind the `sf.MP.SpecConstruction` console variable.

    /** Set by the scaling activation path whenever the grid changes; the authoritative spec to expand. */
    void SetScalingSpec(const FSFScalingSpec& InSpec) { mScalingSpec = InSpec; }
    const FSFScalingSpec& GetScalingSpec() const { return mScalingSpec; }

    /** Client: strip grid children from the serialized construct message (keep the spec only). */
    virtual void PreConstructMessageSerialization() override;
    /** Client (saving): after the message bytes are written, restore the stripped children into
     *  mChildren so the post-fire hologram teardown destroys the previews normally (no orphans). */
    virtual void SerializeConstructMessage(FArchive& ar, FNetConstructionID id) override;
    /** Server: regenerate the grid children from the spec before cost/Construct. */
    virtual void PostConstructMessageDeserialization() override;

protected:
    /** Compact grid description, replicated/serialized with the construct message (CustomSerialization). */
    UPROPERTY(CustomSerialization)
    FSFScalingSpec mScalingSpec;

    /** Children temporarily detached from mChildren during client serialization (kept off the wire). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<class AFGHologram>> mStashedSpecChildren;

    // Recipe copying implementation
    virtual void ApplyStoredRecipe(AActor* Building) const;
    virtual bool IsProductionBuilding(AActor* Building) const;
    
    // Common helper functions (inherited from buildable holograms)
    virtual void LogSmartActivity(const FString& Activity) const;
    virtual void SetSmartMetadata(int32 GroupIndex, int32 ChildIndex);
};
