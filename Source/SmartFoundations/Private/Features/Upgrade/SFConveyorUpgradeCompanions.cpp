// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Upgrade/SFConveyorUpgradeCompanions.h"
#include "SmartFoundations.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorMonitor.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGBlueprintProxy.h"
#include "Hologram/FGHologram.h"
#include "FGInventoryComponent.h"
#include "FGDismantleInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace SFConveyorUpgradeCompanions
{
namespace
{
bool ReadMonitors(AFGBuildableConveyorBase* Conveyor, TArray<AFGBuildableConveyorMonitor*>& Out)
{
    Out.Reset();
    // Inspect the reflected native ownership field without guessed offsets or
    // mutating registration. Bound the count and validate entries before using
    // this membership proof; callers hold the owned pre-factory mutation phase.
    auto* Property = FindFProperty<FArrayProperty>(AFGBuildableConveyorBase::StaticClass(), TEXT("mAttachedThroughputMonitors"));
    auto* Element = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!IsValid(Conveyor) || !Element || Element->PropertyClass != AFGBuildableConveyorMonitor::StaticClass()) return false;
    FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Conveyor));
    if (Array.Num() > 4096) return false;
    TSet<AFGBuildableConveyorMonitor*> Seen;
    for (int32 Index = 0; Index < Array.Num(); ++Index)
    {
        auto* Object = Element->GetObjectPropertyValue(Array.GetRawPtr(Index));
        if (!Object) continue;
        auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Object);
        if (!IsValid(Monitor) || Seen.Contains(Monitor)) return false;
        Seen.Add(Monitor); Out.Add(Monitor);
    }
    return true;
}
bool NativeCompanion(UClass* Class)
{
    if (!IsValid(Class) || !Class->IsChildOf(AFGBuildable::StaticClass())) return false;
    const FString Package = Class->GetOutermost()->GetName();
    return Package.StartsWith(TEXT("/Game/FactoryGame/")) || Package == TEXT("/Script/FactoryGame");
}
bool CaptureInventories(AFGBuildable* Actor, FSFConveyorUpgradeOwner::FCompanion& Value)
{
    TArray<UFGInventoryComponent*> Inventories; Actor->GetComponents(Inventories);
    for (auto* Inventory : Inventories)
    {
        if (!IsValid(Inventory) || Inventory->GetOwner() != Actor) return false;
        FSFConveyorCohort::FBoundaryInventory Snapshot; Snapshot.Inventory = Inventory; Snapshot.Owner = FObjectKey(Actor);
        TArray<FInventoryStack> Stacks; Inventory->GetInventoryStacks(Stacks, true);
        if (!SFInventorySnapshots::Capture(Stacks, Snapshot.Items)) return false;
        Value.Inventories.Add(FObjectKey(Inventory), MoveTemp(Snapshot));
    }
    return true;
}
bool MatchInventories(AFGBuildable* Actor, const FSFConveyorUpgradeOwner::FCompanion& Value)
{
    TArray<UFGInventoryComponent*> Inventories; Actor->GetComponents(Inventories);
    if (Inventories.Num() != Value.Inventories.Num()) return false;
    for (auto* Inventory : Inventories)
    {
        const auto* Before = Value.Inventories.Find(FObjectKey(Inventory));
        if (!IsValid(Inventory) || !Before || Before->Inventory.Get() != Inventory || Inventory->GetOwner() != Actor) return false;
        TArray<FInventoryStack> Stacks; Inventory->GetInventoryStacks(Stacks, true);
        if (!SFInventorySnapshots::Matches(Before->Items, Stacks)) return false;
    }
    return true;
}
bool ReadNativeChildren(AFGBuildable* Root, TArray<AFGBuildable*>& Out, const TSet<FObjectKey>& Excluded)
{
    TArray<AActor*> Pending; IFGDismantleInterface::Execute_GetChildDismantleActors(Root, Pending);
    TSet<FObjectKey> Seen;
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        if (Pending.Num() > 4096) return false;
        auto* Actor = Cast<AFGBuildable>(Pending[Index]);
        // A transferred hole/monitor retains its descendants too. Do not expand
        // that branch and accidentally select a surviving attached conveyor.
        if (Actor && Excluded.Contains(FObjectKey(Actor))) continue;
        if (Actor == Root) continue; // The native expanding selection already contains its root.
        if (!IsValid(Actor) || !NativeCompanion(Actor->GetClass())) return false;
        if (Seen.Contains(FObjectKey(Actor))) continue;
        Seen.Add(FObjectKey(Actor)); Out.Add(Actor);
        TArray<AActor*> Next; IFGDismantleInterface::Execute_GetChildDismantleActors(Actor, Next);
        Pending.Append(Next);
    }
    return true;
}
bool Refund(AFGBuildable* Actor, TMap<TSubclassOf<UFGItemDescriptor>, int32>& Out)
{
    TArray<FInventoryStack> Stacks; Actor->GetDismantleRefundReturns(Stacks);
    for (const auto& Stack : Stacks)
    {
        if (!Stack.Item.GetItemClass() || Stack.NumItems < 0 || Stack.Item.HasState() || Stack.Item.LegacyItemStateActor) return false;
        const int64 Count = int64(Out.FindRef(Stack.Item.GetItemClass())) + Stack.NumItems;
        if (Count > MAX_int32) return false;
        if (Count > 0) Out.Add(Stack.Item.GetItemClass(), static_cast<int32>(Count));
    }
    return true;
}
bool SameMaterials(const auto& A, const auto& B)
{
    if (A.Num() != B.Num()) return false;
    for (const auto& Pair : A) if (!B.Contains(Pair.Key) || B.FindRef(Pair.Key) != Pair.Value) return false;
    return true;
}
}

