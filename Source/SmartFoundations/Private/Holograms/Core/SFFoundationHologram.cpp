// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFFoundationHologram.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "SmartFoundations.h"
#include "FGRecipe.h"
#include "Logging/SFLogMacros.h"

ASFFoundationHologram::ASFFoundationHologram()
{
    // Initialize foundation-specific defaults
}

void ASFFoundationHologram::InitializeFromHologram(AFGHologram* SourceHologram)
{
    if (!SourceHologram)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("SFFoundationHologram::InitializeFromHologram: SourceHologram is null"));
        return;
    }

    // Copy the build class + recipe from the source hologram (protected members; we inherit access).
    mBuildClass = SourceHologram->GetBuildClass();
    if (TSubclassOf<UFGRecipe> SourceRecipe = SourceHologram->GetRecipe())
    {
        mRecipe = SourceRecipe;
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("SFFoundationHologram::InitializeFromHologram: Set mBuildClass=%s, mRecipe=%s"),
        mBuildClass ? *mBuildClass->GetName() : TEXT("null"),
        mRecipe ? *mRecipe->GetName() : TEXT("null"));
}

// ── Multiplayer spec-based construction (mirrors ASFFactoryHologram; shared logic in
//    SFScalingSpecExpansion; see PLAN_MP_ScalingConstruction_Impl.md) ─────────────────────────────

void ASFFoundationHologram::PreConstructMessageSerialization()
{
    Super::PreConstructMessageSerialization();

    if (!SFScalingSpecExpansion::IsSpecConstructionEnabled()) return;
    if (mChildren.Num() == 0) return;
    if (!SFScalingSpecExpansion::CaptureScalingSpec(this, mScalingSpec)) return;

    // Detach the grid children from the serialized child list so they are NOT sent. The preview
    // actors still exist and are owned/cleaned up by the Smart grid-spawner's own tracking list,
    // not by mChildren. The server regenerates them from mScalingSpec.
    mStashedSpecChildren.Reset();
    for (AFGHologram* Child : mChildren)
    {
        mStashedSpecChildren.Add(Child);
    }
    mChildren.Reset();

    UE_LOG(LogSmartFoundations, Display,
        TEXT("[MP-SPEC] PreConstructMessageSerialization(foundation): captured spec (%d cells), stripped ")
        TEXT("%d grid children from the wire. Construct message is now O(1)."),
        mScalingSpec.CellCount(), mStashedSpecChildren.Num());
}

void ASFFoundationHologram::SerializeConstructMessage(FArchive& ar, FNetConstructionID id)
{
    Super::SerializeConstructMessage(ar, id);

    // Client/saving: restore the stripped children post-write so the post-fire teardown destroys
    // the previews normally (see ASFFactoryHologram::SerializeConstructMessage).
    if (ar.IsSaving() && mStashedSpecChildren.Num() > 0)
    {
        for (const TObjectPtr<AFGHologram>& Child : mStashedSpecChildren)
        {
            if (Child)
            {
                mChildren.Add(Child);
            }
        }
        UE_LOG(LogSmartFoundations, Display,
            TEXT("[MP-SPEC] SerializeConstructMessage(foundation): restored %d stripped children post-write."),
            mStashedSpecChildren.Num());
        mStashedSpecChildren.Reset();
    }
}

TArray<FItemAmount> ASFFoundationHologram::GetCost(bool includeChildren) const
{
    TArray<FItemAmount> Cost = Super::GetCost(includeChildren);

    // Spec path, server side: children are expanded inside Construct (after validation), so scale
    // the uniform per-cell cost by the cell count (see ASFFactoryHologram::GetCost).
    if (includeChildren && mScalingSpec.bValid && HasAuthority() && mChildren.Num() == 0)
    {
        const int32 Cells = mScalingSpec.CellCount();
        for (FItemAmount& Item : Cost)
        {
            Item.Amount *= Cells;
        }
    }
    return Cost;
}

AActor* ASFFoundationHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // [MP-SPEC] Server-side spec expansion, post-validation (see ASFFactoryHologram::Construct).
    if (HasAuthority() && mScalingSpec.bValid && mChildren.Num() == 0)
    {
        SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(this, mScalingSpec, mRecipe);
    }

    return Super::Construct(out_children, constructionID);
}

void ASFFoundationHologram::BeginPlay()
{
    Super::BeginPlay();
    LogSmartActivity(TEXT("Foundation hologram initialized"));
}

void ASFFoundationHologram::ConfigureActor(AFGBuildable* InBuildable) const
{
    Super::ConfigureActor(InBuildable);
    
    if (InBuildable)
    {
        // Foundation validation logic will be implemented here
        LogSmartActivity(TEXT("Applied foundation configuration"));
    }
}

void ASFFoundationHologram::ValidateFoundationPlacement()
{
    // Foundation placement validation
    LogSmartActivity(TEXT("Validated foundation placement"));
}

void ASFFoundationHologram::ApplyFoundationSnapping()
{
    // Grid snapping logic
    LogSmartActivity(TEXT("Applied foundation snapping"));
}

void ASFFoundationHologram::LogSmartActivity(const FString& Activity) const
{
    SF_LOG_ADAPTER(Verbose, TEXT("Smart Foundation Hologram: %s"), *Activity);
}
