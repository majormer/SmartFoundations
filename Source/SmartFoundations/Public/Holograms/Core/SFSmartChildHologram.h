#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGFoundationHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFSmartChildHologram.generated.h"

// Minimal wrapper hologram that checks data structure registry for validation control
UCLASS()
class SMARTFOUNDATIONS_API ASFSmartChildHologram : public AFGFoundationHologram {
    GENERATED_BODY()
    
public:
    ASFSmartChildHologram();
    
    // Override validation to check data structure flags
    virtual void CheckValidPlacement() override;
    
    // Override construction to apply stored recipes
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
