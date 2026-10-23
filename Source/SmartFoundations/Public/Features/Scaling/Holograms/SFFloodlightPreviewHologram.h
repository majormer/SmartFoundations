// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFFloodlightPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Selected fixture geometry without a full native floodlight preview child. */
UCLASS()
class SMARTFOUNDATIONS_API ASFFloodlightPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    void SyncGeometry(AFGHologram* Parent);
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
