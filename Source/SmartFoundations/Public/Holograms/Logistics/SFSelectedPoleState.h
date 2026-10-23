// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "SFSelectedPoleState.generated.h"

class AFGBuildable;
class AFGHologram;
class UStaticMeshComponent;

/** Native selected variation and tilt, independent of an owning factory hologram. */
USTRUCT()
struct FSFSelectedPoleState
{
	GENERATED_BODY()
	UPROPERTY() bool bCaptured = false;
	UPROPERTY() float Height = 0;
	UPROPERTY() int32 Variation = INDEX_NONE;
	UPROPERTY() float VerticalAngle = 0;
};

namespace SFSelectedPoleState
{
	bool IsSelectedPole(AFGHologram* Hologram);
	bool GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes);
	void SyncNative(AFGHologram* Parent, AFGHologram* Child);
	bool Capture(AFGBuildable* Buildable, FSFSelectedPoleState& OutState);
	bool Apply(AFGHologram* Hologram, const FSFSelectedPoleState& State);
}