bool Capture(AFGBuildableConveyorBase* Old, AFGHologram* Holo,
    FSFConveyorUpgradeOwner::FOperation& Operation, USFConstructionOperationCoordinator& Owner, FString& Reason)
{
    auto Fail = [&]() { Reason = TEXT("Native conveyor companions have invalid identity, ownership, contents or output plans"); return false; };
    if (!IsValid(Old) || !IsValid(Holo) || !Owner.IsInMutationPhase()) return Fail();
    const FObjectKey Parent(Old);
    FSFConveyorUpgradeOwner::FCompanionPlan Plan;
    // Preserve native holes and monitors independently of the dismantle set.
    TArray<AFGBuildable*> Retained;
    if (auto* Lift = Cast<AFGBuildableConveyorLift>(Old))
        for (auto* Hole : Lift->GetSnappedPassthroughs()) if (Hole) Retained.AddUnique(Hole);
    TArray<AFGBuildableConveyorMonitor*> Monitors;
    if (!ReadMonitors(Old, Monitors)) return Fail();
    for (auto* Monitor : Monitors) if (Monitor) Retained.AddUnique(Monitor);
    for (auto* Actor : Retained)
    {
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetIsDismantled() || Actor->IsAboutToBeDismantled() || !Actor->HasAuthority()
            || Actor->GetWorld() != Owner.GetWorld() || !NativeCompanion(Actor->GetClass())
            || !Owner.AdoptActor(Operation.Key, Actor)) return Fail();
        if (auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Actor); Monitor && Monitor->GetConveyorBase() != Old) return Fail();
        FSFConveyorUpgradeOwner::FCompanion Value;
        Value.Identity = FObjectKey(Actor); Value.Actor = Actor; Value.Class = Actor->GetClass(); Value.Pose = Actor->GetActorTransform();
        if (auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Actor))
        {
            Value.MonitorOffset = Monitor->GetOffsetAlongConveyor();
            if (!FMath::IsFinite(Value.MonitorOffset.GetValue())) return Fail();
        }
        Value.Designer = Actor->GetBlueprintDesigner(); Value.Group = Actor->GetBlueprintProxy();
        if (Value.Pose.ContainsNaN() || !CaptureInventories(Actor, Value)
            || (Value.Designer.IsValid() && !Owner.AdoptActor(Operation.Key, Value.Designer.Get()))
            || (Value.Group.IsValid() && !Owner.AdoptActor(Operation.Key, Value.Group.Get()))) return Fail();
        Plan.Retained.Add(MoveTemp(Value));
    }
    TArray<AFGBuildable*> OldChildren;
    TSet<FObjectKey> Excluded = Operation.RetiredCompanions;
    for (auto* Actor : Retained) Excluded.Add(FObjectKey(Actor));
    if (!ReadNativeChildren(Old, OldChildren, Excluded)) return Fail();
    TSet<FObjectKey> Seen;
    for (int32 Index = 0; Index < OldChildren.Num(); ++Index)
    {
        auto* Child = Cast<AFGBuildable>(OldChildren[Index]);
        if (!OldChildren[Index]) continue;
        if (Retained.Contains(Child)) continue;
        if (!Child) return Fail();
        if (Seen.Contains(FObjectKey(Child))) continue;
        if (Seen.Num() >= 4096 || !IsValid(Child) || Child->IsActorBeingDestroyed() || Child->GetIsDismantled() || Child->IsAboutToBeDismantled() || !Child->HasAuthority()
            || Child->GetWorld() != Owner.GetWorld() || !NativeCompanion(Child->GetClass())
            || !IFGDismantleInterface::Execute_CanDismantle(Child) || !Owner.AdoptActor(Operation.Key, Child)) return Fail();
        Seen.Add(FObjectKey(Child));
        if (Operation.Selected.ContainsByPredicate([&](const auto& Selected) { return Selected.Get() == Child; }))
        { Reason = TEXT("One actor cannot be both an explicit replacement target and another target's dismantle child"); return false; }
        if (const auto* Claim = Operation.CompanionClaims.Find(FObjectKey(Child)); Claim && *Claim != Parent) continue;
        FSFConveyorUpgradeOwner::FCompanion Value;
        Value.Identity = FObjectKey(Child); Value.Actor = Child; Value.Class = Child->GetClass(); Value.Pose = Child->GetActorTransform();
        Value.Designer = Child->GetBlueprintDesigner(); Value.Group = Child->GetBlueprintProxy();
        if ((Value.Designer.IsValid() && !Owner.AdoptActor(Operation.Key, Value.Designer.Get()))
            || (Value.Group.IsValid() && !Owner.AdoptActor(Operation.Key, Value.Group.Get()))) return Fail();
        FSFConveyorCohort::FCargoSnapshot ChildCargo;
        if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Child))
        {
            // Qualify ownership before the native refund getter borrows segment
            // item pointers. Copy all borrowed values before any native removal.
            FSFConveyorCohort Closure; FString Detail;
            if (FSFConveyorCohortValidator::Capture(Owner, {Conveyor}, Closure, Detail) != ESFConveyorCohortStatus::CoherentStructure
                || FSFConveyorCohortValidator::SnapshotCargo(Owner, Closure, ChildCargo, Detail) != ESFConveyorCohortStatus::CoherentStructure) return Fail();
            for (const auto& Weak : Closure.AffectedActors) if (!Owner.AdoptActor(Operation.Key, Weak.Get())) return Fail();
            for (const auto& Pair : Closure.Members)
            {
                Operation.OwnedMembers.Add(Pair.Key);
                if (!Operation.Before.Members.Contains(Pair.Key))
                {
                    auto* Member = Cast<AFGBuildable>(Pair.Key.ResolveObjectPtr());
                    if (!IsValid(Member)) return Fail();
                    auto* Designer = Member->GetBlueprintDesigner(); auto* Group = Member->GetBlueprintProxy();
                    if ((Designer && !Owner.AdoptActor(Operation.Key, Designer)) || (Group && !Owner.AdoptActor(Operation.Key, Group))) return Fail();
                    Operation.OriginalDesigners.Add(Pair.Key, Designer);
                    Operation.OriginalGroups.Add(Pair.Key, Group);
                    Operation.Before.Members.Add(Pair.Key, Pair.Value);
                    Operation.BeforeCargo.Add(Pair.Key, ChildCargo.FindChecked(Pair.Key));
                }
            }
            for (const auto& Group : Closure.Groups)
                if (!Operation.Before.Groups.ContainsByPredicate([&](const auto& Existing) { return Existing.Bucket == Group.Bucket; }))
                    Operation.Before.Groups.Add(Group);
            for (const auto& Pair : Closure.Chains) Operation.Before.Chains.Add(Pair.Key, Pair.Value);
            for (const auto& Pair : Closure.BoundaryInventories) Operation.Before.BoundaryInventories.Add(Pair.Key, Pair.Value);
            for (const auto& Weak : Closure.AffectedActors) Operation.Before.AffectedActors.AddUnique(Weak);
        }
        TArray<FInventoryStack> FullRefund;
        IFGDismantleInterface::Execute_GetDismantleRefund(Child, FullRefund, Operation.bFreeBuild);
        if (Value.Pose.ContainsNaN() || !Refund(Child, Value.Materials) || !CaptureInventories(Child, Value)
            || !SFInventorySnapshots::Capture(FullRefund, Value.Refund)) return Fail();
        if (const auto* Cargo = ChildCargo.Find(Value.Identity))
        {
            TArray<FSFInventoryStackSnapshot> ExpectedCargo;
            for (const auto& Item : *Cargo) ExpectedCargo.Add({Item.Payload, 1});
            if (!SFInventorySnapshots::ContainsContents(ExpectedCargo, FullRefund)) return Fail();
        }
        for (const auto& Existing : Plan.Old)
            if (Existing.Class == Value.Class && Existing.Pose.Equals(Value.Pose, 0.1f)) return Fail();
        Operation.CompanionClaims.Add(Value.Identity, Parent); Plan.Old.Add(MoveTemp(Value));
    }
    TArray<AFGHologram*> Pending = Holo->GetHologramChildren();
    TSet<AFGHologram*> SeenPreviews;
    while (!Pending.IsEmpty())
    {
        auto* Child = Pending.Pop(EAllowShrinking::No);
        if (!IsValid(Child) || SeenPreviews.Contains(Child) || SeenPreviews.Num() >= 4096) return Fail();
        SeenPreviews.Add(Child);
        if (Child->IsDisabled()) continue;
        if (!NativeCompanion(Child->GetBuildClass()) || Child->GetBlueprintDesigner() != Old->GetBlueprintDesigner()) return Fail();
        Pending.Append(Child->GetHologramChildren());
        FSFConveyorUpgradeOwner::FCompanion Value;
        Value.Class = Child->GetBuildClass().Get(); Value.Pose = Child->GetActorTransform();
        if (Value.Pose.ContainsNaN()) return Fail();
        for (const auto& Cost : Child->GetCost(false))
        {
            const int64 Total = int64(Value.Materials.FindRef(Cost.ItemClass)) + Cost.Amount;
            if (!Cost.ItemClass || Cost.Amount < 0 || Total > MAX_int32) return Fail();
            if (Total > 0) Value.Materials.Add(Cost.ItemClass, static_cast<int32>(Total));
        }
        for (const auto& Existing : Plan.Expected)
            if (Existing.Class == Value.Class && Existing.Pose.Equals(Value.Pose, 0.1f)) return Fail();
        Plan.Expected.Add(MoveTemp(Value));
    }
    Operation.Companions.Add(Parent, MoveTemp(Plan));
    return true;
}

