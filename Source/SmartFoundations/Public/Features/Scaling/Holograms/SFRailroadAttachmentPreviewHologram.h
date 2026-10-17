// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFRailroadAttachmentPreviewHologram.generated.h"

class UStaticMeshComponent;

/** Selected native attachment meshes, including independent elevator stops.
 * The retained class identity also serves rail signals/buffers; each native family resolves its own binding.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFRailroadAttachmentPreviewHologram : public ASFBuildableChildHologram
{
    GENERATED_BODY()
public:
    const FTransform& GetRequestedPose() const { return RequestedPose; }
    void RememberRequestedPose();
    void ApplyResolvedPose(const FTransform& Pose);
    void SyncGeometry(AFGHologram* Parent);
    virtual void CheckValidPlacement() override;
    virtual void SetPlacementMaterialState(EHologramMaterialState State) override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID Id) override;
    virtual void Destroyed() override;
private:
    FTransform RequestedPose;
    FTransform LastResolvedPose;
    bool bHasResolvedPose = false;
    bool bGeometryReady = false;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> GeometryPreviews;
};
