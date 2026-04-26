#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "SmartFoundations.h"
#include "FGConstructDisqualifier.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"

ASFPassthroughChildHologram::ASFPassthroughChildHologram()
{
    // Minimal constructor — tick and collision disabled post-spawn (not here)
    // Matches working ASFConveyorAttachmentChildHologram pattern
}

void ASFPassthroughChildHologram::CheckValidPlacement()
{
    // Issue #187: Check data structure for validation control (same as ConveyorAttachment pattern)
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
    {
        if (!Data->bNeedToCheckPlacement)
        {
            SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
            return;
        }
    }
    
    // Fallback: always skip validation for passthrough children
    ResetConstructDisqualifiers();
    SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

AActor* ASFPassthroughChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    UE_LOG(LogSmartFoundations, Log, TEXT("PASSTHROUGH CHILD: Construct() called for %s"), *GetName());
    
    // Delegate to vanilla passthrough construction — handles floor hole building creation
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    
    if (BuiltActor)
    {
        // Register in JsonBuiltActors for post-build passthrough↔lift linking (Issue #260)
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        if (HoloData && !HoloData->JsonCloneId.IsEmpty())
        {
            if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
            {
                if (USFExtendService* ExtendService = SmartSubsystem->GetExtendService())
                {
                    ExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                }
            }
        }
        
        UE_LOG(LogSmartFoundations, Log, TEXT("PASSTHROUGH CHILD: Successfully built %s -> %s (CloneId=%s)"),
            *GetName(), *BuiltActor->GetName(),
            HoloData ? *HoloData->JsonCloneId : TEXT("none"));
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("PASSTHROUGH CHILD: Construct returned nullptr for %s"), *GetName());
    }
    
    return BuiltActor;
}

void ASFPassthroughChildHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}

void ASFPassthroughChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // Block parent from moving this child — our grid system positions it manually.
    // Same pattern as ASFConveyorBeltHologram for Extend children.
}

void ASFPassthroughChildHologram::SetSnappedThickness(float InThickness)
{
    // Access our own protected member via 'this' (valid C++)
    mSnappedBuildingThickness = InThickness;
    
    // Rebuild mesh to reflect the new thickness
    RebuildMeshesAndUpdateClearance();
    
    UE_LOG(LogSmartFoundations, Log, TEXT("PASSTHROUGH CHILD: %s set thickness=%.0f, mesh rebuilt"),
        *GetName(), mSnappedBuildingThickness);
}