bool MatchesPrepared(AFGBuildableConveyorBase* Old, AFGHologram* Holo,
    const FSFConveyorUpgradeOwner::FOperation& Operation)
{
    const auto* Plan = Operation.Companions.Find(FObjectKey(Old));
    if (!Plan) return true; // The first preflight has not captured its plan yet.
    for (const auto& Value : Plan->Retained)
    {
        auto* Actor = Value.Actor.Get();
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetClass() != Value.Class.Get()
            || !Actor->GetActorTransform().Equals(Value.Pose, 0.1f) || !MatchInventories(Actor, Value)) return false;
        if (auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Actor);
            Monitor && (Monitor->GetConveyorBase() != Old || !Value.MonitorOffset.IsSet()
                || !FMath::IsNearlyEqual(Monitor->GetOffsetAlongConveyor(), Value.MonitorOffset.GetValue(), 0.1f))) return false;
    }
    for (const auto& Value : Plan->Old)
    {
        auto* Actor = Value.Actor.Get();
        TMap<TSubclassOf<UFGItemDescriptor>, int32> Materials;
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetClass() != Value.Class.Get()
            || !Actor->GetActorTransform().Equals(Value.Pose, 0.1f) || !MatchInventories(Actor, Value)
            || !Refund(Actor, Materials) || !SameMaterials(Value.Materials, Materials)) return false;
    }
    TArray<AFGHologram*> Pending = Holo->GetHologramChildren();
    TSet<AFGHologram*> Seen;
    TSet<int32> Used;
    while (!Pending.IsEmpty())
    {
        auto* Child = Pending.Pop(EAllowShrinking::No);
        if (!IsValid(Child) || Seen.Contains(Child) || Seen.Num() >= 4096) return false;
        Seen.Add(Child);
        if (Child->IsDisabled()) continue;
        Pending.Append(Child->GetHologramChildren());
        int32 Match = INDEX_NONE;
        for (int32 Index = 0; Index < Plan->Expected.Num(); ++Index)
            if (!Used.Contains(Index) && Plan->Expected[Index].Class.Get() == Child->GetBuildClass().Get()
                && Plan->Expected[Index].Pose.Equals(Child->GetActorTransform(), 0.1f))
            { if (Match != INDEX_NONE) return false; Match = Index; }
        if (Match == INDEX_NONE) return false;
        TMap<TSubclassOf<UFGItemDescriptor>, int32> Materials;
        for (const auto& Cost : Child->GetCost(false))
        {
            const int64 Count = int64(Materials.FindRef(Cost.ItemClass)) + Cost.Amount;
            if (!Cost.ItemClass || Cost.Amount < 0 || Count > MAX_int32) return false;
            if (Count > 0) Materials.Add(Cost.ItemClass, static_cast<int32>(Count));
        }
        if (!SameMaterials(Materials, Plan->Expected[Match].Materials)) return false;
        Used.Add(Match);
    }
    return Used.Num() == Plan->Expected.Num();
}

