#pragma once

#include "CoreMinimal.h"
#include "SFBuildableHologram.h"
#include "FGFactoryHologram.h"
#include "SFFactoryHologram.generated.h"

/**
 * Base class for all Smart factory holograms.
 * Implements recipe copying functionality for production buildings.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFFactoryHologram : public AFGFactoryHologram
{
    GENERATED_BODY()

public:
    ASFFactoryHologram();

    // Factory-specific functionality
    virtual void BeginPlay() override;
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
    virtual void ConfigureActor(AFGBuildable* InBuildable) const override;
    
    // CRITICAL: Override to block repositioning when EXTEND/scaling is active
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
    
    // Override CheckValidPlacement to skip clearance checks during EXTEND mode.
    // Without this, vanilla's clearance detection adds encroachment disqualifiers
    // that prevent building, especially with rotation.
    virtual void CheckValidPlacement() override;
    
    /**
     * Initialize this hologram with the build class from another hologram.
     * Must be called BEFORE BeginPlay (e.g., after SpawnActorDeferred, before FinishSpawning).
     * @param SourceHologram The hologram to copy the build class from
     */
    void InitializeFromHologram(AFGHologram* SourceHologram);

protected:
    // Recipe copying implementation
    virtual void ApplyStoredRecipe(AActor* Building) const;
    virtual bool IsProductionBuilding(AActor* Building) const;
    
    // Common helper functions (inherited from buildable holograms)
    virtual void LogSmartActivity(const FString& Activity) const;
    virtual void SetSmartMetadata(int32 GroupIndex, int32 ChildIndex);
};
