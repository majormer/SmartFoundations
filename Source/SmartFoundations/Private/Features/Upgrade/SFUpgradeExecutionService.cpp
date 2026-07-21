// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"

namespace
{
	bool IsConveyorUpgradeFamily(ESFUpgradeFamily Family)
	{
		return Family == ESFUpgradeFamily::Belt || Family == ESFUpgradeFamily::Lift;
	}
}

void USFUpgradeExecutionService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Initialized"));
}

bool USFUpgradeExecutionService::ComputeUpgradeSettlement(AFGBuildable* Old, TSubclassOf<UFGRecipe> TargetRecipe,
	TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutCharge,
	TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutRefund) const
{
	if (!IsValid(Old) || !TargetRecipe)
	{
		return false;
	}

	const UFGRecipe* TargetCDO = TargetRecipe->GetDefaultObject<UFGRecipe>();
	if (!TargetCDO)
	{
		return false;
	}

	// Refund: vanilla's own consolidated, length-multiplied build-cost return (never contents).
	TArray<FInventoryStack> Refunds;
	Old->GetDismantleRefundReturns(Refunds);
	for (const FInventoryStack& Stack : Refunds)
	{
		if (Stack.Item.GetItemClass())
		{
			OutRefund.FindOrAdd(Stack.Item.GetItemClass()) += Stack.NumItems;
		}
	}

	// Charge: target recipe scaled by the SAME vanilla per-length multiplier the refund used
	// (protected; AccessTransformers friend). Belts/pipes/lifts share the length rule across
	// tiers, so the OLD buildable's multiplier is the exact factor for the SAME-geometry
	// replacement; simple buildables return 1. Charging the hologram's GetBaseCost instead is
	// exactly the #485 defect: the synthetic upgrade flow never gives the belt hologram real
	// spline geometry, so it priced every belt as one recipe unit regardless of length.
	const int32 LengthMultiplier = FMath::Max(1, Old->GetDismantleRefundReturnsMultiplier());
	for (const FItemAmount& Ingredient : TargetCDO->GetIngredients())
	{
		if (Ingredient.ItemClass)
		{
			OutCharge.FindOrAdd(Ingredient.ItemClass) += Ingredient.Amount * LengthMultiplier;
		}
	}
	return true;
}

int32 USFUpgradeExecutionService::PrepareUpgradeSettlement(AFGBuildable* Old, TSubclassOf<UFGRecipe> TargetRecipe,
	AFGCharacterPlayer* PlayerChar, FSFUpgradeSettlementLedger& OutLedger) const
{
	OutLedger = FSFUpgradeSettlementLedger();

	UFGInventoryComponent* PlayerInventory = PlayerChar ? PlayerChar->GetInventory() : nullptr;
	if (!PlayerInventory)
	{
		return 0;
	}
	if (PlayerInventory->GetNoBuildCost())
	{
		OutLedger.bFreeBuild = true;
		return 1;
	}
	OutLedger.bFreeBuild = false;

	TMap<TSubclassOf<UFGItemDescriptor>, int32> Charge;
	TMap<TSubclassOf<UFGItemDescriptor>, int32> Refund;
	if (!ComputeUpgradeSettlement(Old, TargetRecipe, Charge, Refund))
	{
		return 0;
	}

	// Split the net delta by sign: positives are reserved before Construct, negatives are
	// granted only after success.
	TMap<TSubclassOf<UFGItemDescriptor>, int32> Net = Charge;
	for (const auto& Pair : Refund)
	{
		Net.FindOrAdd(Pair.Key) -= Pair.Value;
	}
	for (const auto& Pair : Net)
	{
		if (!Pair.Key || Pair.Value == 0)
		{
			continue;
		}
		if (Pair.Value > 0)
		{
			OutLedger.NetCharge.Add(Pair.Key, Pair.Value);
		}
		else
		{
			OutLedger.NetRefund.Add(Pair.Key, -Pair.Value);
		}
	}

	// Net-based affordability (inventory + Dimensional Depot), matching the pre-existing
	// player-facing behavior: same-material tier changes only need the difference.
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	AFGCentralStorageSubsystem* CentralStorage = World ? AFGCentralStorageSubsystem::Get(World) : nullptr;
	for (const auto& Entry : OutLedger.NetCharge)
	{
		int32 Available = PlayerInventory->GetNumItems(Entry.Key);
		if (CentralStorage)
		{
			Available += CentralStorage->GetNumItemsFromCentralStorage(Entry.Key);
		}
		if (Available < Entry.Value)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: cannot afford %s - need %d %s, have %d (aborting batch)"),
				*GetNameSafe(Old), Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Available);
			return -1;
		}
	}
	return 1;
}

void USFUpgradeExecutionService::ReserveUpgradeCharge(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* PlayerChar)
{
	if (Ledger.bFreeBuild || !PlayerChar)
	{
		return;
	}
	UFGInventoryComponent* PlayerInventory = PlayerChar->GetInventory();
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	AFGCentralStorageSubsystem* CentralStorage = World ? AFGCentralStorageSubsystem::Get(World) : nullptr;
	AFGPlayerState* PlayerState = PlayerChar->GetPlayerState<AFGPlayerState>();
	const bool bTakeFromInventoryFirst = PlayerState ? PlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;

	for (const auto& Entry : Ledger.NetCharge)
	{
		UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(
			PlayerInventory, CentralStorage, bTakeFromInventoryFirst, Entry.Key, Entry.Value);
		BatchChargedTotals.FindOrAdd(Entry.Key) += Entry.Value;
	}
}

void USFUpgradeExecutionService::RollbackUpgradeCharge(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* PlayerChar)
{
	if (Ledger.bFreeBuild || !PlayerChar)
	{
		return;
	}
	UFGInventoryComponent* PlayerInventory = PlayerChar->GetInventory();
	for (const auto& Entry : Ledger.NetCharge)
	{
		const int32 Added = PlayerInventory ? PlayerInventory->AddStack(FInventoryStack(Entry.Value, Entry.Key), true) : 0;
		if (Added < Entry.Value)
		{
			OverflowItems.FindOrAdd(Entry.Key) += Entry.Value - Added;
		}
		BatchChargedTotals.FindOrAdd(Entry.Key) -= Entry.Value;
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: construct failed after reservation - returned %d %s (%d to crate)"),
			Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Entry.Value - Added);
	}
}

void USFUpgradeExecutionService::GrantUpgradeRefund(const FSFUpgradeSettlementLedger& Ledger, AFGCharacterPlayer* PlayerChar)
{
	if (Ledger.bFreeBuild || !PlayerChar)
	{
		return;
	}
	UFGInventoryComponent* PlayerInventory = PlayerChar->GetInventory();
	for (const auto& Entry : Ledger.NetRefund)
	{
		const int32 Added = PlayerInventory ? PlayerInventory->AddStack(FInventoryStack(Entry.Value, Entry.Key), true) : 0;
		if (Added < Entry.Value)
		{
			OverflowItems.FindOrAdd(Entry.Key) += Entry.Value - Added;
		}
		BatchRefundedTotals.FindOrAdd(Entry.Key) += Entry.Value;
	}
}

void USFUpgradeExecutionService::Cleanup()
{
	CancelUpgrade();
	Subsystem = nullptr;
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Cleaned up"));
}

void USFUpgradeExecutionService::Tick(float DeltaTime)
{
	// No longer used - processing moved to timer callback to avoid parallel tick conflicts
}

void USFUpgradeExecutionService::ProcessUpgradeTimer()
{
	if (!bUpgradeInProgress || PendingUpgrades.Num() == 0)
	{
		return;
	}

	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;

	// Process one item per timer callback
	if (CurrentUpgradeIndex < PendingUpgrades.Num())
	{
		AFGBuildable* Buildable = PendingUpgrades[CurrentUpgradeIndex];

		if (IsValid(Buildable))
		{
			TSubclassOf<UFGRecipe> TargetRecipe = GetTargetRecipeForBuildable(Buildable, CurrentTargetRecipe);
			if (TargetRecipe && ProcessSingleUpgrade(Buildable, TargetRecipe))
			{
				WorkingResult.SuccessCount++;
				// Chain rebuild is now batched at end in CompleteUpgrade() to avoid
				// race conditions with parallel factory tick during large batch upgrades
			}
			else
			{
				WorkingResult.FailCount++;
			}
		}
		else
		{
			WorkingResult.SkipCount++;
		}

		WorkingResult.TotalProcessed++;
		CurrentUpgradeIndex++;
	}

	// Broadcast progress
	float Progress = static_cast<float>(CurrentUpgradeIndex) / static_cast<float>(PendingUpgrades.Num());
	OnUpgradeProgressUpdated.Broadcast(Progress, WorkingResult.SuccessCount, PendingUpgrades.Num());

	// Check if complete
	if (CurrentUpgradeIndex >= PendingUpgrades.Num())
	{
		CompleteUpgrade();
	}
}

