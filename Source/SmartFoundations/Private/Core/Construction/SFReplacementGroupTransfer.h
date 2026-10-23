// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBlueprintProxy;

/** Keeps an existing group alive through native replacement callbacks; never creates a group. */
class FSFReplacementGroupTransfer
{
public:
	FSFReplacementGroupTransfer(AFGBuildable* Original, bool bEnabled);
	~FSFReplacementGroupTransfer();
	FSFReplacementGroupTransfer(const FSFReplacementGroupTransfer&) = delete;
	FSFReplacementGroupTransfer& operator=(const FSFReplacementGroupTransfer&) = delete;
	void Apply(AFGBuildable* Replacement);
	static void RegisterHooks();
	static bool IsGroupHeld(AFGBlueprintProxy* Group);

private:
	TWeakObjectPtr<AFGBuildable> Original;
	TWeakObjectPtr<AFGBuildable> AppliedReplacement;
	TWeakObjectPtr<AFGBlueprintProxy> Group;
};
