// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFWallOutletPlacement.h"
#include "SmartFoundations.h"
#include "Hologram/FGPowerPoleWallHologram.h"
#include "Patching/NativeHookManager.h"

void FSFWallOutletPlacement::PostPlacement(TConstArrayView<AFGHologram*> Children, TFunctionRef<void()> NativePlacement)
{
    // CL 502094 native wall-pole post-placement relocates ALL direct children to
    // mSnapConnection, bypassing their virtual placement guards. Keep vanilla's
    // snap/marker/wire work, then restore only the transforms authored by Smart.
    // This is the same boundary-preservation pattern as the Extend belt path;
    // no child-list mutation, delayed refresh, or placement-validation bypass.
    static const FName GridChildTag(TEXT("SF_GridChild"));
    TArray<TPair<TWeakObjectPtr<AFGHologram>, FTransform>, TInlineAllocator<8>> Transforms;
    for (AFGHologram* Child : Children)
    {
        if (IsValid(Child) && (Child->Tags.Contains(GridChildTag) || Child->Tags.Contains(TEXT("SF_ExactPowerPlan"))))
        {
            Transforms.Emplace(Child, Child->GetActorTransform());
        }
    }

    NativePlacement();

    for (const auto& Entry : Transforms)
    {
        AFGHologram* Child = Entry.Key.Get();
        if (IsValid(Child) && (Child->Tags.Contains(GridChildTag) || Child->Tags.Contains(TEXT("SF_ExactPowerPlan"))) &&
            !Child->GetActorTransform().Equals(Entry.Value, 0.001))
        {
            Child->SetActorTransform(Entry.Value);
        }
    }
}

void FSFWallOutletPlacement::RegisterHooks()
{
    static bool bRegistered = false;
    if (bRegistered) return;
    bRegistered = true;

    SUBSCRIBE_METHOD_VIRTUAL(AFGPowerPoleWallHologram::PostHologramPlacement,
        GetMutableDefault<AFGPowerPoleWallHologram>(),
        [](auto& Scope, AFGPowerPoleWallHologram* Self, const FHitResult& HitResult, bool bCallForChildren)
        {
            const TArray<AFGHologram*> Children = Self->GetHologramChildren();
            PostPlacement(Children, [&]() { Scope(Self, HitResult, bCallForChildren); });
        });
}