void USFUpgradeExecutionService::StartUpgrade(const FSFUpgradeExecutionParams& Params)
{
	if (bUpgradeInProgress)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Upgrade already in progress"));
		return;
	}

	CurrentParams = Params;

	// Validate params
	// In traversal mode (HasSpecificBuildables), SourceTier can be 0 as we upgrade all tiers below target
	bool bIsTraversalMode = Params.HasSpecificBuildables();

	if (Params.Family == ESFUpgradeFamily::None || Params.TargetTier == 0)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Invalid params - Family=%d TargetTier=%d"),
			static_cast<int32>(Params.Family), Params.TargetTier);
		return;
	}

	// For radius mode, require valid source tier
	if (!bIsTraversalMode && Params.SourceTier == 0)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Invalid source tier for radius mode"));
		return;
	}

	// For radius mode, target must be greater than source
	if (!bIsTraversalMode && Params.TargetTier <= Params.SourceTier)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Target tier must be greater than source tier"));
		return;
	}

	// Get target recipe
	CurrentTargetRecipe = GetUpgradeRecipe(Params.Family, Params.TargetTier);
	if (!CurrentTargetRecipe)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Could not find recipe for Family=%d Tier=%d"),
			static_cast<int32>(Params.Family), Params.TargetTier);
		return;
	}

	// Reset working result
	WorkingResult = FSFUpgradeExecutionResult();
	CurrentUpgradeIndex = 0;
	PendingUpgrades.Empty();
	UpgradedConveyors.Empty();
	CachedChainConveyors.Empty();
	bChainCached = false;
	AccumulatedCosts.Empty();  // Reset accumulated costs
	OverflowItems.Empty();     // Reset overflow items
	BatchChargedTotals.Empty();   // Settlement audit
	BatchRefundedTotals.Empty();  // Settlement audit
	OldToNewConveyorMap.Empty(); // Reset old->new mapping for batch connection fixes
	OldToNewBuildableMap.Empty(); // Reset general old->new mapping (Option A/B)
	SavedConnectionPairs.Empty(); // Reset saved connection pairs
	SavedPipeConnectionPairs.Empty(); // Reset saved pipe pairs (Option A)
	ExpectedConnectionEdges.Empty(); // Reset expected connection manifest (Option B)
	PreDestroyChainActors.Empty(); // Reset pre-destroy chain capture

	// Gather targets
	GatherUpgradeTargets();

	if (PendingUpgrades.Num() == 0)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No targets found to upgrade"));
		WorkingResult.bCompleted = true;
		LastResult = WorkingResult;
		OnUpgradeExecutionCompleted.Broadcast(LastResult);
		return;
	}

	bUpgradeInProgress = true;
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Starting SYNCHRONOUS upgrade of %d items from Tier %d to Tier %d"),
		PendingUpgrades.Num(), Params.SourceTier, Params.TargetTier);

	// Pre-scan: Save connection pairs between conveyors (and pipes) that will be upgraded
	// This must happen BEFORE any destruction so we can reconnect them after
	SaveBatchConnectionPairs();
	SaveBatchPipeConnectionPairs();

	// Option B: capture the full expected-connection manifest for every pending buildable
	// (covers power, building-side partners, and any edge the pair-based save/fix misses)
	CaptureExpectedConnectionManifests();

	// Process ALL upgrades synchronously in a single frame, just like MassUpgrade does.
	// This prevents the parallel factory tick from running between upgrades and accessing
	// belts in inconsistent states (which causes crashes in Factory_UpdateRadioactivity).
	bool bAbortedDueToFunds = false;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (bAbortedDueToFunds)
		{
			// Ran out of funds - skip remaining items
			WorkingResult.SkipCount++;
			WorkingResult.TotalProcessed++;
			continue;
		}

		if (IsValid(Buildable))
		{
			TSubclassOf<UFGRecipe> TargetRecipe = GetTargetRecipeForBuildable(Buildable, CurrentTargetRecipe);
			int32 ResultCode = TargetRecipe ? ProcessSingleUpgrade(Buildable, TargetRecipe) : 0;
			if (ResultCode == 1)
			{
				WorkingResult.SuccessCount++;
			}
			else if (ResultCode == -1)
			{
				// Ran out of funds - abort remaining upgrades
				bAbortedDueToFunds = true;
				WorkingResult.FailCount++;
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Ran out of funds - aborting remaining upgrades"));
			}
			else
			{
				WorkingResult.FailCount++;
			}
		}
		else
		{
			WorkingResult.SkipCount++;
		}
		WorkingResult.TotalProcessed++;
	}

	if (bAbortedDueToFunds)
	{
		WorkingResult.ErrorMessage = TEXT("Ran out of materials - partial upgrade completed");
	}

	// Complete immediately
	CompleteUpgrade();
}

void USFUpgradeExecutionService::CancelUpgrade()
{
	if (!bUpgradeInProgress)
	{
		return;
	}

	// Stop the timer
	if (UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(UpgradeTimerHandle);
	}

	bUpgradeInProgress = false;
	WorkingResult.bCompleted = false;
	WorkingResult.ErrorMessage = TEXT("Cancelled by user");
	LastResult = WorkingResult;

	PendingUpgrades.Empty();
	CurrentUpgradeIndex = 0;

	OnUpgradeExecutionCompleted.Broadcast(LastResult);
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Upgrade cancelled after %d items"), WorkingResult.TotalProcessed);
}

void USFUpgradeExecutionService::GatherUpgradeTargets()
{
	if (!Subsystem)
	{
		return;
	}

	// If specific buildables were provided (traversal mode), use those directly
	if (CurrentParams.HasSpecificBuildables())
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Using %d specific buildables from traversal scan (TargetTier=%d)"),
			CurrentParams.SpecificBuildables.Num(), CurrentParams.TargetTier);

		for (const TWeakObjectPtr<AFGBuildable>& WeakBuildable : CurrentParams.SpecificBuildables)
		{
			if (AFGBuildable* Buildable = WeakBuildable.Get())
			{
				// [#456] SourceTier > 0 = the user picked a specific tier row in the network scan:
				// upgrade ONLY that tier. SourceTier == 0 keeps the legacy sweep (everything below
				// the target tier).
				const int32 BuildableTier = USFUpgradeTraversalService::GetBuildableTier(Buildable);
				if (BuildableTier <= 0)
				{
					continue;
				}
				const bool bMatch = (CurrentParams.SourceTier > 0)
					? (BuildableTier == CurrentParams.SourceTier)
					: (BuildableTier < CurrentParams.TargetTier);
				if (bMatch)
				{
					PendingUpgrades.Add(Buildable);
				}
			}
		}

		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Filtered to %d targets (source tier %d, target tier %d)"),
			PendingUpgrades.Num(), CurrentParams.SourceTier, CurrentParams.TargetTier);

		if (IsConveyorUpgradeFamily(CurrentParams.Family))
		{
			NormalizeConveyorUpgradeTargets(/*bRespectRadius*/ false);
		}
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		return;
	}

	// Gather source actors.
	// Issue #295/#296: the Pipe family has BOTH indicator and NoIndicator variants per tier;
	// a single SourceClass would miss half the pipelines. For pipes, iterate every
	// AFGBuildablePipeline at the requested tier (excluding pumps/junctions).
	// Conveyor logistics are also special: Belt and Lift remain separate recipe families,
	// but radius selection must treat them as one connected conveyor cohort so Smart does
	// not silently upgrade only part of a belt/lift chain.
	TArray<AActor*> FoundActors;

	if (IsConveyorUpgradeFamily(CurrentParams.Family))
	{
		UE_LOG(LogSmartUpgrade, Verbose,
			TEXT("UpgradeExecutionService: Gathering conveyor seeds at tier %d (belts + lifts, cohort-safe radius mode)"),
			CurrentParams.SourceTier);

		for (TActorIterator<AFGBuildableConveyorBase> It(World); It; ++It)
		{
			AFGBuildableConveyorBase* Conveyor = *It;
			if (!IsValid(Conveyor)) continue;

			const ESFUpgradeFamily Family = USFUpgradeTraversalService::GetUpgradeFamily(Conveyor);
			if (!IsConveyorUpgradeFamily(Family)) continue;

			const int32 ConveyorTier = USFUpgradeTraversalService::GetBuildableTier(Conveyor);
			if (ConveyorTier != CurrentParams.SourceTier) continue;

			if (CurrentParams.Radius > 0.0f && !ConveyorIntersectsRadius(Conveyor))
			{
				continue;
			}

			FoundActors.Add(Conveyor);
		}

		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Found %d conveyor seed(s) at tier %d"),
			FoundActors.Num(), CurrentParams.SourceTier);
	}
	else if (CurrentParams.Family == ESFUpgradeFamily::Pipe)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Gathering pipelines at tier %d (all indicator variants)"),
			CurrentParams.SourceTier);
		for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
		{
			AFGBuildablePipeline* Pipe = *It;
			if (!Pipe) continue;
			// Exclude pumps and junctions. They are AFGBuildablePipelineAttachment subclasses, NOT
			// AFGBuildablePipeline, so the TActorIterator above never yields them; the class-name check
			// below is a defensive backstop for oddly-named (e.g. modded) pipeline variants. (#430)
			const FString PipeClassName = Pipe->GetClass()->GetName();
			if (PipeClassName.Contains(TEXT("Pump")) || PipeClassName.Contains(TEXT("Junction")))
			{
				continue;
			}
			const int32 PipeTier = USFUpgradeTraversalService::GetBuildableTier(Pipe);
			if (PipeTier != CurrentParams.SourceTier) continue;
			FoundActors.Add(Pipe);
		}
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Found %d pipelines at tier %d"),
			FoundActors.Num(), CurrentParams.SourceTier);
	}
	else
	{
		// Get the buildable class for the source tier
		TSubclassOf<AFGBuildable> SourceClass = GetBuildableClass(CurrentParams.Family, CurrentParams.SourceTier);
		if (!SourceClass)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Could not find source class for Family=%d Tier=%d"),
				static_cast<int32>(CurrentParams.Family), CurrentParams.SourceTier);
			return;
		}

		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Looking for source class %s (Full: %s) (Family=%d Tier=%d)"),
			*SourceClass->GetName(), *SourceClass->GetPathName(), static_cast<int32>(CurrentParams.Family), CurrentParams.SourceTier);

		// Gather all actors of the source class - use exact class match to avoid finding subclasses
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			AFGBuildable* Buildable = *It;
			// EXACT class match - not subclass
			if (Buildable && Buildable->GetClass() == SourceClass)
			{
				FoundActors.Add(Buildable);
			}
		}

		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Exact class match found %d actors"), FoundActors.Num());
	}

	float RadiusSq = CurrentParams.Radius * CurrentParams.Radius;

	for (AActor* Actor : FoundActors)
	{
		AFGBuildable* Buildable = Cast<AFGBuildable>(Actor);
		if (!Buildable)
		{
			continue;
		}

		// Check radius if specified
		if (!IsConveyorUpgradeFamily(CurrentParams.Family) && CurrentParams.Radius > 0.0f)
		{
			float DistSq = FVector::DistSquared(Buildable->GetActorLocation(), CurrentParams.Origin);
			if (DistSq > RadiusSq)
			{
				continue;
			}
		}

		PendingUpgrades.Add(Buildable);

		// Check max items limit
		if (!IsConveyorUpgradeFamily(CurrentParams.Family) && CurrentParams.MaxItems > 0 && PendingUpgrades.Num() >= CurrentParams.MaxItems)
		{
			break;
		}
	}

	if (IsConveyorUpgradeFamily(CurrentParams.Family))
	{
		NormalizeConveyorUpgradeTargets(/*bRespectRadius*/ true);
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Gathered %d targets for upgrade"), PendingUpgrades.Num());
}

