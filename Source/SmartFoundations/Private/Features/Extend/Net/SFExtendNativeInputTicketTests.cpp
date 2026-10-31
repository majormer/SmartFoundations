// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/Net/SFExtendNativeInputTicket.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendNativeInputTicketTest, "SmartFoundations.Extend.NativeInputTicket",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendNativeInputTicketTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("A client preview's actor authority never grants world authority"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_Client, true, true, true));
    TestFalse(TEXT("Dedicated remote input retains its separate transport path"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_DedicatedServer, true, true, true));
    TestFalse(TEXT("A listen server's remote requester is not its local input"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_ListenServer, true, false, true));
    TestFalse(TEXT("A multistep advance does not enter the final construction queue"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_Standalone, true, true, false));
    TestTrue(TEXT("Standalone authority final input is eligible"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_Standalone, true, true, true));
    TestTrue(TEXT("Listen-host authority final input is eligible"), FSFExtendNativeInputTicket::CanOwnLocalInput(NM_ListenServer, true, true, true));
    const FGuid World = FGuid::NewGuid();
    TStrongObjectPtr<USceneComponent> ReceiverObject(NewObject<USceneComponent>()), RootObject(NewObject<USceneComponent>());
    TStrongObjectPtr<USceneComponent> ReplacementObject(NewObject<USceneComponent>());
    const FObjectKey Receiver(ReceiverObject.Get()), Root(RootObject.Get()), Replacement(ReplacementObject.Get());
    FSFExtendNativeInputTicket Ticket(World, Receiver, Root);
    TestFalse(TEXT("Unqueued native entry has no permission"), Ticket.Consume(World, Receiver, Root));
    TestFalse(TEXT("Old world cannot permit a queued input"), Ticket.Permit(FGuid::NewGuid(), Receiver, Root, true));
    TestFalse(TEXT("A new hologram generation cannot inherit the old click"), Ticket.Permit(World, Receiver, Replacement, true));
    TestFalse(TEXT("Changed value intent cancels before native entry"), Ticket.Permit(World, Receiver, Root, false));
    TestTrue(TEXT("Exact current callback can permit the named native method"), Ticket.Permit(World, Receiver, Root, true));
    TestFalse(TEXT("A different build-state receiver cannot consume permission"), Ticket.Consume(World, Replacement, Root));
    TestFalse(TEXT("A replaced root cannot consume permission"), Ticket.Consume(World, Receiver, Replacement));
    TestTrue(TEXT("Owner's explicit native re-entry consumes permission"), Ticket.Consume(World, Receiver, Root));
    TestFalse(TEXT("Reentrant/native duplicate cannot execute it twice"), Ticket.Consume(World, Receiver, Root));
    TestFalse(TEXT("Consumed input cannot be rearmed"), Ticket.Permit(World, Receiver, Root, true));
    TestTrue(TEXT("Consumption is recorded independently of construction success"), Ticket.WasConsumed());
    FSFExtendNativeInputTicket MissingReceiver(World, FObjectKey(), Root);
    TestFalse(TEXT("Missing receiver identity cannot authorize native entry"), MissingReceiver.Permit(World, FObjectKey(), Root, true));
    return true;
}
#endif
