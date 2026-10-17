// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Dismantle/SFLightweightGroupRefund.h"

#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "FGBlueprintProxy.h"
#include "FGCharacterPlayer.h"
#include "FGGameState.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPlayerState.h"
#include "FGInventoryComponent.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — Lightweight group refund preview
//  1. Preview — [MP-CLIENT] reads the owning local player's replicated instance data.
//  2. Delivery — vanilla authority dismantle/refund execution is unchanged.
namespace
{
    bool TryGetGroupRefund(const UFGBuildGunStateDismantle* State, TArray<FInventoryStack>& OutRefund)
    {
        if (!IsValid(State) || State->HasAnyFlags(RF_ClassDefaultObject)) return false;
        AFGBuildGun* Gun = State->GetBuildGun();
        AFGCharacterPlayer* Player = IsValid(Gun) ? Gun->GetInstigatorCharacter() : nullptr;
        // [MP-CLIENT] This is only a projection for the owning local HUD.
        if (!IsValid(Player) || !Player->IsLocallyControlled() || Gun->GetCurrentState() != State) return false;
        const AFGPlayerState* PlayerState = State->GetPlayerState();
        const AFGGameState* GameState = State->GetWorld()->GetGameState<AFGGameState>();
        const UFGInventoryComponent* Inventory = Player->GetInventory();
        if (!IsValid(PlayerState) || !IsValid(GameState) || !IsValid(Inventory)
            || PlayerState->GetPlayerRules().NoBuildCost || GameState->GetCheatNoCost() || Inventory->GetNoBuildCost()) return false;

        TArray<AActor*> Selection = State->GetPendingDismantleActors();
        if (AActor* Selected = State->GetSelectedActor()) Selection.AddUnique(Selected);
        TSet<AFGBlueprintProxy*> Groups;
        for (AActor* Actor : Selection)
        {
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return false;
            if (AFGBlueprintProxy* Group = Cast<AFGBlueprintProxy>(Actor))
            {
                // Named blueprints and partially replicated groups retain native behavior.
                if (!Group->GetBlueprintName().IsEmpty() || Group->GetBlueprintDescriptor()
                    || !Group->AreProxyBuildingsRegisteredAndValid()) return false;
                Groups.Add(Group);
            }
        }
        if (Groups.IsEmpty()) return false;

        AFGLightweightBuildableSubsystem* Lightweight = AFGLightweightBuildableSubsystem::Get(State->GetWorld());
        if (!IsValid(Lightweight)) return false;
        TMap<UClass*, TSet<int32>> Seen;
        for (AFGBlueprintProxy* Group : Groups)
        {
            int32 InstanceCount = 0;
            for (const FBuildableClassLightweightIndices& Bundle : Group->GetLightweightClassAndIndices())
            {
                if (!IsValid(Bundle.BuildableClass.Get())) return false;
                const AFGBuildable* Prototype = Bundle.BuildableClass.GetDefaultObject();
                TSet<int32>& Indices = Seen.FindOrAdd(Bundle.BuildableClass.Get());
                for (int32 Index : Bundle.Indices)
                {
                    FRuntimeBuildableInstanceData* Data = Index >= 0
                        ? Lightweight->GetRuntimeDataForBuildableClassAndIndex(Bundle.BuildableClass, Index) : nullptr;
                    if (!Data || !Data->IsValid() || Data->BlueprintProxy.Get() != Group) return false;
                    ++InstanceCount;
                    if (Indices.Contains(Index)) continue;
                    Indices.Add(Index);
                    TArray<FInventoryStack> Refund;
                    // Native CDO API preserves recipe and type-specific geometry multipliers.
                    // It does not materialize full buildable actors or request server refunds.
                    Prototype->GetLightweightBuildableDismantleRefundReturns(State->GetWorld(), Data->BuiltWithRecipe, Data->TypeSpecificData, Refund);
                    for (const FInventoryStack& Stack : Refund)
                    {
                        if (!IsValid(Stack.Item.GetItemClass().Get()) || Stack.NumItems <= 0) return false;
                        FInventoryStack* Existing = OutRefund.FindByPredicate([&](const FInventoryStack& Item)
                            { return Item.Item.GetItemClass() == Stack.Item.GetItemClass(); });
                        if (Existing)
                        {
                            if (Stack.NumItems > MAX_int32 - Existing->NumItems) return false;
                            Existing->NumItems += Stack.NumItems;
                        }
                        else OutRefund.Add(Stack);
                    }
                }
            }
            if (InstanceCount == 0) return false;
            // Native group highlighting temporarily registers actors for the same instances.
            // They are representations, not additional refundable buildings.
            for (AFGBuildable* Member : Group->GetBuildables())
            {
                const TSet<int32>* Indices = IsValid(Member) ? Seen.Find(Member->GetClass()) : nullptr;
                if (!IsValid(Member) || !Member->GetIsLightweightTemporary()
                    || Member->GetBlueprintProxy() != Group || !Indices || !Indices->Contains(Member->GetRuntimeDataIndex())) return false;
            }
        }
        for (AActor* Actor : Selection)
        {
            if (Actor->IsA<AFGBlueprintProxy>()) continue;
            AFGBuildable* Member = Cast<AFGBuildable>(Actor);
            const TSet<int32>* Indices = Member ? Seen.Find(Member->GetClass()) : nullptr;
            if (!Member || !Member->GetIsLightweightTemporary() || !Groups.Contains(Member->GetBlueprintProxy())
                || !Indices || !Indices->Contains(Member->GetRuntimeDataIndex())) return false;
        }
        return true;
    }
}

void FSFLightweightGroupRefund::RegisterHooks()
{
#if !WITH_EDITOR
    // The Editor SDK getter is an empty stub; Shipping has the native array-copy body.
    static bool bRegistered = false;
    if (bRegistered) return;
    bRegistered = true;
    SUBSCRIBE_METHOD(UFGBuildGunStateDismantle::GetPeekDismantleRefund,
        [](auto& Scope, const UFGBuildGunStateDismantle* Self)
        {
            // SML by-value returns must be initialized by invoking the native function
            // before overriding its return slot.
            Scope(Self);
            TArray<FInventoryStack> Refund;
            if (TryGetGroupRefund(Self, Refund)) Scope.Override(MoveTemp(Refund));
        });
#endif
}
