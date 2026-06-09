// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFFactoryHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGRecipe.h"
#include "Hologram/FGHologram.h"
#include "FGConstructDisqualifier.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Logging/SFLogMacros.h"

ASFFactoryHologram::ASFFactoryHologram()
{
    // Initialize factory-specific defaults
}

// ────────────────────────────────────────────────────────────────────────────────────────────
// MP spec-based construction (Option A: ride the natural build-gun fire with an O(1) spec)
// See docs/Features/Multiplayer/PLAN_MP_ScalingConstruction_Impl.md
// ────────────────────────────────────────────────────────────────────────────────────────────

void ASFFactoryHologram::PreConstructMessageSerialization()
{
    Super::PreConstructMessageSerialization();

    // Client-side, at fire time: if the spec path is enabled and we have a populated grid spec,
    // detach the grid children so they are NOT serialized into the construct message (keeps the
    // wire O(1)). The server regenerates them from mScalingSpec in PostConstructMessageDeserialization.
    // Guarded so legacy behaviour (and SP / listen-host, which construct directly without a message)
    // is completely unaffected.
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
        TEXT("[MP-SPEC] PreConstructMessageSerialization(factory): captured spec (%d cells), stripped ")
        TEXT("%d grid children from the wire. Construct message is now O(1)."),
        mScalingSpec.CellCount(), mStashedSpecChildren.Num());
}

void ASFFactoryHologram::SerializeConstructMessage(FArchive& ar, FNetConstructionID id)
{
    Super::SerializeConstructMessage(ar, id);

    // Client/saving: the message bytes were just written from the STRIPPED child list. Restore the
    // stash into mChildren immediately so the hologram is whole again - the build gun's post-fire
    // teardown then destroys the preview children normally. Without this, the stripped previews
    // leak as orphans and the grid-spawner tracking desyncs (live-test finding, 2026-06-09).
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
            TEXT("[MP-SPEC] SerializeConstructMessage(factory): restored %d stripped children post-write."),
            mStashedSpecChildren.Num());
        mStashedSpecChildren.Reset();
    }
}

