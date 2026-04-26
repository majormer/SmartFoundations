#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFPipelineJunctionChildHologram.generated.h"

/**
 * Custom pipeline junction hologram that can skip validation.
 * Used by EXTEND feature to spawn child junctions that don't fail
 * placement checks when floating in the air.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPipelineJunctionChildHologram : public AFGPipelineJunctionHologram
{
    GENERATED_BODY()
    
public:
    ASFPipelineJunctionChildHologram();
    
    // Override validation to check data structure flags
    virtual void CheckValidPlacement() override;
    
    // Override Construct to register built junction with SFExtendService
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
