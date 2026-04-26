#pragma once

#include "CoreMinimal.h"
#include "../Core/ASFLogisticsHologram.h"
#include "SFConveyorAttachmentHologram.generated.h"

/**
 * Smart Conveyor Attachment hologram (splitters, mergers).
 * Includes belt preview costs in the vanilla cost aggregation UI.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorAttachmentHologram : public ASFLogisticsHologram
{
    GENERATED_BODY()

public:
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
	
	/**
	 * Override GetCost to include belt preview costs in the vanilla UI and affordability checks
	 * Vanilla ValidatePlacementAndCost() will automatically check this and add disqualifiers
	 */
	virtual TArray<FItemAmount> GetCost(bool includeChildren) const override;
};
