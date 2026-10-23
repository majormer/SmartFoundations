// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
class AFGHologram;

namespace SFProxyGridConstructionCost
{
	/** Retain the accepted grid price through local native materialization and payment. */
	class FScope
	{
	public:
		explicit FScope(AFGHologram* InParent);
		~FScope();
		FScope(const FScope&) = delete;
		FScope& operator=(const FScope&) = delete;
	private:
		friend bool GetAcceptedCost(const AFGHologram*, TArray<FItemAmount>&);
		const AFGHologram* Parent;
		TArray<FItemAmount> Cost;
		const FScope* Previous;
	};
	bool GetAcceptedCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost);
}
