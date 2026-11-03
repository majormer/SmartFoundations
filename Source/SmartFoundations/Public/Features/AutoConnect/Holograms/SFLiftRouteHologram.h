// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "SFLiftRouteHologram.generated.h"

class UFGFactoryConnectionComponent;

/** Native lift used by a distributor-to-building route. No nearest-port repair. */
UCLASS()
class SMARTFOUNDATIONS_API ASFLiftRouteHologram : public ASFConveyorLiftHologram
{
    GENERATED_BODY()
public:
    void SetAnchor(UFGFactoryConnectionComponent* Connection);
    void SetDirectPeer(UFGFactoryConnectionComponent* Connection);
    bool TrySetDirectPeer(UFGFactoryConnectionComponent* Connection);
    void SetRouteGeometry(const FTransform& Pose, const FTransform& Top);
    UFGFactoryConnectionComponent* GetMouth(int32 Index) const;
    UFGFactoryConnectionComponent* ResolveAnchor() const;
    static UFGFactoryConnectionComponent* ResolveEndpoint(UFGFactoryConnectionComponent* Source);
    static UFGFactoryConnectionComponent* ResolveEndpointForConstruction(UFGFactoryConnectionComponent* Source, const FNetConstructionID& Id);
    class AFGBuildableConveyorLift* GetBuiltLift() const { return BuiltLift.Get(); }
    class AFGBuildableConveyorLift* GetBuiltLiftForConstruction(const FNetConstructionID& Id) const;
    bool IsNativePlacementValid();
    // The owner supplies placement input; native CheckValidPlacement still judges geometry.
    virtual void UpdateHologramPlacement(const FHitResult&) override {}
    virtual void SetHologramLocationAndRotation(const FHitResult&) override {}
    virtual void SetHologramNudgeLocation() override {}
    virtual void PostHologramPlacement(const FHitResult&, bool) override {}
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID) override;
protected:
    virtual void CheckValidPlacement() override;
    virtual void ConfigureComponents(AFGBuildable* Buildable) const override;
private:
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> Anchor;
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> DirectPeer;
    mutable TWeakObjectPtr<class AFGBuildableConveyorLift> BuiltLift;
    int8 BuiltPlayerId = -1;
    uint16 BuiltServerId = 0;
    uint16 BuiltClientId = 0;
};

