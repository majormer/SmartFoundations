#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGPipelineAttachmentHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFPipeAttachmentChildHologram.generated.h"

/**
 * Smart Pipe Attachment child hologram for cloning inline pipe attachments during Extend.
 * Covers valves (Build_Valve_C) and pumps (Build_PipelinePump_C*) — both are
 * AFGBuildablePipelinePump subclasses spawned via AFGPipelineAttachmentHologram.
 *
 * Issue #288: Extend now captures pipe attachments that sit between pipe segments
 * on a chain. The clone spawns a copy at the same relative transform via this
 * wrapper, and the spawn handler copies the source's mUserFlowLimit after
 * construction so valve/pump state is preserved.
 *
 * Pattern mirrors ASFPassthroughChildHologram:
 * 1. SpawnActor with bDeferConstruction
 * 2. SetBuildClass + SetRecipe
 * 3. FinishSpawning
 * 4. AddChild to parent
 * 5. DisableValidation + MarkAsChild via USFHologramDataService
 * 6. Disable collisions and tick post-spawn
 * 7. Post-Construct: Cast buildable to AFGBuildablePipelinePump, SetUserFlowLimit
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFPipeAttachmentChildHologram : public AFGPipelineAttachmentHologram
{
    GENERATED_BODY()

public:
    ASFPipeAttachmentChildHologram();

    /** Skip validation via data registry flag (same pattern as ASFPassthroughChildHologram). */
    virtual void CheckValidPlacement() override;

    /** Delegate to vanilla pipeline-attachment construction (handles FluidIntegrant registration). */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;

    /** Cleanup data registry entry on destruction. */
    virtual void Destroyed() override;

    /** Block parent from repositioning this child — clone topology dictates the transform. */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

    /** Public wrapper to set build class before FinishSpawning. */
    void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
};
