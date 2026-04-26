#include "Holograms/Power/SFPowerPoleChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"

ASFPowerPoleChildHologram::ASFPowerPoleChildHologram()
{
}

void ASFPowerPoleChildHologram::CheckValidPlacement()
{
    bool bShouldSkip = ShouldSkipValidation();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ EXTEND PowerPole CheckValidPlacement: %s - ShouldSkip=%d"), 
        *GetName(), bShouldSkip);
    
    if (bShouldSkip)
    {
        SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
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
                    
                    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND: Power pole %s registered in Construct() with JsonCloneId=%s"),
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
