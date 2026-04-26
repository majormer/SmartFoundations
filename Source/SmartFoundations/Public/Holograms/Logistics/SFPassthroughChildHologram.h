#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGPassthroughHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFPassthroughChildHologram.generated.h"

/**
 * Smart Passthrough child hologram for scaled floor hole placement.
 * Spawned as children of the parent passthrough hologram during grid scaling.
 * 
 * Follows the same spawn pattern as ASFConveyorAttachmentChildHologram (Extend):
 * 1. SpawnActor with bDeferConstruction
 * 2. SetBuildClass + SetRecipe (triggers mesh/visual creation)
 * 3. FinishSpawning
 * 4. AddChild to parent
 * 5. DisableValidation + MarkAsChild via USFHologramDataService
 * 6. Disable collisions and tick post-spawn
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPassthroughChildHologram : public AFGPassthroughHologram
{
    GENERATED_BODY()

public:
    ASFPassthroughChildHologram();

    /** Skip validation via data structure check (same as ConveyorAttachment pattern) */
    virtual void CheckValidPlacement() override;

    /** Delegate to vanilla passthrough construction */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;

    /** Cleanup data structure on destruction */
    virtual void Destroyed() override;

    /** Block parent from moving this child (we set position manually) */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

    /** Public wrapper to set build class before FinishSpawning */
    void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }

    /** Set foundation thickness and rebuild mesh so child matches the parent's height.
     *  Caller reads parent thickness via UE reflection (protected access workaround). */
    void SetSnappedThickness(float InThickness);
};
