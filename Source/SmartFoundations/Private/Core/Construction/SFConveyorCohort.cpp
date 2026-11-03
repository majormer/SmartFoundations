// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFConveyorCohort.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGConveyorChainActor.h"
#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"

namespace
{
bool CaptureBoundaryItems(UFGInventoryComponent* Inventory, TArray<FSFInventoryStackSnapshot>& Out)
{
    TArray<FInventoryStack> Stacks; Inventory->GetInventoryStacks(Stacks, true);
    return SFInventorySnapshots::Capture(Stacks, Out);
}
}

bool FSFConveyorCohortValidator::IsDesignerMember(AActor* Actor, const FObjectKey& DesignerKey)
{
    auto* Buildable = Cast<AFGBuildable>(Actor);
    auto* Designer = Cast<AFGBuildableBlueprintDesigner>(DesignerKey.ResolveObjectPtr());
    if (!IsValid(Buildable) || Buildable->IsActorBeingDestroyed() || !IsValid(Designer)
        || Designer->IsActorBeingDestroyed() || !Designer->HasActorBegunPlay() || !Designer->HasAuthority()
        || !Buildable->HasActorBegunPlay() || !Buildable->HasAuthority()
        || Designer->GetWorld() != Buildable->GetWorld() || Buildable->GetBlueprintDesigner() != Designer)
        return false;
    int32 Occurrences = 0;
    for (auto* Member : Designer->GetBuildablesInBlueprintDesigner()) Occurrences += Member == Buildable;
    return Occurrences == 1;
}

bool FSFConveyorCohortValidator::CanExportChain(USFConstructionOperationCoordinator& Coordinator,
    AFGConveyorChainActor* Chain, const TSet<FObjectKey>& OwnedMembers, FString& Reason)
{
    auto Reject = [&Reason](const TCHAR* Detail) { Reason = Detail; return false; };
    if (!Coordinator.IsInMutationPhase() || !IsValid(Chain) || Chain->IsActorBeingDestroyed()
        || Chain->GetWorld() != Coordinator.GetWorld() || !Chain->HasAuthority())
        return Reject(TEXT("Export receiver is not a live owned-phase authority"));
    auto* Native = AFGBuildableSubsystem::Get(Coordinator.GetWorld());
    FConveyorTickGroup* Group = nullptr;
    int32 Bucket = INDEX_NONE, GroupCount = 0;
    for (int32 Index = 0; Native && Index < Native->mConveyorTickGroup.Num(); ++Index)
        if (Native->mConveyorTickGroup[Index] && Native->mConveyorTickGroup[Index]->ChainActor == Chain)
        { Group = Native->mConveyorTickGroup[Index]; Bucket = Index; ++GroupCount; }
    if (GroupCount != 1 || !Group || Group->Conveyors.Num() != Chain->mChainSplineSegments.Num()
        || Group->Conveyors.IsEmpty())
        return Reject(TEXT("Export source has no unique complete native owner"));
    // Native GetSegmentForConveyorBase scans the segment array. The SDK's renderer
    // map is not an authority ownership contract and need not be populated.
    FSFConveyorCohort::FCargoLayout Layout;
    Layout.AllocatedCapacity = Chain->mConveyorChainItems.Num(); Layout.CachedCapacity = Chain->mNumItems;
    Layout.Lead = Chain->mLeadItemIndex; Layout.Tail = Chain->mTailItemIndex;
    TSet<FObjectKey> Seen;
    const bool bExported = Layout.AllocatedCapacity == 0 && Layout.Lead == INDEX_NONE && Layout.Tail == INDEX_NONE;
    for (int32 Index = 0; Index < Group->Conveyors.Num(); ++Index)
    {
        auto* Member = Group->Conveyors[Index].Get();
        const auto& Segment = Chain->mChainSplineSegments[Index];
        if (!IsValid(Member) || Member->IsActorBeingDestroyed() || Member->GetWorld() != Coordinator.GetWorld()
            || !Member->HasAuthority() || !OwnedMembers.Contains(FObjectKey(Member)) || Seen.Contains(FObjectKey(Member))
            || Member->GetConveyorBucketID() != Bucket || Member->GetConveyorChainActor() != Chain
            || (!bExported && Member->mChainSegmentIndex != Index)
            || (bExported && Member->mChainSegmentIndex != Index && Member->mChainSegmentIndex != INDEX_NONE)
            || Segment.ChainActor != Chain || Segment.ConveyorBase != Member
            || !FMath::IsFinite(Segment.GetItemStartsAtLength())) return Reject(TEXT("Export member identity or segment metadata disagrees"));
        if (!bExported && Member->mItems.Num() > 0) return Reject(TEXT("Export would overwrite member-local cargo"));
        Seen.Add(FObjectKey(Member));
        Layout.SegmentRanges.Add({Segment.FirstItemIndex, Segment.LastItemIndex});
    }
    if (Chain->mFirstConveyor != Group->Conveyors[0] || Chain->mLastConveyor != Group->Conveyors.Last())
        return Reject(TEXT("Export endpoints disagree with native traversal"));
    TArray<FSFConveyorCohort::FCargoSlot> Slots;
    const auto Status = ValidateCargoLayout(Layout, Slots, Reason);
    return Status == ESFConveyorCohortStatus::CoherentStructure || (Status == ESFConveyorCohortStatus::NativePending && bExported);
}

