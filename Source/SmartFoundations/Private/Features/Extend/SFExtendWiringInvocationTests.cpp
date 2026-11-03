// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendWiringInvocation.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendWiringInvocationTest, "SmartFoundations.Extend.NativeWiringInvocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendWiringInvocationTest::RunTest(const FString& Parameters)
{
    const FGuid World = FGuid::NewGuid();
    const FGuid StaleWorld = FGuid::NewGuid();
    TStrongObjectPtr<USceneComponent> RootObject(NewObject<USceneComponent>()), ChildObject(NewObject<USceneComponent>());
    TStrongObjectPtr<USceneComponent> ParentObject(NewObject<USceneComponent>()), ForeignObject(NewObject<USceneComponent>());
    const FObjectKey Root(RootObject.Get());
    const FObjectKey Child(ChildObject.Get());
    const FObjectKey Parent(ParentObject.Get());
    const FObjectKey ForeignParent(ForeignObject.Get());
    FNetConstructionID NativeId;
    NativeId.NetPlayerID = 1;
    NativeId.Client_ID = 65535;
    NativeId.Server_ID = 42;
    FSFExtendWiringInvocation Invocation(World, Root, NativeId);
    TSharedPtr<FSFCloneTopology> Preview = MakeShared<FSFCloneTopology>();
    Invocation.PreviewTopology = Preview;
    TestFalse(TEXT("Unconsumed preview cannot retire before wiring"), Invocation.CanRetirePreview(Preview));
    const FGuid Operation = Invocation.GetOperationId();
    TestFalse(TEXT("No spawn or guessed actor may wire before the root returns"), Invocation.TryClaim(Operation, World, Parent));
    TestFalse(TEXT("A child Construct return cannot bind the parent"), Invocation.BindParent(World, Child, NativeId, ForeignParent));
    TestFalse(TEXT("No returned actor is not a materialized parent"), Invocation.BindParent(World, Root, NativeId, FObjectKey()));
    TestFalse(TEXT("Retired world cannot bind an actor"), Invocation.BindParent(StaleWorld, Root, NativeId, Parent));
    FNetConstructionID AcknowledgedId = NativeId;
    AcknowledgedId.Server_ID = 43;
    TestTrue(TEXT("Exact root return binds even if native acknowledgement populated an ID field"), Invocation.BindParent(World, Root, AcknowledgedId, Parent));
    TestEqual(TEXT("Incoming native ID is retained separately"), Invocation.GetRequestedConstructionId().Server_ID, NativeId.Server_ID);
    TestTrue(TEXT("Repeated observation of the same return is harmless"), Invocation.BindParent(World, Root, AcknowledgedId, Parent));
    TestFalse(TEXT("A deferred callback from an earlier operation is rejected"), Invocation.TryClaim(FGuid::NewGuid(), World, Parent));
    TestFalse(TEXT("An earlier world callback is rejected"), Invocation.TryClaim(Operation, StaleWorld, Parent));
    TestFalse(TEXT("Another factory spawn cannot claim wiring"), Invocation.TryClaim(Operation, World, ForeignParent));
    TestTrue(TEXT("One synchronous or deferred path may invoke the writers"), Invocation.TryClaim(Operation, World, Parent));
    TestFalse(TEXT("Reentrant or next-tick duplicate cannot invoke the writers again"), Invocation.TryClaim(Operation, World, Parent));
    TestTrue(TEXT("Void writer return remains unverified"), Invocation.HasUnverifiedInvocation());
    TestTrue(TEXT("Consumed preview registries can retire without discarding operation evidence"), Invocation.CanRetirePreview(Preview));
    TestFalse(TEXT("Old invocation cannot clear a new preview of the same module"),
        Invocation.CanRetirePreview(MakeShared<FSFCloneTopology>(*Preview)));
    Preview.Reset();
    TestFalse(TEXT("Expired preview cannot authorize cleanup"), Invocation.CanRetirePreview(Preview));

    FSFExtendWiringInvocation ReusedNativeId(World, Root, NativeId);
    ReusedNativeId.BindParent(World, Root, NativeId, Parent);
    TestFalse(TEXT("Reused 16-bit native IDs do not authorize an old callback"),
        ReusedNativeId.TryClaim(Operation, World, Parent));
    TestTrue(TEXT("New invocation has its own claim despite repeated native counters"),
        ReusedNativeId.TryClaim(ReusedNativeId.GetOperationId(), World, Parent));

    FSFExtendWiringInvocation Contradictory(World, Root, NativeId);
    Contradictory.BindParent(World, Root, NativeId, Parent);
    TestFalse(TEXT("Root output cannot be rebound to a different actor"), Contradictory.BindParent(World, Root, NativeId, ForeignParent));
    TestFalse(TEXT("Contradictory outputs prevent even the original actor's wiring claim"),
        Contradictory.TryClaim(Contradictory.GetOperationId(), World, Parent));
    FSFExtendWiringInvocation ContradictoryIds(World, Root, NativeId);
    ContradictoryIds.BindParent(World, Root, NativeId, Parent);
    TestFalse(TEXT("Conflicting repeated root identities are rejected"), ContradictoryIds.BindParent(World, Root, AcknowledgedId, Parent));
    TestFalse(TEXT("An ambiguous root construction cannot wire"), ContradictoryIds.TryClaim(ContradictoryIds.GetOperationId(), World, Parent));
    FSFExtendWiringInvocation MissingWorld(FGuid(), Root, NativeId);
    TestFalse(TEXT("Missing authoritative generation cannot bind"), MissingWorld.BindParent(FGuid(), Root, NativeId, Parent));
    return true;
}
#endif
