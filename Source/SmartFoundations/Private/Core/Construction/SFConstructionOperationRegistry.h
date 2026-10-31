// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "Core/Construction/SFConstructionOperationTypes.h"
#include "Core/Construction/SFConstructionOperationRecord.h"

/** Pure ownership backend. Native authority, cohort expansion and callback dispatch belong to the coordinator. */
class FSFConstructionOperationRegistry
{
public:
    struct FAdmission
    {
        FSFConstructionOperationKey Key;
        FGuid RequestId;
        FString IntentDigest;
        TSet<FObjectKey> Cohort;
    };

    explicit FSFConstructionOperationRegistry(const FGuid& InWorldGeneration, int32 InMaxActive = 64, int32 InRecentLimit = 16)
        : WorldGeneration(InWorldGeneration), MaxActive(FMath::Max(1, InMaxActive)), RecentLimit(FMath::Max(1, InRecentLimit)) {}

    ESFConstructionAdmission Admit(const FAdmission& Request)
    {
        if (!WorldGeneration.IsValid() || Request.Key.Sequence == 0 || Request.Key.Requester == FObjectKey()
            || static_cast<uint8>(Request.Key.Stream) >= static_cast<uint8>(ESFConstructionRequestStream::Count)
            || !Request.RequestId.IsValid() || Request.IntentDigest.IsEmpty() || Request.Cohort.IsEmpty())
            return ESFConstructionAdmission::Invalid;
        if (const auto Replay = ClassifyReplay(Request)) return Replay.GetValue();
        for (const FObjectKey& Actor : Request.Cohort)
            if (Leases.Contains(Actor)) return RememberRejected(Request, ESFConstructionAdmission::Conflict);
        if (ActiveCount >= MaxActive) return RememberRejected(Request, ESFConstructionAdmission::Busy);

        auto Entry = MakeUnique<FEntry>();
        Entry->Admission = Request;
        Entry->LeasedCohort = Request.Cohort;
        Entry->Record = MakeUnique<FSFConstructionOperationRecord>(Request.RequestId, WorldGeneration);
        for (const FObjectKey& Actor : Request.Cohort) Leases.Add(Actor, Request.Key);
        Entries.Add(Request.Key, MoveTemp(Entry));
        HighWatermarks.FindOrAdd(Request.Key.Requester).Add(Request.Key.Stream, Request.Key.Sequence);
        RequestIdentities.Add(Request.RequestId, Request.Key);
        ++ActiveCount;
        return ESFConstructionAdmission::Accepted;
    }

    /** Replays compare original immutable intent even after old actors have retired. */
    TOptional<ESFConstructionAdmission> ClassifyReplay(const FAdmission& Request) const
    {
        if (const auto* PriorKey = RequestIdentities.Find(Request.RequestId); PriorKey && !(*PriorKey == Request.Key))
            return ESFConstructionAdmission::Invalid;
        if (const TUniquePtr<FEntry>* Existing = Entries.Find(Request.Key))
        {
            const FAdmission& Prior = (*Existing)->Admission;
            bool bSame = Prior.RequestId == Request.RequestId && Prior.IntentDigest == Request.IntentDigest
                && Prior.Cohort.Num() == Request.Cohort.Num();
            for (const FObjectKey& Actor : Request.Cohort) bSame &= Prior.Cohort.Contains(Actor);
            return bSame ? ESFConstructionAdmission::Existing : ESFConstructionAdmission::Invalid;
        }
        const auto* Streams = HighWatermarks.Find(Request.Key.Requester);
        const uint64* LastSequence = Streams ? Streams->Find(Request.Key.Stream) : nullptr;
        if (LastSequence && Request.Key.Sequence <= *LastSequence) return ESFConstructionAdmission::ReplayRejected;
        return {};
    }

    FSFConstructionOperationRecord* FindRecord(const FSFConstructionOperationKey& Key)
    {
        TUniquePtr<FEntry>* Entry = Entries.Find(Key);
        return Entry ? (*Entry)->Record.Get() : nullptr;
    }

