#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGBuildableHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFSmartLogisticsChildHologram.generated.h"

// Minimal wrapper hologram for logistics children that checks data structure registry for validation control
UCLASS()
class SMARTFOUNDATIONS_API ASFSmartLogisticsChildHologram : public AFGBuildableHologram {
    GENERATED_BODY()
    
public:
    ASFSmartLogisticsChildHologram();
    
    // Override validation to check data structure flags
    virtual void CheckValidPlacement() override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