bool FSFConveyorCohortValidator::CanRetire(USFConstructionOperationCoordinator& Coordinator,
    AFGBuildableConveyorBase* Conveyor, const TSet<FObjectKey>& OwnedMembers, FString& Reason)
{
    if (!Coordinator.IsInMutationPhase() || !IsValid(Conveyor) || Conveyor->IsActorBeingDestroyed()
        || !OwnedMembers.Contains(FObjectKey(Conveyor))) return false;
    auto* Native = AFGBuildableSubsystem::Get(Coordinator.GetWorld());
    if (!IsValid(Native) || Conveyor->GetWorld() != Coordinator.GetWorld() || !Conveyor->HasAuthority()) return false;
    int32 Occurrences = 0;
    for (auto* Group : Native->mConveyorTickGroup)
        if (Group) for (AFGBuildableConveyorBase* Member : Group->Conveyors) Occurrences += Member == Conveyor;
    const int32 Bucket = Conveyor->GetConveyorBucketID();
    if (Bucket == INDEX_NONE) return Occurrences == 0 && Conveyor->GetConveyorChainActor() == nullptr;
    if (Occurrences != 1 || !Native->mConveyorTickGroup.IsValidIndex(Bucket)
        || !Native->mConveyorTickGroup[Bucket] || !Native->mConveyorTickGroup[Bucket]->Conveyors.Contains(Conveyor)) return false;
    auto* Group = Native->mConveyorTickGroup[Bucket];
    for (AFGBuildableConveyorBase* Member : Group->Conveyors)
        if (!IsValid(Member) || Member->IsActorBeingDestroyed() || Member->GetConveyorBucketID() != Bucket
            || !OwnedMembers.Contains(FObjectKey(Member))) return false;
    if (Group->ChainActor) return CanExportChain(Coordinator, Group->ChainActor, OwnedMembers, Reason);
    return Conveyor->GetConveyorChainActor() == nullptr;
}

