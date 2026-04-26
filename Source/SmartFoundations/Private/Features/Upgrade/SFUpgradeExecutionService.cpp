#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFChainActorService.h"
#include "FGBuildableSubsystem.h"
#include "EngineUtils.h"  // For TActorIterator
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "FGCentralStorageSubsystem.h"
#include "FGGameState.h"
#include "FGPlayerState.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePole.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Hologram/FGHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGRecipe.h"
#include "FGFactoryConnectionComponent.h"
#include "FGSplineBuildableInterface.h"
#include "FGConveyorChainActor.h"
#include "Equipment/FGBuildGun.h"
#include "Features/Extend/SFExtendService.h"
#include "FGItemPickup_Spawnable.h"
#include "FGCrate.h"

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
	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Initialized"));
}

void USFUpgradeExecutionService::Cleanup()
{
	CancelUpgrade();
	Subsystem = nullptr;
	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Cleaned up"));
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Upgrade already in progress"));
		return;
	}

	CurrentParams = Params;

	// Validate params
	// In traversal mode (HasSpecificBuildables), SourceTier can be 0 as we upgrade all tiers below target
	bool bIsTraversalMode = Params.HasSpecificBuildables();

	if (Params.Family == ESFUpgradeFamily::None || Params.TargetTier == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Invalid params - Family=%d TargetTier=%d"),
			static_cast<int32>(Params.Family), Params.TargetTier);
		return;
	}

	// For radius mode, require valid source tier
	if (!bIsTraversalMode && Params.SourceTier == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Invalid source tier for radius mode"));
		return;
	}

	// For radius mode, target must be greater than source
	if (!bIsTraversalMode && Params.TargetTier <= Params.SourceTier)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Target tier must be greater than source tier"));
		return;
	}

	// Get target recipe
	CurrentTargetRecipe = GetUpgradeRecipe(Params.Family, Params.TargetTier);
	if (!CurrentTargetRecipe)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Could not find recipe for Family=%d Tier=%d"),
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
		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: No targets found to upgrade"));
		WorkingResult.bCompleted = true;
		LastResult = WorkingResult;
		OnUpgradeExecutionCompleted.Broadcast(LastResult);
		return;
	}

	bUpgradeInProgress = true;
	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Starting SYNCHRONOUS upgrade of %d items from Tier %d to Tier %d"),
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Ran out of funds - aborting remaining upgrades"));
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
	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Upgrade cancelled after %d items"), WorkingResult.TotalProcessed);
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
		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Using %d specific buildables from traversal scan (TargetTier=%d)"),
			CurrentParams.SpecificBuildables.Num(), CurrentParams.TargetTier);

		for (const TWeakObjectPtr<AFGBuildable>& WeakBuildable : CurrentParams.SpecificBuildables)
		{
			if (AFGBuildable* Buildable = WeakBuildable.Get())
			{
				// Add all buildables below the target tier (they will be upgraded)
				int32 BuildableTier = USFUpgradeTraversalService::GetBuildableTier(Buildable);
				if (BuildableTier < CurrentParams.TargetTier && BuildableTier > 0)
				{
					PendingUpgrades.Add(Buildable);
				}
			}
		}

		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Filtered to %d targets below tier %d"),
			PendingUpgrades.Num(), CurrentParams.TargetTier);

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
		UE_LOG(LogSmartFoundations, Log,
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

		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Found %d conveyor seed(s) at tier %d"),
			FoundActors.Num(), CurrentParams.SourceTier);
	}
	else if (CurrentParams.Family == ESFUpgradeFamily::Pipe)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Gathering pipelines at tier %d (all indicator variants)"),
			CurrentParams.SourceTier);
		for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
		{
			AFGBuildablePipeline* Pipe = *It;
			if (!Pipe) continue;
			// Exclude pumps (they inherit from AFGBuildablePipeline in some variants) and junctions
			if (Cast<AFGBuildablePipelinePump>(Pipe)) continue;
			const FString PipeClassName = Pipe->GetClass()->GetName();
			if (PipeClassName.Contains(TEXT("Pump")) || PipeClassName.Contains(TEXT("Junction")))
			{
				continue;
			}
			const int32 PipeTier = USFUpgradeTraversalService::GetBuildableTier(Pipe);
			if (PipeTier != CurrentParams.SourceTier) continue;
			FoundActors.Add(Pipe);
		}
		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Found %d pipelines at tier %d"),
			FoundActors.Num(), CurrentParams.SourceTier);
	}
	else
	{
		// Get the buildable class for the source tier
		TSubclassOf<AFGBuildable> SourceClass = GetBuildableClass(CurrentParams.Family, CurrentParams.SourceTier);
		if (!SourceClass)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Could not find source class for Family=%d Tier=%d"),
				static_cast<int32>(CurrentParams.Family), CurrentParams.SourceTier);
			return;
		}

		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Looking for source class %s (Full: %s) (Family=%d Tier=%d)"),
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

		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Exact class match found %d actors"), FoundActors.Num());
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

	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Gathered %d targets for upgrade"), PendingUpgrades.Num());
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
			UE_LOG(LogSmartFoundations, Log,
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

			// Radius mode still honors SourceTier as the user's requested source tier.
			if (!CurrentParams.HasSpecificBuildables() && CurrentParams.SourceTier > 0 && Tier != CurrentParams.SourceTier)
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
			UE_LOG(LogSmartFoundations, Log,
				TEXT("UpgradeExecutionService: Skipping conveyor cohort containing %s - adding %d member(s) would exceed MaxItems=%d"),
				*Seed->GetName(), EligibleInCohort.Num(), CurrentParams.MaxItems);
			continue;
		}

		ExpandedTargets.Append(EligibleInCohort);
	}

	PendingUpgrades = MoveTemp(ExpandedTargets);

	UE_LOG(LogSmartFoundations, Log,
		TEXT("UpgradeExecutionService: Conveyor cohort normalization - seeds=%d cohorts=%d targets=%d skipped_partial=%d skipped_max_items=%d"),
		ConveyorSeeds.Num(), CohortCount, PendingUpgrades.Num(), SkippedPartialCohorts, SkippedMaxItemCohorts);
}

