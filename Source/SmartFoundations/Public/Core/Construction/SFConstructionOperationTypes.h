// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class AActor;
class APlayerController;

enum class ESFConstructionOperationState : uint8
{
    Admitted, Materializing, AwaitingNativeSettlement, SettlingResources,
    Completed, CancelledBeforeEffects, PartiallyApplied, Uncertain
};

enum class ESFConstructionEffectOutcome : uint8 { Pending, NoEffect, Applied, Uncertain };

enum class ESFConstructionObligation : uint8
{
    Topology, NativeMembership, Cargo, DeferredWork, Resources, Count
};

/** Different producers cannot assume their client/authority counters share a stream. */
enum class ESFConstructionRequestStream : uint8 { Upgrade, AuthorityBuildInput, RemoteExtend, ConveyorLinks, Count };

/** Sequence is monotonic within one requester/world/producer stream. Leases remain shared. */
struct FSFConstructionOperationKey
{
    FObjectKey Requester;
    uint64 Sequence = 0;
    ESFConstructionRequestStream Stream = ESFConstructionRequestStream::Upgrade;
    bool operator==(const FSFConstructionOperationKey& Other) const
    { return Requester == Other.Requester && Sequence == Other.Sequence && Stream == Other.Stream; }
    friend uint32 GetTypeHash(const FSFConstructionOperationKey& Key)
    { return HashCombine(HashCombine(GetTypeHash(Key.Requester), GetTypeHash(Key.Sequence)), GetTypeHash(static_cast<uint8>(Key.Stream))); }
};

enum class ESFConstructionAdmission : uint8
{
    Accepted,
    Existing,
    Invalid,
    ReplayRejected,
    Conflict,
    Busy,
    Unavailable
};

/** Internal authoritative intent, not a client-supplied authorization or a network payload. */
struct FSFConstructionOperationRequest
{
    APlayerController* Requester = nullptr;
    uint64 Sequence = 0;
    ESFConstructionRequestStream Stream = ESFConstructionRequestStream::Upgrade;
    FGuid RequestId;
    FGuid WorldGeneration;
    FString IntentDigest;
    TSet<FObjectKey> ExpectedCohort;
    TArray<TWeakObjectPtr<AActor>> AffectedActors;
};