void USFUpgradeExecutionService::NormalizeConveyorUpgradeTargets(bool bRespectRadius)
{
	TArray<AFGBuildableConveyorBase*> ConveyorSeeds;
	ConveyorSeeds.Reserve(PendingUpgrades.Num());

	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Buildable))
		{
			ConveyorSeeds.Add(Conveyor);
		}
	}

	if (ConveyorSeeds.Num() == 0)
	{
		return;
	}

	TSet<AFGBuildableConveyorBase*> Visited;
	TArray<AFGBuildable*> ExpandedTargets;
	int32 CohortCount = 0;
	int32 SkippedPartialCohorts = 0;
	int32 SkippedMaxItemCohorts = 0;

	for (AFGBuildableConveyorBase* Seed : ConveyorSeeds)
	{
		if (!IsValid(Seed) || Visited.Contains(Seed)) continue;

		TSet<AFGBuildableConveyorBase*> Cohort;
		CollectConnectedConveyorCohort(Seed, Cohort);
		for (AFGBuildableConveyorBase* Conveyor : Cohort)
		{
			Visited.Add(Conveyor);
		}
		++CohortCount;

		bool bFullyInsideRadius = true;
		if (bRespectRadius && CurrentParams.Radius > 0.0f)
		{
			for (AFGBuildableConveyorBase* Conveyor : Cohort)
			{
				if (!ConveyorFullyInsideRadius(Conveyor))
				{
					bFullyInsideRadius = false;
					break;
				}
			}
		}

		if (!bFullyInsideRadius)
		{
			++SkippedPartialCohorts;
			UE_LOG(LogSmartUpgrade, Verbose,
				TEXT("UpgradeExecutionService: Skipping conveyor cohort containing %s - cohort intersects radius but extends outside it (exclusive-safe prototype)"),
				*Seed->GetName());
			continue;
		}

		TArray<AFGBuildable*> EligibleInCohort;
		for (AFGBuildableConveyorBase* Conveyor : Cohort)
		{
			if (!IsValid(Conveyor)) continue;
			const int32 Tier = USFUpgradeTraversalService::GetBuildableTier(Conveyor);
			if (Tier <= 0 || Tier >= CurrentParams.TargetTier) continue;

			// [#456] Honor SourceTier in BOTH radius and network modes. The cohort re-expansion
			// above pulls in every connected conveyor regardless of tier, so this filter is what
			// keeps "Mk2 -> Mk3 only" from sweeping the Mk3/Mk4 belts in the same run. The old
			// !HasSpecificBuildables() gate here is exactly why network scans couldn't tier-target.
			// SourceTier == 0 = "all tiers below target" (filter inert; legacy sweep).
			if (CurrentParams.SourceTier > 0 && Tier != CurrentParams.SourceTier)
			{
				continue;
			}

			EligibleInCohort.Add(Conveyor);
		}

		if (EligibleInCohort.Num() == 0)
		{
			continue;
		}

		if (CurrentParams.MaxItems > 0 && ExpandedTargets.Num() + EligibleInCohort.Num() > CurrentParams.MaxItems)
		{
			++SkippedMaxItemCohorts;
			UE_LOG(LogSmartUpgrade, Verbose,
				TEXT("UpgradeExecutionService: Skipping conveyor cohort containing %s - adding %d member(s) would exceed MaxItems=%d"),
				*Seed->GetName(), EligibleInCohort.Num(), CurrentParams.MaxItems);
			continue;
		}

		ExpandedTargets.Append(EligibleInCohort);
	}

	PendingUpgrades = MoveTemp(ExpandedTargets);

	UE_LOG(LogSmartUpgrade, Verbose,
		TEXT("UpgradeExecutionService: Conveyor cohort normalization - seeds=%d cohorts=%d targets=%d skipped_partial=%d skipped_max_items=%d"),
		ConveyorSeeds.Num(), CohortCount, PendingUpgrades.Num(), SkippedPartialCohorts, SkippedMaxItemCohorts);
}

void USFUpgradeExecutionService::CollectConnectedConveyorCohort(AFGBuildableConveyorBase* StartConveyor, TSet<AFGBuildableConveyorBase*>& OutCohort)
{
	if (!IsValid(StartConveyor)) return;

	constexpr int32 MaxCohortSize = 10000;
	TArray<AFGBuildableConveyorBase*> Stack;
	Stack.Add(StartConveyor);

	while (Stack.Num() > 0 && OutCohort.Num() < MaxCohortSize)
	{
		AFGBuildableConveyorBase* Current = Stack.Pop(EAllowShrinking::No);
		if (!IsValid(Current) || OutCohort.Contains(Current)) continue;

		OutCohort.Add(Current);

		for (UFGFactoryConnectionComponent* Conn : { Current->GetConnection0(), Current->GetConnection1() })
		{
			if (!Conn || !Conn->IsConnected()) continue;
			UFGFactoryConnectionComponent* PeerConn = Conn->GetConnection();
			if (!PeerConn) continue;

			AFGBuildableConveyorBase* PeerConveyor = Cast<AFGBuildableConveyorBase>(PeerConn->GetOuterBuildable());
			if (!IsValid(PeerConveyor) || OutCohort.Contains(PeerConveyor)) continue;

			if (Current->GetLevel() != PeerConveyor->GetLevel())
			{
				continue;
			}

			Stack.Add(PeerConveyor);
		}
	}

	if (OutCohort.Num() >= MaxCohortSize)
	{
		UE_LOG(LogSmartUpgrade, Verbose,
			TEXT("UpgradeExecutionService: Conveyor cohort traversal hit safety cap of %d from seed %s"),
			MaxCohortSize,
			*StartConveyor->GetName());
	}
}

bool USFUpgradeExecutionService::ConveyorIntersectsRadius(AFGBuildableConveyorBase* Conveyor) const
{
	if (!IsValid(Conveyor) || CurrentParams.Radius <= 0.0f)
	{
		return true;
	}

	const float RadiusSq = CurrentParams.Radius * CurrentParams.Radius;
	const float ClosestOffset = FMath::Clamp(Conveyor->FindOffsetClosestToLocation(CurrentParams.Origin), 0.0f, Conveyor->GetLength());
	FVector ClosestLocation = Conveyor->GetActorLocation();
	FVector ClosestDirection = FVector::ForwardVector;
	Conveyor->GetLocationAndDirectionAtOffset(ClosestOffset, ClosestLocation, ClosestDirection);

	return FVector::DistSquared(ClosestLocation, CurrentParams.Origin) <= RadiusSq;
}

