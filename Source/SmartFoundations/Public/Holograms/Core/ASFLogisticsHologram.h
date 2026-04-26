#pragma once

#include "CoreMinimal.h"
#include "FGSplineHologram.h"
#include "ASFLogisticsHologram.generated.h"

/**
 * Base class for all Smart logistics holograms (conveyors, pipes).
 * Implements Auto-Connect functionality and connection management.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFLogisticsHologram : public AFGSplineHologram
{
    GENERATED_BODY()

public:
    ASFLogisticsHologram();

    // Logistics-specific functionality
    virtual void BeginPlay() override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;

protected:
    // Auto-Connect implementation
    virtual void SpawnLogisticsChildren();
    virtual void ConnectToNearbyBuildings();
    virtual void ApplyAutoRouting();
    
    // Common helper functions
    virtual void LogSmartActivity(const FString& Activity) const;
};
