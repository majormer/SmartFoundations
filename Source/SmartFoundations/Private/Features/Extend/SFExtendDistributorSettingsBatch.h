// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ObjectKey.h"
#include "Features/Extend/SFExtendCloneTopology.h"

namespace SFExtendDistributorSettingsBatch
{
    /** The merged plan and per-copy plans may describe the same returned actor.
     * Apply that actor once, but never allow a second plan to replace its settings.
     * The caller still validates class, lifetime, authority and output ownership. */
    inline bool Add(TMap<FObjectKey, FSFCapturedDistributorSettings>& Batch,
        const FObjectKey& Actor, const FSFCapturedDistributorSettings& Settings)
    {
        if (const auto* Existing = Batch.Find(Actor))
            return FSFCapturedDistributorSettings::StaticStruct()->CompareScriptStruct(Existing, &Settings, 0);
        Batch.Add(Actor, Settings);
        return true;
    }
}
