// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "ItemAmount.h"
#include "SFSignPreviewHologram.generated.h"

class AFGSignPoleHologram;
class UStaticMeshComponent;

/** Sign grid preview: generic recipe meshes, optional support mesh, native commit. */
UCLASS()
class SMARTFOUNDATIONS_API ASFSignPreviewHologram : public ASFBuildableChildHologram
{
	GENERATED_BODY()
public:
	void SyncSupport(AFGSignPoleHologram* Template, bool bWanted,
		const FTransform& PolePose, double Height);
	virtual TArray<FItemAmount> GetCost(bool bIncludeChildren) const override;
	virtual void SetPlacementMaterialState(EHologramMaterialState MaterialState) override;
	virtual AActor* Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID) override;

private:
	void ApplySupportMaterialState(EHologramMaterialState MaterialState);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SupportPreview;
	UPROPERTY(Transient)
	TArray<FItemAmount> SupportCost;
	FTransform SupportPose;
	double SupportHeight = 0.0;
	bool bSupportWanted = false;
	bool bSupportValid = false;
	bool bSupportStateReady = false;
};