void USFUpgradeExecutionService::CollectConnectedConveyorCohort(AFGBuildableConveyorBase* StartConveyor, TSet<AFGBuildableConveyorBase*>& OutCohort) const
{
	if (!IsValid(StartConveyor)) return;

	constexpr int32 MaxCohortSize = 10000;
	TArray<AFGBuildableConveyorBase*> Stack;
	Stack.Add(StartConveyor);

	while (Stack.Num() > 0 && OutCohort.Num() < MaxCohortSize)
	{
		AFGBuildableConveyorBase* Current = Stack.Pop(false);
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
		UE_LOG(LogSmartFoundations, Warning,
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
	if (!IsValid(Conveyor) || CurrentParams.Radius <= 0.0f)
	{
		return true;
	}

	const float RadiusSq = CurrentParams.Radius * CurrentParams.Radius;
	const float Length = Conveyor->GetLength();
	const float SampleOffsets[] = { 0.0f, Length * 0.25f, Length * 0.5f, Length * 0.75f, Length };

	for (float Offset : SampleOffsets)
	{
		FVector SampleLocation = Conveyor->GetActorLocation();
		FVector SampleDirection = FVector::ForwardVector;
		Conveyor->GetLocationAndDirectionAtOffset(FMath::Clamp(Offset, 0.0f, Length), SampleLocation, SampleDirection);
		if (FVector::DistSquared(SampleLocation, CurrentParams.Origin) > RadiusSq)
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
		UE_LOG(LogSmartFoundations, Warning,
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Cannot determine family for %s"),
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
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: No recipe for Family=%d Tier=%d"),
				static_cast<int32>(ActualFamily), CurrentParams.TargetTier);
			return 0;
		}
	}

	// Get the buildable class for this buildable's actual family
	TSubclassOf<AFGBuildable> NewBuildableClass = GetBuildableClass(ActualFamily, CurrentParams.TargetTier);
	if (!NewBuildableClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: No buildable class for Family=%d Tier=%d"),
			static_cast<int32>(ActualFamily), CurrentParams.TargetTier);
		return 0;
	}

	// SKIP if buildable is already the target class (no upgrade needed)
	if (Buildable->GetClass() == NewBuildableClass)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Skipping %s - already target tier"),
			*Buildable->GetName());
		return 1;  // Return 1 to count as "handled" not failed
	}

	// Get buildable subsystem
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);

	// Get player controller and build gun for hologram spawning
	AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: No player controller"));
		return 0;
	}

	AFGCharacterPlayer* PlayerChar = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!PlayerChar)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: No player character"));
		return 0;
	}

	AFGBuildGun* BuildGun = PlayerChar->GetBuildGun();
	if (!BuildGun)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: No build gun"));
		return 0;
	}

	// Handle Conveyor Belts using MassUpgrade's proven approach
	// Key: TryUpgrade + DoMultiStepPlacement + GenerateAndUpdateSpline + PreUpgrade/Upgrade_Implementation
	if (AFGBuildableConveyorBelt* OldBelt = Cast<AFGBuildableConveyorBelt>(Buildable))
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Upgrading belt %s (bucket=%d)"),
			*OldBelt->GetName(), OldBelt->GetConveyorBucketID());

		// Skip if already target class
		if (OldBelt->GetClass() == NewBuildableClass)
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Belt already target class, skipping"));
			return 1;
		}

		// STEP 1: Spawn hologram using vanilla factory method (exactly like MassUpgrade)
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 1: Spawning hologram via SpawnHologramFromRecipe..."));
		AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(
			ActualTargetRecipe,
			BuildGun,
			OldBelt->GetActorLocation(),
			PlayerChar
		);

		if (!Hologram)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: SpawnHologramFromRecipe failed"));
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 1: Hologram spawned: %s"), *Hologram->GetName());

		// STEP 2: Set blueprint designer context if applicable
		Hologram->SetInsideBlueprintDesigner(OldBelt->GetBlueprintDesigner());

		// STEP 3: Create hit result and call TryUpgrade (NOT SetupUpgradeTarget!)
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 2: Calling TryUpgrade..."));
		FHitResult HitResult(
			OldBelt,
			Hologram->GetComponentByClass<UPrimitiveComponent>(),
			OldBelt->GetActorLocation(),
			OldBelt->GetActorRotation().Vector()
		);

		if (!Hologram->TryUpgrade(HitResult))
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: TryUpgrade failed"));
			Hologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 2: TryUpgrade succeeded"));

		// STEP 4: Validate placement
		UFGInventoryComponent* PlayerInventory = PlayerChar->GetInventory();
		Hologram->ValidatePlacementAndCost(PlayerInventory);

		if (!Hologram->IsUpgrade())
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Hologram is not upgrade after validation"));
			Hologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 3: ValidatePlacementAndCost passed, IsUpgrade=true"));

		// STEP 5: Run multi-step placement loop (critical for spline buildables!)
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 4: Running DoMultiStepPlacement loop..."));
		while (Hologram->CanTakeNextBuildStep() && !Hologram->DoMultiStepPlacement(true))
		{
			// Loop until placement is complete
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 4: DoMultiStepPlacement complete"));

		// STEP 5.5: Check affordability and deduct costs per-item
		bool bNoCost = PlayerInventory->GetNoBuildCost();
		if (!bNoCost)
		{
			// Calculate net cost for this item
			TMap<TSubclassOf<UFGItemDescriptor>, int32> ItemCost;

			// Get cost from hologram (length-aware for belts)
			TArray<FItemAmount> BaseCost = Hologram->GetBaseCost();
			UE_LOG(LogSmartFoundations, Log, TEXT("Belt upgrade cost: Hologram->GetBaseCost() returned %d items"), BaseCost.Num());
			for (const FItemAmount& ItemAmount : BaseCost)
			{
				if (ItemAmount.ItemClass)
				{
					ItemCost.FindOrAdd(ItemAmount.ItemClass) += ItemAmount.Amount;
					UE_LOG(LogSmartFoundations, Log, TEXT("  New belt cost: +%d %s"),
						ItemAmount.Amount, *UFGItemDescriptor::GetItemName(ItemAmount.ItemClass).ToString());
				}
			}

			// Get refund from old buildable
			TArray<FInventoryStack> Refunds;
			OldBelt->GetDismantleRefundReturns(Refunds);
			UE_LOG(LogSmartFoundations, Log, TEXT("Belt upgrade refund: GetDismantleRefundReturns() returned %d items"), Refunds.Num());
			for (const FInventoryStack& Stack : Refunds)
			{
				if (Stack.Item.GetItemClass())
				{
					ItemCost.FindOrAdd(Stack.Item.GetItemClass()) -= Stack.NumItems;
					UE_LOG(LogSmartFoundations, Log, TEXT("  Old belt refund: -%d %s"),
						Stack.NumItems, *UFGItemDescriptor::GetItemName(Stack.Item.GetItemClass()).ToString());
				}
			}

			// Log net cost
			for (const auto& Entry : ItemCost)
			{
				if (Entry.Key)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("  Net cost: %d %s"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
			}

			// Check if player can afford this item
			UWorld* CostWorld = Subsystem ? Subsystem->GetWorld() : nullptr;
			AFGCentralStorageSubsystem* CentralStorage = CostWorld ? AFGCentralStorageSubsystem::Get(CostWorld) : nullptr;

			for (const auto& Entry : ItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					int32 Available = PlayerInventory->GetNumItems(Entry.Key);
					if (CentralStorage)
					{
						Available += CentralStorage->GetNumItemsFromCentralStorage(Entry.Key);
					}
					if (Available < Entry.Value)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Cannot afford upgrade - need %d %s, have %d"),
							Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Available);
						Hologram->Destroy();
						return -1;  // Out of funds - abort remaining
					}
				}
			}

			// Deduct costs immediately
			AFGPlayerState* PlayerState = PlayerChar->GetPlayerState<AFGPlayerState>();
			bool bTakeFromInventoryFirst = PlayerState ? PlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;

			for (const auto& Entry : ItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(
						PlayerInventory, CentralStorage, bTakeFromInventoryFirst, Entry.Key, Entry.Value);
					UE_LOG(LogSmartFoundations, Log, TEXT("Belt upgrade: Deducted %d %s"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
				else if (Entry.Value < 0 && Entry.Key)
				{
					int32 RefundAmount = -Entry.Value;
					int32 Added = PlayerInventory->AddStack(FInventoryStack(RefundAmount, Entry.Key), true);
					if (Added < RefundAmount)
					{
						int32 Overflow = RefundAmount - Added;
						OverflowItems.FindOrAdd(Entry.Key) += Overflow;
						UE_LOG(LogSmartFoundations, Warning, TEXT("Belt upgrade: Refund overflow - %d/%d %s added, %d to crate"),
							Added, RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Overflow);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Log, TEXT("Belt upgrade: Refunded %d %s"),
							RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
					}
				}
			}
		}

		// STEP 6: Generate spline for belt hologram (exactly like MassUpgrade)
		// Friend access granted via AccessTransformers.ini
		if (AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram))
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 5: Calling GenerateAndUpdateSpline..."));
			BeltHologram->GenerateAndUpdateSpline(HitResult);
			UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 5: GenerateAndUpdateSpline complete"));
		}

		// Capture chain actor from old belt before PreUpgrade clears upgrade-sensitive state.
		if (AFGConveyorChainActor* OldChain = OldBelt->GetConveyorChainActor())
		{
			PreDestroyChainActors.Add(OldChain);
		}

		// STEP 7: Let vanilla prepare the old belt for upgrade before constructing the replacement.
		// PreUpgrade is documented as the hook that clears connections/state that can interfere
		// with upgrades; calling it after Construct leaves the replacement born into stale chain state.
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 6a: Calling PreUpgrade_Implementation before Construct..."));
		OldBelt->PreUpgrade_Implementation();

		// STEP 8: Construct the new belt
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 6: Calling Construct()..."));
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = Hologram->Construct(ConstructedChildren, ConstructionID);

		Hologram->Destroy();

		AFGBuildableConveyorBelt* NewBelt = Cast<AFGBuildableConveyorBelt>(ConstructedActor);
		if (!NewBelt)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Construct failed for %s"), *OldBelt->GetName());
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 6: Construct() created new belt: %s"), *NewBelt->GetName());

		// STEP 9: Call upgrade interface method on OLD belt (critical for chain transfer!)
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 7: Calling Upgrade_Implementation..."));
		OldBelt->Upgrade_Implementation(NewBelt);
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 7: Upgrade interface method complete"));

		// STEP 10: Check connections. The upgrade hologram plus PreUpgrade/Upgrade should
		// transfer them; this prototype intentionally avoids manual ClearConnection/
		// SetConnection after Construct because that can leave chain actors with stale
		// segment state.
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 8: Checking connections..."));
		UFGFactoryConnectionComponent* OldConn0 = OldBelt->GetConnection0();
		UFGFactoryConnectionComponent* OldConn1 = OldBelt->GetConnection1();
		UFGFactoryConnectionComponent* NewConn0 = NewBelt->GetConnection0();
		UFGFactoryConnectionComponent* NewConn1 = NewBelt->GetConnection1();

		// Log chain actor state after Construct for diagnostics
		UE_LOG(LogSmartFoundations, Log, TEXT("⚙️ STEP 8: NewBelt chain=%s, NewConn0 connected=%s, NewConn1 connected=%s"),
			NewBelt->GetConveyorChainActor() ? *NewBelt->GetConveyorChainActor()->GetName() : TEXT("null"),
			NewConn0 && NewConn0->IsConnected() ? TEXT("yes") : TEXT("no"),
			NewConn1 && NewConn1->IsConnected() ? TEXT("yes") : TEXT("no"));

		if (OldConn0 && NewConn0)
		{
			if (UFGFactoryConnectionComponent* Partner0 = OldConn0->GetConnection())
			{
				if (NewConn0->IsConnected() && NewConn0->GetConnection() == Partner0)
				{
					UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 8a: Conn0 already connected to %s by vanilla upgrade flow"),
						*Partner0->GetOwner()->GetName());
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("⚙️ STEP 8a: Conn0 old partner %s was not transferred by vanilla upgrade flow"),
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
					UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 8b: Conn1 already connected to %s by vanilla upgrade flow"),
						*Partner1->GetOwner()->GetName());
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("⚙️ STEP 8b: Conn1 old partner %s was not transferred by vanilla upgrade flow"),
						*Partner1->GetOwner()->GetName());
				}
			}
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 8: Connections complete"));

		// STEP 11: Destroy old belt and children
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 9: Destroying old belt..."));
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
		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ STEP 9: Old belt destroyed"));

		// Track upgraded conveyor
		UpgradedConveyors.Add(NewBelt);

		// Track old->new mapping for fixing inter-connected batch upgrades
		OldToNewConveyorMap.Add(OldBelt, NewBelt);
		OldToNewBuildableMap.Add(OldBelt, NewBelt);

		UE_LOG(LogSmartFoundations, Display, TEXT("⚙️ UPGRADE COMPLETE: %s → %s"),
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
		UE_LOG(LogSmartFoundations, Display,
			TEXT("UpgradeExecutionService: Pipe upgrade preserving indicator style (source=%s, bWithIndicator=%s, target class=%s)"),
			*OldPipeClassName, bWithIndicator ? TEXT("true") : TEXT("false"),
			NewBuildableClass ? *NewBuildableClass->GetName() : TEXT("null"));

		// Get spline data from old pipe
		TArray<FSplinePointData> SplineData = OldPipe->GetSplinePointData();
		if (SplineData.Num() < 2)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Pipe has insufficient spline points"));
			return 0;
		}

		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Old pipe %s with %d spline points"),
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
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to spawn Smart pipe hologram"));
			return 0;
		}

		// Set build class and recipe BEFORE FinishSpawning
		SmartHolo->SetBuildClass(NewBuildableClass);
		SmartHolo->SetRecipe(ActualTargetRecipe);

		// Finish spawning
		SmartHolo->FinishSpawning(OldPipe->GetActorTransform());

		// Set up spline data from old pipe
		SmartHolo->SetSplineDataAndUpdate(SplineData);

		// Check affordability and deduct costs per-item for pipe
		UFGInventoryComponent* PipePlayerInventory = PlayerChar->GetInventory();
		bool bPipeNoCost = PipePlayerInventory ? PipePlayerInventory->GetNoBuildCost() : false;
		if (!bPipeNoCost)
		{
			// Calculate net cost for this pipe
			TMap<TSubclassOf<UFGItemDescriptor>, int32> PipeItemCost;

			// Get cost from hologram (length-aware for pipes)
			TArray<FItemAmount> PipeBaseCost = SmartHolo->GetBaseCost();
			for (const FItemAmount& ItemAmount : PipeBaseCost)
			{
				if (ItemAmount.ItemClass)
				{
					PipeItemCost.FindOrAdd(ItemAmount.ItemClass) += ItemAmount.Amount;
				}
			}

			// Get refund from old pipe
			TArray<FInventoryStack> PipeRefunds;
			OldPipe->GetDismantleRefundReturns(PipeRefunds);
			for (const FInventoryStack& Stack : PipeRefunds)
			{
				if (Stack.Item.GetItemClass())
				{
					PipeItemCost.FindOrAdd(Stack.Item.GetItemClass()) -= Stack.NumItems;
				}
			}

			// Check if player can afford this pipe
			UWorld* PipeCostWorld = Subsystem ? Subsystem->GetWorld() : nullptr;
			AFGCentralStorageSubsystem* PipeCentralStorage = PipeCostWorld ? AFGCentralStorageSubsystem::Get(PipeCostWorld) : nullptr;

			for (const auto& Entry : PipeItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					int32 Available = PipePlayerInventory->GetNumItems(Entry.Key);
					if (PipeCentralStorage)
					{
						Available += PipeCentralStorage->GetNumItemsFromCentralStorage(Entry.Key);
					}
					if (Available < Entry.Value)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Cannot afford pipe upgrade - need %d %s, have %d"),
							Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Available);
						SmartHolo->Destroy();
						return -1;  // Out of funds - abort remaining
					}
				}
			}

			// Deduct costs immediately
			AFGPlayerState* PipePlayerState = PlayerChar->GetPlayerState<AFGPlayerState>();
			bool bPipeTakeFromInventoryFirst = PipePlayerState ? PipePlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;

			for (const auto& Entry : PipeItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(
						PipePlayerInventory, PipeCentralStorage, bPipeTakeFromInventoryFirst, Entry.Key, Entry.Value);
					UE_LOG(LogSmartFoundations, Log, TEXT("Pipe upgrade: Deducted %d %s"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
				else if (Entry.Value < 0 && Entry.Key)
				{
					int32 RefundAmount = -Entry.Value;
					int32 Added = PipePlayerInventory->AddStack(FInventoryStack(RefundAmount, Entry.Key), true);
					if (Added < RefundAmount)
					{
						int32 Overflow = RefundAmount - Added;
						OverflowItems.FindOrAdd(Entry.Key) += Overflow;
						UE_LOG(LogSmartFoundations, Warning, TEXT("Pipe upgrade: Refund overflow - %d/%d %s added, %d to crate"),
							Added, RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Overflow);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Log, TEXT("Pipe upgrade: Refunded %d %s"),
							RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
					}
				}
			}
		}

		// Construct the new pipe
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = SmartHolo->Construct(ConstructedChildren, ConstructionID);

		SmartHolo->Destroy();

		AFGBuildablePipeline* NewPipe = Cast<AFGBuildablePipeline>(ConstructedActor);
		if (!NewPipe)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Construct failed for pipe %s"), *OldPipe->GetName());
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

		// Track old->new mapping for post-upgrade validation / pipe batch fix
		OldToNewBuildableMap.Add(OldPipe, NewPipe);

		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Successfully upgraded pipe %s to %s"),
			*Buildable->GetName(), *NewPipe->GetClass()->GetName());
		return 1;
	}

	// Handle Conveyor Lifts using vanilla upgrade flow (SpawnHologramFromRecipe pattern)
	if (AFGBuildableConveyorLift* OldLift = Cast<AFGBuildableConveyorLift>(Buildable))
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Upgrading lift %s"),
			*OldLift->GetName());

		// Log connection info for debugging floor hole issues
		UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Lift connections - Conn0=%s Partner0=%s, Conn1=%s Partner1=%s"),
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
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: SpawnHologramFromRecipe failed for lift"));
			return 0;
		}

		// Verify it's a lift hologram
		if (!LiftHologram->IsA<AFGConveyorLiftHologram>())
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Hologram is not a lift hologram"));
			LiftHologram->Destroy();
			return 0;
		}

		// Set blueprint designer (like MassUpgrade)
		LiftHologram->SetInsideBlueprintDesigner(OldLift->GetBlueprintDesigner());

		// Copy the top transform (lift height) from old lift using reflection
		if (OldLift->GetConnection1())
		{
			FTransform TopTransform = OldLift->GetConnection1()->GetRelativeTransform();
			if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
			{
				void* ValuePtr = TopTransformProp->ContainerPtrToValuePtr<void>(LiftHologram);
				TopTransformProp->CopySingleValue(ValuePtr, &TopTransform);
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
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: TryUpgrade failed for lift"));
			LiftHologram->Destroy();
			return 0;
		}

		// Check affordability and deduct costs per-item for lift
		UFGInventoryComponent* LiftPlayerInventory = PlayerChar->GetInventory();
		bool bLiftNoCost = LiftPlayerInventory ? LiftPlayerInventory->GetNoBuildCost() : false;
		if (!bLiftNoCost)
		{
			// Calculate net cost for this lift
			TMap<TSubclassOf<UFGItemDescriptor>, int32> LiftItemCost;

			// Get cost from hologram
			float LiftRefundMultiplier = OldLift->GetDismantleRefundReturnsMultiplier();
			for (const FItemAmount& ItemAmount : LiftHologram->GetBaseCost())
			{
				if (ItemAmount.ItemClass)
				{
					LiftItemCost.FindOrAdd(ItemAmount.ItemClass) += FMath::CeilToInt(ItemAmount.Amount * LiftRefundMultiplier);
				}
			}

			// Get refund from old lift
			TArray<FInventoryStack> LiftRefunds;
			OldLift->GetDismantleRefundReturns(LiftRefunds);
			for (const FInventoryStack& Stack : LiftRefunds)
			{
				if (Stack.Item.GetItemClass())
				{
					LiftItemCost.FindOrAdd(Stack.Item.GetItemClass()) -= Stack.NumItems;
				}
			}

			// Check if player can afford this lift
			UWorld* LiftCostWorld = Subsystem ? Subsystem->GetWorld() : nullptr;
			AFGCentralStorageSubsystem* LiftCentralStorage = LiftCostWorld ? AFGCentralStorageSubsystem::Get(LiftCostWorld) : nullptr;

			for (const auto& Entry : LiftItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					int32 Available = LiftPlayerInventory->GetNumItems(Entry.Key);
					if (LiftCentralStorage)
					{
						Available += LiftCentralStorage->GetNumItemsFromCentralStorage(Entry.Key);
					}
					if (Available < Entry.Value)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Cannot afford lift upgrade - need %d %s, have %d"),
							Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Available);
						LiftHologram->Destroy();
						return -1;  // Out of funds - abort remaining
					}
				}
			}

			// Deduct costs immediately
			AFGPlayerState* LiftPlayerState = PlayerChar->GetPlayerState<AFGPlayerState>();
			bool bLiftTakeFromInventoryFirst = LiftPlayerState ? LiftPlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;

			for (const auto& Entry : LiftItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(
						LiftPlayerInventory, LiftCentralStorage, bLiftTakeFromInventoryFirst, Entry.Key, Entry.Value);
					UE_LOG(LogSmartFoundations, Log, TEXT("Lift upgrade: Deducted %d %s"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
				else if (Entry.Value < 0 && Entry.Key)
				{
					int32 RefundAmount = -Entry.Value;
					int32 Added = LiftPlayerInventory->AddStack(FInventoryStack(RefundAmount, Entry.Key), true);
					if (Added < RefundAmount)
					{
						int32 Overflow = RefundAmount - Added;
						OverflowItems.FindOrAdd(Entry.Key) += Overflow;
						UE_LOG(LogSmartFoundations, Warning, TEXT("Lift upgrade: Refund overflow - %d/%d %s added, %d to crate"),
							Added, RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Overflow);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Log, TEXT("Lift upgrade: Refunded %d %s"),
							RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
					}
				}
			}
		}

		// Capture chain actor from old lift before PreUpgrade clears upgrade-sensitive state.
		if (AFGConveyorChainActor* OldChain = OldLift->GetConveyorChainActor())
		{
			PreDestroyChainActors.Add(OldChain);
		}

		// Let vanilla prepare the old lift for upgrade before constructing the replacement.
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Calling lift PreUpgrade before Construct"));
		OldLift->PreUpgrade_Implementation();

		// Construct the new lift
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = LiftHologram->Construct(ConstructedChildren, ConstructionID);

		LiftHologram->Destroy();

		AFGBuildableConveyorLift* NewLift = Cast<AFGBuildableConveyorLift>(ConstructedActor);
		if (!NewLift)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Construct failed for lift %s"), *OldLift->GetName());
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
				UE_LOG(LogSmartFoundations, Log,
					TEXT("UpgradeExecutionService: Lift Conn0 old partner %s transfer=yes"),
					*OldConn0Partner->GetOwner()->GetName());
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("UpgradeExecutionService: Lift Conn0 old partner %s transfer=no"),
					*OldConn0Partner->GetOwner()->GetName());
			}
		}

		if (UFGFactoryConnectionComponent* OldConn1Partner = OldLiftConn1 ? OldLiftConn1->GetConnection() : nullptr)
		{
			const bool bTransferred = NewLift->GetConnection1() && NewLift->GetConnection1()->IsConnected() && NewLift->GetConnection1()->GetConnection() == OldConn1Partner;
			if (bTransferred)
			{
				UE_LOG(LogSmartFoundations, Log,
					TEXT("UpgradeExecutionService: Lift Conn1 old partner %s transfer=yes"),
					*OldConn1Partner->GetOwner()->GetName());
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning,
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

		// Track upgraded conveyor for chain rebuild
		UpgradedConveyors.Add(NewLift);

		// Track old->new mapping for fixing inter-connected batch upgrades
		OldToNewConveyorMap.Add(OldLift, NewLift);
		OldToNewBuildableMap.Add(OldLift, NewLift);

		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Successfully upgraded lift %s to %s"),
			*Buildable->GetName(), *NewLift->GetClass()->GetName());
		return 1;
	}

	// Handle Power Poles and Wall Outlets (simple buildables - no spline data)
	if (AFGBuildablePowerPole* OldPole = Cast<AFGBuildablePowerPole>(Buildable))
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Upgrading power pole/outlet %s (class=%s) Family=%d TargetTier=%d"),
			*OldPole->GetName(), *OldPole->GetClass()->GetName(),
			static_cast<int32>(ActualFamily), CurrentParams.TargetTier);

		// Check affordability and deduct costs per-item for pole
		UFGInventoryComponent* PolePlayerInventory = PlayerChar->GetInventory();
		bool bPoleNoCost = PolePlayerInventory ? PolePlayerInventory->GetNoBuildCost() : false;
		if (!bPoleNoCost)
		{
			// Calculate net cost for this pole using recipe-based costs
			TMap<TSubclassOf<UFGItemDescriptor>, int32> PoleItemCost;

			// Get cost from target recipe
			const UFGRecipe* TargetCDO = ActualTargetRecipe->GetDefaultObject<UFGRecipe>();
			if (TargetCDO)
			{
				for (const FItemAmount& ItemAmount : TargetCDO->GetIngredients())
				{
					if (ItemAmount.ItemClass)
					{
						PoleItemCost.FindOrAdd(ItemAmount.ItemClass) += ItemAmount.Amount;
					}
				}
			}

			// Get refund from old pole
			TArray<FInventoryStack> PoleRefunds;
			OldPole->GetDismantleRefundReturns(PoleRefunds);
			for (const FInventoryStack& Stack : PoleRefunds)
			{
				if (Stack.Item.GetItemClass())
				{
					PoleItemCost.FindOrAdd(Stack.Item.GetItemClass()) -= Stack.NumItems;
				}
			}

			// Check if player can afford this pole
			UWorld* PoleCostWorld = Subsystem ? Subsystem->GetWorld() : nullptr;
			AFGCentralStorageSubsystem* PoleCentralStorage = PoleCostWorld ? AFGCentralStorageSubsystem::Get(PoleCostWorld) : nullptr;

			for (const auto& Entry : PoleItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					int32 Available = PolePlayerInventory->GetNumItems(Entry.Key);
					if (PoleCentralStorage)
					{
						Available += PoleCentralStorage->GetNumItemsFromCentralStorage(Entry.Key);
					}
					if (Available < Entry.Value)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Cannot afford pole upgrade - need %d %s, have %d"),
							Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Available);
						return -1;  // Out of funds - abort remaining
					}
				}
			}

			// Deduct costs immediately
			AFGPlayerState* PolePlayerState = PlayerChar->GetPlayerState<AFGPlayerState>();
			bool bPoleTakeFromInventoryFirst = PolePlayerState ? PolePlayerState->GetTakeFromInventoryBeforeCentralStorage() : true;

			for (const auto& Entry : PoleItemCost)
			{
				if (Entry.Value > 0 && Entry.Key)
				{
					UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(
						PolePlayerInventory, PoleCentralStorage, bPoleTakeFromInventoryFirst, Entry.Key, Entry.Value);
					UE_LOG(LogSmartFoundations, Log, TEXT("Pole upgrade: Deducted %d %s"),
						Entry.Value, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
				}
				else if (Entry.Value < 0 && Entry.Key)
				{
					int32 RefundAmount = -Entry.Value;
					int32 Added = PolePlayerInventory->AddStack(FInventoryStack(RefundAmount, Entry.Key), true);
					if (Added < RefundAmount)
					{
						int32 Overflow = RefundAmount - Added;
						OverflowItems.FindOrAdd(Entry.Key) += Overflow;
						UE_LOG(LogSmartFoundations, Warning, TEXT("Pole upgrade: Refund overflow - %d/%d %s added, %d to crate"),
							Added, RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString(), Overflow);
					}
					else
					{
						UE_LOG(LogSmartFoundations, Log, TEXT("Pole upgrade: Refunded %d %s"),
							RefundAmount, *UFGItemDescriptor::GetItemName(Entry.Key).ToString());
					}
				}
			}
		}

		// Spawn new pole using vanilla hologram (MassUpgrade pattern)
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Spawning hologram from recipe %s for %s"),
			*GetNameSafe(ActualTargetRecipe), *OldPole->GetName());
		AFGHologram* PoleHologram = AFGHologram::SpawnHologramFromRecipe(
			ActualTargetRecipe,
			BuildGun,
			OldPole->GetActorLocation(),
			PlayerChar
		);

		if (!PoleHologram)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to spawn pole hologram from recipe %s"),
				*GetNameSafe(ActualTargetRecipe));
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Spawned hologram %s (class=%s)"),
			*PoleHologram->GetName(), *PoleHologram->GetClass()->GetName());

		// Set blueprint designer (like MassUpgrade)
		PoleHologram->SetInsideBlueprintDesigner(OldPole->GetBlueprintDesigner());

		// Try upgrade with hit result (critical for wire transfer!)
		FHitResult HitResult(OldPole, PoleHologram->GetComponentByClass<UPrimitiveComponent>(),
			OldPole->GetActorLocation(), OldPole->GetActorRotation().Vector());
		if (!PoleHologram->TryUpgrade(HitResult))
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: TryUpgrade FAILED for %s (hologram=%s, target=%s)"),
				*OldPole->GetName(), *PoleHologram->GetClass()->GetName(), *GetNameSafe(NewBuildableClass));
			PoleHologram->Destroy();
			return 0;
		}
		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: TryUpgrade succeeded for %s"), *OldPole->GetName());

		// Construct the new pole
		TArray<AActor*> ConstructedChildren;
		FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();
		AActor* ConstructedActor = PoleHologram->Construct(ConstructedChildren, ConstructionID);

		PoleHologram->Destroy();

		AFGBuildablePowerPole* NewPole = Cast<AFGBuildablePowerPole>(ConstructedActor);
		if (!NewPole)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Construct failed for pole %s"), *OldPole->GetName());
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

		// Track old->new mapping for post-upgrade validation
		OldToNewBuildableMap.Add(OldPole, NewPole);

		UE_LOG(LogSmartFoundations, Display, TEXT("UpgradeExecutionService: Successfully upgraded pole %s to %s"),
			*Buildable->GetName(), *NewPole->GetClass()->GetName());
		return 1;
	}

	// Unsupported type
	UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Unsupported buildable type: %s"),
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
		UE_LOG(LogSmartFoundations, Display,
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
			UE_LOG(LogSmartFoundations, Display,
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
			UE_LOG(LogSmartFoundations, Warning,
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
					UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Adding %d %s to overflow crate"),
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
				UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Spawned overflow crate with %d item types"),
					Stacks.Num());
			}
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
	UE_LOG(LogSmartFoundations, Log, TEXT("UpgradeExecutionService: Upgrade complete - Success=%d Fail=%d Skip=%d"),
		LastResult.SuccessCount, LastResult.FailCount, LastResult.SkipCount);
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
			static const TCHAR* LiftRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk2.Recipe_ConveyorLiftMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk3.Recipe_ConveyorLiftMk3_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk5.Recipe_ConveyorLiftMk5_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk6.Recipe_ConveyorLiftMk6_C"),
			};
			if (TargetTier >= 1 && TargetTier <= 6)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, LiftRecipeNames[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load lift recipe for tier %d"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::PowerPole:
		{
			// Power pole recipes: Recipe_PowerPoleMk1_C, Recipe_PowerPoleMk2_C, Recipe_PowerPoleMk3_C
			static const TCHAR* PoleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk2.Recipe_PowerPoleMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk3.Recipe_PowerPoleMk3_C"),
			};
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, PoleRecipeNames[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load power pole recipe for tier %d"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletSingle:
		{
			// Single-sided wall outlets Mk1-Mk3 (some via MAM/FICSIT shop)
			static const TCHAR* WallOutletSingleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWall.Recipe_PowerPoleWall_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk2.Recipe_PowerPoleWallMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk3.Recipe_PowerPoleWallMk3_C"),
			};
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, WallOutletSingleRecipeNames[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load single wall outlet recipe for tier %d (may not be unlocked)"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::WallOutletDouble:
		{
			// Double-sided wall outlets Mk1-Mk3 (some via MAM/FICSIT shop)
			static const TCHAR* WallOutletDoubleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDouble.Recipe_PowerPoleWallDouble_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk2.Recipe_PowerPoleWallDoubleMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk3.Recipe_PowerPoleWallDoubleMk3_C"),
			};
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				UClass* RecipeClass = LoadClass<UFGRecipe>(nullptr, WallOutletDoubleRecipeNames[TargetTier - 1]);
				if (RecipeClass)
				{
					return TSubclassOf<UFGRecipe>(RecipeClass);
				}
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load double wall outlet recipe for tier %d (may not be unlocked)"), TargetTier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::Pump:
		case ESFUpgradeFamily::Tower:
			// Pump: TODO if needed
			// Tower: Audit-only, no upgrade execution
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: GetUpgradeRecipe not implemented for family %d"),
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load lift class for tier %d"), Tier);
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load power pole class for tier %d"), Tier);
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load single wall outlet class for tier %d"), Tier);
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: Failed to load double wall outlet class for tier %d"), Tier);
			}
			return nullptr;
		}
		case ESFUpgradeFamily::Pump:
		case ESFUpgradeFamily::Tower:
			// Pump: TODO if needed
			// Tower: Audit-only, no upgrade execution
			UE_LOG(LogSmartFoundations, Warning, TEXT("UpgradeExecutionService: GetBuildableClass not implemented for family %d"),
				static_cast<int32>(Family));
			return nullptr;
		default:
			return nullptr;
	}
}

