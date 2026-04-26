#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGFactoryHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFSmartFactoryChildHologram.generated.h"

// Minimal wrapper hologram for factory children that checks data structure registry for validation control
UCLASS()
class SMARTFOUNDATIONS_API ASFSmartFactoryChildHologram : public AFGFactoryHologram {
    GENERATED_BODY()
    
public:
    ASFSmartFactoryChildHologram();
    
    // Override validation to check data structure flags
    virtual void CheckValidPlacement() override;
    
    // Override ConfigureActor for recipe copying
    virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;
    
    // Cleanup data structure on destruction
    virtual void Destroyed() override;
    
protected:
    // Check if we should skip validation based on data structure
    bool ShouldSkipValidation() const;
};
