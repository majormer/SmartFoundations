// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "AbstractInstanceManager.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
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

    /** Keep collision geometry; resolve the native actor needed by placement. */
    inline void ResolveForPlacement(FHitResult& Hit)
    {
        if (auto* Manager = Cast<AAbstractInstanceManager>(Hit.GetActor()))
        {
            FInstanceHandle Handle;
            FLightweightBuildableInstanceRef Instance;
            if (Manager->ResolveHit(Hit, Handle)
                && AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle, Instance))
            {
                // The collision owner is the lightweight subsystem, not the
                // foundation/wall that native snapping requires. The native pool
                // supplies its unsaved, unreplicated actor representation and
                // owns its retirement; never destroy that actor ourselves.
                AFGBuildable* Temporary = IsInGameThread() ? Instance.SpawnTemporaryBuildable() : nullptr;
                if (IsValid(Temporary) && !Temporary->IsActorBeingDestroyed()
                    && Temporary->GetWorld() == Manager->GetWorld()
                    && Temporary->GetClass() == Instance.GetBuildableClass()
                    && Temporary->GetActorTransform().Equals(Instance.GetBuildableTransform(), 0.01f))
                    Hit.HitObjectHandle = FActorInstanceHandle(Temporary);
                else
                    Hit.HitObjectHandle = FActorInstanceHandle();
                return;
            }
        }
        AActor* Owner = ResolveActor(Hit);
        if (IsValid(Owner) && Owner != Hit.GetActor()) Hit.HitObjectHandle = FActorInstanceHandle(Owner);
    }
}