TArray<FItemAmount> ASFFactoryHologram::GetCost(bool includeChildren) const
{
    TArray<FItemAmount> Cost = Super::GetCost(includeChildren);

    // Spec path, server side: cost is validated/charged BEFORE Construct, but the grid children are
    // expanded INSIDE Construct (after validation - fresh holograms cannot pass vanilla placement
    // validation, live-test finding 2026-06-09). The grid is uniform, so the correct total is the
    // parent's per-cell cost scaled by the cell count. Client side never scales: its children are
    // real (mChildren populated), so vanilla aggregation already yields the full amount.
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

void ASFFactoryHologram::BeginPlay()
{
    Super::BeginPlay();
    
    // Debug: Log component information
    TArray<UMeshComponent*> MeshComponents;
    GetComponents(MeshComponents);
    
    // Check for materials
    int32 MaterialCount = 0;
    FString FirstMatName = TEXT("none");
    if (MeshComponents.Num() > 0 && MeshComponents[0])
    {
        MaterialCount = MeshComponents[0]->GetNumMaterials();
        if (MaterialCount > 0 && MeshComponents[0]->GetMaterial(0))
        {
            FirstMatName = MeshComponents[0]->GetMaterial(0)->GetName();
        }
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔄 SFFactoryHologram BeginPlay: Has %d mesh components, mBuildClass=%s, Materials=%d, FirstMat=%s"),
        MeshComponents.Num(),
        mBuildClass ? *mBuildClass->GetName() : TEXT("null"),
        MaterialCount,
        *FirstMatName);
    
    LogSmartActivity(TEXT("Factory hologram initialized"));
}

AActor* ASFFactoryHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // [MP-SPEC] Server-side spec expansion. Runs HERE - after Server_ConstructHologram validation
    // has passed on the (childless) parent - because fresh holograms cannot pass vanilla placement
    // validation (FGCDInitializing/InvalidFloor/InvalidAimLocation; live-test finding 2026-06-09).
    // Cost was already charged correctly via the GetCost cell-count scaling. The children added
    // here are constructed by Super::Construct's normal child loop, never validated.
    if (HasAuthority() && mScalingSpec.bValid && mChildren.Num() == 0)
    {
        SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(this, mScalingSpec, mRecipe);
    }

    // [MP-SLICE0] TEMP multiplayer instrumentation — remove before release.
    // Parent-commit signal: does the scaled-factory Construct run with authority for a
    // CLIENT-initiated build, and how many children does it carry into the build?
    // NetMode: 0=Standalone 1=DedicatedServer 2=ListenServer 3=Client.
    {
        const int32 NetMode = GetWorld() ? (int32)GetWorld()->GetNetMode() : -1;
        UE_LOG(LogSmartFoundations, Display,
            TEXT("[MP-SLICE0] FactoryHologram::Construct ENTER: %s NetMode=%d HasAuthority=%d mChildren=%d"),
            *GetName(), NetMode, HasAuthority() ? 1 : 0, mChildren.Num());
    }

    AActor* BuiltActor = Super::Construct(out_children, constructionID);

    // [MP-SLICE0] TEMP — out_children = built child actors produced server-side this Construct.
    UE_LOG(LogSmartFoundations, Display,
        TEXT("[MP-SLICE0] FactoryHologram::Construct EXIT: %s BuiltActor=%s out_children=%d mChildren=%d"),
        *GetName(), BuiltActor ? *BuiltActor->GetName() : TEXT("null"), out_children.Num(), mChildren.Num());

    if (BuiltActor)
    {
        USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld());
        USFExtendService* ExtService = SmartSubsystem ? SmartSubsystem->GetExtendService() : nullptr;
        
        // Register parent factory with ExtendService for JSON-based post-build wiring
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        if (ExtService && HoloData && !HoloData->JsonCloneId.IsEmpty())
        {
            ExtService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
            UE_LOG(LogSmartFoundations, Log, TEXT("🔧 FACTORY Construct: Registered parent %s → %s for wiring"),
                *HoloData->JsonCloneId, *BuiltActor->GetName());
        }
        
        // Register child factory holograms (scaled extend clones 2+).
        // These are vanilla FGFactoryHologram children whose Construct() doesn't
        // know about JsonCloneId. Match child holograms to built actors by position.
        if (ExtService && out_children.Num() > 0 && mChildren.Num() > 0)
        {
            for (AFGHologram* ChildHolo : mChildren)
            {
                if (!ChildHolo) continue;
                
                FSFHologramData* ChildData = USFHologramDataRegistry::GetData(ChildHolo);
                if (!ChildData || ChildData->JsonCloneId.IsEmpty()) continue;
                
                // Issue #288: Skip children that registered themselves during their own
                // Construct (e.g. ASFPipeAttachmentChildHologram for valves/pumps). The
                // self-registration knows the exact buildable; this fallback proximity
                // search can't disambiguate an attachment from a coincident pipe and
                // would overwrite the correct registration with a neighbouring pipe at
                // dist=0, breaking Phase 3.8a fluid wiring and Phase 3.8b pump power.
                if (ExtService->GetBuiltActorByCloneId(ChildData->JsonCloneId) != nullptr)
                {
                    continue;
                }
                
                // Find the built actor closest to this child hologram's position
                FVector ChildPos = ChildHolo->GetActorLocation();
                AActor* BestMatch = nullptr;
                float BestDist = 200.0f;  // Max match distance (cm)
                
                for (AActor* ChildActor : out_children)
                {
                    if (!ChildActor) continue;
                    float Dist = FVector::Dist(ChildActor->GetActorLocation(), ChildPos);
                    if (Dist < BestDist)
                    {
                        BestDist = Dist;
                        BestMatch = ChildActor;
                    }
                }
                
                if (BestMatch)
                {
                    ExtService->RegisterJsonBuiltActor(ChildData->JsonCloneId, BestMatch);
                    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 FACTORY Construct: Registered child %s → %s (dist=%.0f)"),
                        *ChildData->JsonCloneId, *BestMatch->GetName(), BestDist);
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 FACTORY Construct: No match for child %s at (%+.0f,%+.0f,%+.0f)"),
                        *ChildData->JsonCloneId, ChildPos.X, ChildPos.Y, ChildPos.Z);
                }
            }
        }
    }
    
    return BuiltActor;
}

void ASFFactoryHologram::ConfigureActor(AFGBuildable* InBuildable) const
{
    Super::ConfigureActor(InBuildable);
    
    if (InBuildable && IsProductionBuilding(InBuildable))
    {
        ApplyStoredRecipe(InBuildable);
    }
}

void ASFFactoryHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // CRITICAL: When EXTEND mode is active, DON'T let the build gun reposition us
    // This is how original Smart! worked - block Super when a feature needs position control
    if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
    {
        if (Subsystem->IsExtendModeActive())
        {
            // EXTEND is handling our position - don't let build gun override it
            return;
        }
        
        // Also block when hologram is locked (for scaling, nudge, etc.)
        if (IsHologramLocked())
        {
            return;
        }
    }
    
    // Normal behavior - let build gun position us
    Super::SetHologramLocationAndRotation(hitResult);
}

