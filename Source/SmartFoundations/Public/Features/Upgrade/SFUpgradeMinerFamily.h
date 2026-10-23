// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

class AFGBuildable;
class UFGRecipe;

/** Native Miner tiers only; other resource extractors have distinct construction contracts. */
namespace SFUpgradeMinerFamily
{
	SMARTFOUNDATIONS_API bool IsMinerClass(const UClass* Class);
	SMARTFOUNDATIONS_API TSubclassOf<UFGRecipe> GetRecipe(int32 Tier);
	SMARTFOUNDATIONS_API TSubclassOf<AFGBuildable> GetBuildableClass(int32 Tier);
	SMARTFOUNDATIONS_API int32 GetHighestUnlockedTier(UWorld* World);
}
