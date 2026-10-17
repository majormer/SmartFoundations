// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGBeamHologram.h"
#include "SFBeamChildHologram.generated.h"

/** Native beam materialization and length pricing with Smart-owned grid transforms. */
UCLASS()
class SMARTFOUNDATIONS_API ASFBeamChildHologram : public AFGBeamHologram
{
	GENERATED_BODY()
public:
	void SetChildBuildClass(UClass* BuildClass) { mBuildClass = BuildClass; }
	virtual void SetHologramLocationAndRotation(const FHitResult& HitResult) override {}
	virtual void SetHologramNudgeLocation() override {}
	virtual void CheckValidPlacement() override;
	virtual void CheckClearance() override {}
	virtual void Destroyed() override;
};