bool Adopt(AFGBuildableConveyorBase* Old, const TArray<AActor*>& Children,
    FSFConveyorUpgradeOwner::FOperation& Operation, USFConstructionOperationCoordinator& Owner, bool bPreserveGroups, FString& Reason)
{
    auto* Plan = Operation.Companions.Find(FObjectKey(Old));
    auto Fail = [&]() { Reason = TEXT("Native returned companions differ from the admitted output/cost plan"); return false; };
    if (!Plan || Children.Num() != Plan->Expected.Num()) return Fail();
    TSet<int32> Used;
    for (auto* Output : Children)
    {
        auto* Child = Cast<AFGBuildable>(Output);
        if (!IsValid(Child) || Child->IsActorBeingDestroyed()
            || !NativeCompanion(Child->GetClass()) || !Owner.AdoptActor(Operation.Key, Child)
            || Child->GetBlueprintDesigner() != Old->GetBlueprintDesigner()) return Fail();
        int32 Match = INDEX_NONE;
        for (int32 Index = 0; Index < Plan->Expected.Num(); ++Index)
            if (!Used.Contains(Index) && Plan->Expected[Index].Class.Get() == Child->GetClass()
                && Plan->Expected[Index].Pose.Equals(Child->GetActorTransform(), 0.1f))
            { if (Match != INDEX_NONE) return Fail(); Match = Index; }
        if (Match == INDEX_NONE) return Fail();
        auto Actual = Plan->Expected[Match];
        Actual.Identity = FObjectKey(Child); Actual.Actor = Child;
        TMap<TSubclassOf<UFGItemDescriptor>, int32> Materials;
        if (!Refund(Child, Materials) || !SameMaterials(Actual.Materials, Materials) || !CaptureInventories(Child, Actual)) return Fail();
        Used.Add(Match);
        // Correspondence uses only this returned set and exact admitted pose/class.
        const FSFConveyorUpgradeOwner::FCompanion* Prior = nullptr;
        for (const auto& Candidate : Plan->Old)
            if (Candidate.Class == Actual.Class && Candidate.Pose.Equals(Actual.Pose, 0.1f))
            { if (Prior) return Fail(); Prior = &Candidate; }
        if (Prior && Prior->Actor.IsValid())
        {
            FSFReplacementGroupTransfer Group(Prior->Actor.Get(), bPreserveGroups, true);
            Group.Apply(Child);
        }
        Actual.Designer = Child->GetBlueprintDesigner(); Actual.Group = Child->GetBlueprintProxy();
        Plan->Actual.Add(MoveTemp(Actual));
    }
    return true;
}