bool USFUpgradeExecutionService::CalculateTotalUpgradeCost(TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutNetCost) const
{
	if (!Subsystem || PendingUpgrades.Num() == 0)
	{
		return false;
	}

	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (!IsValid(Buildable)) continue;

		const ESFUpgradeFamily BuildableFamily = USFUpgradeTraversalService::GetUpgradeFamily(Buildable);
		const int32 SourceTier = USFUpgradeTraversalService::GetBuildableTier(Buildable);
		if (BuildableFamily == ESFUpgradeFamily::None || SourceTier <= 0) continue;

		TSubclassOf<UFGRecipe> SourceRecipe = GetUpgradeRecipe(BuildableFamily, SourceTier);
		TSubclassOf<UFGRecipe> TargetRecipe = GetUpgradeRecipe(BuildableFamily, CurrentParams.TargetTier);

		if (!SourceRecipe || !TargetRecipe)
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("CalculateTotalUpgradeCost: Could not find recipes for %s family=%d source=%d target=%d"),
				*Buildable->GetName(), static_cast<int32>(BuildableFamily), SourceTier, CurrentParams.TargetTier);
			return false;
		}

		const UFGRecipe* SourceCDO = SourceRecipe->GetDefaultObject<UFGRecipe>();
		const UFGRecipe* TargetCDO = TargetRecipe->GetDefaultObject<UFGRecipe>();
		if (!SourceCDO || !TargetCDO)
		{
			return false;
		}

		for (const FItemAmount& Ingredient : TargetCDO->GetIngredients())
		{
			if (Ingredient.ItemClass)
			{
				int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
				CurrentAmount += Ingredient.Amount;
			}
		}

		for (const FItemAmount& Ingredient : SourceCDO->GetIngredients())
		{
			if (Ingredient.ItemClass)
			{
				int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
				CurrentAmount -= Ingredient.Amount;
			}
		}
	}

	// Log the calculated costs
	for (const auto& Pair : OutNetCost)
	{
		if (Pair.Value != 0 && Pair.Key)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("CalculateTotalUpgradeCost: %s = %d"),
				*UFGItemDescriptor::GetItemName(Pair.Key).ToString(), Pair.Value);
		}
	}

	return true;
}

