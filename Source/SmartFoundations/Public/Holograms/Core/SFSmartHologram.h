#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGHologram.h"
#include "SFSmartHologram.generated.h"

/**
 * Base class for all Smart holograms.
 * Provides common Smart functionality and infrastructure.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFSmartHologram : public AFGHologram
{
    GENERATED_BODY()

public:
    ASFSmartHologram();

    // Common Smart functionality
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // Parent-child relationship management
    void AddChild(AFGHologram* InChild, FName HologramName);
    
    // Smart child management
    virtual void ReplaceChildWithSmartHologram(AFGHologram* OriginalChild, AFGHologram* SmartChild);

protected:
    // Smart metadata and tracking
    UPROPERTY(BlueprintReadOnly, Category = "Smart")
    int32 PlacementGroupIndex = -1;
    
    UPROPERTY(BlueprintReadOnly, Category = "Smart")
    int32 PlacementChildIndex = -1;

    // Common helper functions
    virtual void LogSmartActivity(const FString& Activity);
    virtual void SetSmartMetadata(int32 GroupIndex, int32 ChildIndex);
};
