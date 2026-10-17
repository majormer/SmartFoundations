// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

namespace SFPipelineIndicatorGrouping
{
struct FContext
{
	bool bGameWorld = false;
	bool bAuthority = false;
	bool bNativeOwnerMatches = false;
	bool bParentRegistered = false;
	bool bParentInDesigner = false;
	bool bIndicatorInDesigner = false;
	bool bIndicatorHasGroup = false;
};

inline bool CanRegister(const FContext& Context)
{
	return Context.bGameWorld && Context.bAuthority && Context.bNativeOwnerMatches
		&& Context.bParentRegistered && !Context.bParentInDesigner
		&& !Context.bIndicatorInDesigner && !Context.bIndicatorHasGroup;
}

void RegisterHooks();
}
