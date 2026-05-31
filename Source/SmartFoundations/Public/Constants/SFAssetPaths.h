// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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

	/**
	 * Building recipe classes indexed by tier (Mk.N at index N-1). Used by Smart Upgrade to
	 * compute net upgrade cost (target recipe ingredients minus source recipe refund). These
	 * arrays were duplicated verbatim in the upgrade panel and the upgrade execution service;
	 * declare them once here. Belt and Pipe recipes are resolved via the subsystem's tier
	 * getters and so are not listed here.
	 */
	namespace UpgradeRecipes
	{
		inline constexpr const TCHAR* ConveyorLift[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk2.Recipe_ConveyorLiftMk2_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk3.Recipe_ConveyorLiftMk3_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk5.Recipe_ConveyorLiftMk5_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk6.Recipe_ConveyorLiftMk6_C"),
		};

		inline constexpr const TCHAR* PowerPole[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk2.Recipe_PowerPoleMk2_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk3.Recipe_PowerPoleMk3_C"),
		};

		inline constexpr const TCHAR* WallOutletSingle[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWall.Recipe_PowerPoleWall_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk2.Recipe_PowerPoleWallMk2_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk3.Recipe_PowerPoleWallMk3_C"),
		};

		inline constexpr const TCHAR* WallOutletDouble[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDouble.Recipe_PowerPoleWallDouble_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk2.Recipe_PowerPoleWallDoubleMk2_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk3.Recipe_PowerPoleWallDoubleMk3_C"),
		};
	}
}
