// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "../Core/ASFLogisticsHologram.h"
#include "SFConveyorAttachmentHologram.generated.h"

/**
 * Smart Conveyor Attachment hologram (splitters, mergers).
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorAttachmentHologram : public ASFLogisticsHologram
{
    GENERATED_BODY()

public:
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
};
