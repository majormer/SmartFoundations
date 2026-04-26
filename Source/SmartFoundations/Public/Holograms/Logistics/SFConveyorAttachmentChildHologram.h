#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFConveyorAttachmentChildHologram.generated.h"

/**
 * Custom conveyor attachment (splitter/merger) hologram that can skip validation.
 * Used by EXTEND feature to spawn child splitters/mergers that don't fail
 * "Surface is too uneven" checks when floating in the air.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorAttachmentChildHologram : public AFGConveyorAttachmentHologram
{
    GENERATED_BODY()
    
public:
    ASFConveyorAttachmentChildHologram();
    
    // Override validation to check data structure flags
    virtual void CheckValidPlacement() override;
    
    // Override Construct to register built distributor for EXTEND wiring
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