ESFConveyorCohortStatus FSFConveyorCohortValidator::ValidateStructure(const FSFConveyorCohort& Cohort, FString& Reason)
{
    using EStatus = ESFConveyorCohortStatus;
    auto Reject = [&Reason](const TCHAR* Detail) { Reason = Detail; return EStatus::Inconsistent; };
    Reason.Reset();
    if (Cohort.Members.IsEmpty()) return Reject(TEXT("Missing native membership"));
    if (Cohort.Designer.IsSet())
    {
        if (!Cohort.Groups.IsEmpty() || !Cohort.Chains.IsEmpty())
            return Reject(TEXT("Designer templates cannot own factory groups or chains"));
        for (const auto& Pair : Cohort.Members)
        {
            const auto& Member = Pair.Value;
            if (Member.Identity != Pair.Key || Member.Designer != Cohort.Designer
                || Member.Bucket != INDEX_NONE || Member.Chain.IsSet() || Member.Next.IsSet())
                return Reject(TEXT("Designer template ownership or factory exclusion disagrees"));
            for (const auto& Neighbor : {Member.InputNeighbor, Member.OutputNeighbor})
                if (Neighbor.IsSet() && !Cohort.Members.Contains(Neighbor.GetValue()))
                    return Reject(TEXT("Designer conveyor closure is incomplete"));
            if (Member.InputNeighbor.IsSet()
                && Cohort.Members.FindChecked(Member.InputNeighbor.GetValue()).OutputNeighbor != TOptional<FObjectKey>(Pair.Key))
                return Reject(TEXT("Designer input is not reciprocal"));
            if (Member.OutputNeighbor.IsSet()
                && Cohort.Members.FindChecked(Member.OutputNeighbor.GetValue()).InputNeighbor != TOptional<FObjectKey>(Pair.Key))
                return Reject(TEXT("Designer output is not reciprocal"));
        }
        return EStatus::CoherentStructure;
    }
    if (Cohort.Groups.IsEmpty()) return Reject(TEXT("Missing registered native membership"));
    TSet<FObjectKey> SeenMembers, SeenChains;
    TSet<int32> SeenBuckets;
    bool bPending = false;
    for (const auto& Group : Cohort.Groups)
    {
        if (Group.Bucket < 0 || SeenBuckets.Contains(Group.Bucket) || Group.Members.IsEmpty())
            return Reject(TEXT("Duplicate, invalid or empty native group"));
        SeenBuckets.Add(Group.Bucket);
        if (Group.PendingOccurrences < 0 || Group.PendingOccurrences > 1)
            return Reject(TEXT("Duplicate native migration request"));
        if (Group.Chain.IsSet() && Group.PendingOccurrences != 0)
            return Reject(TEXT("Assigned chain also pending migration"));
        // A shared scheduling owner does not exclude a materialized chain.
        // Native migration publishes ChainActor without clearing that owner.
        // Validate actual chain ownership independently of scheduling affinity.
        const FSFConveyorCohort::FChain* Chain = nullptr;
        if (Group.Chain.IsSet())
        {
            if (SeenChains.Contains(Group.Chain.GetValue())) return Reject(TEXT("Chain claimed by multiple groups"));
            SeenChains.Add(Group.Chain.GetValue());
            Chain = Cohort.Chains.Find(Group.Chain.GetValue());
            if (!Chain || Chain->NativeGroupOccurrences != 1 || Chain->Identity != Group.Chain.GetValue()
                || Chain->Segments != Group.Members
                || !Chain->First.IsSet() || !Chain->Last.IsSet()
                || Chain->First.GetValue() != Group.Members[0] || Chain->Last.GetValue() != Group.Members.Last())
                return Reject(TEXT("Chain endpoints, segments and group disagree"));
        }
        else if (Group.PendingOccurrences == 1) bPending = true;
        // An unqueued primary group is also a native settled representation:
        // local arrays own its cargo. Do not require synthetic chain creation.

        for (int32 Index = 0; Index < Group.Members.Num(); ++Index)
        {
            const FObjectKey& Key = Group.Members[Index];
            const auto* Member = Cohort.Members.Find(Key);
            if (!Member || Member->Designer.IsSet() || Member->Identity != Key || Member->Bucket != Group.Bucket || SeenMembers.Contains(Key))
                return Reject(TEXT("Member bucket, identity or unique ownership disagrees"));
            SeenMembers.Add(Key);
            if (Member->Chain != Group.Chain) return Reject(TEXT("Member chain backpointer disagrees"));
            if (Chain)
            {
                if (Member->SegmentIndex != Index)
                    return Reject(TEXT("Chain segment index disagrees"));
            }
            // A segment index is meaningful only with an assigned chain. Native
            // RemoveConveyor clears that association through SetConveyorChainActor
            // without clearing the cached index (CL513885 server 0x180500020,
            // REA ev_468318d452d7d78e4db41048f3fe250219af28f0ac670dee3ed6e7a7138b6419).
            // Pending/local groups own cargo in mItems; never use their stale
            // segment index to address a chain, or reject native re-registration.
            // Ordinary runs use output-to-input traversal. A native local loop
            // may close its tail onto the first member. Only a shared LOCAL
            // group may use scheduler order instead of chain traversal order.
            if (Chain || !Group.bShared)
            {
                if (Index + 1 < Group.Members.Num())
                {
                    if (!Member->Next.IsSet() || Member->Next.GetValue() != Group.Members[Index + 1])
                        return Reject(TEXT("Cached traversal disagrees with native member order"));
                    const auto* Upstream = Cohort.Members.Find(Group.Members[Index + 1]);
                    if (!Member->InputNeighbor.IsSet() || Member->InputNeighbor.GetValue() != Group.Members[Index + 1]
                        || !Upstream || !Upstream->OutputNeighbor.IsSet() || Upstream->OutputNeighbor.GetValue() != Key)
                        return Reject(TEXT("Physical internal edge disagrees with cached traversal"));
                }
                else if (Member->Next.IsSet())
                {
                    const auto* First = Cohort.Members.Find(Group.Members[0]);
                    if (Chain || Member->Next.GetValue() != Group.Members[0]
                        || !Member->InputNeighbor.IsSet() || Member->InputNeighbor.GetValue() != Group.Members[0]
                        || !First || !First->OutputNeighbor.IsSet() || First->OutputNeighbor.GetValue() != Key)
                        return Reject(TEXT("Local loop closure disagrees with native reciprocal traversal"));
                }
            }
        }
    }
    if (SeenMembers.Num() != Cohort.Members.Num() || SeenChains.Num() != Cohort.Chains.Num())
        return Reject(TEXT("Captured owner has no unique native group"));
    if (bPending) { Reason = TEXT("Retain native pending migration; no topology mutation admitted"); return EStatus::NativePending; }
    return EStatus::CoherentStructure;
}

