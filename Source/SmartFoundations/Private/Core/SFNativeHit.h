// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "AbstractInstanceManager.h"
#include "Engine/EngineTypes.h"

namespace SFNativeHit
{
    /** Resolve the exact collision instance, preserving ordinary actor hits. */
    inline AActor* ResolveActor(const FHitResult& Hit)
    {
        AActor* Actor = Hit.GetActor();
        if (AAbstractInstanceManager* Manager = Cast<AAbstractInstanceManager>(Actor))
        {
            FInstanceHandle Handle;
            if (Manager->ResolveHit(Hit, Handle))
                if (AActor* Owner = AAbstractInstanceManager::GetOwnerByHandle(Handle); IsValid(Owner)) return Owner;
        }
        return Actor;
    }

    /** Keep the real collision geometry; replace only an instanced actor identity. */
    inline void ResolveForPlacement(FHitResult& Hit)
    {
        AActor* Owner = ResolveActor(Hit);
        if (IsValid(Owner) && Owner != Hit.GetActor()) Hit.HitObjectHandle = FActorInstanceHandle(Owner);
    }
}