void ASFFactoryHologram::CheckValidPlacement()
{
    // During EXTEND mode, skip vanilla's clearance checks entirely.
    // Our extend service manages placement validity — vanilla's CheckValidPlacement
    // adds encroachment disqualifiers (especially with rotation) that block building.
    // Per modding community guidance: override CheckValidPlacement, don't call Super,
    // and don't add any disqualifiers when placement is managed by our code.
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(this))
    {
        if (SmartSubsystem->IsExtendModeActive())
        {
            // Check if scaled extend validation failed (lane segments too long/steep)
            if (USFExtendService* ExtendSvc = SmartSubsystem->GetExtendService())
            {
                if (!ExtendSvc->IsScaledExtendValid())
                {
                    AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                    return;
                }
            }
            // Extend is active and valid — don't call Super.
            return;
        }
    }
    
    // Normal mode — use vanilla clearance checks
    Super::CheckValidPlacement();
}

void ASFFactoryHologram::CheckCanAfford(UFGInventoryComponent* inventory)
{
    // Vanilla CheckCanAfford ignores the Dimensional Depot, so an extend that is buildable only
    // from the depot would be flagged FGCDUnaffordable (red). During extend, defer to the extend
    // service's depot-aware affordability so the preview matches what construction will actually do.
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(this))
    {
        if (SmartSubsystem->IsExtendModeActive())
        {
            if (USFExtendService* ExtendSvc = SmartSubsystem->GetExtendService())
            {
                if (ExtendSvc->CanAffordExtendCost(this, inventory))
                {
                    return;  // Affordable via inventory + Dimensional Depot - add no disqualifier.
                }
                AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
                return;
            }
        }
    }

    Super::CheckCanAfford(inventory);
}

void ASFFactoryHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
    Super::SetPlacementMaterialState(materialState);

    for (AFGHologram* Child : mChildren)
    {
        if (!IsValid(Child))
        {
            continue;
        }

        const bool bIsSmartChild =
            Child->Tags.Contains(FName(TEXT("SF_ExtendChild"))) ||
            Child->GetFName().ToString().StartsWith(TEXT("Json"));

        if (!bIsSmartChild)
        {
            continue;
        }

        Child->SetPlacementMaterialState(materialState);

        if (ASFConveyorLiftHologram* LiftChild = Cast<ASFConveyorLiftHologram>(Child))
        {
            LiftChild->ForceApplyHologramMaterial();
        }
        else if (ASFConveyorBeltHologram* BeltChild = Cast<ASFConveyorBeltHologram>(Child))
        {
            BeltChild->ForceApplyHologramMaterial();
        }
        else if (ASFPipelineHologram* PipeChild = Cast<ASFPipelineHologram>(Child))
        {
            PipeChild->ForceApplyHologramMaterial();
        }
    }
}

void ASFFactoryHologram::ApplyStoredRecipe(AActor* Building) const
{
    // Get the Smart subsystem to access recipe system
    if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
    {
        // Check if subsystem has a stored recipe
        if (Subsystem->bHasStoredProductionRecipe && Subsystem->StoredProductionRecipe)
        {
            // Apply recipe to production building
            Subsystem->ApplyStoredProductionRecipeToBuilding(Cast<AFGBuildable>(Building));
        }
        else
        {
            LogSmartActivity(TEXT("No stored recipe available to apply"));
        }
    }
    else
    {
        LogSmartActivity(TEXT("Smart subsystem not available - cannot apply recipe"));
    }
}

bool ASFFactoryHologram::IsProductionBuilding(AActor* Building) const
{
    return Building && Building->GetClass()->IsChildOf(AFGBuildableFactory::StaticClass());
}

void ASFFactoryHologram::LogSmartActivity(const FString& Activity) const
{
    SF_LOG_ADAPTER(Verbose, TEXT("Smart Factory Hologram: %s"), *Activity);
}

void ASFFactoryHologram::SetSmartMetadata(int32 GroupIndex, int32 ChildIndex)
{
    // This would be implemented if we had the Smart metadata base
    // For now, just log the activity
    LogSmartActivity(FString::Printf(TEXT("Set metadata: Group=%d, Child=%d"), GroupIndex, ChildIndex));
}

void ASFFactoryHologram::InitializeFromHologram(AFGHologram* SourceHologram)
{
    if (!SourceHologram)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("InitializeFromHologram: SourceHologram is null"));
        return;
    }
    
    // Copy the build class from the source hologram
    // mBuildClass is protected in AFGHologram, but we inherit from it so we can access it
    mBuildClass = SourceHologram->GetBuildClass();
    
    // Also copy the recipe if available
    // mRecipe is also protected
    if (TSubclassOf<UFGRecipe> SourceRecipe = SourceHologram->GetRecipe())
    {
        mRecipe = SourceRecipe;
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("InitializeFromHologram: Set mBuildClass=%s, mRecipe=%s"), 
        mBuildClass ? *mBuildClass->GetName() : TEXT("null"),
        mRecipe ? *mRecipe->GetName() : TEXT("null"));
}
