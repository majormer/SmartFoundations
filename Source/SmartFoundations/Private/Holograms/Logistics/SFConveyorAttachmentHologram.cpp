// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFConveyorAttachmentHologram.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"

AActor* ASFConveyorAttachmentHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	AActor* BuiltActor = Super::Construct(out_children, constructionID);
	AFGBuildable* BuiltDistributor = Cast<AFGBuildable>(BuiltActor);
	if (!BuiltDistributor)
	{
		return BuiltActor;
	}

	// CHILD HOLOGRAM REFACTOR: BuildBeltsForDistributor is no longer needed.
	// Belt children are now added via AddChild() and vanilla's Construct() builds them automatically.
	// This eliminates the "double-build" issue where belts were created both as children AND post-build.
	// See: IMPL_AutoConnect_ChildHologram_Refactor.md

	return BuiltActor;
}

// #348: GetCost override removed. It manually added GetBeltPreviewsCost on top of Super::GetCost,
// but the auto-connect belts are child holograms already counted by vanilla GetCost(includeChildren),
// so this double-counted the belt cost. The base class cost (children included) is correct as-is.
