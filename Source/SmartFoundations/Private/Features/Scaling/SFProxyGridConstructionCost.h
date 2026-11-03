// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
class AFGHologram;

namespace SFProxyGridConstructionCost
{
	/** Keep an authority-validated quote after its staged plan is consumed by Construct. */
	class FRequestScope
	{
	public:
		FRequestScope();
		~FRequestScope();
		FRequestScope(const FRequestScope&) = delete;
		FRequestScope& operator=(const FRequestScope&) = delete;
	private:
		friend void RetainForRequest(AFGHologram*, const TArray<FItemAmount>&);
		friend bool GetAcceptedCost(const AFGHologram*, TArray<FItemAmount>&);
		TMap<TWeakObjectPtr<AFGHologram>, TArray<FItemAmount>> Accepted;
		FRequestScope* Previous;
	};
	void RetainForRequest(AFGHologram* Parent, const TArray<FItemAmount>& Cost);

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