bool USFUpgradeExecutionService::CanAffordUpgrade(const TMap<TSubclassOf<UFGItemDescriptor>, int32>& NetCost) const
{
	if (!CurrentParams.PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CanAffordUpgrade: No player controller"));
		return false;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(CurrentParams.PlayerController->GetPawn());
	if (!Character)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CanAffordUpgrade: No player character"));
		return false;
	}

	UFGInventoryComponent* Inventory = Character->GetInventory();
	if (!Inventory)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CanAffordUpgrade: No player inventory"));
		return false;
	}

	// Get central storage
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	AFGCentralStorageSubsystem* CentralStorage = World ? AFGCentralStorageSubsystem::Get(World) : nullptr;

	// Check each item that needs to be deducted (positive values only)
	for (const auto& Pair : NetCost)
	{
		if (Pair.Value > 0 && Pair.Key)
		{
			int32 Needed = Pair.Value;
			int32 Available = 0;

			// Check central storage
			if (CentralStorage)
			{
				Available += CentralStorage->GetNumItemsFromCentralStorage(Pair.Key);
			}

			// Check personal inventory
			Available += Inventory->GetNumItems(Pair.Key);

			if (Available < Needed)
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("CanAffordUpgrade: Not enough %s - need %d, have %d"),
					*UFGItemDescriptor::GetItemName(Pair.Key).ToString(), Needed, Available);
				return false;
			}
		}
	}

	return true;
}

