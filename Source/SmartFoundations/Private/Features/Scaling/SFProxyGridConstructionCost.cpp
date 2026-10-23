// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFProxyGridConstructionCost.h"
#include "Hologram/FGHologram.h"

// SP/MP DIVERGENCE MAP — selected-geometry proxy grid payment
//  1. Local authority constructs before paying; retain the accepted root quote.
//  2. Remote construction uses the existing staged authority cost before expansion.
namespace
{
const SFProxyGridConstructionCost::FScope* LocalConstruction = nullptr;
}

SFProxyGridConstructionCost::FScope::FScope(AFGHologram* InParent)
	: Parent(InParent), Previous(LocalConstruction)
{
	Cost = Parent->GetCost(true);
	LocalConstruction = this;
}

SFProxyGridConstructionCost::FScope::~FScope() { LocalConstruction = Previous; }

bool SFProxyGridConstructionCost::GetAcceptedCost(const AFGHologram* Parent, TArray<FItemAmount>& Cost)
{
	for (const FScope* Scope = LocalConstruction; Scope; Scope = Scope->Previous)
	{
		if (Scope->Parent == Parent) { Cost = Scope->Cost; return true; }
	}
	return false;
}
