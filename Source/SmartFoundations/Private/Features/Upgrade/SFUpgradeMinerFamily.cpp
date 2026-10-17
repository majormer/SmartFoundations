// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Upgrade/SFUpgradeMinerFamily.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Constants/SFAssetPaths.h"

bool SFUpgradeMinerFamily::IsMinerClass(const UClass* Class)
{
	if (!Class || !Class->IsChildOf(AFGBuildableResourceExtractor::StaticClass())) return false;
	const FName Name = Class->GetFName();
	return Name == TEXT("Build_MinerMk1_C") || Name == TEXT("Build_MinerMk2_C") || Name == TEXT("Build_MinerMk3_C");
}

TSubclassOf<UFGRecipe> SFUpgradeMinerFamily::GetRecipe(int32 Tier)
{
	return Tier >= 1 && Tier <= UE_ARRAY_COUNT(SFAssetPaths::UpgradeRecipes::Miner)
		? LoadClass<UFGRecipe>(nullptr, SFAssetPaths::UpgradeRecipes::Miner[Tier - 1]) : nullptr;
}

TSubclassOf<AFGBuildable> SFUpgradeMinerFamily::GetBuildableClass(int32 Tier)
{
	static const TCHAR* Paths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/MinerMK1/Build_MinerMk1.Build_MinerMk1_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/MinerMk2/Build_MinerMk2.Build_MinerMk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/MinerMk3/Build_MinerMk3.Build_MinerMk3_C")
	};
	return Tier >= 1 && Tier <= UE_ARRAY_COUNT(Paths) ? LoadClass<AFGBuildable>(nullptr, Paths[Tier - 1]) : nullptr;
}

int32 SFUpgradeMinerFamily::GetHighestUnlockedTier(UWorld* World)
{
	AFGRecipeManager* Manager = World ? AFGRecipeManager::Get(World) : nullptr;
	if (!Manager) return 0;
	for (int32 Tier = 3; Tier >= 1; --Tier)
	{
		const TSubclassOf<UFGRecipe> Recipe = GetRecipe(Tier);
		if (Recipe && Manager->IsRecipeAvailable(Recipe)) return Tier;
	}
	return 0;
}
