// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFElevatorPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Mesh-only selected shaft and floor stops, with one ordinary native assembly at commit. */
UCLASS()
class SMARTFOUNDATIONS_API ASFElevatorPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    void SyncGeometry(AFGHologram* Parent);
    virtual int32 GetBaseCostMultiplier() const override;
    virtual TArray<FItemAmount> GetCost(bool bIncludeChildren) const override;
    virtual void SetPlacementMaterialState(EHologramMaterialState MaterialState) override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID) override;
private:
    void ApplyGeometryMaterialState(EHologramMaterialState MaterialState);
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> GeometryPreviews;
    UPROPERTY(Transient)
    TArray<FItemAmount> StopCost;
    bool bDefaultMeshesHidden = false;
    bool bGeometryReady = false;
    int32 NativeCostMultiplier = 1;
};
