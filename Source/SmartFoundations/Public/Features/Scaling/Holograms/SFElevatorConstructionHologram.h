// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGElevatorHologram.h"
#include "SFElevatorConstructionHologram.generated.h"

/** Brief native assembly cell; the vanilla elevator configures its own floor stops. */
UCLASS()
class SMARTFOUNDATIONS_API ASFElevatorConstructionHologram : public AFGElevatorHologram
{
    GENERATED_BODY()
public:
    void SetChildBuildClass(UClass* BuildClass) { mBuildClass = BuildClass; }
    virtual void SetHologramLocationAndRotation(const FHitResult& Hit) override {}
    virtual void SetHologramNudgeLocation() override {}
    virtual void Destroyed() override;
};
