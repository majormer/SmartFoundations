// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFConveyorAttachmentChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "Logging/LogMacros.h"

ASFConveyorAttachmentChildHologram::ASFConveyorAttachmentChildHologram()
{
    // Minimal constructor - most behavior handled by base class
}

void ASFConveyorAttachmentChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // #497 drift-proof no-op: the clone topology owns the transform. Vanilla parent propagation calls
    // this on every mChildren entry per frame and would reposition the distributor toward the parent
    // hit result — the reason the extend service historically re-applied positions every frame.
}

void ASFConveyorAttachmentChildHologram::CheckValidPlacement()
{
    // Check data structure for validation control
    bool bShouldSkip = ShouldSkipValidation();
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND CheckValidPlacement: %s - ShouldSkip=%d"), 
        *GetName(), bShouldSkip);
    
    if (bShouldSkip)
    {
        // The build gun derives a preview's red/cyan from its construct disqualifiers, NOT from
        // SetPlacementMaterialState (GetHologramMaterialState reflects disqualifiers). Mirror the
        // extend's authoritative state by carrying the same "unaffordable" disqualifier the parent
        // does; cleared when affordable so the preview returns to cyan.
        const EHologramMaterialState ChildState = USFExtendService::ResolveChildPreviewMaterialState(this);
        ResetConstructDisqualifiers();
        if (ChildState == EHologramMaterialState::HMS_ERROR)
        {
            AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
        }
        SetPlacementMaterialState(ChildState);
        return; // Skip own validation - state mirrors the extend parent
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

AActor* ASFConveyorAttachmentChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // Call parent to build the distributor
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
                    ExtendService->RegisterBuiltDistributor(HoloData->ExtendChainId, BuiltBuildable);
                    
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Distributor %s registered in Construct() for chain %d"),
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

void ASFConveyorAttachmentChildHologram::Destroyed()
{
    // Clean up data structure
    USFHologramDataRegistry::ClearData(this);
    
    Super::Destroyed();
}

bool ASFConveyorAttachmentChildHologram::ShouldSkipValidation() const
{
    // Check if we have data structure with validation disabled
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
    {
        return !Data->bNeedToCheckPlacement;
    }

    return false; // Default to validation if no data structure
}

void ASFConveyorAttachmentChildHologram::SetHologramNudgeLocation()
{
	// [#497] Vanilla's locked-parent placement path (UFGBuildGunStateBuild::TickState ->
	// AFGHologram::UpdateHologramPlacement (FGHologram.cpp:440) -> SetHologramNudgeLocation
	// (FGHologram.cpp:2120)) cascades through mChildren with a PLAIN SetActorLocation of
	// lock-location + nudge offset - bypassing the SetHologramLocationAndRotation no-op entirely.
	// Extend locks its parent, children never capture a lock location (ZeroVector), so the cascade
	// dragged every child to world origin every tick (caught by the #497 origin-trap stack dump).
	// Smart owns this child's transform; parent nudges are propagated by Smart's own
	// transform-change follow. No-op, mirroring the #418 drift contract.
}
