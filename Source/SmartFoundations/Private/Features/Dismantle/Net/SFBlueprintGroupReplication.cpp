// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Dismantle/Net/SFBlueprintGroupReplication.h"
#include "SmartFoundations.h"
#include "Features/Dismantle/Net/SFBlueprintGroupRegistrationPolicy.h"
#include "Features/Dismantle/Net/SFPipelineIndicatorGrouping.h"
#include "Engine/World.h"
#include "FGBlueprintProxy.h"
#include "Buildables/FGBuildable.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — Blueprint group registration
//  1. Authority/local actors retain native registration and group reconstruction.
//  2. [MP-REPL] A replicated client list can contain unresolved actor references.
//     Defer an absent replicated member's local AddUnique until those references resolve;
//     the server's replicated array supplies membership. Never rewrite the native list.
void FSFBlueprintGroupReplication::RegisterHooks()
{
	SFPipelineIndicatorGrouping::RegisterHooks();
	SUBSCRIBE_METHOD(AFGBlueprintProxy::RegisterBuildable,
		[](auto& Scope, AFGBlueprintProxy* Proxy, AFGBuildable* Buildable)
		{
			if (!IsInGameThread() || !IsValid(Proxy) || !IsValid(Buildable) || !Proxy->GetWorld()) return;
			SFBlueprintGroupRegistrationPolicy::FContext Context;
			Context.bClientWorld = Proxy->GetWorld()->GetNetMode() == NM_Client;
			Context.bProxyAuthority = Proxy->HasAuthority();
			Context.bBuildableAuthority = Buildable->HasAuthority();
			Context.bReplicatedProxy = Proxy->GetIsReplicated();
			Context.bReplicatedBuildable = Buildable->GetIsReplicated();
			Context.bMatchesAssignedProxy = Buildable->GetBlueprintProxy() == Proxy;
			if (!Context.bClientWorld || Context.bProxyAuthority || Context.bBuildableAuthority
				|| !Context.bReplicatedProxy || !Context.bReplicatedBuildable || !Context.bMatchesAssignedProxy) return;
			for (AFGBuildable* Member : Proxy->GetBuildables())
			{
				Context.bHasUnresolvedMembers |= Member == nullptr;
				Context.bAlreadyRegistered |= Member == Buildable;
			}
			// AddUnique cannot match an actor against its unresolved replicated slot.
			// Appending now duplicates it when that slot resolves. The native void body
			// only performs AddUnique; no authority membership or bookkeeping is skipped.
			if (SFBlueprintGroupRegistrationPolicy::ShouldDefer(Context)) Scope.Cancel();
		});
}

TArray<AFGBuildable*> FSFBlueprintGroupReplication::FindReassignedMembers(const AFGBlueprintProxy* Proxy)
{
	TArray<AFGBuildable*> Reassigned;
	if (!IsValid(Proxy)) return Reassigned;
	for (AFGBuildable* Member : Proxy->GetBuildables())
	{
		if (!IsValid(Member)) continue;
		AFGBlueprintProxy* Assigned = Member->GetBlueprintProxy();
		if (IsValid(Assigned) && Assigned != Proxy && Assigned->GetWorld() == Proxy->GetWorld()
			&& Assigned->GetBuildables().Contains(Member))
		{
			Reassigned.AddUnique(Member);
		}
	}
	return Reassigned;
}

void FSFBlueprintGroupReplication::ReconcileConstructedGroups(const TArray<AActor*>& BuiltActors)
{
	TSet<AFGBlueprintProxy*> Groups;
	for (AActor* Actor : BuiltActors)
	{
		AFGBuildable* Buildable = Cast<AFGBuildable>(Actor);
		if (IsValid(Buildable) && Buildable->HasAuthority())
		{
			if (AFGBlueprintProxy* Proxy = Buildable->GetBlueprintProxy(); IsValid(Proxy)) Groups.Add(Proxy);
		}
	}
	for (AFGBlueprintProxy* Proxy : Groups)
	{
		if (!Proxy->HasAuthority()) continue;
		// The spawn observer can register a member before native Blueprint construction
		// assigns its final proxy. Keep that assignment; retire only the stale old entry.
		const TArray<AFGBuildable*> Reassigned = FindReassignedMembers(Proxy);
		for (AFGBuildable* Member : Reassigned) Proxy->UnregisterBuildable(Member);
		if (!Reassigned.IsEmpty()) Proxy->ForceNetUpdate();
	}
}

AFGBlueprintProxy* FSFBlueprintGroupReplication::GroupConstructedSeams(const TArray<AActor*>& SeamActors)
{
	TArray<AFGBuildable*> Members;
	UWorld* World = nullptr;
	for (AActor* Actor : SeamActors)
	{
		AFGBuildable* Member = Cast<AFGBuildable>(Actor);
		if (!IsValid(Member) || !Member->HasAuthority() || Member->GetBlueprintDesigner()
			|| Member->GetBlueprintProxy() || !Member->GetWorld()) continue;
		if (!World) World = Member->GetWorld();
		if (Member->GetWorld() == World) Members.AddUnique(Member);
	}
	if (Members.IsEmpty()) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFGBlueprintProxy* Proxy = World->SpawnActor<AFGBlueprintProxy>(
		AFGBlueprintProxy::StaticClass(), Members[0]->GetActorTransform(), Params);
	if (!Proxy) return nullptr;
	for (AFGBuildable* Member : Members)
	{
		Member->SetBlueprintProxy(Proxy);
		Proxy->RegisterBuildable(Member);
		Member->ForceNetUpdate();
	}
	Proxy->ForceNetUpdate();
	return Proxy;
}
