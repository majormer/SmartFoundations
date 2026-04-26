#include "Holograms/Core/SFFactoryHologram.h"
#include "SmartFoundations.h"
#include "FGBuildableFactory.h"
#include "FGRecipe.h"
#include "Hologram/FGHologram.h"
#include "FGConstructDisqualifier.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Logging/SFLogMacros.h"

ASFFactoryHologram::ASFFactoryHologram()
{
    // Initialize factory-specific defaults
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
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    
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
