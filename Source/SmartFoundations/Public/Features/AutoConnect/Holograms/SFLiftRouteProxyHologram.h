// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "SFLiftRouteProxyHologram.generated.h"

class FSFLiftRouteNativePreview;
class UFGFactoryConnectionComponent;
class UStaticMeshComponent;

/** Mesh-only route cell. Native validators are shared by the owning grid. */
UCLASS()
class SMARTFOUNDATIONS_API ASFLiftRouteProxyHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    bool Capture(AFGHologram* Native, TSharedPtr<FSFLiftRouteNativePreview> InNativePreview,
        UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To, bool bLift);
    virtual void CheckValidPlacement() override;
    virtual void SetPlacementMaterialState(EHologramMaterialState State) override;
    virtual int32 GetBaseCostMultiplier() const override { return NativeMultiplier; }
    virtual TArray<FItemAmount> GetCost(bool IncludeChildren) const override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID) override;
    virtual void Destroyed() override;
    UFGFactoryConnectionComponent* ResolveBuiltPort(FName Name) const;
    UFGFactoryConnectionComponent* GetRouteFrom() const { return RouteFrom; }
    UFGFactoryConnectionComponent* GetRouteTo() const { return RouteTo; }
    bool ValidateCapturedRoute();
    bool IsLift() const { return bIsLift; }
    const FTransform& GetTop() const { return Top; }
    const TArray<FSplinePointData>& GetRouteSpline() const { return Spline; }
private:
    void CopyMeshes(AFGHologram* Native);
    TSharedPtr<FSFLiftRouteNativePreview> NativePreview;
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> RouteFrom;
    UPROPERTY() TObjectPtr<UFGFactoryConnectionComponent> RouteTo;
    UPROPERTY() TArray<FSplinePointData> Spline;
    UPROPERTY() TArray<FItemAmount> NativeCost;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    TWeakObjectPtr<AFGBuildable> Built;
    UPROPERTY() FTransform RouteFromPose;
    UPROPERTY() FTransform RouteToPose;
    UPROPERTY() int32 FromDirection = -1;
    UPROPERTY() int32 ToDirection = -1;
    UPROPERTY() bool bHasRouteTo = false;
    FTransform Top;
    int32 NativeMultiplier = 1;
    bool bIsLift = false;
    bool bReady = false;
};
