#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FactoryGame/Public/Hologram/FGConveyorBeltHologram.h"
#include "FactoryGame/Public/Hologram/FGSplineHologram.h"
#include "SF_ATAnchor.generated.h"

/**
 * UObject anchor to force UHT to parse target classes for AccessTransformers
 * This guarantees UHT sees both classes and applies transformers
 */
UCLASS()
class SMARTFOUNDATIONS_API USF_ATAnchor : public UObject
{
    GENERATED_BODY()

public:
    // Force UHT to include these classes in reflection graph
    UPROPERTY() TSubclassOf<AFGConveyorBeltHologram> ForceRefBelt;
    UPROPERTY() TSubclassOf<AFGSplineHologram>       ForceRefSpline;
};
