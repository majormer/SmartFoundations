// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionOperationRecordTest, "SmartFoundations.Construction.OperationRecord",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionOperationRecordTest::RunTest(const FString& Parameters)
{
    const FGuid World = FGuid::NewGuid();
    const FGuid StaleWorld = FGuid::NewGuid();
    const FName Construct(TEXT("Construct:parent"));
    const FName Wiring(TEXT("Wire:parent"));
    auto VerifyNative = [&](FSFConstructionOperationRecord& Record)
    {
        for (const auto Obligation : {ESFConstructionObligation::Topology, ESFConstructionObligation::NativeMembership,
            ESFConstructionObligation::Cargo, ESFConstructionObligation::DeferredWork})
            TestTrue(TEXT("Observed native obligation can settle"), Record.VerifyObligation(World, Obligation));
    };

    FSFConstructionOperationRecord Record(FGuid::NewGuid(), World);
    TestFalse(TEXT("Old world cannot begin work"), Record.BeginMutation(StaleWorld));
    TestTrue(TEXT("Current world begins work"), Record.BeginMutation(World));
    TestTrue(TEXT("Claim construction before invoking it"), Record.ClaimEffect(World, Construct));
    TestFalse(TEXT("Duplicate callback cannot invoke same construction again"), Record.ClaimEffect(World, Construct));
    TestTrue(TEXT("Observed construction effect retained"), Record.ObserveEffect(World, Construct, ESFConstructionEffectOutcome::Applied));
    TestFalse(TEXT("Known applied effect cannot be rewritten as no effect"), Record.ObserveEffect(World, Construct, ESFConstructionEffectOutcome::NoEffect));
    TestTrue(TEXT("Deferred wiring obligation claimed"), Record.ClaimEffect(World, Wiring));
    TestTrue(TEXT("All mutation attempts now enumerated"), Record.SealMutation(World));
    TestFalse(TEXT("Pending callback prevents premature native proof"), Record.VerifyObligation(World, ESFConstructionObligation::Topology));
    TestFalse(TEXT("Returned construct is not operation completion"), Record.Complete(World));
    TestFalse(TEXT("Late callback from old world cannot settle wiring"), Record.ObserveEffect(StaleWorld, Wiring, ESFConstructionEffectOutcome::Applied));
    TestTrue(TEXT("Exact current callback settles wiring"), Record.ObserveEffect(World, Wiring, ESFConstructionEffectOutcome::Applied));
    TestFalse(TEXT("Material proof cannot hide missing native settlement"), Record.VerifyObligation(World, ESFConstructionObligation::Resources));
    VerifyNative(Record);
    TestFalse(TEXT("Native settlement alone does not prove payment"), Record.Complete(World));
    TestTrue(TEXT("Observed resources settle last obligation"), Record.VerifyObligation(World, ESFConstructionObligation::Resources));
    TestTrue(TEXT("All effects and obligations settled"), Record.Complete(World));
    TestFalse(TEXT("Completed operation rejects late mutation"), Record.ClaimEffect(World, FName(TEXT("late"))));
    TestFalse(TEXT("Completed result cannot become cancellation"), Record.RequestCancellation(World));

    FSFConstructionOperationRecord Partial(FGuid::NewGuid(), World);
    Partial.BeginMutation(World); Partial.ClaimEffect(World, Construct);
    Partial.ObserveEffect(World, Construct, ESFConstructionEffectOutcome::Applied);
    Partial.RequestCancellation(World);
    TestTrue(TEXT("Cancellation after effects reports partial"), Partial.GetState() == ESFConstructionOperationState::PartiallyApplied);
    TestFalse(TEXT("Partial effects cannot disappear by canceling"), Partial.SettleCancellation(World));
    TestFalse(TEXT("Canceled request cannot start more effects"), Partial.ClaimEffect(World, Wiring));
    VerifyNative(Partial);
    Partial.VerifyObligation(World, ESFConstructionObligation::Resources);
    TestTrue(TEXT("Verified partial outcome can settle without claiming success"), Partial.SettleCancellation(World));
    TestTrue(TEXT("Settled partial stays partial"), Partial.IsSettled() && Partial.GetState() == ESFConstructionOperationState::PartiallyApplied);

    FSFConstructionOperationRecord Uncertain(FGuid::NewGuid(), World);
    Uncertain.BeginMutation(World); Uncertain.ClaimEffect(World, Construct); Uncertain.RequestCancellation(World);
    TestTrue(TEXT("In-flight cancellation is uncertain"), Uncertain.GetState() == ESFConstructionOperationState::Uncertain);
    TestFalse(TEXT("Unknown invocation cannot be discarded"), Uncertain.SettleCancellation(World));
    Uncertain.ObserveEffect(World, Construct, ESFConstructionEffectOutcome::Uncertain);
    TestFalse(TEXT("Explicit uncertainty is still outstanding"), Uncertain.VerifyObligation(World, ESFConstructionObligation::Resources));
    TestTrue(TEXT("Later evidence can establish no effect"), Uncertain.ObserveEffect(World, Construct, ESFConstructionEffectOutcome::NoEffect));
    TestTrue(TEXT("Verified no-effect cancellation can settle"), Uncertain.SettleCancellation(World));
    TestTrue(TEXT("No-effect cancellation distinguished"), Uncertain.GetState() == ESFConstructionOperationState::CancelledBeforeEffects);
    FSFConstructionOperationRecord Invalid(FGuid(), World);
    TestFalse(TEXT("Missing request identity cannot mutate"), Invalid.BeginMutation(World));
    FSFConstructionOperationRecord Deferred(FGuid::NewGuid(), World);
    TestFalse(TEXT("Unclaimed effects cannot queue a continuation"), Deferred.CanContinueEffect(World, Wiring));
    Deferred.BeginMutation(World); Deferred.ClaimEffect(World, Wiring); Deferred.SealMutation(World);
    TestTrue(TEXT("Sealed admission can continue its still-pending claimed effect"), Deferred.CanContinueEffect(World, Wiring));
    TestFalse(TEXT("An old world cannot continue a current pending effect"), Deferred.CanContinueEffect(StaleWorld, Wiring));
    TestFalse(TEXT("A continuation cannot introduce an unclaimed effect"), Deferred.CanContinueEffect(World, Construct));
    Deferred.ObserveEffect(World, Wiring, ESFConstructionEffectOutcome::Uncertain);
    TestFalse(TEXT("Uncertain native invocation must not be automatically retried"), Deferred.CanContinueEffect(World, Wiring));
    FSFConstructionOperationRecord CanceledDeferred(FGuid::NewGuid(), World);
    CanceledDeferred.BeginMutation(World); CanceledDeferred.ClaimEffect(World, Wiring); CanceledDeferred.SealMutation(World);
    CanceledDeferred.RequestCancellation(World);
    TestFalse(TEXT("Cancellation excludes a queued not-yet-invoked effect"), CanceledDeferred.CanContinueEffect(World, Wiring));
    TestTrue(TEXT("Cancellation remains visible to the observer"), CanceledDeferred.IsCancellationRequested());
    CanceledDeferred.ObserveEffect(World, Wiring, ESFConstructionEffectOutcome::NoEffect);
    TestTrue(TEXT("Canceled queued work settles only after its no-effect outcome"), CanceledDeferred.SettleCancellation(World));
    TestFalse(TEXT("A settled queued effect cannot resume"), CanceledDeferred.CanContinueEffect(World, Wiring));
    return true;
}
#endif
