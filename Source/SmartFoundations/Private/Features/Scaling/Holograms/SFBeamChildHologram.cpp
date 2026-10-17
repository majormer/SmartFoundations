// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Holograms/SFBeamChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"

void ASFBeamChildHologram::CheckValidPlacement()
{
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFBeamChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}
