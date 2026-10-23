// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFConveyorLiftPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Mesh-only lift grid cell; native selected state and bindings are validated at commit. */
UCLASS()
class SMARTFOUNDATIONS_API ASFConveyorLiftPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    const FTransform& GetRequestedPose() const { return RequestedPose; }
    void RememberRequestedPose();
    void ApplyResolvedPose(const FTransform& Pose);
    void SyncGeometry(AFGHologram* Parent);
    virtual int32 GetBaseCostMultiplier() const override;
    virtual void CheckValidPlacement() override;
    virtual void SetPlacementMaterialState(EHologramMaterialState State) override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID Id) override;
    virtual void Destroyed() override;
private:
    FTransform RequestedPose;
    FTransform LastResolvedPose;
    bool bHasResolvedPose = false;
    bool bGeometryReady = false;
    int32 NativeCostMultiplier = 1;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> GeometryPreviews;
};