bool USFUpgradeExecutionService::ConveyorFullyInsideRadius(AFGBuildableConveyorBase* Conveyor) const
{
	if (CurrentParams.Radius <= 0.0f)
	{
		return true;
	}
	return IsConveyorFullyInsideRadius(Conveyor, CurrentParams.Origin, CurrentParams.Radius * CurrentParams.Radius);
}

bool USFUpgradeExecutionService::IsConveyorFullyInsideRadius(AFGBuildableConveyorBase* Conveyor, const FVector& Origin, float RadiusSq)
{
	if (!IsValid(Conveyor) || RadiusSq <= 0.0f)
	{
		return true;
	}

	const float Length = Conveyor->GetLength();
	const float SampleOffsets[] = { 0.0f, Length * 0.25f, Length * 0.5f, Length * 0.75f, Length };

	for (float Offset : SampleOffsets)
	{
		FVector SampleLocation = Conveyor->GetActorLocation();
		FVector SampleDirection = FVector::ForwardVector;
		Conveyor->GetLocationAndDirectionAtOffset(FMath::Clamp(Offset, 0.0f, Length), SampleLocation, SampleDirection);
		if (FVector::DistSquared(SampleLocation, Origin) > RadiusSq)
		{
			return false;
		}
	}

	return true;
}

TSubclassOf<UFGRecipe> USFUpgradeExecutionService::GetTargetRecipeForBuildable(AFGBuildable* Buildable, TSubclassOf<UFGRecipe> FallbackRecipe) const
{
	if (!Buildable)
	{
		return FallbackRecipe;
	}

	const ESFUpgradeFamily BuildableFamily = USFUpgradeTraversalService::GetUpgradeFamily(Buildable);
	if (BuildableFamily == ESFUpgradeFamily::None)
	{
		return FallbackRecipe;
	}

	TSubclassOf<UFGRecipe> Recipe = GetUpgradeRecipe(BuildableFamily, CurrentParams.TargetTier);
	if (!Recipe)
	{
		UE_LOG(LogSmartUpgrade, Verbose,
			TEXT("UpgradeExecutionService: No target recipe for %s family=%d tier=%d"),
			*Buildable->GetName(), static_cast<int32>(BuildableFamily), CurrentParams.TargetTier);
		return nullptr;
	}

	return Recipe;
}

