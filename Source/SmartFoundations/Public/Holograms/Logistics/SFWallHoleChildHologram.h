#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGWallAttachmentHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFWallHoleChildHologram.generated.h"

/**
 * Smart Wall Hole child hologram for Extend / Scaled Extend cloning.
 *
 * Wall holes (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C, and any
 * "*WallHole_C" variants) are Blueprint-only AFGWallAttachmentHologram derivatives.
 * At runtime the built actor is a SnapOnly decorator with no factory/pipe connection
 * components — so cloning is pure transform + mesh + snap-point consumption.
 *
 * Pattern mirrors ASFPassthroughChildHologram / ASFConveyorAttachmentChildHologram:
 *   1. SpawnActor with bDeferConstruction
 *   2. SetBuildClass + SetRecipe (triggers mesh/visual creation)
 *   3. FinishSpawning
 *   4. AddChild to parent
 *   5. DisableValidation + MarkAsChild via USFHologramDataService
 *   6. Disable collisions and tick post-spawn
 *
 * CheckValidPlacement and SetHologramLocationAndRotation are both overridden to
 * bypass the vanilla wall-hit validation (we already know the exact world transform
 * from the clone topology; there's no live hit result to consult).
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFWallHoleChildHologram : public AFGWallAttachmentHologram
{
    GENERATED_BODY()

public:
    ASFWallHoleChildHologram();

    /** Skip wall-hit validation via data-structure check (matches SFPassthroughChildHologram). */
    virtual void CheckValidPlacement() override;

    /** Delegate to vanilla wall-attachment construction. */
    virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;

    /** Cleanup data structure on destruction. */
    virtual void Destroyed() override;

    /** Block parent from moving this child — our clone topology positions it explicitly. */
    virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

    /** Public wrapper to set build class before FinishSpawning. */
    void SetBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
};
