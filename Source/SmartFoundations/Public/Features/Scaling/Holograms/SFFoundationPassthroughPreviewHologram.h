// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFFoundationPassthroughPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Reuses selected native mesh poses and sockets without a native passthrough preview grid. */
UCLASS()
class SMARTFOUNDATIONS_API ASFFoundationPassthroughPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    void SyncGeometry(AFGHologram* Parent);
    bool IsGeometryReady() const { return bGeometryReady; }
    virtual int32 GetBaseCostMultiplier() const override;
    virtual void SetPlacementMaterialState(EHologramMaterialState MaterialState) override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID) override;
private:
    void ApplyGeometryMaterialState(EHologramMaterialState MaterialState);
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> GeometryPreviews;
    bool bDefaultMeshesHidden = false;
    bool bGeometryReady = false;
    int32 NativeCostMultiplier = 1;
};