int32 USFUpgradeExecutionService::ProcessSingleUpgrade(AFGBuildable* Buildable, TSubclassOf<UFGRecipe> TargetRecipe)
{
	// Return codes: 1 = success, 0 = failed, -1 = out of funds (abort remaining)
	if (!Buildable || !Subsystem)
	{
		return 0;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		return 0;
	}

	// Determine the actual family of THIS buildable (may differ from CurrentParams.Family in traversal mode)
	ESFUpgradeFamily ActualFamily = USFUpgradeTraversalService::GetUpgradeFamily(Buildable);
	if (ActualFamily == ESFUpgradeFamily::None)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Cannot determine family for %s"),
			*Buildable->GetName());
		return 0;
	}

	// Get the correct recipe for this buildable's actual family
	TSubclassOf<UFGRecipe> ActualTargetRecipe = TargetRecipe;
	if (ActualFamily != CurrentParams.Family)
	{
		// Buildable is different family (e.g., lift in belt network) - get correct recipe
		ActualTargetRecipe = GetUpgradeRecipe(ActualFamily, CurrentParams.TargetTier);
		if (!ActualTargetRecipe)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No recipe for Family=%d Tier=%d"),
				static_cast<int32>(ActualFamily), CurrentParams.TargetTier);
			return 0;
		}
	}

	// Get the buildable class for this buildable's actual family
	TSubclassOf<AFGBuildable> NewBuildableClass = GetBuildableClass(ActualFamily, CurrentParams.TargetTier);
	if (!NewBuildableClass)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No buildable class for Family=%d Tier=%d"),
			static_cast<int32>(ActualFamily), CurrentParams.TargetTier);
		return 0;
	}

	// SKIP if buildable is already the target class (no upgrade needed)
	if (Buildable->GetClass() == NewBuildableClass)
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Skipping %s - already target tier"),
			*Buildable->GetName());
		return 1;  // Return 1 to count as "handled" not failed
	}

	// Get buildable subsystem
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);

	// Get player controller and build gun for hologram spawning. [UPGRADE-MP] Use the REQUESTING
	// player's controller: on a dedicated server GetFirstPlayerController is whichever client
	// happened to connect first (the known wrong-player build-path hazard) - and the requester's
	// build gun/pawn exist server-side. Fall back to first PC for legacy callers without one.
	AFGPlayerController* PC = CurrentParams.PlayerController
		? CurrentParams.PlayerController.Get()
		: Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!PC)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No player controller"));
		return 0;
	}

	AFGCharacterPlayer* PlayerChar = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!PlayerChar)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No player character"));
		return 0;
	}

	AFGBuildGun* BuildGun = PlayerChar->GetBuildGun();
	if (!BuildGun)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: No build gun"));
		return 0;
	}

	// Handle Conveyor Belts using MassUpgrade's proven approach
	// Key: TryUpgrade + DoMultiStepPlacement + GenerateAndUpdateSpline + PreUpgrade/Upgrade_Implementation
	if (AFGBuildableConveyorBelt* OldBelt = Cast<AFGBuildableConveyorBelt>(Buildable))
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Upgrading belt %s (bucket=%d)"),
			*OldBelt->GetName(), OldBelt->GetConveyorBucketID());

		// Skip if already target class
		if (OldBelt->GetClass() == NewBuildableClass)
		{
			UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Belt already target class, skipping"));
			return 1;
		}

		// STEP 1: Spawn hologram using vanilla factory method (exactly like MassUpgrade)
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 1: Spawning hologram via SpawnHologramFromRecipe..."));
		AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(
			ActualTargetRecipe,
			BuildGun,
			OldBelt->GetActorLocation(),
			PlayerChar
		);

		if (!Hologram)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: SpawnHologramFromRecipe failed"));
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 1: Hologram spawned: %s"), *Hologram->GetName());

		// STEP 2: Set blueprint designer context if applicable
		Hologram->SetInsideBlueprintDesigner(OldBelt->GetBlueprintDesigner());

		// STEP 3: Create hit result and call TryUpgrade (NOT SetupUpgradeTarget!)
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 2: Calling TryUpgrade..."));
		FHitResult HitResult(
			OldBelt,
			Hologram->GetComponentByClass<UPrimitiveComponent>(),
			OldBelt->GetActorLocation(),
			OldBelt->GetActorRotation().Vector()
		);

		if (!Hologram->TryUpgrade(HitResult))
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: TryUpgrade failed"));
			Hologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 2: TryUpgrade succeeded"));

		// STEP 4: Validate placement
		UFGInventoryComponent* PlayerInventory = PlayerChar->GetInventory();
		Hologram->ValidatePlacementAndCost(PlayerInventory);

		if (!Hologram->IsUpgrade())
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Hologram is not upgrade after validation"));
			Hologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 3: ValidatePlacementAndCost passed, IsUpgrade=true"));

		// STEP 5: Run multi-step placement loop (critical for spline buildables!)
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 4: Running DoMultiStepPlacement loop..."));
		while (Hologram->CanTakeNextBuildStep() && !Hologram->DoMultiStepPlacement(true))
		{
			// Loop until placement is complete
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 4: DoMultiStepPlacement complete"));

		// STEP 5.5 [#485]: Price exactly from the old belt's geometry and check affordability.
		// The hologram's GetBaseCost is unusable here: the synthetic upgrade flow never gives it
		// real spline data, so it priced every belt as ONE recipe unit regardless of length while
		// the refund stayed length-correct (a 72% undercharge + duplication on the 96m test rig).
		// Settlement is transactional: charge reserved before Construct, refund granted after.
		FSFUpgradeSettlementLedger Ledger;
		{
			const int32 PrepResult = PrepareUpgradeSettlement(OldBelt, ActualTargetRecipe, PlayerChar, Ledger);
			if (PrepResult != 1)
			{
				Hologram->Destroy();
				return PrepResult == -1 ? -1 : 0;  // -1 = out of funds, abort remaining
			}
		}

		// STEP 6: Generate spline for belt hologram (exactly like MassUpgrade)
		// Friend access granted via AccessTransformers.ini
		if (AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram))
		{
			UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 5: Calling GenerateAndUpdateSpline..."));
			BeltHologram->GenerateAndUpdateSpline(HitResult);
			UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 5: GenerateAndUpdateSpline complete"));
		}

		// Capture chain actor from old belt before PreUpgrade clears upgrade-sensitive state.
		if (AFGConveyorChainActor* OldChain = OldBelt->GetConveyorChainActor())
		{
			PreDestroyChainActors.Add(OldChain);
		}

		// [#485] Reserve the net charge now that all preflight passed; refund waits for success.
		ReserveUpgradeCharge(Ledger, PlayerChar);

		// STEP 7: Let vanilla prepare the old belt for upgrade before constructing the replacement.
		// PreUpgrade is documented as the hook that clears connections/state that can interfere
		// with upgrades; calling it after Construct leaves the replacement born into stale chain state.
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 6a: Calling PreUpgrade_Implementation before Construct..."));
		OldBelt->PreUpgrade_Implementation();

		// STEP 8: Construct the new belt
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 6: Calling Construct()..."));
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = Hologram->Construct(ConstructedChildren, ConstructionID);

		Hologram->Destroy();

		AFGBuildableConveyorBelt* NewBelt = Cast<AFGBuildableConveyorBelt>(ConstructedActor);
		if (!NewBelt)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Construct failed for %s"), *OldBelt->GetName());
			RollbackUpgradeCharge(Ledger, PlayerChar);  // [#485] failed target earns no refund
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 6: Construct() created new belt: %s"), *NewBelt->GetName());

		// STEP 9: Call upgrade interface method on OLD belt (critical for chain transfer!)
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 7: Calling Upgrade_Implementation..."));
		OldBelt->Upgrade_Implementation(NewBelt);
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 7: Upgrade interface method complete"));

		// STEP 10: Check connections. The upgrade hologram plus PreUpgrade/Upgrade should
		// transfer them; this prototype intentionally avoids manual ClearConnection/
		// SetConnection after Construct because that can leave chain actors with stale
		// segment state.
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 8: Checking connections..."));
		UFGFactoryConnectionComponent* OldConn0 = OldBelt->GetConnection0();
		UFGFactoryConnectionComponent* OldConn1 = OldBelt->GetConnection1();
		UFGFactoryConnectionComponent* NewConn0 = NewBelt->GetConnection0();
		UFGFactoryConnectionComponent* NewConn1 = NewBelt->GetConnection1();

		// Log chain actor state after Construct for diagnostics
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 8: NewBelt chain=%s, NewConn0 connected=%s, NewConn1 connected=%s"),
			NewBelt->GetConveyorChainActor() ? *NewBelt->GetConveyorChainActor()->GetName() : TEXT("null"),
			NewConn0 && NewConn0->IsConnected() ? TEXT("yes") : TEXT("no"),
			NewConn1 && NewConn1->IsConnected() ? TEXT("yes") : TEXT("no"));

		if (OldConn0 && NewConn0)
		{
			if (UFGFactoryConnectionComponent* Partner0 = OldConn0->GetConnection())
			{
				if (NewConn0->IsConnected() && NewConn0->GetConnection() == Partner0)
				{
					UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 8a: Conn0 already connected to %s by vanilla upgrade flow"),
						*Partner0->GetOwner()->GetName());
				}
				else
				{
					UE_LOG(LogSmartUpgrade, Verbose, TEXT("⚙️ STEP 8a: Conn0 old partner %s was not transferred by vanilla upgrade flow"),
						*Partner0->GetOwner()->GetName());
				}
			}
		}

		if (OldConn1 && NewConn1)
		{
			if (UFGFactoryConnectionComponent* Partner1 = OldConn1->GetConnection())
			{
				if (NewConn1->IsConnected() && NewConn1->GetConnection() == Partner1)
				{
					UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 8b: Conn1 already connected to %s by vanilla upgrade flow"),
						*Partner1->GetOwner()->GetName());
				}
				else
				{
					UE_LOG(LogSmartUpgrade, Verbose, TEXT("⚙️ STEP 8b: Conn1 old partner %s was not transferred by vanilla upgrade flow"),
						*Partner1->GetOwner()->GetName());
				}
			}
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 8: Connections complete"));

		// STEP 11: Destroy old belt and children
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 9: Destroying old belt..."));
		TArray<AActor*> ChildDismantleActors;
		OldBelt->GetChildDismantleActors_Implementation(ChildDismantleActors);
		for (AActor* ChildActor : ChildDismantleActors)
		{
			if (ChildActor && IsValid(ChildActor))
			{
				ChildActor->Destroy();
			}
		}
		OldBelt->Destroy();
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ STEP 9: Old belt destroyed"));

		// [#485] Replacement is live and the old belt retired - the refund is now earned.
		GrantUpgradeRefund(Ledger, PlayerChar);

		// Track upgraded conveyor
		UpgradedConveyors.Add(NewBelt);

		// Track old->new mapping for fixing inter-connected batch upgrades
		OldToNewConveyorMap.Add(OldBelt, NewBelt);
		OldToNewBuildableMap.Add(OldBelt, NewBelt);

		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("⚙️ UPGRADE COMPLETE: %s → %s"),
			*Buildable->GetName(), *NewBelt->GetClass()->GetName());
		return 1;
	}

	// Handle Pipelines using Smart hologram with spline data copy
	if (AFGBuildablePipeline* OldPipe = Cast<AFGBuildablePipeline>(Buildable))
	{
		// Preserve indicator style across the upgrade (Issue #295/#296):
		// Build_Pipeline_NoIndicator_C (Mk1 clean) -> Build_PipelineMK2_NoIndicator_C (Mk2 clean)
		// Build_Pipeline_C             (Mk1)       -> Build_PipelineMK2_C             (Mk2)
		// Previously GetPipeRecipeForTier/GetPipeClassForTier were called with a hardcoded
		// bWithIndicator=true, so NoIndicator sources silently converted to indicator targets.
		const FString OldPipeClassName = OldPipe->GetClass()->GetName();
		const bool bSourceNoIndicator = OldPipeClassName.Contains(TEXT("NoIndicator"));
		const bool bWithIndicator = !bSourceNoIndicator;
		if (TSubclassOf<UFGRecipe> StyleRecipe = Subsystem->GetPipeRecipeForTier(CurrentParams.TargetTier, bWithIndicator))
		{
			ActualTargetRecipe = StyleRecipe;
		}
		if (UClass* StyleClass = Subsystem->GetPipeClassForTier(CurrentParams.TargetTier, bWithIndicator))
		{
			NewBuildableClass = TSubclassOf<AFGBuildable>(StyleClass);
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose,
			TEXT("UpgradeExecutionService: Pipe upgrade preserving indicator style (source=%s, bWithIndicator=%s, target class=%s)"),
			*OldPipeClassName, bWithIndicator ? TEXT("true") : TEXT("false"),
			NewBuildableClass ? *NewBuildableClass->GetName() : TEXT("null"));

		// Get spline data from old pipe
		TArray<FSplinePointData> SplineData = OldPipe->GetSplinePointData();
		if (SplineData.Num() < 2)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Pipe has insufficient spline points"));
			return 0;
		}

		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Old pipe %s with %d spline points"),
			*OldPipe->GetName(), SplineData.Num());

		// Spawn our Smart hologram with deferred construction
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;

		ASFPipelineHologram* SmartHolo = World->SpawnActor<ASFPipelineHologram>(
			ASFPipelineHologram::StaticClass(),
			OldPipe->GetActorTransform(),
			SpawnParams
		);

		if (!SmartHolo)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to spawn Smart pipe hologram"));
			return 0;
		}

		// Set build class and recipe BEFORE FinishSpawning
		SmartHolo->SetBuildClass(NewBuildableClass);
		SmartHolo->SetRecipe(ActualTargetRecipe);

		// Finish spawning
		SmartHolo->FinishSpawning(OldPipe->GetActorTransform());

		// Set up spline data from old pipe
		SmartHolo->SetSplineDataAndUpdate(SplineData);

		// [#485] Exact settlement (geometry-derived, shared with the panel). The pipe hologram's
		// GetBaseCost is length-aware here (real spline copied above), but the planner is the single
		// pricing source so panel and execution can never diverge.
		FSFUpgradeSettlementLedger Ledger;
		{
			const int32 PrepResult = PrepareUpgradeSettlement(OldPipe, ActualTargetRecipe, PlayerChar, Ledger);
			if (PrepResult != 1)
			{
				SmartHolo->Destroy();
				return PrepResult == -1 ? -1 : 0;  // -1 = out of funds, abort remaining
			}
		}

		// [#485] Reserve the net charge; refund waits for a successful construct.
		ReserveUpgradeCharge(Ledger, PlayerChar);

		// Construct the new pipe
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = SmartHolo->Construct(ConstructedChildren, ConstructionID);

		SmartHolo->Destroy();

		AFGBuildablePipeline* NewPipe = Cast<AFGBuildablePipeline>(ConstructedActor);
		if (!NewPipe)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Construct failed for pipe %s"), *OldPipe->GetName());
			RollbackUpgradeCharge(Ledger, PlayerChar);  // [#485] failed target earns no refund
			return 0;
		}

		// Call upgrade hooks
		OldPipe->PreUpgrade_Implementation();
		OldPipe->Upgrade_Implementation(NewPipe);

		// Transfer pipe connections
		if (UFGPipeConnectionComponentBase* Conn0Partner = OldPipe->GetPipeConnection0() ? OldPipe->GetPipeConnection0()->GetConnection() : nullptr)
		{
			OldPipe->GetPipeConnection0()->ClearConnection();
			NewPipe->GetPipeConnection0()->SetConnection(Conn0Partner);
		}
		if (UFGPipeConnectionComponentBase* Conn1Partner = OldPipe->GetPipeConnection1() ? OldPipe->GetPipeConnection1()->GetConnection() : nullptr)
		{
			OldPipe->GetPipeConnection1()->ClearConnection();
			NewPipe->GetPipeConnection1()->SetConnection(Conn1Partner);
		}

		// Cleanup old pipe and its children
		TArray<AActor*> ChildDismantleActors;
		OldPipe->GetChildDismantleActors_Implementation(ChildDismantleActors);
		for (AActor* ChildActor : ChildDismantleActors)
		{
			if (ChildActor) ChildActor->Destroy();
		}
		OldPipe->Destroy();

		// [#485] Replacement is live and the old pipe retired - the refund is now earned.
		GrantUpgradeRefund(Ledger, PlayerChar);

		// Track old->new mapping for post-upgrade validation / pipe batch fix
		OldToNewBuildableMap.Add(OldPipe, NewPipe);

		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Successfully upgraded pipe %s to %s"),
			*Buildable->GetName(), *NewPipe->GetClass()->GetName());
		return 1;
	}

	// Handle Conveyor Lifts using vanilla upgrade flow (SpawnHologramFromRecipe pattern)
	if (AFGBuildableConveyorLift* OldLift = Cast<AFGBuildableConveyorLift>(Buildable))
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Upgrading lift %s"),
			*OldLift->GetName());

		// Log connection info for debugging floor hole issues
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Lift connections - Conn0=%s Partner0=%s, Conn1=%s Partner1=%s"),
			OldLift->GetConnection0() ? *OldLift->GetConnection0()->GetName() : TEXT("null"),
			OldLift->GetConnection0() && OldLift->GetConnection0()->GetConnection() ? *OldLift->GetConnection0()->GetConnection()->GetOwner()->GetName() : TEXT("null"),
			OldLift->GetConnection1() ? *OldLift->GetConnection1()->GetName() : TEXT("null"),
			OldLift->GetConnection1() && OldLift->GetConnection1()->GetConnection() ? *OldLift->GetConnection1()->GetConnection()->GetOwner()->GetName() : TEXT("null"));

		// Spawn hologram using vanilla factory method (like MassUpgrade does)
		// This properly initializes all hologram context
		AFGHologram* LiftHologram = AFGHologram::SpawnHologramFromRecipe(
			ActualTargetRecipe,
			BuildGun,
			OldLift->GetActorLocation(),
			PlayerChar
		);

		if (!LiftHologram)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: SpawnHologramFromRecipe failed for lift"));
			return 0;
		}

		// Verify it's a lift hologram
		if (!LiftHologram->IsA<AFGConveyorLiftHologram>())
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Hologram is not a lift hologram"));
			LiftHologram->Destroy();
			return 0;
		}

		// Set blueprint designer (like MassUpgrade)
		LiftHologram->SetInsideBlueprintDesigner(OldLift->GetBlueprintDesigner());

		// #399: Copy the lift's AUTHORITATIVE top transform (mTopTransform). Its Z SIGN encodes flow
		// direction (up vs down); |Z| is the height. Source it from OldLift->GetTopTransform(), NOT the
		// output connector's relative transform: on a lift snapped to a floor-hole passthrough the
		// connector is inset into the foundation, so its relative Z has the wrong magnitude and can carry
		// the wrong SIGN, flipping the rebuilt lift. Every Extend lift site already uses GetTopTransform().
		{
			FTransform TopTransform = OldLift->GetTopTransform();
			if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
			{
				void* ValuePtr = TopTransformProp->ContainerPtrToValuePtr<void>(LiftHologram);
				TopTransformProp->CopySingleValue(ValuePtr, &TopTransform);
			}
		}

		// #399: Carry the old lift's snapped floor-hole passthroughs onto the upgrade hologram (index
		// [0]=bottom, [1]=top). Without this, a floor-hole-through lift loses all passthrough context, so
		// vanilla re-resolves its forced normal / mesh mode from the synthetic upgrade hit and flips the
		// lift (and mangles the whole run). Mirrors the Extend path (SFExtendCloneSpawner.cpp:1407-1428).
		// MP-safe: the upgrade batch is host-authoritative and this reads the OLD lift's own same-machine
		// passthrough actors, so nothing crosses the client/server actor-id boundary.
		{
			TArray<AFGBuildablePassthrough*> SnappedPassthroughs = OldLift->GetSnappedPassthroughs();
			if (SnappedPassthroughs.Num() > 0)
			{
				if (FProperty* SnappedProp = AFGConveyorLiftHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs")))
				{
					if (TArray<AFGBuildablePassthrough*>* HoloPassthroughs = SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(LiftHologram))
					{
						*HoloPassthroughs = SnappedPassthroughs;
					}
				}
			}
		}

		// Call TryUpgrade with hit result (same pattern as belts and power poles)
		FHitResult LiftHitResult(
			OldLift,
			LiftHologram->GetComponentByClass<UPrimitiveComponent>(),
			OldLift->GetActorLocation(),
			OldLift->GetActorRotation().Vector()
		);

		if (!LiftHologram->TryUpgrade(LiftHitResult))
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: TryUpgrade failed for lift"));
			LiftHologram->Destroy();
			return 0;
		}

		// [#485] Exact settlement (geometry-derived, shared with the panel). Charge = target recipe
		// x the old lift's own refund multiplier - the SAME factor its refund uses, applied to the
		// bare recipe (NOT on top of hologram GetBaseCost - that was the #432 double-charge).
		FSFUpgradeSettlementLedger Ledger;
		{
			const int32 PrepResult = PrepareUpgradeSettlement(OldLift, ActualTargetRecipe, PlayerChar, Ledger);
			if (PrepResult != 1)
			{
				LiftHologram->Destroy();
				return PrepResult == -1 ? -1 : 0;  // -1 = out of funds, abort remaining
			}
		}

		// [#485] Reserve the net charge; refund waits for a successful construct.
		ReserveUpgradeCharge(Ledger, PlayerChar);

		// Capture chain actor from old lift before PreUpgrade clears upgrade-sensitive state.
		if (AFGConveyorChainActor* OldChain = OldLift->GetConveyorChainActor())
		{
			PreDestroyChainActors.Add(OldChain);
		}

		// Let vanilla prepare the old lift for upgrade before constructing the replacement.
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Calling lift PreUpgrade before Construct"));
		OldLift->PreUpgrade_Implementation();

		// Construct the new lift
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = LiftHologram->Construct(ConstructedChildren, ConstructionID);

		LiftHologram->Destroy();

		AFGBuildableConveyorLift* NewLift = Cast<AFGBuildableConveyorLift>(ConstructedActor);
		if (!NewLift)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Construct failed for lift %s"), *OldLift->GetName());
			RollbackUpgradeCharge(Ledger, PlayerChar);  // [#485] failed target earns no refund
			return 0;
		}

		// Call vanilla upgrade method (like MassUpgrade does). This handles chain transfer
		// and other internal state after PreUpgrade prepared the old lift.
		OldLift->Upgrade_Implementation(NewLift);

		// Check for connections the upgrade hologram did not transfer. Do not manually
		// clear/set them in this prototype; connection mutation after Construct is one
		// suspected source of stale chain actor state.
		UFGFactoryConnectionComponent* OldLiftConn0 = OldLift->GetConnection0();
		UFGFactoryConnectionComponent* OldLiftConn1 = OldLift->GetConnection1();
		if (UFGFactoryConnectionComponent* OldConn0Partner = OldLiftConn0 ? OldLiftConn0->GetConnection() : nullptr)
		{
			const bool bTransferred = NewLift->GetConnection0() && NewLift->GetConnection0()->IsConnected() && NewLift->GetConnection0()->GetConnection() == OldConn0Partner;
			if (bTransferred)
			{
				UE_LOG(LogSmartUpgrade, Verbose,
					TEXT("UpgradeExecutionService: Lift Conn0 old partner %s transfer=yes"),
					*OldConn0Partner->GetOwner()->GetName());
			}
			else
			{
				UE_LOG(LogSmartUpgrade, Verbose,
					TEXT("UpgradeExecutionService: Lift Conn0 old partner %s transfer=no"),
					*OldConn0Partner->GetOwner()->GetName());
			}
		}

		if (UFGFactoryConnectionComponent* OldConn1Partner = OldLiftConn1 ? OldLiftConn1->GetConnection() : nullptr)
		{
			const bool bTransferred = NewLift->GetConnection1() && NewLift->GetConnection1()->IsConnected() && NewLift->GetConnection1()->GetConnection() == OldConn1Partner;
			if (bTransferred)
			{
				UE_LOG(LogSmartUpgrade, Verbose,
					TEXT("UpgradeExecutionService: Lift Conn1 old partner %s transfer=yes"),
					*OldConn1Partner->GetOwner()->GetName());
			}
			else
			{
				UE_LOG(LogSmartUpgrade, Verbose,
					TEXT("UpgradeExecutionService: Lift Conn1 old partner %s transfer=no"),
					*OldConn1Partner->GetOwner()->GetName());
			}
		}

		// Destroy old lift if it still exists
		if (IsValid(OldLift) && !OldLift->IsPendingKillPending())
		{
			TArray<AActor*> ChildDismantleActors;
			OldLift->GetChildDismantleActors_Implementation(ChildDismantleActors);
			for (AActor* ChildActor : ChildDismantleActors)
			{
				if (ChildActor && IsValid(ChildActor))
				{
					ChildActor->SetLifeSpan(0.001f);
				}
			}
			OldLift->SetLifeSpan(0.001f);
		}

		// [#485] Replacement is live and the old lift retired - the refund is now earned.
		GrantUpgradeRefund(Ledger, PlayerChar);

		// Track upgraded conveyor for chain rebuild
		UpgradedConveyors.Add(NewLift);

		// Track old->new mapping for fixing inter-connected batch upgrades
		OldToNewConveyorMap.Add(OldLift, NewLift);
		OldToNewBuildableMap.Add(OldLift, NewLift);

		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Successfully upgraded lift %s to %s"),
			*Buildable->GetName(), *NewLift->GetClass()->GetName());
		return 1;
	}

	// Handle Power Poles and Wall Outlets (simple buildables - no spline data)
	if (AFGBuildablePowerPole* OldPole = Cast<AFGBuildablePowerPole>(Buildable))
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Upgrading power pole/outlet %s (class=%s) Family=%d TargetTier=%d"),
			*OldPole->GetName(), *OldPole->GetClass()->GetName(),
			static_cast<int32>(ActualFamily), CurrentParams.TargetTier);

		// [#485] Exact settlement (shared with the panel; multiplier is 1 for simple buildables,
		// so poles/outlets keep their recipe-delta pricing). Transactional: charge reserved
		// before Construct, refund granted after success.
		FSFUpgradeSettlementLedger Ledger;
		{
			const int32 PrepResult = PrepareUpgradeSettlement(OldPole, ActualTargetRecipe, PlayerChar, Ledger);
			if (PrepResult != 1)
			{
				return PrepResult == -1 ? -1 : 0;  // -1 = out of funds, abort remaining
			}
		}

		// Spawn new pole using vanilla hologram (MassUpgrade pattern)
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Spawning hologram from recipe %s for %s"),
			*GetNameSafe(ActualTargetRecipe), *OldPole->GetName());
		AFGHologram* PoleHologram = AFGHologram::SpawnHologramFromRecipe(
			ActualTargetRecipe,
			BuildGun,
			OldPole->GetActorLocation(),
			PlayerChar
		);

		if (!PoleHologram)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to spawn pole hologram from recipe %s"),
				*GetNameSafe(ActualTargetRecipe));
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Spawned hologram %s (class=%s)"),
			*PoleHologram->GetName(), *PoleHologram->GetClass()->GetName());

		// Set blueprint designer (like MassUpgrade)
		PoleHologram->SetInsideBlueprintDesigner(OldPole->GetBlueprintDesigner());

		// Try upgrade with hit result (critical for wire transfer!)
		FHitResult HitResult(OldPole, PoleHologram->GetComponentByClass<UPrimitiveComponent>(),
			OldPole->GetActorLocation(), OldPole->GetActorRotation().Vector());
		if (!PoleHologram->TryUpgrade(HitResult))
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: TryUpgrade FAILED for %s (hologram=%s, target=%s)"),
				*OldPole->GetName(), *PoleHologram->GetClass()->GetName(), *GetNameSafe(NewBuildableClass));
			PoleHologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: TryUpgrade succeeded for %s"), *OldPole->GetName());

		// [#485] Reserve the net charge; refund waits for a successful construct.
		ReserveUpgradeCharge(Ledger, PlayerChar);

		// Construct the new pole
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = PoleHologram->Construct(ConstructedChildren, ConstructionID);

		PoleHologram->Destroy();

		AFGBuildablePowerPole* NewPole = Cast<AFGBuildablePowerPole>(ConstructedActor);
		if (!NewPole)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Construct failed for pole %s"), *OldPole->GetName());
			RollbackUpgradeCharge(Ledger, PlayerChar);  // [#485] failed target earns no refund
			return 0;
		}

		// Call vanilla upgrade methods (transfers wires to new pole)
		OldPole->PreUpgrade_Implementation();
		OldPole->Upgrade_Implementation(NewPole);

		// Destroy old pole and children
		TArray<AActor*> ChildDismantleActors;
		OldPole->GetChildDismantleActors_Implementation(ChildDismantleActors);
		for (AActor* ChildActor : ChildDismantleActors)
		{
			if (ChildActor && IsValid(ChildActor))
			{
				ChildActor->Destroy();
			}
		}
		OldPole->Destroy();

		// [#485] Replacement is live and the old pole retired - the refund is now earned.
		GrantUpgradeRefund(Ledger, PlayerChar);

		// Track old->new mapping for post-upgrade validation
		OldToNewBuildableMap.Add(OldPole, NewPole);

		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Successfully upgraded pole %s to %s"),
			*Buildable->GetName(), *NewPole->GetClass()->GetName());
		return 1;
	}

	// Unsupported type
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Unsupported buildable type: %s"),
		*Buildable->GetClass()->GetName());
	return 0;
}

