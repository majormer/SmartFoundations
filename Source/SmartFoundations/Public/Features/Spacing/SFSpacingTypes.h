// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Spacing Feature Types

#pragma once

#include "CoreMinimal.h"
#include "SFSpacingTypes.generated.h"

/**
 * Spacing mode enumeration
 * Controls which axes have automatic gap spacing applied
 */
UENUM(BlueprintType)
enum class ESFSpacingMode : uint8
{
	None UMETA(DisplayName = "No Spacing"),
	X UMETA(DisplayName = "X-Axis Spacing"),
	XY UMETA(DisplayName = "X+Y Spacing"),
	XYZ UMETA(DisplayName = "X+Y+Z Spacing")
};
