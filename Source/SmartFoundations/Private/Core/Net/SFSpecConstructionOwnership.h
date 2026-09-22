// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Hologram/FGHologram.h"

namespace SFSpecConstructionOwnership
{
    // Staging is keyed by instigator/class, which a factory child shares with its root.
    // Only the root may look up or consume that request.
    inline bool CanOwnStagedRequest(const AFGHologram* Hologram)
    {
        return IsValid(Hologram) && Hologram->GetParentHologram() == nullptr;
    }

    inline bool CanUseLocalPowerPlan(bool IsRoot, bool IsLocalInstigator, bool IsActiveHologram,
        bool IsExtendOrRestore, bool HasStagedRequest)
    {
        return IsRoot && IsLocalInstigator && IsActiveHologram && !IsExtendOrRestore && !HasStagedRequest;
    }
}
