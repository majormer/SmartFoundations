// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFRouteSupportPreviewHologram.generated.h"

/** Fixed route support proxy; reconstruct the recorded pose through its native family. */
UCLASS()
class SMARTFOUNDATIONS_API ASFRouteSupportPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID) override;
};
