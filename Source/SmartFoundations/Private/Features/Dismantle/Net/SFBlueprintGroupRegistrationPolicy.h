// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

namespace SFBlueprintGroupRegistrationPolicy
{
struct FContext
{
	bool bClientWorld = false;
	bool bProxyAuthority = false;
	bool bBuildableAuthority = false;
	bool bReplicatedProxy = false;
	bool bReplicatedBuildable = false;
	bool bMatchesAssignedProxy = false;
	bool bHasUnresolvedMembers = false;
	bool bAlreadyRegistered = false;
};

inline bool ShouldDefer(const FContext& Context)
{
	return Context.bClientWorld && !Context.bProxyAuthority && !Context.bBuildableAuthority
		&& Context.bReplicatedProxy && Context.bReplicatedBuildable && Context.bMatchesAssignedProxy
		&& Context.bHasUnresolvedMembers && !Context.bAlreadyRegistered;
}
}
