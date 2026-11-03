// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Core/Construction/SFConstructionOperationTypes.h"
#include "Core/Construction/SFConveyorCohort.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "UObject/StrongObjectPtr.h"

class AFGPlayerController;
class AFGBuildableConveyorBase;
class UFGFactoryConnectionComponent;
class UFGItemDescriptor;

/** Retained feature intent/results, distinct from the shared native conflict/effect registry. */
struct FSFConveyorUpgradeOwner
{
    struct FRequest
    {
        FSFUpgradeExecutionParams Intent;
        FSFUpgradeExecutionResult Result;
        bool bTerminal = false;
    };
    struct FEdge
    {
        FObjectKey Owner;
        FName Port;
        TOptional<FObjectKey> PeerOwner;
        FName PeerPort;
        TWeakObjectPtr<UFGFactoryConnectionComponent> OriginalPort;
        TWeakObjectPtr<UFGFactoryConnectionComponent> OriginalPeer;
        FVector Position = FVector::ZeroVector;
        FVector PeerPosition = FVector::ZeroVector;
        uint8 Direction = 0;
        uint8 PeerDirection = 0;
    };
    struct FLiftHole
    {
        TWeakObjectPtr<class AFGBuildablePassthrough> Hole;
        FTransform Pose;
        float Thickness = 0;
        FEdge Bottom;
        FEdge Top;
        bool bBottom = false;
        bool bTop = false;
    };
    struct FLiftState
    {
        FTransform Top;
        bool bReversed = false;
        TArray<FLiftHole> Slots;
    };
    struct FCompanion
    {
        FObjectKey Identity;
        TWeakObjectPtr<AFGBuildable> Actor;
        TWeakObjectPtr<UClass> Class;
        FTransform Pose;
        TMap<TSubclassOf<UFGItemDescriptor>, int32> Materials;
        TArray<FSFInventoryStackSnapshot> Refund;
        TMap<FObjectKey, FSFConveyorCohort::FBoundaryInventory> Inventories;
        TSharedPtr<TStrongObjectPtr<AFGBuildable>> RetirementPin;
        TOptional<float> MonitorOffset;
        TWeakObjectPtr<class AFGBuildableBlueprintDesigner> Designer;
        TWeakObjectPtr<class AFGBlueprintProxy> Group;
    };
    struct FCompanionPlan
    {
        TArray<FCompanion> Old;
        TArray<FCompanion> Expected;
        TArray<FCompanion> Actual;
        /** Existing native floor/wall holes and spline monitors are transferred,
         * never treated as new outputs or eligible dismantle refunds. */
        TArray<FCompanion> Retained;
        TSet<FObjectKey> Retiring;
    };
    struct FOperation
    {
        FSFConstructionOperationKey Key;
        FGuid WorldGeneration;
        TWeakObjectPtr<AFGPlayerController> Requester;
        TWeakObjectPtr<AActor> Payer;
        TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Selected;
        TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Remaining;
        TArray<FEdge> Edges;
        TMap<FObjectKey, FLiftState> Lifts;
        TMap<FObjectKey, TWeakObjectPtr<AFGBuildableConveyorBase>> Replacements;
        FSFConveyorCohort Before;
        FSFUpgradeExecutionResult Result;
        bool bCargoVerified = false;
        bool bResourcesVerified = false;
        bool bUncertain = false;
        bool bExecuting = false;
        bool bReportedUncertain = false;
        bool bEffectsStarted = false;
        bool bFreeBuild = false;
        int32 ObservedNativePhases = 0;
        TSet<FObjectKey> OwnedMembers;
        FSFConveyorCohort::FCargoSnapshot BeforeCargo;
        TMap<TSubclassOf<UFGItemDescriptor>, int64> MaterialsBefore;
        /** Actual current-target debit, refundable only before any target output exists. */
        TMap<TSubclassOf<UFGItemDescriptor>, int32> ReservedCharges;
        TMap<FObjectKey, TWeakObjectPtr<class AFGBuildableBlueprintDesigner>> OriginalDesigners;
        TMap<FObjectKey, FCompanionPlan> Companions;
        TMap<FObjectKey, FObjectKey> CompanionClaims;
        TSet<FObjectKey> RetiredCompanions;
        TSet<FObjectKey> RetiredSources;
        TArray<FSFInventoryStackSnapshot> CompanionRefunds;
        TArray<FSFInventoryStackSnapshot> CompanionOverflowRefunds;
        TMap<FObjectKey, TSharedPtr<FSFInventoryItemSnapshot>> LegacyRefundConversions;
        TArray<TWeakObjectPtr<class AFGCrate>> CompanionRefundCrates;
        int32 DeliveredCompanionRefunds = 0;
        bool bCompanionRefundDeliveryFailed = false;
        TWeakObjectPtr<class AFGCrate> OverflowCrate;
        TArray<TWeakObjectPtr<AActor>> UnexpectedOutputs;
        TMap<FObjectKey, TWeakObjectPtr<class AFGBlueprintProxy>> OriginalGroups;
        class FSFConstructionOperationRecord* ExecutingRecord = nullptr; // Only during this owned callback.
    };

