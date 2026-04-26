#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGPowerPoleHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFPowerPoleChildHologram.generated.h"

/**
 * Custom power pole hologram for EXTEND feature (Issue #229).
 * Spawned as a child of the factory hologram to clone power infrastructure.
 * Skips validation, registers built pole for post-build wiring.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPowerPoleChildHologram : public AFGPowerPoleHologram
{
    GENERATED_BODY()
    
public:
    ASFPowerPoleChildHologram();
    
    // Override validation to skip when flagged as extend child
    virtual void CheckValidPlacement() override;
    
    // Override Construct to register built pole for post-build wiring
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
