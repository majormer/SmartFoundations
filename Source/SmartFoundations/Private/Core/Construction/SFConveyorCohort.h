// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "Core/Construction/SFInventoryItemSnapshot.h"

class AActor;
class AFGBuildableConveyorBase;
class USFConstructionOperationCoordinator;
class UFGItemDescriptor;
class AFGConveyorChainActor;
class UFGInventoryComponent;

/** Read-only structural admission. This is not a cargo/resource conservation certificate. */
enum class ESFConveyorCohortStatus : uint8
{
    CoherentStructure,
    NativePending,
    Unsupported,
    Inconsistent,
    Unavailable
};

struct FSFConveyorCohort
{
    struct FCargoItem
    {
        TSubclassOf<UFGItemDescriptor> Descriptor;
        float Offset = 0.0f;
        TSharedPtr<FSFInventoryItemSnapshot> Payload;
        bool SameValue(const FCargoItem& Other) const
        {
            return Descriptor == Other.Descriptor && (Payload && Other.Payload
                ? Payload->Matches(Other.Payload->Get()) : !Payload && !Other.Payload);
        }
    };
    using FCargoSnapshot = TMap<FObjectKey, TArray<FCargoItem>>;
    struct FCargoRange
    {
        int32 First = INDEX_NONE;
        int32 Last = INDEX_NONE;
    };
    struct FCargoLayout
    {
        int32 AllocatedCapacity = 0;
        int32 CachedCapacity = 0;
        int32 Lead = INDEX_NONE;
        int32 Tail = INDEX_NONE;
        TArray<FCargoRange> SegmentRanges;
    };
    struct FCargoSlot
    {
        int32 PhysicalIndex = INDEX_NONE;
        int32 SegmentIndex = INDEX_NONE;
    };
    struct FMember
    {
        FObjectKey Identity;
        TOptional<FObjectKey> Designer;
        int32 Bucket = INDEX_NONE;
        TOptional<FObjectKey> Chain;
        TOptional<FObjectKey> Next;
        TOptional<FObjectKey> InputNeighbor;
        TOptional<FObjectKey> OutputNeighbor;
        int32 SegmentIndex = INDEX_NONE;
        int32 LocalItemCount = 0;
    };
    struct FBoundaryInventory
    {
        FObjectKey Owner;
        TWeakObjectPtr<UFGInventoryComponent> Inventory;
        TArray<FSFInventoryStackSnapshot> Items;
    };
    struct FGroup
    {
        int32 Bucket = INDEX_NONE;
        TArray<FObjectKey> Members;
        TOptional<FObjectKey> Chain;
        int32 PendingOccurrences = 0;
        bool bShared = false;
    };
    struct FChain
    {
        FObjectKey Identity;
        int32 NativeGroupOccurrences = 0;
        TArray<FObjectKey> Segments;
        TOptional<FObjectKey> First;
        TOptional<FObjectKey> Last;
        FCargoLayout CargoLayout;
        /** Active lead-to-tail slots only, never unused ring preallocation. */
        TArray<FCargoSlot> CargoSlots;
    };

    TMap<FObjectKey, FMember> Members;
    /** Explicit template ownership, never a synthetic factory tick group. */
    TOptional<FObjectKey> Designer;
    TArray<FGroup> Groups;
    TMap<FObjectKey, FChain> Chains;
    /** Includes boundary owners and chain actors; membership here does not authorize upgrading them. */
    TArray<TWeakObjectPtr<AActor>> AffectedActors;
    /** Existing attachment/factory buffers are separate from conveyor-local/ring cargo. */
    TMap<FObjectKey, FBoundaryInventory> BoundaryInventories;
};

class FSFConveyorCohortValidator
{
public:
    /** Native containers must be captured only in the coordinator's owned pre/post-factory callback.
     * No native teardown, cargo getter, registration or recovery is performed. */
    static ESFConveyorCohortStatus Capture(USFConstructionOperationCoordinator& Coordinator,
        const TArray<TWeakObjectPtr<AFGBuildableConveyorBase>>& Seeds, FSFConveyorCohort& Out, FString& Reason,
        bool bAllowDesigner = false);

    /** Exact native membership, used for new edge endpoints as well as existing peers. */
    static bool IsDesignerMember(AActor* Actor, const FObjectKey& Designer);

    /** Validates materialized runs and native local/shared/closed-loop groups.
     * Local arrays remain the cargo owner until native migration materializes a chain.
     * Pending transitions never become permission for legacy fallback. */
    static ESFConveyorCohortStatus ValidateStructure(const FSFConveyorCohort& Cohort, FString& Reason);
    /** Bounds and unique segment claims only. No native getters, payload reads or conservation claim.
     * On rejection the slot output is empty. Oversized captures require a separate bounded adapter. */
    static ESFConveyorCohortStatus ValidateCargoLayout(const FSFConveyorCohort::FCargoLayout& Layout,
        TArray<FSFConveyorCohort::FCargoSlot>& Slots, FString& Reason);
    /** Immediate full native item values in conveyor-local coordinates. Dynamic
     * storage is independently copied and UObject references remain GC-visible. */
    static ESFConveyorCohortStatus SnapshotCargo(USFConstructionOperationCoordinator& Coordinator,
        const FSFConveyorCohort& Cohort, FSFConveyorCohort::FCargoSnapshot& Out, FString& Reason);
    /** Re-resolve the current native source immediately before a stock export/retirement.
     * No saved group/chain pointer is reinstalled, and a retiring receiver is never invoked. */
    static bool CanExportChain(USFConstructionOperationCoordinator& Coordinator, AFGConveyorChainActor* Chain,
        const TSet<FObjectKey>& OwnedMembers, FString& Reason);
    static bool CanRetire(USFConstructionOperationCoordinator& Coordinator, AFGBuildableConveyorBase* Conveyor,
        const TSet<FObjectKey>& OwnedMembers, FString& Reason);
    static bool VerifyBoundaryInventories(USFConstructionOperationCoordinator& Coordinator,
        const FSFConveyorCohort& Before, FString& Reason, const TSet<FObjectKey>* RetiredOwners = nullptr);
};
