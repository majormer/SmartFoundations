// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "FGBlueprintProxy.h"
#include "Patching/NativeHookManager.h"

namespace
{
	// Replacement is synchronous on the authority game thread. Holds are scoped to that
	// operation, including Construct/PreUpgrade failure; ordinary dismantling is unaffected.
	TMap<TWeakObjectPtr<AFGBlueprintProxy>, int32> GroupHolds;
}

void FSFReplacementGroupTransfer::RegisterHooks()
{
#if !WITH_EDITOR
	// The Editor SDK contains an empty generated stub here, too short to hook.
	// Shipping supplies the real native proxy lifetime implementation.
	static bool bRegistered = false;
	if (bRegistered) return;
	bRegistered = true;
	SUBSCRIBE_METHOD(AFGBlueprintProxy::ValidateExistanceOtherwiseSelfDestruct,
		[](auto& Scope, AFGBlueprintProxy* Self)
		{
			if (IsGroupHeld(Self)) Scope.Cancel();
		});
#endif
}

bool FSFReplacementGroupTransfer::IsGroupHeld(AFGBlueprintProxy* InGroup)
{
	return GroupHolds.Contains(InGroup);
}

FSFReplacementGroupTransfer::FSFReplacementGroupTransfer(AFGBuildable* InOriginal, bool bEnabled)
	: Original(InOriginal)
{
	if (!bEnabled || !IsValid(InOriginal) || !InOriginal->HasAuthority()
		|| InOriginal->GetBlueprintDesigner()) return;
	AFGBlueprintProxy* ExistingGroup = InOriginal->GetBlueprintProxy();
	if (!IsValid(ExistingGroup)) return;
	RegisterHooks();
	Group = ExistingGroup;
	++GroupHolds.FindOrAdd(ExistingGroup);
}

void FSFReplacementGroupTransfer::Apply(AFGBuildable* Replacement)
{
	AFGBlueprintProxy* ExistingGroup = Group.Get();
	if (!IsValid(ExistingGroup) || !IsValid(Replacement) || !Replacement->HasAuthority()
		|| Replacement->GetBlueprintDesigner()) return;

	AFGBlueprintProxy* SpawnGroup = Replacement->GetBlueprintProxy();
	// Register first: unregistering a last member can synchronously destroy its proxy.
	Replacement->SetBlueprintProxy(ExistingGroup);
	if (!ExistingGroup->GetBuildables().Contains(Replacement))
	{
		ExistingGroup->RegisterBuildable(Replacement);
	}
	if (IsValid(SpawnGroup) && SpawnGroup != ExistingGroup)
	{
		SpawnGroup->UnregisterBuildable(Replacement);
	}
	Replacement->ForceNetUpdate();
	ExistingGroup->ForceNetUpdate();
	AppliedReplacement = Replacement;
}

FSFReplacementGroupTransfer::~FSFReplacementGroupTransfer()
{
	int32* Holds = GroupHolds.Find(Group);
	if (!Holds) return;
	AFGBlueprintProxy* ExistingGroup = Group.Get();
	// A failed construction keeps any surviving original in its exact original group.
	// Reconcile the original after native callbacks, including lifespan-retired lifts.
	if (AFGBuildable* Old = Original.Get(); Old && ExistingGroup)
	{
		if (AppliedReplacement.IsValid())
		{
			Old->SetBlueprintProxy(nullptr);
			ExistingGroup->UnregisterBuildable(Old);
		}
		else
		{
			Old->SetBlueprintProxy(ExistingGroup);
			if (!ExistingGroup->GetBuildables().Contains(Old)) ExistingGroup->RegisterBuildable(Old);
		}
	}
	if (--*Holds == 0)
	{
		GroupHolds.Remove(Group);
		if (ExistingGroup) ExistingGroup->ValidateExistanceOtherwiseSelfDestruct();
	}
}
