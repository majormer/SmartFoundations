// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "SFBuildableHologram.h"
#include "Hologram/FGFoundationHologram.h"
#include "Features/Scaling/SFScalingSpec.h"  // FSFScalingSpec (MP spec-based construction)
#include "SFFoundationHologram.generated.h"

/**
 * Base class for all Smart foundation holograms.
 * Handles foundation-specific validation and grid snapping.
 *
 * Also the spec-capable scaling parent for FOUNDATION grids in multiplayer: mirrors
 * ASFFactoryHologram's spec hooks (capture + strip client-side, regenerate server-side), delegating
 * the shared logic to SFScalingSpecExpansion. The plain-scaling swap in RegisterActiveHologram picks
 * this class for foundation-family holograms (production buildings use ASFFactoryHologram).
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFFoundationHologram : public AFGFoundationHologram
{
    GENERATED_BODY()

public:
    ASFFoundationHologram();

    // Foundation-specific functionality
    virtual void BeginPlay() override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;

    /** Copy mBuildClass/mRecipe from the vanilla hologram being swapped out (call before FinishSpawning). */
    void InitializeFromHologram(AFGHologram* SourceHologram);

    // ── Multiplayer spec-based construction (see ASFFactoryHologram for the model description)
    void SetScalingSpec(const FSFScalingSpec& InSpec) { mScalingSpec = InSpec; }
    const FSFScalingSpec& GetScalingSpec() const { return mScalingSpec; }

    /** Client: capture the grid spec + strip grid children from the serialized construct message. */
    virtual void PreConstructMessageSerialization() override;
    /** Client (saving): after the message bytes are written, restore the stripped children into
     *  mChildren so the post-fire hologram teardown destroys the previews normally (no orphans). */
    virtual void SerializeConstructMessage(FArchive& ar, FNetConstructionID id) override;

    /** Spec path: at server cost-charge time the grid children do not exist yet (they are expanded
     *  inside Construct, AFTER validation), so scale the uniform per-cell cost by the cell count. */
    virtual TArray<FItemAmount> GetCost(bool includeChildren) const override;

    /** Server: expand the grid children from the spec post-validation, then construct normally. */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;

protected:
    /** Compact grid description, replicated/serialized with the construct message (CustomSerialization). */
    UPROPERTY(CustomSerialization)
    FSFScalingSpec mScalingSpec;

    /** Children temporarily detached from mChildren during client serialization (kept off the wire). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<class AFGHologram>> mStashedSpecChildren;

    // Foundation validation and snapping
    virtual void ValidateFoundationPlacement();
    virtual void ApplyFoundationSnapping();

    // Common helper functions
    virtual void LogSmartActivity(const FString& Activity) const;
};
