#pragma once

#include "CoreMinimal.h"
#include "SFBuildableHologram.h"
#include "FGFoundationHologram.h"
#include "SFFoundationHologram.generated.h"

/**
 * Base class for all Smart foundation holograms.
 * Handles foundation-specific validation and grid snapping.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFFoundationHologram : public AFGFoundationHologram
{
    GENERATED_BODY()

public:
    ASFFoundationHologram();

    // Foundation-specific functionality
    virtual void BeginPlay() override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;

protected:
    // Foundation validation and snapping
    virtual void ValidateFoundationPlacement();
    virtual void ApplyFoundationSnapping();
    
    // Common helper functions
    virtual void LogSmartActivity(const FString& Activity) const;
};