ESFConveyorCohortStatus FSFConveyorCohortValidator::Capture(USFConstructionOperationCoordinator& Coordinator,
    const TArray<TWeakObjectPtr<AFGBuildableConveyorBase>>& Seeds, FSFConveyorCohort& Out, FString& Reason,
    bool bAllowDesigner)
{
    using EStatus = ESFConveyorCohortStatus;
    Out = FSFConveyorCohort();
    Reason.Reset();
    UWorld* World = Coordinator.GetWorld();
    // [MP-AUTH] A game-thread flag alone does not establish the factory mutation boundary.
    if ((!Coordinator.IsInMutationPhase() && !Coordinator.IsInObservationPhase())
        || !IsValid(World) || World->bIsTearingDown || Seeds.IsEmpty())
    { Reason = TEXT("No owned native mutation phase or no seed"); return EStatus::Unavailable; }
    AFGBuildableSubsystem* Native = AFGBuildableSubsystem::Get(World);
    if (!IsValid(Native) || !Native->HasAuthority()) return EStatus::Unavailable;
    auto Live = [World](AActor* Actor)
    {
        return IsValid(Actor) && Actor->GetWorld() == World && Actor->HasAuthority()
            && Actor->HasActorBegunPlay() && !Actor->IsActorBeingDestroyed();
    };
    auto Reject = [&Out, &Reason](const TCHAR* Detail)
    { Out = FSFConveyorCohort(); Reason = Detail; return EStatus::Inconsistent; };

    TArray<AFGBuildableConveyorBase*> Queue;
    for (const auto& Seed : Seeds)
    {
        if (!Live(Seed.Get())) return Reject(TEXT("Stale or ineligible conveyor seed"));
        Queue.Add(Seed.Get());
    }
    TSet<FObjectKey> Owners;
    auto AddOwner = [&Out, &Owners](AActor* Actor)
    {
        const FObjectKey Key(Actor);
        if (!Owners.Contains(Key)) { Owners.Add(Key); Out.AffectedActors.Add(Actor); }
    };
    if (auto* Designer = Seeds[0]->GetBlueprintDesigner())
    {
        if (!bAllowDesigner) return Reject(TEXT("Designer templates require an explicit local connection contract"));
        if (!Live(Designer)) return Reject(TEXT("Stale Designer owner"));
        Out.Designer = FObjectKey(Designer);
        AddOwner(Designer);
    }
    TSet<FConveyorTickGroup*> CapturedGroups;
    TMap<AFGBuildableConveyorBase*, int32> NativeOccurrences;
    TMap<AFGConveyorChainActor*, int32> NativeChainOccurrences;
    for (const auto* Group : Native->mConveyorTickGroup)
        if (Group)
        {
            for (AFGBuildableConveyorBase* Member : Group->Conveyors) ++NativeOccurrences.FindOrAdd(Member);
            if (Group->ChainActor) ++NativeChainOccurrences.FindOrAdd(Group->ChainActor);
        }
    constexpr int32 MaxAffectedConveyors = 8192;
    for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
    {
        AFGBuildableConveyorBase* Conveyor = Queue[Cursor];
        if (!Live(Conveyor)) return Reject(TEXT("Stale native member"));
        const FObjectKey Key(Conveyor);
        if (Out.Members.Contains(Key)) continue;
        if (Out.Members.Num() >= MaxAffectedConveyors)
        { Out = FSFConveyorCohort(); Reason = TEXT("Affected cohort exceeds bounded capture"); return EStatus::Unsupported; }
        const int32 Bucket = Conveyor->GetConveyorBucketID();
        FConveyorTickGroup* Group = nullptr;
        if (Out.Designer.IsSet())
        {
            if (!IsDesignerMember(Conveyor, Out.Designer.GetValue()) || Bucket != INDEX_NONE
                || Conveyor->GetConveyorChainActor() || Conveyor->GetNextTickConveyor()
                || NativeOccurrences.FindRef(Conveyor) != 0 || Native->mSerialConveyorGroup.Contains(Conveyor)
                || Native->mBuildableToSharedBucketIndexMap.Contains(Conveyor))
                return Reject(TEXT("Designer template is mixed, unowned or factory scheduled"));
        }
        else
        {
        if (Conveyor->GetBlueprintDesigner()) return Reject(TEXT("Ordinary cohort contains a Designer template"));
        if (!Native->mConveyorTickGroup.IsValidIndex(Bucket) || !Native->mConveyorTickGroup[Bucket])
            return Reject(TEXT("Missing or out-of-range native bucket"));
        Group = Native->mConveyorTickGroup[Bucket];
        // Check all native slots, including aliases of a group pointer, before following it.
        if (NativeOccurrences.FindRef(Conveyor) != 1 || !Group->Conveyors.Contains(Conveyor))
            return Reject(TEXT("Contradictory native group membership"));
        }

        FSFConveyorCohort::FMember Member;
        Member.Identity = Key; Member.Bucket = Bucket; Member.SegmentIndex = Conveyor->mChainSegmentIndex;
        Member.Designer = Out.Designer;
        Member.LocalItemCount = Conveyor->mItems.Num();
        if (AFGConveyorChainActor* Chain = Conveyor->GetConveyorChainActor())
        {
            if (!Live(Chain)) return Reject(TEXT("Retiring or stale chain backpointer"));
            Member.Chain = FObjectKey(Chain);
        }
        if (AFGBuildableConveyorBase* Next = Conveyor->GetNextTickConveyor())
        {
            if (!Live(Next)) return Reject(TEXT("Retiring or stale cached traversal"));
            Member.Next = FObjectKey(Next);
        }
        Out.Members.Add(Key, MoveTemp(Member));
        AddOwner(Conveyor);
        if (Group && !CapturedGroups.Contains(Group))
        {
            CapturedGroups.Add(Group);
            FSFConveyorCohort::FGroup Captured;
            Captured.Bucket = Bucket;
            for (const auto* Pending : Native->mConveyorGroupsPendingChainActors)
                Captured.PendingOccurrences += Pending == Group;
            Captured.bShared = Group->ForceIntoSharedByBuildable != nullptr;
            if (Group->ForceIntoSharedByBuildable)
            {
                if (!Live(Group->ForceIntoSharedByBuildable)) return Reject(TEXT("Stale shared tick owner"));
                AddOwner(Group->ForceIntoSharedByBuildable);
            }
            for (AFGBuildableConveyorBase* GroupMember : Group->Conveyors)
            {
                if (!Live(GroupMember)) return Reject(TEXT("Stale member in affected group"));
                Captured.Members.Add(FObjectKey(GroupMember)); Queue.Add(GroupMember);
            }
            if (AFGConveyorChainActor* Chain = Group->ChainActor)
            {
                if (!Live(Chain)) return Reject(TEXT("Retiring or stale native group chain"));
                Captured.Chain = FObjectKey(Chain); AddOwner(Chain);
                FSFConveyorCohort::FChain CapturedChain;
                CapturedChain.Identity = FObjectKey(Chain);
                CapturedChain.NativeGroupOccurrences = NativeChainOccurrences.FindRef(Chain);
                CapturedChain.CargoLayout.AllocatedCapacity = Chain->mConveyorChainItems.Num();
                CapturedChain.CargoLayout.CachedCapacity = Chain->mNumItems;
                CapturedChain.CargoLayout.Lead = Chain->mLeadItemIndex;
                CapturedChain.CargoLayout.Tail = Chain->mTailItemIndex;
                for (const auto& Segment : Chain->GetChainSegments())
                {
                    if (!Live(Segment.ConveyorBase) || Segment.ChainActor != Chain)
                        return Reject(TEXT("Invalid segment owner; ordinary reversion prohibited"));
                    CapturedChain.Segments.Add(FObjectKey(Segment.ConveyorBase));
                    CapturedChain.CargoLayout.SegmentRanges.Add({Segment.FirstItemIndex, Segment.LastItemIndex});
                    Queue.Add(Segment.ConveyorBase);
                }
                if (!Live(Chain->mFirstConveyor) || !Live(Chain->mLastConveyor))
                    return Reject(TEXT("Stale native chain endpoint"));
                CapturedChain.First = FObjectKey(Chain->mFirstConveyor);
                CapturedChain.Last = FObjectKey(Chain->mLastConveyor);
                Out.Chains.Add(CapturedChain.Identity, MoveTemp(CapturedChain));
            }
            Out.Groups.Add(MoveTemp(Captured));
        }

        // Boundary owners participate in conflict leases; don't select their other ports for upgrade.
        for (UFGFactoryConnectionComponent* Port : {Conveyor->GetConnection0(), Conveyor->GetConnection1()})
        {
            if (!IsValid(Port) || Port->GetOwner() != Conveyor) return Reject(TEXT("Invalid effective conveyor port"));
            UFGFactoryConnectionComponent* Peer = Port->GetConnection();
            if (!Peer) { if (Port->IsConnected()) return Reject(TEXT("Connected flag without peer")); continue; }
            if (!IsValid(Peer) || !Live(Peer->GetOwner()) || !Port->IsConnected() || !Peer->IsConnected()
                || Peer->GetConnection() != Port || Port == Peer)
                return Reject(TEXT("Nonreciprocal authoritative port"));
            const auto Direction = Port->GetDirection(), PeerDirection = Peer->GetDirection();
            if (!((Direction == EFactoryConnectionDirection::FCD_INPUT && PeerDirection == EFactoryConnectionDirection::FCD_OUTPUT)
                || (Direction == EFactoryConnectionDirection::FCD_OUTPUT && PeerDirection == EFactoryConnectionDirection::FCD_INPUT)))
                return Reject(TEXT("Unresolved or incompatible effective port direction"));
            AddOwner(Peer->GetOwner());
            if (Out.Designer.IsSet() && !IsDesignerMember(Peer->GetOwner(), Out.Designer.GetValue()))
                return Reject(TEXT("Designer edge crosses its exact native ownership boundary"));
            if (auto* Neighbor = Cast<AFGBuildableConveyorBase>(Peer->GetOwner()))
            {
                auto& Captured = Out.Members.FindChecked(Key);
                auto& NeighborSlot = Direction == EFactoryConnectionDirection::FCD_INPUT ? Captured.InputNeighbor : Captured.OutputNeighbor;
                if (NeighborSlot.IsSet()) return Reject(TEXT("Ambiguous effective conveyor port direction"));
                NeighborSlot = FObjectKey(Neighbor);
                Queue.Add(Neighbor);
            }
        }
    }
    // Native registration/export can touch attachments and factory endpoints.
    // Account for their real buffers separately; never substitute conveyor ring
    // totals for a complete affected-inventory conservation assertion.
    for (const auto& Weak : Out.AffectedActors)
    {
        auto* Actor = Weak.Get();
        if (!Live(Actor)) return Reject(TEXT("Boundary actor expired before inventory capture"));
        if (Cast<AFGBuildableConveyorBase>(Actor) || Cast<AFGConveyorChainActor>(Actor)) continue;
        TArray<UFGInventoryComponent*> Inventories; Actor->GetComponents(Inventories);
        for (auto* Inventory : Inventories)
        {
            if (!IsValid(Inventory)) return Reject(TEXT("Invalid boundary inventory identity"));
            FSFConveyorCohort::FBoundaryInventory Value; Value.Inventory = Inventory; Value.Owner = FObjectKey(Actor);
            if (!CaptureBoundaryItems(Inventory, Value.Items))
            {
                Reason = TEXT("Affected factory/attachment buffer has invalid or oversized contents");
                return EStatus::Unsupported;
            }
            Out.BoundaryInventories.Add(FObjectKey(Inventory), MoveTemp(Value));
        }
    }
    const EStatus Structure = ValidateStructure(Out, Reason);
    if (Structure != EStatus::CoherentStructure) return Structure;
    if (Out.Designer.IsSet())
    {
        // A null member backpointer is insufficient if an orphan native chain
        // still claims the template. Inspect actual segment owners as well.
        for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
            for (const auto& Segment : It->GetChainSegments())
                if (Segment.ConveyorBase && Out.Members.Contains(FObjectKey(Segment.ConveyorBase)))
                    return Reject(TEXT("Designer template is still claimed by a native chain segment"));
    }
    for (auto& Pair : Out.Chains)
    {
        auto& Chain = Pair.Value;
        const EStatus Cargo = ValidateCargoLayout(Chain.CargoLayout, Chain.CargoSlots, Reason);
        if (Cargo != EStatus::CoherentStructure) return Cargo;
        for (const FObjectKey& MemberKey : Chain.Segments)
        {
            if (Out.Members.FindChecked(MemberKey).LocalItemCount > 0)
            {
                Reason = TEXT("Assigned chain still has member-local cargo; ownership transition unresolved");
                return EStatus::NativePending;
            }
        }
    }
    // Primary membership alone does not prove that workers will use it. The
    // native outer registration paths dirty this cache; allow that rebuild,
    // then require each affected current group to be scheduled exactly once.
    if (Native->mConveyorBucketGroupsDirty)
    {
        Reason = TEXT("Retain native pending worker scheduling-cache rebuild");
        return EStatus::NativePending;
    }
    TSet<const FConveyorTickGroup*> RegisteredGroups;
    for (const auto* Group : Native->mConveyorTickGroup) RegisteredGroups.Add(Group);
    if (Out.Designer.IsSet())
        for (const auto* Pending : Native->mConveyorGroupsPendingChainActors)
            if (!Pending || !RegisteredGroups.Contains(Pending))
                return Reject(TEXT("Designer exclusion cannot be proven with an orphan pending group"));
    TSet<const FConveyorTickGroup*> ScheduledGroups;
    for (const auto& Bucket : Native->mConveyorBucketGroups)
        for (const auto* Scheduled : Bucket)
        {
            // Never dereference a cached group before proving it still belongs
            // to the primary owner. A freed entry can outlive an otherwise
            // correctly scheduled affected group and crash a later worker.
            if (!Scheduled || !RegisteredGroups.Contains(Scheduled) || ScheduledGroups.Contains(Scheduled))
                return Reject(TEXT("Worker scheduling cache contains an obsolete or duplicate native group"));
            ScheduledGroups.Add(Scheduled);
        }
    for (const auto& Group : Out.Groups)
        if (!ScheduledGroups.Contains(Native->mConveyorTickGroup[Group.Bucket]))
            return Reject(TEXT("Affected native group missing from worker scheduling cache"));
    return EStatus::CoherentStructure;
}

