// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Holograms/Core/SFSignPoleChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"

void ASFSignPoleChildHologram::CheckValidPlacement()
{
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFSignPoleChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}
