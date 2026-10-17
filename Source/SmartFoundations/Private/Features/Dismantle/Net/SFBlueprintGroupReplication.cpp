// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Dismantle/Net/SFBlueprintGroupReplication.h"
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
