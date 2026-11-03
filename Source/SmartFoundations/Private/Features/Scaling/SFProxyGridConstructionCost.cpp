// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFProxyGridConstructionCost.h"
#include "Hologram/FGHologram.h"

// SP/MP DIVERGENCE MAP — selected-geometry proxy grid payment
//  1. Local authority constructs before paying; retain the accepted root quote.
//  2. Remote construction retains the validated staged quote through native payment,
//     including post-construct wires whose plan was consumed during expansion.
namespace
{
const SFProxyGridConstructionCost::FScope* LocalConstruction = nullptr;
SFProxyGridConstructionCost::FRequestScope* CurrentRequest = nullptr;
}

SFProxyGridConstructionCost::FRequestScope::FRequestScope() : Previous(CurrentRequest)
{
	CurrentRequest = this;
}

SFProxyGridConstructionCost::FRequestScope::~FRequestScope() { CurrentRequest = Previous; }

void SFProxyGridConstructionCost::RetainForRequest(AFGHologram* Parent, const TArray<FItemAmount>& Cost)
{
	if (CurrentRequest && IsValid(Parent) && Parent->HasAuthority() && !Parent->GetParentHologram())
		CurrentRequest->Accepted.Add(Parent, Cost);
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
	if (CurrentRequest)
		if (const auto* Accepted = CurrentRequest->Accepted.Find(const_cast<AFGHologram*>(Parent)))
		{
			Cost = *Accepted;
			return true;
		}
	return false;
}