bool FSFConveyorCohortValidator::VerifyBoundaryInventories(USFConstructionOperationCoordinator& Coordinator,
    const FSFConveyorCohort& Before, FString& Reason, const TSet<FObjectKey>* RetiredOwners)
{
    if (!Coordinator.IsInMutationPhase()) { Reason = TEXT("Boundary conservation requires the immediate owned mutation phase"); return false; }
    for (const auto& Pair : Before.BoundaryInventories)
    {
        // An explicitly retired native child returns its full contents through
        // the separate refund ledger; it cannot remain a live buffer owner.
        if (RetiredOwners && RetiredOwners->Contains(Pair.Value.Owner)) continue;
        auto* Inventory = Pair.Value.Inventory.Get();
        auto* Actor = IsValid(Inventory) ? Inventory->GetOwner() : nullptr;
        TArray<FInventoryStack> Current;
        if (IsValid(Inventory)) Inventory->GetInventoryStacks(Current, true);
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasAuthority()
            || Actor->GetWorld() != Coordinator.GetWorld() || FObjectKey(Inventory) != Pair.Key
            || FObjectKey(Actor) != Pair.Value.Owner
            || !SFInventorySnapshots::Matches(Pair.Value.Items, Current))
        { Reason = TEXT("Affected boundary inventory identity or contents changed during mutation"); return false; }
    }
    return true;
}

