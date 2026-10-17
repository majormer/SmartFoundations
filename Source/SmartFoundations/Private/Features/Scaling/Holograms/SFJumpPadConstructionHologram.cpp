// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFJumpPadConstructionHologram.h"
#include "Data/SFHologramDataRegistry.h"

void ASFJumpPadConstructionHologram::Destroyed()
{
    USFHologramDataRegistry::ClearData(this);
    Super::Destroyed();
}