void USFUpgradeExecutionService::CompleteUpgrade()
{
	// Stop the timer
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().ClearTimer(UpgradeTimerHandle);
	}

	USFChainActorService* ChainService = Subsystem ? Subsystem->GetChainActorService() : nullptr;
	int32 PreRepairQueuedGroups = 0;
	int32 PostRepairQueuedGroups = 0;

	// If this batch touched conveyors, invalidate their current chain ownership before
	// repairing endpoints. Otherwise thousands of ClearConnection/SetConnection calls can
	// leave old chain actors ticking stale segment/item state until the end of this frame.
	if (UpgradedConveyors.Num() > 0 && ChainService)
	{
		PreRepairQueuedGroups = ChainService->InvalidateAndQueueVanillaRebuildForBelts(UpgradedConveyors, PreDestroyChainActors);
		UE_LOG(LogSmartUpgrade, VeryVerbose,
			TEXT("ChainActorService: pre-repair conveyor invalidation queued %d group(s) before connection repair"),
			PreRepairQueuedGroups);
	}

	// Fix connections between conveyors that were both upgraded in this batch
	// (e.g., two lifts connected to each other - both need to point to the new versions)
	FixBatchConnectionReferences();
	FixBatchPipeConnectionReferences();

	// Option B: verify every expected edge exists on the post-upgrade graph and repair any that don't
	ValidateAndRepairConnections();

	// After endpoint repair, let FactoryGame re-register upgraded conveyors against the
	// completed live graph. Manual tick-group coalescing repeatedly produced sorted,
	// bucket-normalized groups that vanilla still rejected with NO_SEGMENTS zombies.
	if (UpgradedConveyors.Num() > 0)
	{
		if (ChainService)
		{
			PostRepairQueuedGroups = ChainService->ReRegisterAndQueueVanillaRebuildForBelts(UpgradedConveyors, PreDestroyChainActors);
			UE_LOG(LogSmartUpgrade, VeryVerbose,
				TEXT("⚙️ Upgrade batch complete - %d conveyors upgraded, chain groups queued pre-repair=%d post-reregister=%d"),
				UpgradedConveyors.Num(), PreRepairQueuedGroups, PostRepairQueuedGroups);

			// Safety net: if vanilla still produces NO_SEGMENTS zombies while processing
			// its pending rebuild queue, purge them after a few ticks. Orphaned tick
			// groups remain diagnostic-only; we do not bounce conveyors or mutate bucket
			// arrays from product code.
			ChainService->ScheduleDeferredZombiePurge(3.0f);
		}
		else
		{
			UE_LOG(LogSmartUpgrade, Verbose,
				TEXT("⚙️ Upgrade batch complete - %d conveyors upgraded (ChainActorService unavailable — chain topology may be stale until vanilla rebuilds next frame)"),
				UpgradedConveyors.Num());
		}
	}

	// NOTE: Cost deduction now happens per-item during ProcessSingleUpgrade
	// This allows us to abort immediately when funds run out
	AccumulatedCosts.Empty();

	// Spawn crate with overflow items (refunds that couldn't fit in inventory)
	if (!OverflowItems.IsEmpty() && CurrentParams.PlayerController)
	{
		AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(CurrentParams.PlayerController->GetPawn());
		if (Player && World)
		{
			TArray<FInventoryStack> Stacks;
			for (const auto& Entry : OverflowItems)
			{
				if (Entry.Key && Entry.Value > 0)
				{
					Stacks.Add(FInventoryStack(Entry.Value, Entry.Key));
					UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Adding %d %s to overflow crate"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
			}

			if (Stacks.Num() > 0)
			{
				AFGCrate* OutCrate = nullptr;
				AFGItemPickup_Spawnable::SpawnInventoryCrate(
					World,
					Stacks,
					Player->GetActorLocation() + FVector(0, 0, 50),  // Spawn slightly above player
					TArray<AActor*>(),
					OutCrate,
					EFGCrateType::CT_DismantleCrate
				);
				UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("UpgradeExecutionService: Spawned overflow crate with %d item types"),
					Stacks.Num());
			}
		}
	}
	// Settlement audit: one Display line per item type - exact charged/refunded/crated totals
	// for the whole batch. Item conservation is structural (same length multiplier both sides);
	// this line exists so any "the refund felt short" report becomes numbers in the log.
	for (const auto& Entry : BatchChargedTotals)
	{
		if (Entry.Key && Entry.Value != 0)
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UPGRADE SETTLEMENT: charged %d x %s"),
				Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
		}
	}
	for (const auto& Entry : BatchRefundedTotals)
	{
		if (Entry.Key && Entry.Value != 0)
		{
			const int32* Crated = OverflowItems.Find(Entry.Key);
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UPGRADE SETTLEMENT: refunded %d x %s (%d of those in the overflow crate)"),
				Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Crated ? *Crated : 0);
		}
	}
	OverflowItems.Empty();

	bUpgradeInProgress = false;
	WorkingResult.bCompleted = true;
	LastResult = WorkingResult;

	PendingUpgrades.Empty();
	UpgradedConveyors.Empty();
	OldToNewConveyorMap.Empty();
	OldToNewBuildableMap.Empty();
	SavedConnectionPairs.Empty();
	SavedPipeConnectionPairs.Empty();
	ExpectedConnectionEdges.Empty();
	PreDestroyChainActors.Empty();
	CurrentUpgradeIndex = 0;

	OnUpgradeExecutionCompleted.Broadcast(LastResult);
	UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Upgrade complete - Success=%d Fail=%d Skip=%d"),
		LastResult.SuccessCount, LastResult.FailCount, LastResult.SkipCount);

	// [UPGRADE-MP] When the request came from a REMOTE client (RCO-routed), echo the result back
	// to that player's RCO so their local service broadcasts to their panel - the same delivery
	// the audit service uses.
	if (CurrentParams.PlayerController && !CurrentParams.PlayerController->IsLocalController() && World)
	{
		// RCOs are not actors — resolve via the owning PC, never GetAllActorsOfClass
		// (the audit service's identical actor scan never found one; live finding 2026-06-10).
		if (USFRCO* RCO = CurrentParams.PlayerController->GetRemoteCallObjectOfClass<USFRCO>())
		{
			RCO->Client_ReceiveUpgradeResult(LastResult);
			UE_LOG(LogSmartUpgrade, Verbose,
				TEXT("[UPGRADE-MP] Sent upgrade result to client %s via RCO (Success=%d Fail=%d Skip=%d)."),
				*GetNameSafe(CurrentParams.PlayerController), LastResult.SuccessCount,
				LastResult.FailCount, LastResult.SkipCount);
		}
		else
		{
			UE_LOG(LogSmartUpgrade, Verbose,
				TEXT("[UPGRADE-MP] Could not resolve USFRCO for %s - upgrade result not delivered to the client."),
				*GetNameSafe(CurrentParams.PlayerController));
		}
	}
}