bool USFUpgradeExecutionService::DeductUpgradeCosts(const TMap<TSubclassOf<UFGItemDescriptor>, int32>& NetCost)
{
	if (!CurrentParams.PlayerController)
	{
		return false;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(CurrentParams.PlayerController->GetPawn());
	if (!Character)
	{
		return false;
	}

	UFGInventoryComponent* Inventory = Character->GetInventory();
	if (!Inventory)
	{
		return false;
	}

	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	AFGCentralStorageSubsystem* CentralStorage = World ? AFGCentralStorageSubsystem::Get(World) : nullptr;

	// Deduct items (positive values only - these are costs)
	// Negative values are refunds which we skip (vanilla upgrade handles refund via dismantle)
	for (const auto& Pair : NetCost)
	{
		if (Pair.Value > 0 && Pair.Key)
		{
			int32 Remaining = Pair.Value;

			// Try central storage first
			if (CentralStorage && Remaining > 0)
			{
				int32 RemovedFromCentral = CentralStorage->TryRemoveItemsFromCentralStorage(Pair.Key, Remaining);
				Remaining -= RemovedFromCentral;

				if (RemovedFromCentral > 0)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("DeductUpgradeCosts: Removed %d %s from central storage"),
						RemovedFromCentral, *UFGItemDescriptor::GetItemName(Pair.Key).ToString());
				}
			}

			// Remove remainder from personal inventory
			if (Remaining > 0)
			{
				Inventory->Remove(Pair.Key, Remaining);
				UE_LOG(LogSmartFoundations, Log, TEXT("DeductUpgradeCosts: Removed %d %s from personal inventory"),
					Remaining, *UFGItemDescriptor::GetItemName(Pair.Key).ToString());
			}
		}
	}

	return true;
}

