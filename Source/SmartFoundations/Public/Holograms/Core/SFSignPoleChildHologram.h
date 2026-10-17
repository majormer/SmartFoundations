// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGSignPoleHologram.h"
#include "SFSignPoleChildHologram.generated.h"

/** A native sign support whose selected state and transform follow its grid sign. */
UCLASS()
class SMARTFOUNDATIONS_API ASFSignPoleChildHologram : public AFGSignPoleHologram
{
	GENERATED_BODY()
public:
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
	virtual void SetHologramLocationAndRotation(const FHitResult& HitResult) override {}
	virtual void SetHologramNudgeLocation() override {}
	virtual bool DoMultiStepPlacement(bool bInputFromRelease) override { return true; }
	virtual void CheckValidPlacement() override;
	virtual void CheckClearance() override {}
	virtual void Destroyed() override;
};
