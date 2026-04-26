#pragma once

#include "CoreMinimal.h"
#include "SFSmartHologram.h"
#include "FGBuildableHologram.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "SFBuildableHologram.generated.h"

/**
 * Base class for all Smart buildable holograms.
 * Handles building registration and metadata tracking.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFBuildableHologram : public AFGBuildableHologram
{
    GENERATED_BODY()

public:
    ASFBuildableHologram();

    // Buildable-specific functionality
    virtual void BeginPlay() override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;

    // Smart feature support for adapters
    virtual FBoxSphereBounds GetSmartBuildingBounds() const;
    virtual bool SupportsSmartFeature(ESFFeature Feature) const;
    virtual void ApplySmartTransformOffset(const FVector& Offset);
    
    // Metadata access for adapters
    int32 GetPlacementGroupIndex() const { return PlacementGroupIndex; }
    int32 GetPlacementChildIndex() const { return PlacementChildIndex; }

protected:
    // Smart metadata and tracking
    UPROPERTY(BlueprintReadOnly, Category = "Smart")
    int32 PlacementGroupIndex = -1;
    
    UPROPERTY(BlueprintReadOnly, Category = "Smart")
    int32 PlacementChildIndex = -1;

    // Building registration system
    virtual void RegisterConstructedBuilding(AActor* Building) const;
    virtual void SetBuildingMetadata(AActor* Building) const;
    
    // Common helper functions
    virtual void LogSmartActivity(const FString& Activity) const;
    virtual void SetSmartMetadata(int32 GroupIndex, int32 ChildIndex);
};
