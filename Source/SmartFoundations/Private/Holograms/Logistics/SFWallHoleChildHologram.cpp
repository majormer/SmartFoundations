#include "Holograms/Logistics/SFWallHoleChildHologram.h"
#include "SmartFoundations.h"
#include "FGConstructDisqualifier.h"
#include "Data/SFHologramDataRegistry.h"
#include "Data/SFHologramData.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"

ASFWallHoleChildHologram::ASFWallHoleChildHologram()
{
    // Minimal constructor — tick and collision disabled post-spawn by the spawner,
    // not here. Matches ASFPassthroughChildHologram / ASFConveyorAttachmentChildHologram.
}

void ASFWallHoleChildHologram::CheckValidPlacement()
{
    // Same pattern as ASFPassthroughChildHologram: honor the data-structure flag when
    // present, otherwise always force valid state. The clone topology guarantees a
    // good transform; the vanilla wall-hit validation has no hit result to check.
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

AActor* ASFWallHoleChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🧱 WALL HOLE CHILD: Construct() called for %s"), *GetName());

    // Delegate to vanilla wall-attachment construction — spawns the AFGBuildable, triggers
    // snap-point consumption on the underlying wall, registers with the buildable subsystem.
    AActor* BuiltActor = Super::Construct(out_children, constructionID);

    if (BuiltActor)
    {
        // Register in JsonBuiltActors so the wiring manifest (and future sibling-aware
        // features) can resolve this clone by its HologramId.
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

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🧱 WALL HOLE CHILD: Successfully built %s -> %s (CloneId=%s)"),
            *GetName(), *BuiltActor->GetName(),
            HoloData ? *HoloData->JsonCloneId : TEXT("none"));
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🧱 WALL HOLE CHILD: Construct returned nullptr for %s"), *GetName());
    }

    return BuiltActor;
}

void ASFWallHoleChildHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}

void ASFWallHoleChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // Block parent from moving this child — the clone topology positions it explicitly.
    // Same pattern as ASFPassthroughChildHologram / ASFConveyorBeltHologram for Extend.
}
