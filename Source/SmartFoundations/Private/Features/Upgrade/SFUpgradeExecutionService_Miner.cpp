// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "Features/Upgrade/SFUpgradeMinerFamily.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "FGDismantleInterface.h"
#include "FGConstructDisqualifier.h"
#include "FGInventoryComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

// SP/MP DIVERGENCE MAP — Miner replacement
//  1. Request/settlement — inherited [MP-AUTH] execution; requester owns inventory and gun.
//  2. Materialization — native hologram owns resource binding, geometry and factory state.
//  3. Membership — shared group snapshot is applied on authority; native proxy replicates.

int32 USFUpgradeExecutionService::ProcessMinerReplacement(AFGBuildable* Buildable,
	TSubclassOf<UFGRecipe> TargetRecipe, AFGBuildGun* BuildGun, AFGCharacterPlayer* Player,
	FSFReplacementGroupTransfer& GroupTransfer)
{
	auto Reject = [this](const FString& Message)
	{
		// Keep the first actionable rejection for the requesting player's result.
		if (WorkingResult.ErrorMessage.IsEmpty()) WorkingResult.ErrorMessage = Message;
		return 0;
	};
	AFGBuildableResourceExtractor* Old = Cast<AFGBuildableResourceExtractor>(Buildable);
	if (!IsValid(Old) || !SFUpgradeMinerFamily::IsMinerClass(Old->GetClass()) || !Old->HasAuthority()
		|| !IsValid(Old->GetExtractableResource().GetObject()) || !BuildGun || !Player)
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingContext", "The selected Miner is missing its native resource or construction context.").ToString());

	AFGHologram* Preview = AFGHologram::SpawnHologramFromRecipe(TargetRecipe, BuildGun, Old->GetActorLocation(), Player);
	ON_SCOPE_EXIT { if (IsValid(Preview)) Preview->Destroy(); };
	AFGResourceExtractorHologram* Hologram = Cast<AFGResourceExtractorHologram>(Preview);
	if (!Hologram) return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingPreview", "The native Miner replacement preview could not be created.").ToString());
	Hologram->SetInsideBlueprintDesigner(Old->GetBlueprintDesigner());

	// Obtain an ordinary blocking hit on this exact extractor. A synthetic actor hit
	// and TryUpgrade alone leave the standalone preview's native aim/init state unset.
	const FVector Aim = Old->GetActorLocation() + FVector(0.0, 0.0, 300.0);
	const FVector Start = Old->GetActorLocation() + Old->GetActorForwardVector() * 1200.0
		+ FVector(0.0, 0.0, 300.0);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartMinerReplacement), false, Hologram);
	Query.AddIgnoredActor(Player);
	Query.AddIgnoredActor(BuildGun);
	FHitResult Hit;
	if (!Old->GetWorld()->LineTraceSingleByChannel(Hit, Start, Aim, ECC_Visibility, Query)
		|| Hit.GetActor() != Old || !Hologram->IsValidHitResult(Hit))
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerInvalidAim", "The selected Miner has no valid native replacement aim.").ToString());

	// Start the same fresh placement pass used by the build gun, then run the full
	// native lifecycle and validation. Never clear disqualifiers after validation,
	// populate resource ownership fields, or force visibility/attachment permission.
	Hologram->ResetConstructDisqualifiers();
	Hologram->UpdateHologramPlacement(Hit);
	if (!Hologram->IsUpgrade() || Hologram->GetUpgradedActor() != Old)
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerCannotBind", "The native Miner replacement could not bind to the selected extractor.").ToString());

	// Classify exhausted materials through the shared settlement contract before
	// native cost validation can turn them into an ordinary placement rejection.
	// Returning -1 lets the batch retain and skip every remaining original.
	FSFUpgradeSettlementLedger Ledger;
	const int32 Prepared = PrepareUpgradeSettlement(Old, TargetRecipe, Player, Ledger);
	if (Prepared != 1) return Prepared == -1 ? -1 : 0;
	// Native build steps consume an already validated placement, as on the gun.
	Hologram->ValidatePlacementAndCost(Player->GetInventory());
	int32 Steps = 0;
	while (Hologram->CanTakeNextBuildStep() && !Hologram->DoMultiStepPlacement(false))
	{
		if (++Steps >= 8) return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerPlacementIncomplete", "The native Miner replacement placement could not finish.").ToString());
	}
	Hologram->ValidatePlacementAndCost(Player->GetInventory());
	if (!Hologram->IsUpgrade() || Hologram->GetUpgradedActor() != Old)
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerSelectionLost", "The native Miner replacement lost its selected extractor during placement.").ToString());
	if (!Hologram->CanConstruct())
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		Hologram->GetConstructDisqualifiers(Disqualifiers);
		FString Message = NSLOCTEXT("SmartFoundations", "Upgrade_MinerPlacementRejected", "Native placement rejected the Miner replacement.").ToString();
		for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
		{
			if (!UFGConstructDisqualifier::GetIsSoftDisqualifier(Disqualifier))
				Message += TEXT(" ") + UFGConstructDisqualifier::GetDisqualifyingText(Disqualifier).ToString();
		}
		return Reject(Message);
	}

	// Native extractor Upgrade migrates connections, but does not migrate factory
	// cargo or standby. Retain the existing factory state independently of callbacks.
	UFGInventoryComponent* Output = Old->GetOutputInventory();
	UFGInventoryComponent* Potential = Old->GetPotentialInventory();
	if (!IsValid(Output) || !IsValid(Potential))
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerMissingInventory", "The selected Miner is missing its native factory inventories.").ToString());
	const bool bPaused = Old->IsProductionPaused();
	const float CurrentPotential = Old->GetCurrentPotential();
	const float PendingPotential = Old->GetPendingPotential();
	const float CurrentBoost = Old->GetCurrentProductionBoost();
	const float PendingBoost = Old->GetPendingProductionBoost();

	ReserveUpgradeCharge(Ledger, Player);
	TArray<AActor*> Children;
	AFGBuildableSubsystem* Builds = AFGBuildableSubsystem::Get(Old->GetWorld());
	const FNetConstructionID ID = Builds ? Builds->GetNewNetConstructionID() : FNetConstructionID();
	AFGBuildableResourceExtractor* New = Cast<AFGBuildableResourceExtractor>(Hologram->Construct(Children, ID));
	if (!IsValid(New))
	{
		RollbackUpgradeCharge(Ledger, Player);
		return Reject(NSLOCTEXT("SmartFoundations", "Upgrade_MinerConstructionFailed", "The native Miner replacement could not be constructed.").ToString());
	}

	// Leave the original intact until native construction has succeeded. Dispatch
	// the interface events so recipe Blueprint overrides participate in migration.
	// The three native Miner tiers share output/shard inventory contracts. Copy
	// before callbacks can clear the source; retiring it below prevents duplication.
	New->GetOutputInventory()->CopyFromOtherComponent(Output);
	New->GetPotentialInventory()->CopyFromOtherComponent(Potential);
	IFGDismantleInterface::Execute_PreUpgrade(Old);
	GroupTransfer.Apply(New);
	IFGDismantleInterface::Execute_Upgrade(Old, New);
	New->SetCurrentPotential(CurrentPotential);
	New->SetPendingPotential(PendingPotential);
	New->SetCurrentProductionBoost(CurrentBoost);
	New->SetPendingProductionBoost(PendingBoost);
	New->SetIsProductionPaused(bPaused);
	GroupTransfer.Apply(New);
	TArray<AActor*> OldChildren;
	IFGDismantleInterface::Execute_GetChildDismantleActors(Old, OldChildren);
	for (AActor* Child : OldChildren)
	{
		if (IsValid(Child) && Child != New && !Children.Contains(Child)) Child->Destroy();
	}
	Old->Destroy();
	GrantUpgradeRefund(Ledger, Player);
	OldToNewBuildableMap.Add(Old, New);
	return 1;
}
