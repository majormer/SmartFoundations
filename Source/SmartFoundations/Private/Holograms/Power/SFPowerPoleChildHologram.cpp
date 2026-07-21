// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Power/SFPowerPoleChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"

ASFPowerPoleChildHologram::ASFPowerPoleChildHologram()
{
}

void ASFPowerPoleChildHologram::CheckValidPlacement()
{
    bool bShouldSkip = ShouldSkipValidation();
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ EXTEND PowerPole CheckValidPlacement: %s - ShouldSkip=%d"), 
        *GetName(), bShouldSkip);
    
    if (bShouldSkip)
    {
        // Build gun paints previews from construct disqualifiers; carry the parent's when unaffordable.
        const EHologramMaterialState ChildState = USFExtendService::ResolveChildPreviewMaterialState(this);
        ResetConstructDisqualifiers();
        if (ChildState == EHologramMaterialState::HMS_ERROR)
        {
            AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
        }
        SetPlacementMaterialState(ChildState);
        return;
    }
    
    Super::CheckValidPlacement();
}

AActor* ASFPowerPoleChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    
    if (BuiltActor)
    {
        // Register with ExtendService for JSON-based post-build wiring
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        if (HoloData && !HoloData->JsonCloneId.IsEmpty())
        {
            USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
            if (Subsystem)
            {
                USFExtendService* ExtendService = Subsystem->GetExtendService();
                if (ExtendService)
                {
                    ExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                    
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Log, TEXT("⚡ EXTEND: Power pole %s registered in Construct() with JsonCloneId=%s"),
                        *BuiltActor->GetName(), *HoloData->JsonCloneId);
                }
            }
        }
    }
    
    return BuiltActor;
}

void ASFPowerPoleChildHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}

bool ASFPowerPoleChildHologram::ShouldSkipValidation() const
{
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
    {
        return !Data->bNeedToCheckPlacement;
    }
    return false;
}

void ASFPowerPoleChildHologram::SetHologramNudgeLocation()
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

void ASFPowerPoleChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// [#497] Drift-proof no-op: Smart owns this child's transform. Every sibling child class blocks
	// vanilla parent propagation this way; the pole child was the one that never got the override.
}
