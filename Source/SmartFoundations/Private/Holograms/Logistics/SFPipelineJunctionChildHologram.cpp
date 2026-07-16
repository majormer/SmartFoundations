// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFPipelineJunctionChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "Logging/LogMacros.h"

ASFPipelineJunctionChildHologram::ASFPipelineJunctionChildHologram()
{
    // Minimal constructor - most behavior handled by base class
}

void ASFPipelineJunctionChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // #497 drift-proof no-op: the clone topology owns the transform. Vanilla parent propagation calls
    // this on every mChildren entry per frame and would reposition the junction toward the parent
    // hit result — the reason the extend service historically re-applied positions every frame.
}

void ASFPipelineJunctionChildHologram::CheckValidPlacement()
{
    // Check data structure for validation control
    bool bShouldSkip = ShouldSkipValidation();
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND Junction CheckValidPlacement: %s - ShouldSkip=%d"), 
        *GetName(), bShouldSkip);
    
    if (bShouldSkip)
    {
        // CRITICAL: Force HMS_OK when skipping validation
        SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
        return; // Skip validation - always valid
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

AActor* ASFPipelineJunctionChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // Call parent to build the junction
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    
    // Register with SFExtendService if this is an EXTEND child
    if (BuiltActor)
    {
        AFGBuildable* BuiltBuildable = Cast<AFGBuildable>(BuiltActor);
        
        // Get the chain ID from our hologram data
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        if (HoloData && HoloData->ExtendChainId >= 0 && BuiltBuildable)
        {
            // Get the subsystem and extend service
            USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
            if (Subsystem)
            {
                USFExtendService* ExtendService = Subsystem->GetExtendService();
                if (ExtendService)
                {
                    ExtendService->RegisterBuiltJunction(HoloData->ExtendChainId, BuiltBuildable);
                    
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Junction %s registered in Construct() for pipe chain %d"),
                        *BuiltBuildable->GetName(), HoloData->ExtendChainId);
                }
            }
        }
        
        // Register with ExtendService for JSON-based post-build wiring
        if (HoloData && !HoloData->JsonCloneId.IsEmpty())
        {
            USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
            if (Subsystem)
            {
                USFExtendService* ExtendService = Subsystem->GetExtendService();
                if (ExtendService)
                {
                    ExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                }
            }
        }
    }
    
    return BuiltActor;
}

void ASFPipelineJunctionChildHologram::Destroyed()
{
    // Clean up data structure
    USFHologramDataRegistry::ClearData(this);
    
    Super::Destroyed();
}

bool ASFPipelineJunctionChildHologram::ShouldSkipValidation() const
{
    // Check if we have data structure with validation disabled
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
    {
        return !Data->bNeedToCheckPlacement;
    }

    return false; // Default to validation if no data structure
}
