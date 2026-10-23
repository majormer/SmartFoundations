// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Logistics/SFSelectedPoleState.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SFHypertubeSupportPreviewHologram.generated.h"

class AFGPoleHologram;
class UStaticMeshComponent;

/** Mesh-only selected pole preview for conveyor/pipeline/hypertube floor supports.
 * The legacy class name is retained; height and applicable tilt belong to the native root. */
UCLASS()
class SMARTFOUNDATIONS_API ASFHypertubeSupportPreviewHologram : public ASFBuildableChildHologram
{
	GENERATED_BODY()
public:
	void SyncGeometry(AFGPoleHologram* Parent);
	bool InitializeRecordedState(const FSFSelectedPoleState& State);
	virtual void CheckValidPlacement() override;
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
	UPROPERTY(Transient)
	FSFSelectedPoleState RecordedState;
};