void USFUpgradeExecutionService::SaveBatchConnectionPairs()
{
	// Pre-scan all pending conveyors to find inter-connected pairs
	// We need to save this BEFORE any upgrades because the old actors will be destroyed

	TSet<AFGBuildableConveyorBase*> PendingConveyorSet;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Buildable))
		{
			PendingConveyorSet.Add(Conveyor);
		}
	}

	for (AFGBuildableConveyorBase* Conveyor : PendingConveyorSet)
	{
		if (!IsValid(Conveyor))
		{
			continue;
		}

		// Check Connection0
		if (UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0())
		{
			if (UFGFactoryConnectionComponent* Partner = Conn0->GetConnection())
			{
				if (AFGBuildableConveyorBase* PartnerConveyor = Cast<AFGBuildableConveyorBase>(Partner->GetOwner()))
				{
					// Is the partner also being upgraded?
					if (PendingConveyorSet.Contains(PartnerConveyor))
					{
						// Determine which connection index on the partner
						int32 PartnerConnIndex = (Partner == PartnerConveyor->GetConnection0()) ? 0 : 1;
						SavedConnectionPairs.Add(
							TPair<AFGBuildableConveyorBase*, int32>(Conveyor, 0),
							TPair<AFGBuildableConveyorBase*, int32>(PartnerConveyor, PartnerConnIndex));
						UE_LOG(LogSmartFoundations, Log, TEXT("SaveBatchConnectionPairs: %s.Conn0 -> %s.Conn%d"),
							*Conveyor->GetName(), *PartnerConveyor->GetName(), PartnerConnIndex);
					}
				}
			}
		}

		// Check Connection1
		if (UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1())
		{
			if (UFGFactoryConnectionComponent* Partner = Conn1->GetConnection())
			{
				if (AFGBuildableConveyorBase* PartnerConveyor = Cast<AFGBuildableConveyorBase>(Partner->GetOwner()))
				{
					// Is the partner also being upgraded?
					if (PendingConveyorSet.Contains(PartnerConveyor))
					{
						// Determine which connection index on the partner
						int32 PartnerConnIndex = (Partner == PartnerConveyor->GetConnection0()) ? 0 : 1;
						SavedConnectionPairs.Add(
							TPair<AFGBuildableConveyorBase*, int32>(Conveyor, 1),
							TPair<AFGBuildableConveyorBase*, int32>(PartnerConveyor, PartnerConnIndex));
						UE_LOG(LogSmartFoundations, Log, TEXT("SaveBatchConnectionPairs: %s.Conn1 -> %s.Conn%d"),
							*Conveyor->GetName(), *PartnerConveyor->GetName(), PartnerConnIndex);
					}
				}
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("SaveBatchConnectionPairs: Saved %d inter-connected pairs"), SavedConnectionPairs.Num());
}

void USFUpgradeExecutionService::FixBatchConnectionReferences()
{
	if (SavedConnectionPairs.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchConnectionReferences: No inter-connected pairs to fix"));
		return;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchConnectionReferences: Processing %d saved connection pairs"),
		SavedConnectionPairs.Num());

	int32 FixedCount = 0;
	TSet<TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*>> AlreadyConnected;

	// For each saved connection pair, reconnect the new conveyors
	for (const auto& Pair : SavedConnectionPairs)
	{
		AFGBuildableConveyorBase* OldConveyor = Pair.Key.Key;
		int32 ConnectionIndex = Pair.Key.Value;
		AFGBuildableConveyorBase* OldPartner = Pair.Value.Key;
		int32 PartnerConnectionIndex = Pair.Value.Value;

		// Find the new versions
		AFGBuildableConveyorBase** NewConveyorPtr = OldToNewConveyorMap.Find(OldConveyor);
		AFGBuildableConveyorBase** NewPartnerPtr = OldToNewConveyorMap.Find(OldPartner);

		if (!NewConveyorPtr || !*NewConveyorPtr || !IsValid(*NewConveyorPtr))
		{
			continue;
		}
		if (!NewPartnerPtr || !*NewPartnerPtr || !IsValid(*NewPartnerPtr))
		{
			continue;
		}

		AFGBuildableConveyorBase* NewConveyor = *NewConveyorPtr;
		AFGBuildableConveyorBase* NewPartner = *NewPartnerPtr;

		// Skip if we've already connected these two conveyors (avoid double-connecting)
		TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*> ConveyorPair(NewConveyor, NewPartner);
		TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*> ReversePair(NewPartner, NewConveyor);
		if (AlreadyConnected.Contains(ConveyorPair) || AlreadyConnected.Contains(ReversePair))
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchConnectionReferences: Skipping %s <-> %s (already connected)"),
				*NewConveyor->GetName(), *NewPartner->GetName());
			continue;
		}

		// Get the connection to fix
		UFGFactoryConnectionComponent* ConnToFix = (ConnectionIndex == 0)
			? NewConveyor->GetConnection0()
			: NewConveyor->GetConnection1();

		if (!ConnToFix)
		{
			continue;
		}

		// Get the EXACT connection on the partner (not just any available)
		UFGFactoryConnectionComponent* NewPartnerConn = (PartnerConnectionIndex == 0)
			? NewPartner->GetConnection0()
			: NewPartner->GetConnection1();

		if (NewPartnerConn)
		{
			// Clear any existing (broken) connections and reconnect after every replacement exists.
			// This batch-level repair runs before the queued vanilla chain rebuild, so chain
			// stabilization sees the final physical graph.
			if (ConnToFix->IsConnected())
			{
				ConnToFix->ClearConnection();
			}
			if (NewPartnerConn->IsConnected())
			{
				NewPartnerConn->ClearConnection();
			}
			ConnToFix->SetConnection(NewPartnerConn);
			FixedCount++;
			AlreadyConnected.Add(ConveyorPair);
			UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchConnectionReferences: Connected %s.Conn%d -> %s.Conn%d"),
				*NewConveyor->GetName(), ConnectionIndex, *NewPartner->GetName(), PartnerConnectionIndex);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("FixBatchConnectionReferences: Connection%d not found on %s"),
				PartnerConnectionIndex, *NewPartner->GetName());
		}
	}

	if (FixedCount > 0)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("FixBatchConnectionReferences: Fixed %d inter-connected references"), FixedCount);

		// NOTE: Do NOT call RemoveConveyor/AddConveyor here.
		// The vanilla hologram upgrade path (TryUpgrade → ConfigureComponents → AddConveyor)
		// handles chain formation during Construct(). Manual chain rebuilds corrupt bucket
		// indices, causing crashes in RemoveAndSplitConveyorBucket when belts are later
		// dismantled (Array index out of bounds: -1). See RESEARCH_MassUpgrade_ChainActorSafety.md.
	}
}