bool PrepareRetirement(AFGBuildableConveyorBase* Old, FSFConveyorUpgradeOwner::FOperation& Operation,
    USFConstructionOperationCoordinator& Owner, FString& Reason)
{
    auto* Plan = Operation.Companions.Find(FObjectKey(Old));
    if (!Plan || !Owner.IsInMutationPhase()) return false;
    // Native Upgrade has already transferred lift-hole slots. Monitor binding
    // and conveyor/chain registration are separate native contracts.
    auto* Replacement = Operation.Replacements.Find(FObjectKey(Old));
    if (!Replacement || !Replacement->IsValid()) return false;
    for (const auto& Value : Plan->Retained)
    {
        auto* Actor = Value.Actor.Get();
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !MatchInventories(Actor, Value))
        { Reason = TEXT("Retained native companion expired or changed contents"); return false; }
        TStrongObjectPtr<AFGBuildable> RetainedPin(Actor);
        if (auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Actor))
        {
            TArray<AFGBuildableConveyorMonitor*> Previous, Attached;
            if (!ReadMonitors(Old, Previous) || !ReadMonitors(Replacement->Get(), Attached))
            { Reason = TEXT("Native monitor registration cannot be read before transfer"); return false; }
            int32 PreviousOccurrences = 0, AttachedOccurrences = 0;
            for (auto* Entry : Previous) PreviousOccurrences += Entry == Monitor;
            for (auto* Entry : Attached) AttachedOccurrences += Entry == Monitor;
            if (Monitor->GetConveyorBase() == Old)
            {
                if (PreviousOccurrences != 1 || AttachedOccurrences != 0)
                { Reason = TEXT("Native monitor has ambiguous registration before transfer"); return false; }
                // SetSnappedSplineBuildable updates the backpointer and sampling
                // state, but does not unregister/register either conveyor or chain.
                Old->UnregisterThroughputMonitor(Monitor);
                Monitor->SetSnappedSplineBuildable(Replacement->Get());
                Replacement->Get()->RegisterThroughputMonitor(Monitor);
            }
            else if (Monitor->GetConveyorBase() != Replacement->Get()
                || PreviousOccurrences != 0 || AttachedOccurrences != 1)
            { Reason = TEXT("Native monitor binding and registration disagree before retirement"); return false; }
            if (Monitor->GetConveyorBase() != Replacement->Get())
            { Reason = TEXT("Native monitor did not transfer to the replacement conveyor"); return false; }
            if (!ReadMonitors(Replacement->Get(), Attached) || !ReadMonitors(Old, Previous))
            { Reason = TEXT("Native monitor registration cannot be read after transfer"); return false; }
            int32 Occurrences = 0;
            for (auto* Entry : Attached) Occurrences += Entry == Monitor;
            if (Occurrences != 1 || Previous.Contains(Monitor) || !Value.MonitorOffset.IsSet()
                || !FMath::IsNearlyEqual(Monitor->GetOffsetAlongConveyor(), Value.MonitorOffset.GetValue(), 0.1f))
            { Reason = TEXT("Native monitor transfer changed registration multiplicity or captured offset"); return false; }
        }
    }
    TArray<AFGBuildable*> Current;
    TSet<FObjectKey> Excluded = Operation.RetiredCompanions;
    for (const auto& Value : Plan->Retained) Excluded.Add(Value.Identity);
    if (!ReadNativeChildren(Old, Current, Excluded)) return false;
    for (auto* Child : Current)
    {
        const FObjectKey Identity(Child);
        if (Plan->Retained.ContainsByPredicate([&](const auto& Value) { return Value.Identity == Identity; })) continue;
        const auto* Claim = Operation.CompanionClaims.Find(Identity);
        if (!Claim) { Reason = TEXT("Native child selection expanded after effects; no speculative retirement"); return false; }
        if (*Claim != FObjectKey(Old) || Operation.RetiredCompanions.Contains(Identity)) continue;
        const auto* Value = Plan->Old.FindByPredicate([&](const auto& Entry) { return Entry.Identity == Identity; });
        if (!Value || !IsValid(Child) || Child->IsActorBeingDestroyed() || Child->GetClass() != Value->Class.Get()
            || !Child->GetActorTransform().Equals(Value->Pose, 0.1f) || !MatchInventories(Child, *Value)
            || !IFGDismantleInterface::Execute_CanDismantle(Child))
        { Reason = TEXT("Admitted native companion changed before retirement"); return false; }
        TArray<FInventoryStack> RefundNow;
        IFGDismantleInterface::Execute_GetDismantleRefund(Child, RefundNow, Operation.bFreeBuild);
        if (!SFInventorySnapshots::SameContents(Value->Refund, RefundNow))
        { Reason = TEXT("Native companion refund changed after admission"); return false; }
        if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Child);
            Conveyor && !FSFConveyorCohortValidator::CanRetire(Owner, Conveyor, Operation.OwnedMembers, Reason)) return false;
        auto* Mutable = Plan->Old.FindByPredicate([&](const auto& Entry) { return Entry.Identity == Identity; });
        Mutable->RetirementPin = MakeShared<TStrongObjectPtr<AFGBuildable>>(Child);
        Plan->Retiring.Add(Identity);
    }
    // Native Upgrade can relinquish a previous child relationship. Such actors
    // stay alive and receive no dismantle refund, rather than being deleted from
    // a stale pre-Upgrade list.
    for (const auto& Value : Plan->Old)
        if (!Plan->Retiring.Contains(Value.Identity))
        {
            auto* Actor = Value.Actor.Get();
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !MatchInventories(Actor, Value)) return false;
            Plan->Retained.Add(Value);
        }
    return true;
}

