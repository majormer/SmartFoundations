// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGJumpPadLauncherHologram.h"
#include "SFJumpPadConstructionHologram.generated.h"

/** Brief native commit cell retaining the selected launch angle and vanilla configuration. */
UCLASS()
class SMARTFOUNDATIONS_API ASFJumpPadConstructionHologram : public AFGJumpPadLauncherHologram
{
    GENERATED_BODY()
public:
    void SetChildBuildClass(UClass* BuildClass) { mBuildClass = BuildClass; }
    virtual void SetHologramLocationAndRotation(const FHitResult& Hit) override {}
    virtual void SetHologramNudgeLocation() override {}
    virtual void Destroyed() override;
};