// =============================================================================
// Option A (Pipe extension) + Option B (Expected-vs-Actual validation)
// See docs/Open_Issues/_pending_issue_upgrade_incomplete_connections.md
// =============================================================================

static UFGFactoryConnectionComponent* FindFactoryConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGFactoryConnectionComponent*> Comps;
	Buildable->GetComponents<UFGFactoryConnectionComponent>(Comps);
	for (UFGFactoryConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

static UFGPipeConnectionComponent* FindPipeConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGPipeConnectionComponent*> Comps;
	Buildable->GetComponents<UFGPipeConnectionComponent>(Comps);
	for (UFGPipeConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

static UFGCircuitConnectionComponent* FindPowerConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGCircuitConnectionComponent*> Comps;
	Buildable->GetComponents<UFGCircuitConnectionComponent>(Comps);
	for (UFGCircuitConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

void USFUpgradeExecutionService::SaveBatchPipeConnectionPairs()
{
	// Pre-scan pipelines in the batch to find inter-connected pipe-to-pipe pairs.
	// Mirrors SaveBatchConnectionPairs but for AFGBuildablePipeline.

	TSet<AFGBuildablePipeline*> PendingPipeSet;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable))
		{
			PendingPipeSet.Add(Pipe);
		}
	}

	for (AFGBuildablePipeline* Pipe : PendingPipeSet)
	{
		if (!IsValid(Pipe)) continue;

		UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
		UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();

		auto TrySave = [&](UFGPipeConnectionComponent* Conn, int32 ConnIndex)
		{
			if (!Conn) return;
			UFGPipeConnectionComponentBase* PartnerBase = Conn->GetConnection();
			if (!PartnerBase) return;

			AActor* PartnerOwner = PartnerBase->GetOwner();
			AFGBuildablePipeline* PartnerPipe = Cast<AFGBuildablePipeline>(PartnerOwner);
			if (!PartnerPipe || !PendingPipeSet.Contains(PartnerPipe)) return;

			// Determine partner's connection index (0 or 1)
			int32 PartnerConnIndex = -1;
			if (PartnerBase == PartnerPipe->GetPipeConnection0()) PartnerConnIndex = 0;
			else if (PartnerBase == PartnerPipe->GetPipeConnection1()) PartnerConnIndex = 1;
			if (PartnerConnIndex < 0) return;

			SavedPipeConnectionPairs.Add(
				TPair<AFGBuildablePipeline*, int32>(Pipe, ConnIndex),
				TPair<AFGBuildablePipeline*, int32>(PartnerPipe, PartnerConnIndex));

			UE_LOG(LogSmartFoundations, Log, TEXT("SaveBatchPipeConnectionPairs: %s.PipeConn%d -> %s.PipeConn%d"),
				*Pipe->GetName(), ConnIndex, *PartnerPipe->GetName(), PartnerConnIndex);
		};

		TrySave(Conn0, 0);
		TrySave(Conn1, 1);
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("SaveBatchPipeConnectionPairs: Saved %d inter-connected pipe pairs"), SavedPipeConnectionPairs.Num());
}

void USFUpgradeExecutionService::FixBatchPipeConnectionReferences()
{
	if (SavedPipeConnectionPairs.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchPipeConnectionReferences: Processing %d saved pipe pairs"),
		SavedPipeConnectionPairs.Num());

	int32 FixedCount = 0;
	TSet<TPair<AFGBuildablePipeline*, AFGBuildablePipeline*>> AlreadyConnected;

	for (const auto& Pair : SavedPipeConnectionPairs)
	{
		AFGBuildablePipeline* OldPipe = Pair.Key.Key;
		int32 ConnIdx = Pair.Key.Value;
		AFGBuildablePipeline* OldPartner = Pair.Value.Key;
		int32 PartnerConnIdx = Pair.Value.Value;

		AFGBuildable** NewPipePtr = OldToNewBuildableMap.Find(OldPipe);
		AFGBuildable** NewPartnerPtr = OldToNewBuildableMap.Find(OldPartner);
		if (!NewPipePtr || !*NewPipePtr || !IsValid(*NewPipePtr)) continue;
		if (!NewPartnerPtr || !*NewPartnerPtr || !IsValid(*NewPartnerPtr)) continue;

		AFGBuildablePipeline* NewPipe = Cast<AFGBuildablePipeline>(*NewPipePtr);
		AFGBuildablePipeline* NewPartner = Cast<AFGBuildablePipeline>(*NewPartnerPtr);
		if (!NewPipe || !NewPartner) continue;

		TPair<AFGBuildablePipeline*, AFGBuildablePipeline*> Fwd(NewPipe, NewPartner);
		TPair<AFGBuildablePipeline*, AFGBuildablePipeline*> Rev(NewPartner, NewPipe);
		if (AlreadyConnected.Contains(Fwd) || AlreadyConnected.Contains(Rev)) continue;

		UFGPipeConnectionComponent* LocalConn = (ConnIdx == 0) ? NewPipe->GetPipeConnection0() : NewPipe->GetPipeConnection1();
		UFGPipeConnectionComponent* PartnerConn = (PartnerConnIdx == 0) ? NewPartner->GetPipeConnection0() : NewPartner->GetPipeConnection1();
		if (!LocalConn || !PartnerConn) continue;

		if (LocalConn->IsConnected()) LocalConn->ClearConnection();
		if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
		LocalConn->SetConnection(PartnerConn);
		FixedCount++;
		AlreadyConnected.Add(Fwd);

		UE_LOG(LogSmartFoundations, Log, TEXT("FixBatchPipeConnectionReferences: Connected %s.PipeConn%d -> %s.PipeConn%d"),
			*NewPipe->GetName(), ConnIdx, *NewPartner->GetName(), PartnerConnIdx);
	}

	if (FixedCount > 0)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("FixBatchPipeConnectionReferences: Fixed %d inter-connected pipe references"), FixedCount);
	}
}

