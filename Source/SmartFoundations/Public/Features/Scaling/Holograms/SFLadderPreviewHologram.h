// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFLadderPreviewHologram.generated.h"

class AFGLadderHologram;
class UInstancedStaticMeshComponent;

/** One instanced mesh per grid cell; the native root owns height selection. */
UCLASS()
class SMARTFOUNDATIONS_API ASFLadderPreviewHologram : public ASFBuildableChildHologram
{
	GENERATED_BODY()
public:
	void SyncGeometry(AFGLadderHologram* Parent);
	virtual int32 GetBaseCostMultiplier() const override;
	virtual void SetPlacementMaterialState(EHologramMaterialState MaterialState) override;
	virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID) override;

private:
	void ApplyGeometryMaterialState(EHologramMaterialState MaterialState);
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> GeometryPreview;
	TArray<FTransform> InstancePoses;
	bool bGeometryReady = false;
	int32 NativeCostMultiplier = 1;
};
