// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGPassthroughPipeBaseHologram.h"
#include "SFHypertubePassthroughConstructionHologram.generated.h"

/** Brief native commit cell retaining the hypertube-specific ConfigureActor contract. */
UCLASS()
class SMARTFOUNDATIONS_API ASFHypertubePassthroughConstructionHologram : public AFGPassthroughPipeHyperHologram
{
    GENERATED_BODY()
public:
    void SetCellBuildClass(UClass* Class) { mBuildClass = Class; }
    void SetSelectedThickness(float Thickness);
    virtual void SetHologramLocationAndRotation(const FHitResult& Hit) override;
    virtual void SetHologramNudgeLocation() override;
    virtual void CheckValidPlacement() override;
};