ESFConveyorCohortStatus FSFConveyorCohortValidator::SnapshotCargo(USFConstructionOperationCoordinator& Coordinator,
    const FSFConveyorCohort& Cohort, FSFConveyorCohort::FCargoSnapshot& Out, FString& Reason)
{
    using EStatus = ESFConveyorCohortStatus;
    Out.Reset(); Reason.Reset();
    if ((!Coordinator.IsInMutationPhase() && !Coordinator.IsInObservationPhase()) || Cohort.Members.IsEmpty())
        return EStatus::Unavailable;
    FSFConveyorCohort::FCargoSnapshot Snapshot;
    auto Append = [&Reason](TArray<FSFConveyorCohort::FCargoItem>& Items, const FConveyorBeltItem& Item, float Offset)
    {
        if (!IsValid(Item.Item.GetItemClass().Get()) || !FMath::IsFinite(Offset))
        { Reason = TEXT("Invalid cargo descriptor or offset"); return EStatus::Inconsistent; }
        if (!FSFInventoryItemSnapshot::IsValidValue(Item.Item))
        { Reason = TEXT("Invalid native cargo payload"); return EStatus::Inconsistent; }
        Items.Add({Item.Item.GetItemClass(), Offset, MakeShared<FSFInventoryItemSnapshot>(Item.Item)});
        return EStatus::CoherentStructure;
    };
    for (const auto& Pair : Cohort.Members)
    {
        auto* Conveyor = Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr());
        if (!IsValid(Conveyor) || Conveyor->IsActorBeingDestroyed() || Conveyor->GetWorld() != Coordinator.GetWorld()
            || !Conveyor->HasAuthority()) return EStatus::Unavailable;
        auto& Items = Snapshot.Add(Pair.Key);
        for (int32 Index = 0; Index < Conveyor->mItems.Num(); ++Index)
        {
            const auto& Item = Conveyor->mItems[Index];
            const EStatus Status = Append(Items, Item, Item.Offset);
            if (Status != EStatus::CoherentStructure) return Status;
        }
    }
    for (const auto& Pair : Cohort.Chains)
    {
        auto* Chain = Cast<AFGConveyorChainActor>(Pair.Key.ResolveObjectPtr());
        if (!IsValid(Chain) || Chain->IsActorBeingDestroyed() || Chain->GetWorld() != Coordinator.GetWorld()
            || !Chain->HasAuthority()) return EStatus::Unavailable;
        TArray<FSFConveyorCohort::FCargoSlot> Slots;
        const EStatus Layout = ValidateCargoLayout(Pair.Value.CargoLayout, Slots, Reason);
        const bool bExported = Chain->mConveyorChainItems.IsEmpty() && Chain->mLeadItemIndex == INDEX_NONE
            && Chain->mTailItemIndex == INDEX_NONE;
        if (Layout == EStatus::NativePending && bExported) continue;
        if (Layout != EStatus::CoherentStructure) return Layout;
        for (const auto& Slot : Slots)
        {
            if (!Chain->mConveyorChainItems.IsValidIndex(Slot.PhysicalIndex)
                || !Chain->mChainSplineSegments.IsValidIndex(Slot.SegmentIndex)) return EStatus::Inconsistent;
            const auto& Segment = Chain->mChainSplineSegments[Slot.SegmentIndex];
            const FObjectKey MemberKey(Segment.ConveyorBase);
            auto* Items = Snapshot.Find(MemberKey);
            if (!Items || !Pair.Value.Segments.IsValidIndex(Slot.SegmentIndex)
                || MemberKey != Pair.Value.Segments[Slot.SegmentIndex]) return EStatus::Inconsistent;
            const auto& Item = Chain->mConveyorChainItems[Slot.PhysicalIndex];
            const EStatus Status = Append(*Items, Item, Item.Offset - Segment.GetItemStartsAtLength());
            if (Status != EStatus::CoherentStructure) return Status;
        }
    }
    Out = MoveTemp(Snapshot);
    return EStatus::CoherentStructure;
}