void USFUpgradeExecutionService::InjectUpgradeResult(const FSFUpgradeExecutionResult& Result)
{
	LastResult = Result;
	bUpgradeInProgress = false;
	OnUpgradeExecutionCompleted.Broadcast(LastResult);
	UE_LOG(LogSmartUpgrade, Verbose,
		TEXT("[UPGRADE-MP] Injected server upgrade result - Success=%d Fail=%d Skip=%d"),
		Result.SuccessCount, Result.FailCount, Result.SkipCount);
}

TSubclassOf<UFGRecipe> USFUpgradeExecutionService::GetUpgradeRecipe(ESFUpgradeFamily Family, int32 TargetTier) const
{
	if (!Subsystem)
	{
		return nullptr;
	}

	// Use subsystem's existing tier-to-recipe mappings
	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
			return Subsystem->GetBeltRecipeForTier(TargetTier);

		case ESFUpgradeFamily::Pipe:
			return Subsystem->GetPipeRecipeForTier(TargetTier, true);  // true = with indicator

		case ESFUpgradeFamily::Lift:
		{
			// Conveyor lift recipes: Recipe_ConveyorLiftMk1_C through Recipe_ConveyorLiftMk6_C
			using namespace SFAssetPaths::UpgradeRecipes;
			if (TargetTier >= 1 && TargetTier <= 6)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, ConveyorLift[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load lift recipe for tier %d"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::PowerPole:
		{
			// Power pole recipes: Recipe_PowerPoleMk1_C, Recipe_PowerPoleMk2_C, Recipe_PowerPoleMk3_C
			using namespace SFAssetPaths::UpgradeRecipes;
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, PowerPole[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load power pole recipe for tier %d"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletSingle:
		{
			// Single-sided wall outlets Mk1-Mk3 (some via MAM/FICSIT shop)
			using namespace SFAssetPaths::UpgradeRecipes;
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, WallOutletSingle[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load single wall outlet recipe for tier %d (may not be unlocked)"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletDouble:
		{
			// Double-sided wall outlets Mk1-Mk3 (some via MAM/FICSIT shop)
			using namespace SFAssetPaths::UpgradeRecipes;
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, WallOutletDouble[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load double wall outlet recipe for tier %d (may not be unlocked)"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::Pump:
		case ESFUpgradeFamily::Tower:
			// Pump: TODO if needed
			// Tower: Audit-only, no upgrade execution
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: GetUpgradeRecipe not implemented for family %d"),
				static_cast<int32>(Family));
			return nullptr;

		default:
			return nullptr;
	}
}

TSubclassOf<AFGBuildable> USFUpgradeExecutionService::GetBuildableClass(ESFUpgradeFamily Family, int32 Tier) const
{
	if (!Subsystem)
	{
		return nullptr;
	}

	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
		{
			UClass* BeltClass = Subsystem->GetBeltClassForTier(Tier);
			return BeltClass ? TSubclassOf<AFGBuildable>(BeltClass) : nullptr;
		}
		case ESFUpgradeFamily::Pipe:
		{
			UClass* PipeClass = Subsystem->GetPipeClassForTier(Tier, true);  // true = with indicator
			return PipeClass ? TSubclassOf<AFGBuildable>(PipeClass) : nullptr;
		}
		case ESFUpgradeFamily::Lift:
		{
			// Conveyor lifts: Build_ConveyorLiftMk1_C through Build_ConveyorLiftMk6_C
			static const TCHAR* LiftClassNames[] = {
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk1/Build_ConveyorLiftMk1.Build_ConveyorLiftMk1_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk2/Build_ConveyorLiftMk2.Build_ConveyorLiftMk2_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk3/Build_ConveyorLiftMk3.Build_ConveyorLiftMk3_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk4/Build_ConveyorLiftMk4.Build_ConveyorLiftMk4_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk5/Build_ConveyorLiftMk5.Build_ConveyorLiftMk5_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk6/Build_ConveyorLiftMk6.Build_ConveyorLiftMk6_C"),
			};
			if (Tier >= 1 && Tier <= 6)
			{
				UClass* LiftClass = LoadClass<AFGBuildable>(nullptr, LiftClassNames[Tier - 1]);
				if (LiftClass)
				{
					return TSubclassOf<AFGBuildable>(LiftClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load lift class for tier %d"), Tier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::PowerPole:
		{
			// Power poles: Build_PowerPoleMk1_C, Build_PowerPoleMk2_C, Build_PowerPoleMk3_C
			static const TCHAR* PoleClassNames[] = {
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1.Build_PowerPoleMk1_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk2/Build_PowerPoleMk2.Build_PowerPoleMk2_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3.Build_PowerPoleMk3_C"),
			};
			if (Tier >= 1 && Tier <= 3)
			{
				UClass* PoleClass = LoadClass<AFGBuildable>(nullptr, PoleClassNames[Tier - 1]);
				if (PoleClass)
				{
					return TSubclassOf<AFGBuildable>(PoleClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load power pole class for tier %d"), Tier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletSingle:
		{
			// Single-sided wall outlets Mk1-Mk3
			static const TCHAR* WallOutletSingleClassNames[] = {
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall.Build_PowerPoleWall_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk2.Build_PowerPoleWall_Mk2_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk3.Build_PowerPoleWall_Mk3_C"),
			};
			if (Tier >= 1 && Tier <= 3)
			{
				UClass* WallOutletClass = LoadClass<AFGBuildable>(nullptr, WallOutletSingleClassNames[Tier - 1]);
				if (WallOutletClass)
				{
					return TSubclassOf<AFGBuildable>(WallOutletClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load single wall outlet class for tier %d"), Tier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletDouble:
		{
			// Double-sided wall outlets Mk1-Mk3
			static const TCHAR* WallOutletDoubleClassNames[] = {
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble.Build_PowerPoleWallDouble_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk2.Build_PowerPoleWallDouble_Mk2_C"),
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk3.Build_PowerPoleWallDouble_Mk3_C"),
			};
			if (Tier >= 1 && Tier <= 3)
			{
				UClass* WallOutletClass = LoadClass<AFGBuildable>(nullptr, WallOutletDoubleClassNames[Tier - 1]);
				if (WallOutletClass)
				{
					return TSubclassOf<AFGBuildable>(WallOutletClass);
				}
				UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: Failed to load double wall outlet class for tier %d"), Tier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::Pump:
		case ESFUpgradeFamily::Tower:
			// Pump: TODO if needed
			// Tower: Audit-only, no upgrade execution
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("UpgradeExecutionService: GetBuildableClass not implemented for family %d"),
				static_cast<int32>(Family));
			return nullptr;
		default:
			return nullptr;
	}
}
