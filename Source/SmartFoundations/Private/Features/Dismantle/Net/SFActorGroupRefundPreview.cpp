// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Dismantle/Net/SFActorGroupRefundPreview.h"
#include "SmartFoundations.h"

#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "Engine/World.h"
#include "FGBlueprintProxy.h"
#include "FGDismantleInterface.h"
#include "FGInventoryLibrary.h"
#include "Patching/NativeHookManager.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — Actor group refund preview
//  1. Selection — [MP-SEAM] uses the existing vanilla refund request and its native acknowledgement.
//  2. Pricing   — [MP-AUTH] expands complete authority membership rather than local relevance.
//  3. Response  — [MP-REPL] replaces only refund stacks in the existing replicated preview cache.
//  4. Delivery  — native dismantle and material delivery remain unchanged.
namespace
{
    bool TryGetActorGroupRefund(UFGBuildGunStateDismantle* State,
        const TArray<AActor*>& SelectedActors, bool bNoBuildCostEnabled,
        TArray<FInventoryStack>& OutRefund)
    {
        TSet<AFGBlueprintProxy*> Groups;
        TSet<AActor*> Members;
        for (AActor* Actor : SelectedActors)
        {
            if (AFGBlueprintProxy* Group = Cast<AFGBlueprintProxy>(Actor))
            {
                if (!IsValid(Group) || !Group->HasAuthority() || Group->IsActorBeingDestroyed()
                    || Group->GetWorld() != State->GetWorld() || !Group->GetBlueprintName().IsEmpty()
                    || Group->GetBlueprintDescriptor() || !Group->AreProxyBuildingsRegisteredAndValid()) return false;
                // Lightweight refunds remain on their dedicated native instance/preview path.
                // Mixing them into the actor cache could double the local lightweight refund.
                for (const FBuildableClassLightweightIndices& Bundle : Group->GetLightweightClassAndIndices())
                    if (!Bundle.Indices.IsEmpty()) return false;
                Groups.Add(Group);
                for (AFGBuildable* Member : Group->GetBuildables())
                {
                    if (!IsValid(Member) || Member->IsActorBeingDestroyed() || !Member->HasAuthority()
                        || Member->GetBlueprintProxy() != Group || Member->GetIsLightweightTemporary()) return false;
                    Members.Add(Member);
                }
            }
        }
        if (Groups.IsEmpty() || Members.IsEmpty()) return false;
        for (AActor* Actor : SelectedActors)
        {
            // Keep native handling for individual selections and selections mixing other roots.
            // An unresolved entry is not enough evidence to replace the native calculation.
            if (!IsValid(Actor) || (!Groups.Contains(Cast<AFGBlueprintProxy>(Actor)) && !Members.Contains(Actor))) return false;
        }

        TArray<AActor*> Queue;
        for (AFGBlueprintProxy* Group : Groups) Queue.Add(Group);
        TSet<AActor*> Seen;
        // Bound a single preview query; oversized graphs retain native behavior.
        constexpr int32 MaxActorsPerQuery = 4096;
        for (int32 Index = 0; Index < Queue.Num(); ++Index)
        {
            AActor* Actor = Queue[Index];
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasAuthority()
                || Actor->GetWorld() != State->GetWorld() || !Actor->Implements<UFGDismantleInterface>()) return false;
            if (Seen.Contains(Actor)) continue;
            Seen.Add(Actor);
            if (Seen.Num() > MaxActorsPerQuery) return false;
            if (const AFGBuildable* Buildable = Cast<AFGBuildable>(Actor); Buildable && Buildable->GetIsLightweightTemporary()) return false;
            TArray<FInventoryStack> Refund;
            IFGDismantleInterface::Execute_GetDismantleRefund(Actor, Refund, bNoBuildCostEnabled);
            // Preserve native stacks, including inventory item state, until native consolidation.
            OutRefund.Append(Refund);
            TArray<AActor*> Dependencies;
            IFGDismantleInterface::Execute_GetDismantleDependencies(Actor, Dependencies);
            IFGDismantleInterface::Execute_GetChildDismantleActors(Actor, Dependencies);
            for (AActor* Dependency : Dependencies)
            {
                if (!IsValid(Dependency)) return false;
                if (!Seen.Contains(Dependency)) Queue.AddUnique(Dependency);
            }
            if (Queue.Num() > MaxActorsPerQuery) return false;
        }
        for (AActor* Member : Members) if (!Seen.Contains(Member)) return false;
        UFGInventoryLibrary::ConsolidateInventoryItems(OutRefund);
        return true;
    }
}

void FSFActorGroupRefundPreview::RegisterHooks()
{
#if !WITH_EDITOR
    // The Editor SDK RPC body is an empty stub; only the live native body is hooked.
    static bool bRegistered = false;
    if (bRegistered) return;
    bRegistered = true;
    SUBSCRIBE_METHOD_VIRTUAL(UFGBuildGunStateDismantle::Server_PeekAtDismantleRefund_Implementation,
        GetMutableDefault<UFGBuildGunStateDismantle>(),
        [](auto& Scope, UFGBuildGunStateDismantle* Self, const TArray<AActor*>& SelectedActors, bool bNoBuildCostEnabled)
        {
            Scope(Self, SelectedActors, bNoBuildCostEnabled);
            if (!IsValid(Self) || !Self->GetWorld() || Self->GetWorld()->GetNetMode() == NM_Client) return;
            const FStructProperty* Property = FindFProperty<FStructProperty>(Self->GetClass(), TEXT("mPeekDismantleRefundNetworked"));
            if (!Property || Property->Struct != FDismantleRefunds::StaticStruct()) return;
            TArray<FInventoryStack> Refund;
            if (!TryGetActorGroupRefund(Self, SelectedActors, bNoBuildCostEnabled, Refund)) return;
            FDismantleRefunds* Cache = Property->ContainerPtrToValuePtr<FDismantleRefunds>(Self);
            // Preserve vanilla's acknowledgement/count and replication lifecycle. Only prices change.
            Cache->PeekDismantleRefund = MoveTemp(Refund);
        });
#endif
}