ESFConveyorCohortStatus FSFConveyorCohortValidator::ValidateCargoLayout(const FSFConveyorCohort::FCargoLayout& Layout,
    TArray<FSFConveyorCohort::FCargoSlot>& Slots, FString& Reason)
{
    using EStatus = ESFConveyorCohortStatus;
    Slots.Reset(); Reason.Reset();
    auto Reject = [&Reason](const TCHAR* Detail) { Reason = Detail; return EStatus::Inconsistent; };
    constexpr int32 MaxCaptureSlots = 1 << 20;
    const int32 Capacity = Layout.AllocatedCapacity;
    if (Capacity < 0 || Layout.CachedCapacity < 0) return Reject(TEXT("Negative ring capacity"));
    const bool bLeadEmpty = Layout.Lead == INDEX_NONE, bTailEmpty = Layout.Tail == INDEX_NONE;
    if (bLeadEmpty != bTailEmpty) return Reject(TEXT("Mixed empty ring sentinels"));
    if (Capacity == 0)
    {
        if (!bLeadEmpty) return Reject(TEXT("Nonempty header without backing storage"));
        // Native export clears allocated storage and both endpoints but can retain cached capacity.
        // Do not reinterpret this unfinished ownership transition as an ordinary settled chain.
        Reason = TEXT("Chain ring is not initialized or has been exported; native transition unresolved");
        return EStatus::NativePending;
    }
    if (Capacity != Layout.CachedCapacity) return Reject(TEXT("Allocated and cached ring capacity disagree"));
    if (Capacity > MaxCaptureSlots)
    { Reason = TEXT("Ring exceeds bounded cargo capture"); return EStatus::Unsupported; }
    if (!bLeadEmpty && (Layout.Lead < 0 || Layout.Lead >= Capacity || Layout.Tail < 0 || Layout.Tail >= Capacity))
        return Reject(TEXT("Active ring endpoint outside backing array"));
    const int64 ActiveCount = bLeadEmpty ? 0 : Layout.Tail >= Layout.Lead
        ? int64(Layout.Tail) - Layout.Lead + 1 : int64(Capacity) - Layout.Lead + Layout.Tail + 1;
    TArray<int32> SegmentForActiveSlot;
    SegmentForActiveSlot.Init(INDEX_NONE, static_cast<int32>(ActiveCount));
    for (int32 SegmentIndex = 0; SegmentIndex < Layout.SegmentRanges.Num(); ++SegmentIndex)
    {
        const auto& Range = Layout.SegmentRanges[SegmentIndex];
        const bool bFirstEmpty = Range.First == INDEX_NONE, bLastEmpty = Range.Last == INDEX_NONE;
        if (bFirstEmpty != bLastEmpty) return Reject(TEXT("Mixed empty segment sentinels"));
        if (bFirstEmpty) continue;
        if (Range.First < 0 || Range.First >= Capacity || Range.Last < 0 || Range.Last >= Capacity)
            return Reject(TEXT("Segment range outside backing array"));
        const int64 Count = Range.Last >= Range.First ? int64(Range.Last) - Range.First + 1
            : int64(Capacity) - Range.First + Range.Last + 1;
        if (Count > ActiveCount) return Reject(TEXT("Segment claims more than active cargo"));
        int32 PhysicalIndex = Range.First;
        for (int64 Item = 0; Item < Count; ++Item)
        {
            const int64 Distance = PhysicalIndex >= Layout.Lead ? int64(PhysicalIndex) - Layout.Lead
                : int64(Capacity) - Layout.Lead + PhysicalIndex;
            if (Distance < 0 || Distance >= ActiveCount) return Reject(TEXT("Segment claims unused ring storage"));
            if (SegmentForActiveSlot[static_cast<int32>(Distance)] != INDEX_NONE)
                return Reject(TEXT("Multiple segments claim one cargo slot"));
            SegmentForActiveSlot[static_cast<int32>(Distance)] = SegmentIndex;
            PhysicalIndex = PhysicalIndex + 1 == Capacity ? 0 : PhysicalIndex + 1;
        }
    }
    if (SegmentForActiveSlot.Contains(INDEX_NONE)) return Reject(TEXT("Active cargo has no segment owner"));
    // Publish only after the complete layout passes; no native pointer escapes the capture.
    int32 PhysicalIndex = Layout.Lead;
    for (int32 SegmentIndex : SegmentForActiveSlot)
    {
        Slots.Add({PhysicalIndex, SegmentIndex});
        PhysicalIndex = PhysicalIndex + 1 == Capacity ? 0 : PhysicalIndex + 1;
    }
    return EStatus::CoherentStructure;
}
