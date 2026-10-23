// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class UFGBuildGunStateBuild;
class UFGRecipe;

/** Per-placement choice; native pose and disabled companion state carry construction across the wire. */
namespace SFSignSupportMode
{
	enum class EMode : uint8 { Auto, None };
	bool IsAvailable(const AFGHologram* Hologram);
	EMode Get(const AFGHologram* Hologram);
	FText GetLabel(const AFGHologram* Hologram);
	void Toggle(AFGHologram* Hologram);
	void BeforeNativePlacement(AFGHologram* Hologram);
	void AfterNativePlacement(AFGHologram* Hologram);
	/** Align the root before capturing child transforms, including locked grids and the last child removal. */
	void BeforeGridUpdate(AFGHologram* Hologram);
	void BeginConstruction(UFGBuildGunStateBuild* BuildState);
	void OnRecipeBuilt(UFGBuildGunStateBuild* BuildState, TSubclassOf<UFGRecipe> Recipe, int32 Count);
}