    TOptional<ESFConstructionOperationState> GetState(const FSFConstructionOperationKey& Key) const
    {
        const TUniquePtr<FEntry>* Entry = Entries.Find(Key);
        if (!Entry) return {};
        return (*Entry)->Record ? (*Entry)->Record->GetState() : (*Entry)->SettledState;
    }

    bool IsActive(const FSFConstructionOperationKey& Key) const
    {
        const TUniquePtr<FEntry>* Entry = Entries.Find(Key);
        return Entry && (*Entry)->Record.IsValid();
    }

    /** Returned actor identity joins this operation's lease before dependent work uses it. */
    bool Adopt(const FSFConstructionOperationKey& Key, const FObjectKey& Actor)
    {
        TUniquePtr<FEntry>* Entry = Entries.Find(Key);
        if (!Entry || !(*Entry)->Record || (*Entry)->Record->IsSettled() || Actor == FObjectKey()) return false;
        const FSFConstructionOperationKey* Lease = Leases.Find(Actor);
        if (Lease && !(*Lease == Key)) return false;
        (*Entry)->LeasedCohort.Add(Actor);
        Leases.Add(Actor, Key);
        return true;
    }

    /** No release while pending/partial obligations remain. Recent metadata is bounded; replay protection is not. */
    bool ReleaseSettled(const FSFConstructionOperationKey& Key)
    {
        TUniquePtr<FEntry>* Entry = Entries.Find(Key);
        if (!Entry || !(*Entry)->Record || !(*Entry)->Record->IsSettled()) return false;
        (*Entry)->SettledState = (*Entry)->Record->GetState();
        for (const FObjectKey& Actor : (*Entry)->LeasedCohort)
        {
            const FSFConstructionOperationKey* Lease = Leases.Find(Actor);
            if (Lease && *Lease == Key) Leases.Remove(Actor);
        }
        (*Entry)->Record.Reset();
        (*Entry)->LeasedCohort.Empty();
        --ActiveCount;
        Recent.Add(Key);
        while (Recent.Num() > RecentLimit)
        {
            Entries.Remove(Recent[0]);
            Recent.RemoveAt(0);
        }
        return true;
    }

    int32 GetActiveCount() const { return ActiveCount; }
    const FGuid& GetWorldGeneration() const { return WorldGeneration; }

private:
    struct FEntry
    {
        FAdmission Admission;
        TSet<FObjectKey> LeasedCohort;
        TUniquePtr<FSFConstructionOperationRecord> Record;
        ESFConstructionOperationState SettledState = ESFConstructionOperationState::Admitted;
    };
    ESFConstructionAdmission RememberRejected(const FAdmission& Request, ESFConstructionAdmission Outcome)
    {
        // Rejection is a terminal disposition of this exact intent, not a queue.
        // A delayed duplicate must not become a paid construction merely because
        // the conflicting operation finished after the original rejection ACK.
        auto Entry = MakeUnique<FEntry>();
        Entry->Admission = Request;
        Entry->SettledState = ESFConstructionOperationState::CancelledBeforeEffects;
        Entries.Add(Request.Key, MoveTemp(Entry));
        RequestIdentities.Add(Request.RequestId, Request.Key);
        HighWatermarks.FindOrAdd(Request.Key.Requester).Add(Request.Key.Stream, Request.Key.Sequence);
        Recent.Add(Request.Key);
        while (Recent.Num() > RecentLimit)
        {
            Entries.Remove(Recent[0]);
            Recent.RemoveAt(0);
        }
        return Outcome;
    }
    FGuid WorldGeneration;
    int32 MaxActive;
    int32 RecentLimit;
    int32 ActiveCount = 0;
    TMap<FSFConstructionOperationKey, TUniquePtr<FEntry>> Entries;
    TMap<FObjectKey, FSFConstructionOperationKey> Leases;
    TMap<FObjectKey, TMap<ESFConstructionRequestStream, uint64>> HighWatermarks;
    TMap<FGuid, FSFConstructionOperationKey> RequestIdentities;
    TArray<FSFConstructionOperationKey> Recent;
};
