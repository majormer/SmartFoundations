// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFJumpPadPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Selected launch geometry without a full native jump-pad preview child. */
UCLASS()
class SMARTFOUNDATIONS_API ASFJumpPadPreviewHologram : public ASFBuildableChildHologram
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
