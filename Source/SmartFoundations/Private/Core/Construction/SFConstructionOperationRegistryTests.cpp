// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Core/Construction/SFConstructionOperationRegistry.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionOperationRegistryTest, "SmartFoundations.Construction.OperationRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionOperationRegistryTest::RunTest(const FString& Parameters)
{
    const FGuid World = FGuid::NewGuid();
    TStrongObjectPtr<UObject> OwnerA(NewObject<USceneComponent>()), OwnerB(NewObject<USceneComponent>());
    TStrongObjectPtr<UObject> ActorA(NewObject<USceneComponent>()), ActorB(NewObject<USceneComponent>()), Replacement(NewObject<USceneComponent>());
    TStrongObjectPtr<UObject> Independent(NewObject<USceneComponent>());
    FSFConstructionOperationRegistry Registry(World, 2, 1);
    auto Request = [&](UObject* Owner, uint64 Sequence, UObject* Actor)
    {
        FSFConstructionOperationRegistry::FAdmission Result;
        Result.Key = {FObjectKey(Owner), Sequence};
        Result.RequestId = FGuid::NewGuid();
        Result.IntentDigest = TEXT("authoritatively-captured-intent");
        Result.Cohort.Add(FObjectKey(Actor));
        return Result;
    };
    auto First = Request(OwnerA.Get(), 1, ActorA.Get());
    TestTrue(TEXT("First coherent cohort admitted"), Registry.Admit(First) == ESFConstructionAdmission::Accepted);
    TestTrue(TEXT("Exact duplicate refers to same operation"), Registry.Admit(First) == ESFConstructionAdmission::Existing);
    auto Changed = First; Changed.IntentDigest = TEXT("different-intent");
    TestTrue(TEXT("Same identity cannot authorize changed intent"), Registry.Admit(Changed) == ESFConstructionAdmission::Invalid);
    Changed = First; Changed.Cohort.Add(FObjectKey(Independent.Get()));
    TestTrue(TEXT("Same identity cannot expand its original request on replay"), Registry.Admit(Changed) == ESFConstructionAdmission::Invalid);
    const auto Conflict = Request(OwnerB.Get(), 1, ActorA.Get());
    TestTrue(TEXT("Another requester cannot overlap the active cohort"), Registry.Admit(Conflict) == ESFConstructionAdmission::Conflict);
    TestTrue(TEXT("Rejected delivery remains an immutable terminal result"), Registry.Admit(Conflict) == ESFConstructionAdmission::Existing);
    TestNull(TEXT("Rejected delivery never receives an active effect record"), Registry.FindRecord(Conflict.Key));
    TestFalse(TEXT("Unsettled operation cannot release its leases"), Registry.ReleaseSettled(First.Key));
    TestTrue(TEXT("Actual replacement joins the same lease"), Registry.Adopt(First.Key, FObjectKey(Replacement.Get())));
    TestTrue(TEXT("Adoption does not rewrite original replay intent"), Registry.Admit(First) == ESFConstructionAdmission::Existing);
    TestTrue(TEXT("Replacement remains protected from another operation"), Registry.Admit(Request(OwnerB.Get(), 2, Replacement.Get())) == ESFConstructionAdmission::Conflict);

    auto Alias = First; Alias.Key = {FObjectKey(OwnerB.Get()), 3};
    TestTrue(TEXT("Request GUID cannot be reused under another operation key"), Registry.Admit(Alias) == ESFConstructionAdmission::Invalid);
    auto Second = Request(OwnerB.Get(), 3, ActorB.Get());
    TestTrue(TEXT("Independent cohort can proceed"), Registry.Admit(Second) == ESFConstructionAdmission::Accepted);
    const auto Busy = Request(OwnerA.Get(), 2, Independent.Get());
    TestTrue(TEXT("Capacity applies before any new effects"), Registry.Admit(Busy) == ESFConstructionAdmission::Busy);
    TestNull(TEXT("Capacity rejection has no active effect record"), Registry.FindRecord(Busy.Key));
    TestFalse(TEXT("One operation cannot adopt another's live actor"), Registry.Adopt(First.Key, FObjectKey(ActorB.Get())));
    FSFConstructionOperationRecord* Record = Registry.FindRecord(First.Key);
    if (!TestNotNull(TEXT("Accepted first operation retains its record"), Record)) return false;
    Record->BeginMutation(World);
    Record->ClaimEffect(World, FName(TEXT("Construct")));
    Record->ObserveEffect(World, FName(TEXT("Construct")), ESFConstructionEffectOutcome::Applied);
    Record->RequestCancellation(World);
    TestFalse(TEXT("Partial cancellation retains actor leases"), Registry.ReleaseSettled(First.Key));
    for (const auto Obligation : {ESFConstructionObligation::Topology, ESFConstructionObligation::NativeMembership,
        ESFConstructionObligation::Cargo, ESFConstructionObligation::DeferredWork, ESFConstructionObligation::Resources})
        Record->VerifyObligation(World, Obligation);
    Record->SettleCancellation(World);
    TestFalse(TEXT("Settled result cannot adopt more actors"), Registry.Adopt(First.Key, FObjectKey(ActorA.Get())));
    TestTrue(TEXT("Verified partial settlement releases its exact leases"), Registry.ReleaseSettled(First.Key));
    TestTrue(TEXT("Recent result remains a duplicate without rerunning work"), Registry.Admit(First) == ESFConstructionAdmission::Existing);
    TestTrue(TEXT("Partial result remains distinct from success"), Registry.GetState(First.Key).GetValue() == ESFConstructionOperationState::PartiallyApplied);

    Record = Registry.FindRecord(Second.Key);
    if (!TestNotNull(TEXT("Independent accepted operation retains its record"), Record)) return false;
    Record->RequestCancellation(World); Record->SettleCancellation(World);
    Registry.ReleaseSettled(Second.Key);
    TestTrue(TEXT("Evicting an old result cannot permit old execution"), Registry.Admit(First) == ESFConstructionAdmission::ReplayRejected);
    TestTrue(TEXT("Previously rejected sequence cannot become paid work after capacity frees"), Registry.Admit(Busy) == ESFConstructionAdmission::ReplayRejected);
    TestTrue(TEXT("Fresh sequence can reuse settled cohort"), Registry.Admit(Request(OwnerA.Get(), 3, ActorA.Get())) == ESFConstructionAdmission::Accepted);
    TestEqual(TEXT("Only fresh unsettled work is active"), Registry.GetActiveCount(), 1);

    // Client Upgrade and authority-native input counters are independent. They must
    // neither classify one another as replay nor bypass the common actor lease.
    FSFConstructionOperationRegistry Streams(World, 8, 1);
    auto Upgrade = Request(OwnerA.Get(), 500, ActorA.Get());
    TestTrue(TEXT("Upgrade's client sequence admitted"), Streams.Admit(Upgrade) == ESFConstructionAdmission::Accepted);
    auto Native = Request(OwnerA.Get(), 1, ActorB.Get());
    Native.Key.Stream = ESFConstructionRequestStream::AuthorityBuildInput;
    TestTrue(TEXT("Authority sequence one does not collide with client sequence five hundred"), Streams.Admit(Native) == ESFConstructionAdmission::Accepted);
    auto Overlap = Request(OwnerB.Get(), 1, ActorA.Get());
    Overlap.Key.Stream = ESFConstructionRequestStream::AuthorityBuildInput;
    TestTrue(TEXT("Stream separation never separates actor conflict ownership"), Streams.Admit(Overlap) == ESFConstructionAdmission::Conflict);
    TestTrue(TEXT("Replayed native input still finds the same record"), Streams.Admit(Native) == ESFConstructionAdmission::Existing);
    auto* NativeRecord = Streams.FindRecord(Native.Key);
    if (!TestNotNull(TEXT("Accepted native operation retains its record"), NativeRecord)) return false;
    NativeRecord->RequestCancellation(World); NativeRecord->SettleCancellation(World); Streams.ReleaseSettled(Native.Key);
    auto* UpgradeRecord = Streams.FindRecord(Upgrade.Key);
    if (!TestNotNull(TEXT("Accepted Upgrade operation retains its record"), UpgradeRecord)) return false;
    UpgradeRecord->RequestCancellation(World); UpgradeRecord->SettleCancellation(World); Streams.ReleaseSettled(Upgrade.Key);
    TestTrue(TEXT("Evicted native result retains its own high water mark"), Streams.Admit(Native) == ESFConstructionAdmission::ReplayRejected);
    Native = Request(OwnerA.Get(), 2, ActorB.Get()); Native.Key.Stream = ESFConstructionRequestStream::AuthorityBuildInput;
    TestTrue(TEXT("Fresh native input remains valid below the unrelated client counter"), Streams.Admit(Native) == ESFConstructionAdmission::Accepted);
    return true;
}
#endif
