// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "SFLiftRouteBeltHologram.generated.h"
/** The flat belt uses the exact route mouths before native chain registration. */
UCLASS()
class SMARTFOUNDATIONS_API ASFLiftRouteBeltHologram : public ASFConveyorBeltHologram
{
    GENERATED_BODY()
public:
    void SetRouteEndpoints(UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To);
    bool IsNativePlacementValid();
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID) override;
protected:
    virtual void ConfigureComponents(AFGBuildable* Buildable) const override;
private:
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> RouteFrom;
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> RouteTo;
};
