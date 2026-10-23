// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Holograms/SFElevatorConstructionHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"

void ASFElevatorConstructionHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}