    TMap<FSFConstructionOperationKey, FRequest> Requests;
    TMap<FObjectKey, uint64> HighWatermarks;
    TArray<FSFConstructionOperationKey> Recent;
    TSharedPtr<FOperation> Active;

    TOptional<ESFConstructionAdmission> ClassifyReplay(const FSFConstructionOperationKey& Key,
        const FSFUpgradeExecutionParams& Params) const
    {
        if (const auto* Prior = Requests.Find(Key))
            return SameIntent(Prior->Intent, Params) ? ESFConstructionAdmission::Existing : ESFConstructionAdmission::Invalid;
        const uint64* Last = HighWatermarks.Find(Key.Requester);
        if (Last && Key.Sequence <= *Last) return ESFConstructionAdmission::ReplayRejected;
        return {};
    }

    /** A native ownership closure must never enlarge an explicit replacement list. */
    static bool SelectionWithinIntent(const FSFUpgradeExecutionParams& Intent,
        const TArray<TWeakObjectPtr<AFGBuildableConveyorBase>>& Selected)
    {
        if (Selected.IsEmpty() || !Intent.HasValidSpecificSelection()) return false;
        TSet<FObjectKey> Declared;
        if (Intent.HasSpecificBuildables())
            for (const auto& Weak : Intent.SpecificBuildables)
            {
                auto* Actor = Weak.Get();
                if (!IsValid(Actor)) return false;
                Declared.Add(FObjectKey(Actor));
            }
        TSet<FObjectKey> Seen;
        for (const auto& Weak : Selected)
        {
            auto* Actor = Weak.Get();
            if (!IsValid(Actor)) return false;
            const FObjectKey Key(Actor);
            if (Seen.Contains(Key) || (Intent.HasSpecificBuildables() && !Declared.Contains(Key))) return false;
            Seen.Add(Key);
        }
        return true;
    }

    static bool SameIntent(const FSFUpgradeExecutionParams& A, const FSFUpgradeExecutionParams& B)
    {
        if (A.RequestId != B.RequestId || A.RequestSequence != B.RequestSequence
            || A.AuthorityWorldGeneration != B.AuthorityWorldGeneration || A.Family != B.Family
            || A.SourceTier != B.SourceTier || A.TargetTier != B.TargetTier || A.Origin != B.Origin
            || A.Radius != B.Radius || A.MaxItems != B.MaxItems
            || A.bPreserveDismantleGroupMembership != B.bPreserveDismantleGroupMembership
            || A.bUseSpecificBuildables != B.bUseSpecificBuildables || A.SpecificBuildables.Num() != B.SpecificBuildables.Num()) return false;
        for (int32 Index = 0; Index < A.SpecificBuildables.Num(); ++Index)
            if (!A.SpecificBuildables[Index].HasSameIndexAndSerialNumber(B.SpecificBuildables[Index])) return false;
        return true;
    }

    void Remember(const FSFConstructionOperationKey& Key, const FSFUpgradeExecutionParams& Params)
    {
        FRequest Request;
        Request.Intent = Params;
        Request.Intent.PlayerController = nullptr; // The weak requester/key owns its generation, not an untraced UObject reference.
        Request.Result.RequestId = Params.RequestId;
        Request.Result.RequestSequence = Params.RequestSequence;
        Request.Result.AuthorityWorldGeneration = Params.AuthorityWorldGeneration;
        Request.Result.bPending = true;
        Requests.Add(Key, MoveTemp(Request));
        HighWatermarks.Add(Key.Requester, Key.Sequence);
    }

    void Settle(const FSFConstructionOperationKey& Key, const FSFUpgradeExecutionResult& Result)
    {
        auto* Request = Requests.Find(Key);
        if (!Request || Request->bTerminal) return;
        Request->Result = Result; Request->bTerminal = true;
        Recent.Add(Key);
        while (Recent.Num() > 16)
        {
            Requests.Remove(Recent[0]); Recent.RemoveAt(0);
        }
    }
};
