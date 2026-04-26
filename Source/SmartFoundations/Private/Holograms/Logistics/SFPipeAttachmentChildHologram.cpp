#include "Holograms/Logistics/SFPipeAttachmentChildHologram.h"
#include "SmartFoundations.h"
#include "FGConstructDisqualifier.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"

ASFPipeAttachmentChildHologram::ASFPipeAttachmentChildHologram()
{
    // Minimal constructor. Tick and collision are disabled post-spawn by the caller,
    // matching the ASFPassthroughChildHologram / ASFWallHoleChildHologram pattern.
}

void ASFPipeAttachmentChildHologram::CheckValidPlacement()
{
    // Clone topology dictates the transform — the vanilla wall/floor snap checks have
    // nothing to consult. Honour the registry flag (shared pattern across SF child
    // holograms) and otherwise force HMS_OK.
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
    {
        if (!Data->bNeedToCheckPlacement)
        {
            SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
            return;
        }
    }

    ResetConstructDisqualifiers();
    SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

AActor* ASFPipeAttachmentChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE ATTACHMENT CHILD: Construct() called for %s"), *GetName());

    // Vanilla AFGPipelineAttachmentHologram::Construct spawns the AFGBuildable, hooks
    // the FluidIntegrant into the pipe subsystem, and registers with
    // AFGBuildableSubsystem. We just delegate.
    AActor* BuiltActor = Super::Construct(out_children, constructionID);

    if (BuiltActor)
    {
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        
        // Register the clone so downstream wiring can resolve by HologramId.
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
        
        // Issue #288: Copy the source valve/pump's user-configured flow limit onto
        // the clone. Must happen after Super::Construct so the AFGBuildable exists
        // and its fluid box is initialized. SetUserFlowLimit internally calls
        // UpdateFlowLimitOnFluidBox so the limit takes effect immediately.
        if (HoloData && HoloData->bIsPipeAttachmentClone)
        {
            if (AFGBuildablePipelinePump* Pump = Cast<AFGBuildablePipelinePump>(BuiltActor))
            {
                Pump->SetUserFlowLimit(HoloData->PipeAttachmentUserFlowLimit);
                UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE ATTACHMENT CHILD: Applied UserFlowLimit=%.3f to cloned pump/valve %s"),
                    HoloData->PipeAttachmentUserFlowLimit, *BuiltActor->GetName());
            }
            else
            {
                UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE ATTACHMENT CHILD: Built actor %s is not an AFGBuildablePipelinePump — cannot apply UserFlowLimit"),
                    *BuiltActor->GetName());
            }
        }

        UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE ATTACHMENT CHILD: Successfully built %s -> %s (CloneId=%s)"),
            *GetName(), *BuiltActor->GetName(),
            HoloData ? *HoloData->JsonCloneId : TEXT("none"));
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE ATTACHMENT CHILD: Construct returned nullptr for %s"), *GetName());
    }

    return BuiltActor;
}

void ASFPipeAttachmentChildHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}

void ASFPipeAttachmentChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // No-op — the clone topology owns the transform.
}
