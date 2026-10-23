// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Dismantle/Net/SFBlueprintGroupRegistrationPolicy.h"
#include "CoreMinimal.h"
#include "Features/Dismantle/Net/SFBlueprintGroupReplication.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGBlueprintProxy.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintGroupRegistrationTest,
	"SmartFoundations.Dismantle.ReplicatedGroupRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintGroupRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace SFBlueprintGroupRegistrationPolicy;
	FContext Race;
	Race.bClientWorld = Race.bReplicatedProxy = Race.bReplicatedBuildable
		= Race.bMatchesAssignedProxy = Race.bHasUnresolvedMembers = true;
	TestTrue(TEXT("Absent replicated member waits for the authoritative slot"), ShouldDefer(Race));
	for (int32 Gate = 0; Gate < 8; ++Gate)
	{
		FContext Control = Race;
		switch (Gate)
		{
		case 0: Control.bClientWorld = false; break;
		case 1: Control.bProxyAuthority = true; break;
		case 2: Control.bBuildableAuthority = true; break;
		case 3: Control.bReplicatedProxy = false; break;
		case 4: Control.bReplicatedBuildable = false; break;
		case 5: Control.bMatchesAssignedProxy = false; break;
		case 6: Control.bHasUnresolvedMembers = false; break;
		case 7: Control.bAlreadyRegistered = true; break;
		}
		TestFalse(FString::Printf(TEXT("Native registration retained for control %d"), Gate), ShouldDefer(Control));
	}
	// Replay the measured ordering with scalar identities: four resolved actors,
	// five pending references, then registration followed by reference resolution.
	// This tests the policy/array race, not the SDK's stubbed native RegisterBuildable.
	TArray<int32> Native{101, 102, 103, 104, 0, 0, 0, 0, 0};
	TArray<int32> Guarded = Native;
	int32 Deferred = 0;
	for (int32 Identity = 105; Identity <= 109; ++Identity)
	{
		Native.AddUnique(Identity);
		FContext Context = Race;
		Context.bHasUnresolvedMembers = Guarded.Contains(0);
		Context.bAlreadyRegistered = Guarded.Contains(Identity);
		if (ShouldDefer(Context)) ++Deferred; else Guarded.AddUnique(Identity);
	}
	for (int32 Slot = 4; Slot < 9; ++Slot) Native[Slot] = Guarded[Slot] = 101 + Slot;
	TSet<int32> NativeUnique;
	TSet<int32> GuardedUnique;
	for (int32 Identity : Native) NativeUnique.Add(Identity);
	for (int32 Identity : Guarded) GuardedUnique.Add(Identity);
	TestEqual(TEXT("Unguarded AddUnique reproduces fourteen slots"), Native.Num(), 14);
	TestEqual(TEXT("Unguarded final list has only nine identities"), NativeUnique.Num(), 9);
	TestEqual(TEXT("Five appends deferred without editing replicated slots"), Deferred, 5);
	TestEqual(TEXT("Guarded final list retains nine slots"), Guarded.Num(), 9);
	TestEqual(TEXT("All nine authoritative members resolve"), GuardedUnique.Num(), 9);
	FContext Resolved = Race;
	Resolved.bHasUnresolvedMembers = false;
	Resolved.bAlreadyRegistered = true;
	TestFalse(TEXT("Resolved repeated registration retains native behavior"), ShouldDefer(Resolved));
	Guarded.AddUnique(105);
	TestEqual(TEXT("Resolved AddUnique remains idempotent"), Guarded.Num(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintReassignedMemberTest,
	"SmartFoundations.Dismantle.BlueprintReassignedMembers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintReassignedMemberTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Preview world"), World)) return false;
	AFGBlueprintProxy* Outer = World->SpawnActor<AFGBlueprintProxy>();
	AFGBlueprintProxy* Copy = World->SpawnActor<AFGBlueprintProxy>();
	AFGBlueprintProxy* Pending = World->SpawnActor<AFGBlueprintProxy>();
	AFGBuildableConveyorBelt* Seam = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* Moved = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* Unconfirmed = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* Unowned = World->SpawnActor<AFGBuildableConveyorBelt>();
	FArrayProperty* Members = FindFProperty<FArrayProperty>(AFGBlueprintProxy::StaticClass(), TEXT("mBuildables"));
	FObjectPropertyBase* MemberType = Members ? CastField<FObjectPropertyBase>(Members->Inner) : nullptr;
	if (!TestNotNull(TEXT("Native member array"), MemberType)) { World->DestroyWorld(false); return false; }
	auto SetMembers = [Members, MemberType](AFGBlueprintProxy* Proxy, const TArray<AFGBuildable*>& Values)
	{
		FScriptArrayHelper Array(Members, Members->ContainerPtrToValuePtr<void>(Proxy));
		Array.EmptyValues();
		for (AFGBuildable* Value : Values) MemberType->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Value);
	};
	Seam->SetBlueprintProxy(Outer);
	Moved->SetBlueprintProxy(Copy);
	Unconfirmed->SetBlueprintProxy(Pending);
	SetMembers(Outer, { Seam, Moved, Unconfirmed, Unowned, nullptr });
	SetMembers(Copy, { Moved });
	const TArray<AFGBuildable*> Reassigned = FSFBlueprintGroupReplication::FindReassignedMembers(Outer);
	TestEqual(TEXT("Only confirmed reassignment is selected"), Reassigned.Num(), 1);
	TestTrue(TEXT("Confirmed member belongs to the native copy"), Reassigned.Contains(Moved));
	TestFalse(TEXT("Own seam retained"), Reassigned.Contains(Seam));
	TestFalse(TEXT("Unconfirmed owner is not sufficient"), Reassigned.Contains(Unconfirmed));
	TestFalse(TEXT("Unowned member is not discarded"), Reassigned.Contains(Unowned));
	TestEqual(TEXT("Collector leaves native membership untouched"), Outer->GetBuildables().Num(), 5);
	TestEqual(TEXT("Native copy is not changed"), Copy->GetBuildables().Num(), 1);
	TestTrue(TEXT("Native assignment is unchanged"), Moved->GetBlueprintProxy() == Copy);
	TestTrue(TEXT("Null proxy produces no candidates"), FSFBlueprintGroupReplication::FindReassignedMembers(nullptr).IsEmpty());
	// Register/Unregister are SDK stubs. Actual removal and persistence are tested in Shipping.
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintSeamGroupTest,
	"SmartFoundations.Dismantle.BlueprintSeamGroupOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintSeamGroupTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Preview world"), World)) return false;
	AFGBlueprintProxy* Copy = World->SpawnActor<AFGBlueprintProxy>();
	AFGBuildableConveyorBelt* NativeMember = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* Seam = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* Control = World->SpawnActor<AFGBuildableConveyorBelt>();
	NativeMember->SetBlueprintProxy(Copy);
	AFGBlueprintProxy* Group = FSFBlueprintGroupReplication::GroupConstructedSeams({NativeMember, Seam, Seam, nullptr});
	TestNotNull(TEXT("Unowned operation seam receives a group"), Group);
	TestTrue(TEXT("Seam assigned to its own group"), Seam->GetBlueprintProxy() == Group && Group != Copy);
	TestTrue(TEXT("Native copy ownership preserved"), NativeMember->GetBlueprintProxy() == Copy);
	TestNull(TEXT("Unrelated world member untouched"), Control->GetBlueprintProxy());
	TestNull(TEXT("Already-owned operation creates no replacement group"),
		FSFBlueprintGroupReplication::GroupConstructedSeams({NativeMember, Seam}));
	TestTrue(TEXT("Repeated operation keeps seam ownership"), Seam->GetBlueprintProxy() == Group);
	// SDK RegisterBuildable is a stub. Member-list replication and lifecycle are Shipping checks.
	World->DestroyWorld(false);
	return true;
}
#endif
