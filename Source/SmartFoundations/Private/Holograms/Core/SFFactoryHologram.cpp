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
#include "Subsystem/SFPositionCalculator.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Logging/SFLogMacros.h"

// MP spec-based construction toggle. Off by default: the existing path (and the oversized-grid
// safety guard) remain authoritative until this is validated in a live multiplayer session.
// Set `sf.MP.SpecConstruction 1` on the CLIENT to ship the compact grid spec instead of N children.
static TAutoConsoleVariable<int32> CVarSFMPSpecConstruction(
    TEXT("sf.MP.SpecConstruction"),
    0,
    TEXT("Smart!: when 1, scaling grids commit via a compact server-expanded spec (MP) instead of ")
    TEXT("serializing N child holograms. Experimental; default 0 (legacy path + oversized guard)."),
    ECVF_Default);

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
    if (CVarSFMPSpecConstruction.GetValueOnAnyThread() == 0) return;
    if (mChildren.Num() == 0) return;

    // Capture the grid into a compact spec from the authoritative counter state + size registry.
    // (Self-contained on the hologram: the client knows its own grid via the subsystem.)
    USFSubsystem* SS = USFSubsystem::Get(GetWorld());
    if (!SS) return;

    const FSFCounterState Counters = SS->GetCounterState();
    const int32 NX = FMath::Max(1, FMath::Abs(Counters.GridCounters.X));
    const int32 NY = FMath::Max(1, FMath::Abs(Counters.GridCounters.Y));
    const int32 NZ = FMath::Max(1, FMath::Abs(Counters.GridCounters.Z));
    if (NX * NY * NZ <= 1) return; // trivial grid: nothing to expand server-side

    USFBuildableSizeRegistry::Initialize();
    const FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(GetBuildClass());

    mScalingSpec.Counters = Counters;
    mScalingSpec.ItemSize = Profile.DefaultSize;
    mScalingSpec.AnchorOffset = Profile.AnchorOffset;
    mScalingSpec.bValid = true;

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
        TEXT("[MP-SPEC] PreConstructMessageSerialization: captured spec (%d cells), stripped %d grid ")
        TEXT("children from the wire. Construct message is now O(1)."),
        mScalingSpec.CellCount(), mStashedSpecChildren.Num());
}

void ASFFactoryHologram::PostConstructMessageDeserialization()
{
    Super::PostConstructMessageDeserialization();

    // Server-side, after the spec has been read off the wire and before vanilla cost-aggregation +
    // Construct run: regenerate the grid children from the compact spec. With the children present
    // again, vanilla GetCost(includeChildren) charges the full grid and Super::Construct builds it.
    if (!mScalingSpec.bValid) return;
    if (!HasAuthority()) return;
    if (mChildren.Num() > 0) return; // already populated (shouldn't happen on the spec path)

    ExpandScalingSpecIntoChildren();
}

void ASFFactoryHologram::ExpandScalingSpecIntoChildren()
{
    UWorld* World = GetWorld();
    if (!World || !mScalingSpec.bValid) return;
    if (!mRecipe)
    {
        UE_LOG(LogSmartFoundations, Warning,
            TEXT("[MP-SPEC] ExpandScalingSpecIntoChildren: no recipe on parent hologram %s; cannot expand."),
            *GetName());
        return;
    }

    const FSFCounterState& C = mScalingSpec.Counters;
    const FVector ParentLoc = GetActorLocation();
    const FRotator ParentRot = GetActorRotation();

    const int32 NX = FMath::Max(1, FMath::Abs(C.GridCounters.X));
    const int32 NY = FMath::Max(1, FMath::Abs(C.GridCounters.Y));
    const int32 NZ = FMath::Max(1, FMath::Abs(C.GridCounters.Z));
    const int32 SgnX = (C.GridCounters.X < 0) ? -1 : 1;
    const int32 SgnY = (C.GridCounters.Y < 0) ? -1 : 1;
    const int32 SgnZ = (C.GridCounters.Z < 0) ? -1 : 1;

    FSFPositionCalculator Calc;
    AActor* HoloOwner = GetOwner();
    int32 SpawnedChildren = 0;
    int32 LinearIndex = 0;

    for (int32 ZI = 0; ZI < NZ; ++ZI)
    {
        for (int32 YI = 0; YI < NY; ++YI)
        {
            for (int32 XI = 0; XI < NX; ++XI)
            {
                // (0,0,0) is the parent buildable itself (built by Super::Construct), not a child.
                if (XI == 0 && YI == 0 && ZI == 0) continue;

                const int32 GX = XI * SgnX;
                const int32 GY = YI * SgnY;
                const int32 GZ = ZI * SgnZ;

                const FVector CellLoc = Calc.CalculateChildPosition(
                    GX, GY, GZ, ParentLoc, ParentRot,
                    mScalingSpec.ItemSize, C, LinearIndex, mScalingSpec.AnchorOffset);
                ++LinearIndex;

                const FName ChildName(*FString::Printf(TEXT("SFSpecCell_%d_%d_%d"), GX, GY, GZ));

                AFGHologram* Child = AFGHologram::SpawnChildHologramFromRecipe(
                    this, ChildName, mRecipe, HoloOwner, CellLoc,
                    [ParentRot](AFGHologram* NewChild)
                    {
                        if (NewChild)
                        {
                            NewChild->SetActorRotation(ParentRot);
                            NewChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
                        }
                    });

                if (Child)
                {
                    ++SpawnedChildren;
                }
            }
        }
    }

    UE_LOG(LogSmartFoundations, Display,
        TEXT("[MP-SPEC] ExpandScalingSpecIntoChildren: regenerated %d/%d grid children server-side ")
        TEXT("for %s (recipe=%s). Vanilla cost + Construct will now build the full grid."),
        SpawnedChildren, mScalingSpec.CellCount() - 1, *GetName(),
        mRecipe ? *mRecipe->GetName() : TEXT("null"));
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
