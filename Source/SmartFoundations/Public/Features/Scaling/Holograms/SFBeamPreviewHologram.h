// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFBeamPreviewHologram.generated.h"

class AFGBeamHologram;
class UStaticMeshComponent;

/** Mesh-only beam cell; native root owns length selection and native commit owns configuration. */
UCLASS()
class SMARTFOUNDATIONS_API ASFBeamPreviewHologram : public ASFBuildableChildHologram
{
	GENERATED_BODY()
public:
	void SyncGeometry(AFGBeamHologram* Parent);
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
