// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Dismantle/Net/SFPipelineIndicatorGrouping.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGBuildablePipelineFlowIndicator.h"
#include "FGBlueprintProxy.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — Native pipeline indicator membership
//  1. [MP-AUTH] After native BeginPlay, adopt only the pipe's own native indicator
//     into its already-established group. Never create an indicator or a group.
//  2. [MP-REPL] Clients receive native group replication; they never append members.
void SFPipelineIndicatorGrouping::RegisterHooks()
{
#if !WITH_EDITOR
	// The Editor SDK's generated BeginPlay body is an unhookable empty stub.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildablePipeline::BeginPlay,
		GetMutableDefault<AFGBuildablePipeline>(), [](AFGBuildablePipeline* Pipe)
		{
			if (!IsInGameThread() || !IsValid(Pipe) || !Pipe->GetWorld()) return;
			AFGBlueprintProxy* Group = Pipe->GetBlueprintProxy();
			AFGBuildablePipelineFlowIndicator* Indicator = Pipe->GetFlowIndicator();
			if (!IsValid(Group) || !IsValid(Indicator)) return;

			FContext Context;
			Context.bGameWorld = Pipe->GetWorld()->IsGameWorld();
			Context.bAuthority = Pipe->GetWorld()->GetNetMode() != NM_Client
				&& Pipe->HasAuthority() && Group->HasAuthority() && Indicator->HasAuthority();
			Context.bNativeOwnerMatches = Indicator->GetOwner() == Pipe;
			Context.bParentRegistered = Group->GetBuildables().Contains(Pipe);
			Context.bParentInDesigner = Pipe->GetBlueprintDesigner() != nullptr;
			Context.bIndicatorInDesigner = Indicator->GetBlueprintDesigner() != nullptr;
			Context.bIndicatorHasGroup = Indicator->GetBlueprintProxy() != nullptr;
			if (!CanRegister(Context)) return;

			// The cached native child is established by vanilla, including load-time
			// reconstruction. Its actor-spawn callback can precede that association.
			// Assigned children keep native PostLoad registration, even while their
			// proxy's member list is still being reconstructed. Never reassign them.
			Indicator->SetBlueprintProxy(Group);
			if (!Group->GetBuildables().Contains(Indicator))
			{
				Group->RegisterBuildable(Indicator);
			}
			Indicator->ForceNetUpdate();
			Group->ForceNetUpdate();
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("Pipeline indicator group restored: pipe=%s indicator=%s members=%d"),
				*GetNameSafe(Pipe), *GetNameSafe(Indicator), Group->GetBuildables().Num());
		});
#endif
}