bool Retire(const FObjectKey& Old, FSFConveyorUpgradeOwner::FOperation& Operation,
    USFConstructionOperationCoordinator& Owner, FString& Reason)
{
    auto* Plan = Operation.Companions.Find(Old);
    if (!Plan || !Owner.IsInMutationPhase()) return false;
    for (auto& Value : Plan->Old)
    {
        if (!Plan->Retiring.Contains(Value.Identity) || Operation.RetiredCompanions.Contains(Value.Identity)) continue;
        auto* Child = Value.RetirementPin ? Value.RetirementPin->Get() : nullptr;
        if (!Child || FObjectKey(Child) != Value.Identity) return false;
        // A native parent retirement may already have retired a selected child.
        // The pinned pre-retirement refund remains owed, but never dismantle twice.
        if (!Child->IsActorBeingDestroyed() && !Child->GetIsDismantled())
        {
            if (!IsValid(Child) || !MatchInventories(Child, Value)) return false;
            TArray<FInventoryStack> RefundNow;
            IFGDismantleInterface::Execute_GetDismantleRefund(Child, RefundNow, Operation.bFreeBuild);
            if (!SFInventorySnapshots::SameContents(Value.Refund, RefundNow)) return false;
            if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Child);
                Conveyor && !FSFConveyorCohortValidator::CanRetire(Owner, Conveyor, Operation.OwnedMembers, Reason)) return false;
            IFGDismantleInterface::Execute_Dismantle(Child);
        }
        if (!Child->IsActorBeingDestroyed() && !Child->GetIsDismantled())
        { Reason = TEXT("Native companion retirement did not start"); return false; }
        Operation.RetiredCompanions.Add(Value.Identity);
        Operation.CompanionRefunds.Append(Value.Refund);
        Value.RetirementPin.Reset();
    }
    return true;
}

