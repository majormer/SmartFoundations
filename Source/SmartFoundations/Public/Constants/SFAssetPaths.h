// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Centralized FactoryGame asset/class paths used by Smart!.
 *
 * Several features hard-code the same `/Game/FactoryGame/...` object paths (the power-line
 * build class, pipeline builds, building recipes, etc.). Duplicating those string literals
 * means a single game content move turns into a multi-file hunt. Declare each shared path
 * once here and reference the constant instead.
 */
namespace SFAssetPaths
{
	/** Vanilla power line buildable class (`AFGBuildableWire`). Used for power auto-connect,
	 *  Extend power wiring, and the power-line preview. */
	inline constexpr const TCHAR* PowerLineBuildClass =
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C");
}