void USFUpgradeExecutionService::CaptureExpectedConnectionManifests()
{
	// Walk every pending buildable and snapshot its full connection surface BEFORE any destruction.
	// Partner identity = (old raw ptr, partner connector FName). Pointer is used as identity only
	// (map key / lookup into OldToNewBuildableMap); never dereferenced after destruction.
	//
	// This manifest covers power, building-side partners (splitters/factories), and any edge
	// that the pair-based Save/Fix helpers don't explicitly handle — the safety net for Option A.

	int32 TotalEdges = 0;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (!IsValid(Buildable)) continue;

		TArray<FConnectionEdge>& Edges = ExpectedConnectionEdges.FindOrAdd(Buildable);

		// Factory (belt/lift) connections
		{
			TArray<UFGFactoryConnectionComponent*> Comps;
			Buildable->GetComponents<UFGFactoryConnectionComponent>(Comps);
			for (UFGFactoryConnectionComponent* C : Comps)
			{
				if (!C || !C->IsConnected()) continue;
				UFGFactoryConnectionComponent* Partner = C->GetConnection();
				if (!Partner) continue;
				AActor* PartnerOwner = Partner->GetOwner();
				if (!PartnerOwner) continue;

				FConnectionEdge Edge;
				Edge.LocalConnectorName = C->GetFName();
				Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
				Edge.PartnerConnectorName = Partner->GetFName();
				Edge.Kind = EConnectionEdgeKind::Factory;
				Edges.Add(Edge);
				TotalEdges++;
			}
		}

		// Pipe connections
		{
			TArray<UFGPipeConnectionComponent*> Comps;
			Buildable->GetComponents<UFGPipeConnectionComponent>(Comps);
			for (UFGPipeConnectionComponent* C : Comps)
			{
				if (!C || !C->IsConnected()) continue;
				UFGPipeConnectionComponentBase* Partner = C->GetConnection();
				if (!Partner) continue;
				AActor* PartnerOwner = Partner->GetOwner();
				if (!PartnerOwner) continue;

				FConnectionEdge Edge;
				Edge.LocalConnectorName = C->GetFName();
				Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
				Edge.PartnerConnectorName = Partner->GetFName();
				Edge.Kind = EConnectionEdgeKind::Pipe;
				Edges.Add(Edge);
				TotalEdges++;
			}
		}

		// Power connections (each pole/outlet can have multiple wires per connector)
		// Use UFGCircuitConnectionComponent base class so wall outlets and all pole variants are covered.
		{
			TArray<UFGCircuitConnectionComponent*> Comps;
			Buildable->GetComponents<UFGCircuitConnectionComponent>(Comps);
			for (UFGCircuitConnectionComponent* C : Comps)
			{
				if (!C) continue;
				TArray<UFGCircuitConnectionComponent*> Partners;
				C->GetConnections(Partners);
				for (UFGCircuitConnectionComponent* Partner : Partners)
				{
					if (!Partner) continue;
					AActor* PartnerOwner = Partner->GetOwner();
					if (!PartnerOwner) continue;

					FConnectionEdge Edge;
					Edge.LocalConnectorName = C->GetFName();
					Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
					Edge.PartnerConnectorName = Partner->GetFName();
					Edge.Kind = EConnectionEdgeKind::Power;
					Edges.Add(Edge);
					TotalEdges++;
				}
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("CaptureExpectedConnectionManifests: captured %d edges across %d buildables"),
		TotalEdges, ExpectedConnectionEdges.Num());
}

void USFUpgradeExecutionService::ValidateAndRepairConnections()
{
	if (ExpectedConnectionEdges.Num() == 0)
	{
		return;
	}

	int32 Validated = 0;
	int32 Repaired = 0;
	int32 Broken = 0;

	for (auto& ManifestEntry : ExpectedConnectionEdges)
	{
		AFGBuildable* OldLocal = ManifestEntry.Key;
		const TArray<FConnectionEdge>& Edges = ManifestEntry.Value;

		// Translate old local buildable to new via the map
		AFGBuildable** NewLocalPtr = OldToNewBuildableMap.Find(OldLocal);
		if (!NewLocalPtr || !*NewLocalPtr || !IsValid(*NewLocalPtr))
		{
			// Local was skipped / failed to upgrade. Skip its edges (they belong to a buildable
			// that was never replaced; original connections should still be intact).
			continue;
		}
		AFGBuildable* NewLocal = *NewLocalPtr;

		for (const FConnectionEdge& Edge : Edges)
		{
			// Resolve partner: if partner was also in our batch, translate via map; otherwise
			// use the original pointer if still valid (non-upgraded partner).
			AFGBuildable* PartnerNew = nullptr;
			if (AFGBuildable** PartnerMapped = OldToNewBuildableMap.Find(Edge.PartnerOldPtr))
			{
				if (*PartnerMapped && IsValid(*PartnerMapped))
				{
					PartnerNew = *PartnerMapped;
				}
			}
			else if (IsValid(Edge.PartnerOldPtr))
			{
				PartnerNew = Edge.PartnerOldPtr;
			}

			if (!PartnerNew)
			{
				// Partner could not be resolved — this is a broken edge we cannot repair.
				Broken++;
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("ValidateAndRepairConnections: %s.%s -> <unresolvable partner> (kind=%d)"),
					*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(), (int32)Edge.Kind);
				continue;
			}

			// Kind-specific validation and repair
			switch (Edge.Kind)
			{
				case EConnectionEdgeKind::Factory:
				{
					UFGFactoryConnectionComponent* LocalConn = FindFactoryConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGFactoryConnectionComponent* PartnerConn = FindFactoryConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ValidateAndRepairConnections: factory edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					if (LocalConn->GetConnection() == PartnerConn)
					{
						// Edge intact
						break;
					}

					// Missing — attempt repair
					if (LocalConn->IsConnected()) LocalConn->ClearConnection();
					if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
					LocalConn->SetConnection(PartnerConn);
					if (LocalConn->GetConnection() == PartnerConn)
					{
						Repaired++;
						UE_LOG(LogSmartFoundations, Display,
							TEXT("ValidateAndRepairConnections: REPAIRED factory %s.%s <-> %s.%s"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					else
					{
						Broken++;
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ValidateAndRepairConnections: BROKEN factory %s.%s <-> %s.%s (repair failed)"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					break;
				}

				case EConnectionEdgeKind::Pipe:
				{
					UFGPipeConnectionComponent* LocalConn = FindPipeConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGPipeConnectionComponent* PartnerConn = FindPipeConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ValidateAndRepairConnections: pipe edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					if (LocalConn->GetConnection() == PartnerConn)
					{
						break;
					}

					if (LocalConn->IsConnected()) LocalConn->ClearConnection();
					if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
					LocalConn->SetConnection(PartnerConn);
					if (LocalConn->GetConnection() == PartnerConn)
					{
						Repaired++;
						UE_LOG(LogSmartFoundations, Display,
							TEXT("ValidateAndRepairConnections: REPAIRED pipe %s.%s <-> %s.%s"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					else
					{
						Broken++;
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ValidateAndRepairConnections: BROKEN pipe %s.%s <-> %s.%s (repair failed)"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					break;
				}

				case EConnectionEdgeKind::Power:
				{
					UFGCircuitConnectionComponent* LocalConn = FindPowerConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGCircuitConnectionComponent* PartnerConn = FindPowerConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ValidateAndRepairConnections: power edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					// Power connections are many-to-many (each connector holds N wires).
					// Verify the specific partner is among LocalConn's connections.
					TArray<UFGCircuitConnectionComponent*> CurrentPartners;
					LocalConn->GetConnections(CurrentPartners);
					const bool bAlreadyConnected = CurrentPartners.Contains(PartnerConn);
					if (bAlreadyConnected)
					{
						break;
					}

					// Power wires are AFGBuildableWire actors owned by the buildable subsystem;
					// we cannot cleanly re-create them here. Flag as broken so the operator knows.
					// In practice, vanilla's pole Upgrade_Implementation transfers wires, so this
					// branch should be rare. Log for diagnostic purposes.
					Broken++;
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("ValidateAndRepairConnections: BROKEN power %s.%s <-> %s.%s (wire-based repair not implemented; vanilla Upgrade should have transferred)"),
						*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
						*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					break;
				}
			}
		}
	}

	WorkingResult.ValidatedConnectionCount = Validated;
	WorkingResult.RepairedConnectionCount = Repaired;
	WorkingResult.BrokenConnectionCount = Broken;

	UE_LOG(LogSmartFoundations, Display,
		TEXT("ValidateAndRepairConnections: validated=%d repaired=%d broken=%d"),
		Validated, Repaired, Broken);
}