bool HasPendingLifecycle(const FSFConveyorUpgradeOwner::FOperation& Operation)
{
    for (const auto& Pair : Operation.Companions)
        if (Operation.Replacements.Contains(Pair.Key))
        {
            for (const auto& Old : Pair.Value.Old)
                if (Operation.RetiredCompanions.Contains(Old.Identity) && Old.Actor.IsValid() && !Old.Actor->IsActorBeingDestroyed() && Old.Actor->GetIsDismantled()) return true;
            for (const auto& Current : Pair.Value.Actual)
                if (Current.Actor.IsValid() && !Current.Actor->IsActorBeingDestroyed() && !Current.Actor->HasActorBegunPlay()) return true;
        }
    return false;
}

bool Verify(const FSFConveyorUpgradeOwner::FOperation& Operation)
{
    for (const auto& Pair : Operation.Companions)
    {
        if (!Operation.Replacements.Contains(Pair.Key)) continue;
        for (const auto& Old : Pair.Value.Old)
            if (Operation.RetiredCompanions.Contains(Old.Identity)
                && Old.Actor.IsValid() && !Old.Actor->IsActorBeingDestroyed()) return false;
        for (const auto& Value : Pair.Value.Retained)
        {
            auto* Actor = Value.Actor.Get();
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasActorBegunPlay()
                || Actor->GetClass() != Value.Class.Get() || !Actor->GetActorTransform().Equals(Value.Pose, 0.1f)
                || Value.Designer.IsStale() || Value.Group.IsStale()
                || Actor->GetBlueprintDesigner() != Value.Designer.Get() || Actor->GetBlueprintProxy() != Value.Group.Get()) return false;
            if (auto* Monitor = Cast<AFGBuildableConveyorMonitor>(Actor);
                Monitor && (Monitor->GetConveyorBase() != Operation.Replacements.FindChecked(Pair.Key).Get()
                    || !Value.MonitorOffset.IsSet() || !FMath::IsNearlyEqual(Monitor->GetOffsetAlongConveyor(), Value.MonitorOffset.GetValue(), 0.1f))) return false;
            if (auto* Designer = Value.Designer.Get())
            {
                int32 Occurrences = 0;
                for (auto* Member : Designer->GetBuildablesInBlueprintDesigner()) Occurrences += Member == Actor;
                if (Occurrences != 1) return false;
            }
            if (auto* Group = Value.Group.Get())
            {
                int32 Occurrences = 0;
                for (auto* Member : Group->GetBuildables()) Occurrences += Member == Actor;
                if (Occurrences != 1) return false;
            }
        }
        for (const auto& Value : Pair.Value.Actual)
        {
            auto* Actor = Value.Actor.Get();
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasActorBegunPlay() || Actor->GetClass() != Value.Class.Get()
                || !Actor->GetActorTransform().Equals(Value.Pose, 0.1f)
                || Value.Designer.IsStale() || Value.Group.IsStale()
                || Actor->GetBlueprintDesigner() != Value.Designer.Get() || Actor->GetBlueprintProxy() != Value.Group.Get()) return false;
            if (auto* Designer = Value.Designer.Get())
            {
                int32 Occurrences = 0;
                for (auto* Member : Designer->GetBuildablesInBlueprintDesigner()) Occurrences += Member == Actor;
                if (Occurrences != 1) return false;
            }
            if (auto* Group = Value.Group.Get())
            {
                int32 Occurrences = 0;
                for (auto* Member : Group->GetBuildables()) Occurrences += Member == Actor;
                if (Occurrences != 1) return false;
            }
        }
    }
    return true;
}
}
