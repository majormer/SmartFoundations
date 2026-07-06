// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Subsystem/SFHologramHelperService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFHologramHelperServiceImpl.h"
#include "Holograms/Logistics/SFPipelinePoleChildHologram.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	void RefreshHologramVisibility(AFGHologram* Hologram)
	{
		if (!IsValid(Hologram))
		{
			return;
		}

		Hologram->SetActorHiddenInGame(false);
		Hologram->UpdateComponentTransforms();

		if (USceneComponent* Root = Hologram->GetRootComponent())
		{
			Root->MarkRenderStateDirty();
		}

		// Do not recursively force component visibility here. Vanilla holograms carry
		// clearance/bounds primitives that are intentionally hidden; propagating visibility
		// from the root exposes the red wireframe boundary boxes during Smart scaling.
	}
}

FSFHologramHelperService::FSFHologramHelperService()
{
}

FSFHologramHelperService::~FSFHologramHelperService()
{
}

void FSFHologramHelperService::Initialize(UWorld* InWorld)
{
	WorldContext = InWorld;
	UE_LOG(LogSmartFoundations, Log, TEXT("HologramHelperService: Initialized"));
}

void FSFHologramHelperService::Shutdown()
{
	// Clean up all children
	DestroyAllChildren();

	// Clear state
	ActiveHologram.Reset();
	WorldContext.Reset();

	UE_LOG(LogSmartFoundations, Verbose, TEXT("HologramHelperService: Shutdown complete"));
}

void FSFHologramHelperService::RegisterActiveHologram(AFGHologram* Hologram)
{
	// TODO: Extract from SFSubsystem::RegisterActiveHologram

	if (!Hologram || !IsValid(Hologram))
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("HologramHelperService: Cannot register invalid hologram"));
		return;
	}

	// Unregister previous hologram if any
	if (ActiveHologram.IsValid() && ActiveHologram.Get() != Hologram)
	{
		UnregisterActiveHologram(ActiveHologram.Get());
	}

	ActiveHologram = Hologram;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HologramHelperService: Registered hologram %s"), *Hologram->GetName());
}

void FSFHologramHelperService::UnregisterActiveHologram(AFGHologram* Hologram)
{
	// TODO: Extract from SFSubsystem::UnregisterActiveHologram

	if (!Hologram || !ActiveHologram.IsValid() || ActiveHologram.Get() != Hologram)
	{
		return;
	}

	// Clean up children
	DestroyAllChildren();

	// Issue #160: Clear Zoop flag when hologram is unregistered
	bZoopActive = false;

	// Clear active hologram
	ActiveHologram.Reset();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HologramHelperService: Unregistered hologram"));
}

void FSFHologramHelperService::PollForActiveHologram()
{
	// TODO: Extract from SFSubsystem::PollForActiveHologram
	// This is called periodically to auto-detect active holograms
}

void FSFHologramHelperService::TrackScalingChildTransform(AFGHologram* ChildHologram, const FVector& IntendedLocation, const FRotator& IntendedRotation)
{
	if (!ChildHologram || !IsValid(ChildHologram))
	{
		return;
	}

	ScalingChildIntendedTransforms.FindOrAdd(ChildHologram) = FTransform(IntendedRotation, IntendedLocation);
}

void FSFHologramHelperService::RefreshTrackedScalingChildTransforms(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !IsValid(ParentHologram) || ScalingChildIntendedTransforms.Num() == 0)
	{
		return;
	}

	const bool bParentLocked = ParentHologram->IsHologramLocked();
	if (!bParentLocked)
	{
		return;
	}

	for (int32 ChildIndex = SpawnedChildren.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		if (!SpawnedChildren[ChildIndex].IsValid())
		{
			SpawnedChildren.RemoveAtSwap(ChildIndex);
			continue;
		}

		AFGHologram* Child = SpawnedChildren[ChildIndex].Get();
		const FTransform* IntendedTransform = ScalingChildIntendedTransforms.Find(Child);
		if (!IntendedTransform)
		{
			continue;
		}

		const FVector IntendedLocation = IntendedTransform->GetLocation();
		const FRotator IntendedRotation = IntendedTransform->Rotator();
		const bool bLocationDrifted = !Child->GetActorLocation().Equals(IntendedLocation, 0.5f);
		const bool bRotationDrifted = !Child->GetActorRotation().Equals(IntendedRotation, 0.1f);
		if (!bLocationDrifted && !bRotationDrifted)
		{
			continue;
		}

		Child->SetActorLocationAndRotation(IntendedLocation, IntendedRotation);
		RefreshHologramVisibility(Child);
	}
}

void FSFHologramHelperService::TickTrackedScalingChildTransformRefresh(AFGHologram* ParentHologram)
{
	if (PendingTrackedScalingChildTransformRefreshTicks <= 0)
	{
		return;
	}

	if (!ParentHologram || !IsValid(ParentHologram) || !ParentHologram->IsHologramLocked() || ScalingChildIntendedTransforms.Num() == 0)
	{
		PendingTrackedScalingChildTransformRefreshTicks = 0;
		return;
	}

	RefreshTrackedScalingChildTransforms(ParentHologram);
	PendingTrackedScalingChildTransformRefreshTicks--;
}

void FSFHologramHelperService::RegenerateChildHologramGrid(
	AFGHologram* ParentHologram,
	FIntVector& GridCounters,
	FSFValidationService* ValidationService,
	TSharedPtr<ISFHologramAdapter> CurrentAdapter,
	APlayerController* LastController,
	float& BaselineHeightZ,
	TFunction<void()> UpdateChildPositionsCallback
)
{
	// Extracted from SFSubsystem.cpp lines 1541-1790 (Phase 2 - Task #61.6)
	// Full grid regeneration logic moved to HologramHelperService

	if (!ParentHologram || !IsValid(ParentHologram))
	{
		return;
	}

	// #418: every regen owns its own continuation decision; the spawn loop below re-sets this
	// if it hits the frame budget with children still missing.
	bSpawnContinuationPending = false;

	// Issue #160: Detect vanilla Zoop and force 1x1x1 grid to prevent overlapping holograms
	// When Zoop is active (mDesiredZoop != 0), both Smart! and Zoop would create children,
	// resulting in duplicate buildings at the same location.
	// Issue #330: cast to AFGBuildableHologram (was AFGFactoryBuildingHologram). The zoop API
	// (GetZoopInstanceTransforms / mDesiredZoop) is declared on AFGBuildableHologram, so this now
	// also covers standalone signs/billboards - AFGStandaloneSignHologram is AFGGenericBuildableHologram,
	// NOT a factory building, so the narrower cast skipped it and sign zoop + Smart scaling could collide.
	// Non-zooping holograms return an empty transform array below, so their behavior is unchanged.
	if (AFGBuildableHologram* ZoopableHolo = Cast<AFGBuildableHologram>(ParentHologram))
	{
		// Access mDesiredZoop - non-zero means Zoop is active
		// Note: mDesiredZoop is protected, but we can check via GetZoopInstanceTransforms()
		const TArray<FTransform>& ZoopTransforms = ZoopableHolo->GetZoopInstanceTransforms();
		if (ZoopTransforms.Num() > 0)
		{
			// Zoop is active - set flag for HUD display
			if (!bZoopActive)
			{
				bZoopActive = true;
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("⚠️ Zoop detected (%d instances) - Smart! scaling disabled to prevent overlap."),
					ZoopTransforms.Num());
			}

			// Force grid to 1x1x1 to let Zoop handle the scaling
			if (GridCounters.X != 1 || GridCounters.Y != 1 || GridCounters.Z != 1)
			{
				GridCounters = FIntVector(1, 1, 1);

				// Clear any existing Smart! children since Zoop is handling placement
				if (SpawnedChildren.Num() > 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Clearing %d Smart! children - Zoop takes priority"), SpawnedChildren.Num());
					while (SpawnedChildren.Num() > 0)
					{
						TWeakObjectPtr<AFGHologram> ChildToRemove = SpawnedChildren.Pop();
						if (ChildToRemove.IsValid())
						{
							QueueChildForDestroy(ChildToRemove.Get());
						}
					}
				}
			}
			return;  // Let Zoop handle everything
		}
		else
		{
			// Zoop not active - clear flag
			bZoopActive = false;
		}
	}
	else
	{
		// Not a buildable hologram - clear Zoop flag
		bZoopActive = false;
	}

	// Check if hologram supports grid features before regenerating
	if (CurrentAdapter && !CurrentAdapter->SupportsFeature(ESFFeature::ScaleX))
	{
		// Clear any existing children if features disabled
		if (SpawnedChildren.Num() > 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("Clearing grid for unsupported hologram type %s - destroying %d children"),
				*CurrentAdapter->GetAdapterTypeName(), SpawnedChildren.Num());

			// Queue all children for destruction
			while (SpawnedChildren.Num() > 0)
			{
				TWeakObjectPtr<AFGHologram> ChildToRemove = SpawnedChildren.Pop();
				if (ChildToRemove.IsValid())
				{
					QueueChildForDestroy(ChildToRemove.Get());
				}
			}
		}
		return;
	}

	// Task 38: Log parent lock state during grid regeneration
	const bool bParentLocked = ParentHologram->IsHologramLocked();

	// #418 Tier true-up: decide the child spawn strategy once per regen. STACKABLE
	// conveyor/pipe/hypertube supports are the only family left on vanilla child holograms
	// (Tier 3 vanilla-delegate) - their vanilla holograms carry the stack/connection behavior
	// the stackable AC preview builds against (#341/#354/#364). Build-class-based check, same
	// predicate family the stackable AC uses. Every parent type not caught by an explicit
	// Tier-2 branch below goes through the generic drift-proof ASFBuildableChildHologram
	// (Tier 1, docs/Features/Scaling/DESIGN_Scaling_ChildTypeSelection.md).
	const bool bVanillaDelegateChildren = USFAutoConnectService::IsStackableSupportHologram(ParentHologram);

	// Phase 0: Forward grid size validation to ValidationService (Task #61.6)
	int32 ChildrenNeeded = 0;
	if (ValidationService)
	{
		ValidationService->ValidateAndAdjustGridSize(GridCounters, ChildrenNeeded);
		// GridCounters may have been modified if size was too large
		// ChildrenNeeded now contains the validated count
	}
	else
	{
		// Fallback if module not initialized (shouldn't happen)
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ValidationService module not initialized!"));
		int32 TotalItems = FMath::Abs(GridCounters.X) * FMath::Abs(GridCounters.Y) * FMath::Abs(GridCounters.Z);
		ChildrenNeeded = FMath::Max(0, TotalItems - 1);
	}

	int32 TotalItems = FMath::Abs(GridCounters.X) * FMath::Abs(GridCounters.Y) * FMath::Abs(GridCounters.Z);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Grid[%d,%d,%d] = %d items, %d children needed | Parent Locked=%s"),
		GridCounters.X, GridCounters.Y, GridCounters.Z, TotalItems, ChildrenNeeded,
		bParentLocked ? TEXT("YES") : TEXT("NO"));

	// PERFORMANCE PROFILING: Log UObject stats for large grids
	if (ChildrenNeeded >= LARGE_GRID_WARNING_THRESHOLD || SpawnedChildren.Num() >= LARGE_GRID_WARNING_THRESHOLD)
	{
		FSFHologramPerformanceProfiler::LogUObjectStats(FString::Printf(TEXT("Grid %dx%dx%d (%d children)"),
			GridCounters.X, GridCounters.Y, GridCounters.Z, ChildrenNeeded));
	}

	// Phase 5: UObject Warning System - Check for memory limits
	EUObjectWarningLevel WarningLevel = CheckUObjectUtilization(ChildrenNeeded, GridCounters);

	// CRITICAL: Cap grid size if approaching engine limit
	if (WarningLevel == EUObjectWarningLevel::Critical)
	{
		if (ChildrenNeeded > GRID_CHILDREN_HARD_CAP)
		{
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("🛑 CRITICAL: Grid size capped from %d to %d children to prevent engine crash!"),
				ChildrenNeeded, GRID_CHILDREN_HARD_CAP);

			// Cap the grid size by proportionally reducing all dimensions
			const float ScaleFactor = FMath::Sqrt(static_cast<float>(GRID_CHILDREN_HARD_CAP) / ChildrenNeeded);
			GridCounters.X = FMath::Max(1, FMath::RoundToInt(GridCounters.X * ScaleFactor));
			GridCounters.Y = FMath::Max(1, FMath::RoundToInt(GridCounters.Y * ScaleFactor));
			GridCounters.Z = FMath::Max(1, FMath::RoundToInt(GridCounters.Z * ScaleFactor));

			// Recalculate children needed with capped dimensions
			ChildrenNeeded = FMath::Max(0, (FMath::Abs(GridCounters.X) * FMath::Abs(GridCounters.Y) * FMath::Abs(GridCounters.Z)) - 1);

			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("   Grid adjusted to %dx%dx%d = %d children"),
				GridCounters.X, GridCounters.Y, GridCounters.Z, ChildrenNeeded);
		}
	}

	// Clean up invalid weak pointers
	SpawnedChildren.RemoveAll([](const TWeakObjectPtr<AFGHologram>& Child) {
		return !Child.IsValid();
	});

	// Resync from parent if our list diverges (e.g., after rapid input bursts or missed callbacks)
	{
		auto IsNormalGridChild = [](AFGHologram* Candidate) -> bool
		{
			if (!IsValid(Candidate))
			{
				return false;
			}

			const FSFHologramData* Data = USFHologramDataRegistry::GetData(Candidate);
			const bool bIsExtendChild = Candidate->Tags.Contains(FName(TEXT("SF_ExtendChild")))
				|| (Data && Data->ChildType == ESFChildHologramType::ExtendClone);
			if (bIsExtendChild)
			{
				return false;
			}

			return Candidate->ActorHasTag(FName(TEXT("SF_GridChild")))
				|| (Data && Data->ChildType == ESFChildHologramType::ScalingGrid);
		};

		const TArray<AFGHologram*> ParentChildrenNow = ParentHologram->GetHologramChildren();
		TSet<AFGHologram*> OurSet;
		for (const TWeakObjectPtr<AFGHologram>& W : SpawnedChildren)
		{
			if (W.IsValid()) OurSet.Add(W.Get());
		}
		int32 ParentAlive = 0;
		bool NeedResync = false;
		for (AFGHologram* P : ParentChildrenNow)
		{
			if (IsNormalGridChild(P))
			{
				const bool bPendingRemoval = P->ActorHasTag(FName(TEXT("SF_GridChild_PendingDestroy")));
				if (!bPendingRemoval)
				{
					ParentAlive++;
					if (!OurSet.Contains(P))
					{
						NeedResync = true;
					}
				}
			}
		}
		if (NeedResync || ParentAlive != OurSet.Num())
		{
			UWorld* World = WorldContext.Get();
			const double TSR = World ? World->GetTimeSeconds() : 0.0;
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Resync SpawnedChildren from parent: ours=%d parentAlive=%d"),
				(unsigned long long)GFrameCounter, TSR, OurSet.Num(), ParentAlive);
			SpawnedChildren.Empty();
			for (AFGHologram* P : ParentChildrenNow)
			{
				if (IsNormalGridChild(P))
				{
					const bool bPendingRemoval = P->ActorHasTag(FName(TEXT("SF_GridChild_PendingDestroy")));
					if (bPendingRemoval) { continue; }

					SpawnedChildren.Add(P);
				}
			}
		}
	}

	int32 CurrentChildren = SpawnedChildren.Num();

	// #418 coordinate keying - RECONCILE PASS (runs before the count-delta branches). One sweep
	// over the children decides everything from CELLS, not counts:
	//   - children whose cell fell OUTSIDE the new grid bounds are evicted here (also covers
	//     mixed axis changes like X-shrink + Y-grow in a single Smart Panel apply, which the
	//     count-delta branches alone mis-handle);
	//   - children without a cell adopt a free cell (self-heal; normally none);
	//   - FreeCells = cell universe minus occupied cells, canonical Z->X->Y order, ready for
	//     the spawn branch below.
	// CRASH-SAFE eviction ordering: remove from SpawnedChildren FIRST, then queue.
	// QueueChildForDestroy itself calls SpawnedChildren.Remove(Child), so queue-then-RemoveAt
	// double-removes and indexes one past the end (the walls-silo "1438 into 1438" crash).
	TArray<FIntVector> FreeCells;
	{
		const int32 CellXCount = FMath::Abs(GridCounters.X);
		const int32 CellYCount = FMath::Abs(GridCounters.Y);
		const int32 CellZCount = FMath::Abs(GridCounters.Z);

		TSet<FIntVector> OccupiedCells;
		TArray<TWeakObjectPtr<AFGHologram>> UnassignedChildren;
		bool bEvictedAny = false;
		for (int32 Idx = SpawnedChildren.Num() - 1; Idx >= 0; --Idx)
		{
			const TWeakObjectPtr<AFGHologram> Candidate = SpawnedChildren[Idx];  // BY VALUE - the array mutates below
			if (!Candidate.IsValid())
			{
				continue;  // stale entries were scrubbed above
			}

			FIntVector Cell;
			if (!USFGridCoordComponent::TryGetCell(Candidate.Get(), Cell))
			{
				UnassignedChildren.Add(Candidate);
				continue;
			}

			if (Cell.X >= CellXCount || Cell.Y >= CellYCount || Cell.Z >= CellZCount)
			{
				SpawnedChildren.RemoveAt(Idx);  // FIRST - see ordering note above
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Evicting out-of-bounds cell [%d,%d,%d] child %s"),
					Cell.X, Cell.Y, Cell.Z, *Candidate->GetName());
				QueueChildForDestroy(Candidate.Get());
				bEvictedAny = true;
			}
			else
			{
				OccupiedCells.Add(Cell);
			}
		}

		for (int32 Z = 0; Z < CellZCount; ++Z)
		{
			for (int32 X = 0; X < CellXCount; ++X)
			{
				for (int32 Y = 0; Y < CellYCount; ++Y)
				{
					if (X == 0 && Y == 0 && Z == 0)
					{
						continue; // parent cell - never assigned to a child
					}
					const FIntVector Cell(X, Y, Z);
					if (!OccupiedCells.Contains(Cell))
					{
						FreeCells.Add(Cell);
					}
				}
			}
		}

		// Children that predate coordinate keying (or lost their component) adopt free cells
		// first so they re-enter the stable-identity model. Normally empty.
		for (const TWeakObjectPtr<AFGHologram>& Unassigned : UnassignedChildren)
		{
			if (FreeCells.Num() == 0)
			{
				break;
			}
			USFGridCoordComponent::AssignCell(Unassigned.Get(), FreeCells[0]);
			FreeCells.RemoveAt(0);
		}

		if (bEvictedAny)
		{
			CurrentChildren = SpawnedChildren.Num();

			// Evicted children may carry cached auto-connect belt costs - clear them so the
			// parent's next cost aggregation doesn't include destroyed previews.
			if (USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld()))
			{
				if (USFAutoConnectService* AutoConnect = Subsystem->GetAutoConnectService())
				{
					for (const TWeakObjectPtr<AFGHologram>& RemovedChild : PendingDestroyChildren)
					{
						if (RemovedChild.IsValid())
						{
							AutoConnect->ClearBeltCostsForDistributor(RemovedChild.Get());
						}
					}
				}
			}
		}
	}
	int32 NextFreeCell = 0;

	// Track if grid changed for belt preview cleanup
	int32 ToSpawn = 0;
	int32 ToRemove = 0;

	// Spawn or remove children as needed (incremental approach like original Smart!)
	if (ChildrenNeeded > CurrentChildren)
	{
		// Need to spawn more children
		ToSpawn = ChildrenNeeded - CurrentChildren;

		TSubclassOf<UFGRecipe> Recipe = ParentHologram->GetRecipe();
		if (!Recipe)
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("RegenerateChildHologramGrid: Parent hologram has no recipe!"));
			return;
		}

		AActor* HologramOwner = ParentHologram->GetOwner();
		UWorld* World = WorldContext.Get();
		if (!World)
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("RegenerateChildHologramGrid: No world context!"));
			return;
		}

		// Spawn at parent location initially (UpdateChildPositions will place them correctly)
		// NOTE: We don't adjust for anchor offsets here - UpdateChildPositions handles all positioning
		// with proper pivot compensation based on whether parent and child have same/different types
		FVector SpawnLocation = ParentHologram->GetActorLocation();

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Spawning %d new children..."), ToSpawn);

		// PERFORMANCE PROFILING: Track spawn performance
		FSFHologramPerformanceProfiler::BeginSpawnProfile("RegenerateChildHologramGrid", ToSpawn);

		// Set baseline height if this is the first time spawning children (for nudge delta tracking)
		const bool bFirstSpawn = (SpawnedChildren.Num() == 0);
		if (bFirstSpawn)
		{
			BaselineHeightZ = SpawnLocation.Z;
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📍 Baseline height set: %.2f cm (first child spawn)"), BaselineHeightZ);
		}

		// Check if this is a water extractor for extra logging
		bool bIsWaterExtractor = ParentHologram->IsA(AFGWaterPumpHologram::StaticClass());

		// #418 coordinate keying: FreeCells was computed by the reconcile pass above (cell
		// universe minus occupied, canonical Z->X->Y order). New children bind to those cells;
		// existing children keep theirs, so their targets are untouched by this regen.

		// #418: time-budget the spawn burst. A Smart Panel stack jump used to spawn 15K-45K
		// children in ONE frame (multi-second client freeze). Now the loop yields at
		// GRID_SPAWN_FRAME_BUDGET_MS and USFSubsystem::Tick re-runs the regen next frame - the
		// reconcile pass re-derives exactly the still-missing cells, so repeated regens converge.
		const double SpawnSliceStart = FPlatformTime::Seconds();
		const double SpawnSliceBudget = GRID_SPAWN_FRAME_BUDGET_MS / 1000.0;

		for (int32 i = 0; i < ToSpawn; ++i)
		{
			if (i > 0 && (i & 7) == 0
				&& (FPlatformTime::Seconds() - SpawnSliceStart) >= SpawnSliceBudget)
			{
				bSpawnContinuationPending = true;
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("RegenerateChildHologramGrid: spawn budget reached after %d/%d children - continuing next frame."),
					i, ToSpawn);
				break;
			}
			// Use global counter for unique names (prevents collisions when children are destroyed and respawned)
			FName ChildName = FName(*FString::Printf(TEXT("GridChild_%d"), ChildSpawnCounter++));

			// Issue #187: For passthrough holograms, spawn custom ASFPassthroughChildHologram
			// using the same pattern as ASFConveyorAttachmentChildHologram in Extend:
			// 1. SpawnActor (deferred) → 2. SetBuildClass + SetRecipe → 3. FinishSpawning
			// → 4. AddChild → 5. DisableValidation + MarkAsChild → 6. Disable collision/tick
			AFGHologram* ChildHologram = nullptr;
			if (ParentHologram->IsA(AFGPassthroughHologram::StaticClass()))
			{
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFPassthroughChildHologram* PassthroughChild = SpawnWorld->SpawnActor<ASFPassthroughChildHologram>(
						ASFPassthroughChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (PassthroughChild)
					{
						// Match working EXTEND code order exactly:
						// SetBuildClass + SetRecipe BEFORE FinishSpawning (triggers mesh/visual creation)
						PassthroughChild->SetBuildClass(ParentHologram->GetBuildClass());
						PassthroughChild->SetRecipe(Recipe);

						PassthroughChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

						// Add as child IMMEDIATELY after FinishSpawning (matches working EXTEND code)
						ParentHologram->AddChild(PassthroughChild, ChildName);

						// Disable validation AFTER AddChild (data structure approach)
						USFHologramDataService::DisableValidation(PassthroughChild);
						USFHologramDataService::MarkAsChild(PassthroughChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						// Configure visibility
						if (PassthroughChild->IsHologramLocked())
						{
							PassthroughChild->LockHologramPosition(false);
						}
						PassthroughChild->SetActorHiddenInGame(false);
						PassthroughChild->SetActorEnableCollision(false);

						// Disable collision on ALL primitive components (not just BoxComponents)
						TArray<UPrimitiveComponent*> Primitives;
						PassthroughChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						// Disable tick to prevent validation from running
						PassthroughChild->SetActorTickEnabled(false);
						PassthroughChild->RegisterAllComponents();
						PassthroughChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

						PassthroughChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						// Issue #187: Propagate parent's foundation thickness to child.
						// mSnappedBuildingThickness is protected, so read via UE reflection.
						// Without this, children default to 200cm (shortest) instead of matching
						// the parent's snapped foundation height (e.g., 400cm for 4m foundations).
						FFloatProperty* ThickProp = CastField<FFloatProperty>(
							ParentHologram->GetClass()->FindPropertyByName(FName(TEXT("mSnappedBuildingThickness"))));
						if (ThickProp)
						{
							float ParentThickness = ThickProp->GetPropertyValue_InContainer(ParentHologram);
							PassthroughChild->SetSnappedThickness(ParentThickness);
						}

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  PASSTHROUGH: Spawned child %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = PassthroughChild;
				}
			}
			else if (AFGBlueprintHologram* ParentBlueprint = Cast<AFGBlueprintHologram>(ParentHologram))
			{
				// [#168] BLUEPRINT COMPOSITES. The generic ASFBuildableChildHologram below can neither
				// render nor construct a blueprint's contents (that's the old "only the parent places"
				// break). Spawn the PARENT'S OWN hologram class (Holo_Blueprint_C or subclass) so the
				// copy carries the configured blueprint build modes - including the game's own blueprint
				// auto-connect - then stage it with the parent's descriptor. Connections between copies
				// are the GAME's job (FGBlueprintOpenConnectionManager), never Smart wiring.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					AFGBlueprintHologram* BlueprintChild = SpawnWorld->SpawnActor<AFGBlueprintHologram>(
						ParentHologram->GetClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (BlueprintChild)
					{
						// mBuildClass MUST be set before FinishSpawning: AFGHologram::BeginPlay
						// asserts on it (FGHologram.cpp:288), and SetRecipe alone does not derive it
						// on a raw deferred SpawnActor (vanilla's SpawnHologramFromRecipe sets both).
						BlueprintChild->SetBuildClass(ParentHologram->GetBuildClass());
						BlueprintChild->SetRecipe(Recipe);
						BlueprintChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(BlueprintChild, ChildName);

						// Stage the copy: descriptor + blueprint world load = contents render and
						// the child constructs through its own AFGBlueprintHologram::Construct.
						if (ParentBlueprint->mBlueprintDescriptor)
						{
							BlueprintChild->SetBlueprintDescriptor(ParentBlueprint->mBlueprintDescriptor);
							BlueprintChild->LoadBlueprintToOtherWorld();
							// Vanilla's own root-bounds alignment: the PARENT received this through
							// the interactive build-gun flow; without it the clone's contents render
							// offset from its grid cell.
							BlueprintChild->AlignBuildableRootWithBounds();
							UE_LOG(LogSmartFoundations, Log,
								TEXT("[#168] Staged blueprint child %s from descriptor %s"),
								*ChildName.ToString(), *GetNameSafe(ParentBlueprint->mBlueprintDescriptor));
						}
						else
						{
							UE_LOG(LogSmartFoundations, Warning,
								TEXT("[#168] Parent blueprint hologram %s has no descriptor - child %s left unstaged"),
								*ParentHologram->GetName(), *ChildName.ToString());
						}

						USFHologramDataService::DisableValidation(BlueprintChild);
						USFHologramDataService::MarkAsChild(BlueprintChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (BlueprintChild->IsHologramLocked())
						{
							BlueprintChild->LockHologramPosition(false);
						}
						BlueprintChild->SetActorHiddenInGame(false);
						BlueprintChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						BlueprintChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						BlueprintChild->SetActorTickEnabled(false);
						BlueprintChild->RegisterAllComponents();
						BlueprintChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						BlueprintChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
					}
					ChildHologram = BlueprintChild;
				}
			}
			else if (ParentHologram->IsA(AFGFloodlightHologram::StaticClass()))
			{
				// Issue #200: Wall floodlights check wall snapping in CheckValidPlacement.
				// Use ASFFloodlightChildHologram (extends AFGFloodlightHologram) so the existing
				// multi-step property sync (mFixtureAngle, mBuildStep) works via Cast<AFGFloodlightHologram>.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFFloodlightChildHologram* FloodlightChild = SpawnWorld->SpawnActor<ASFFloodlightChildHologram>(
						ASFFloodlightChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (FloodlightChild)
					{
						FloodlightChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						FloodlightChild->SetRecipe(Recipe);
						FloodlightChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(FloodlightChild, ChildName);

						USFHologramDataService::DisableValidation(FloodlightChild);
						USFHologramDataService::MarkAsChild(FloodlightChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (FloodlightChild->IsHologramLocked())
						{
							FloodlightChild->LockHologramPosition(false);
						}
						FloodlightChild->SetActorHiddenInGame(false);
						FloodlightChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						FloodlightChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						FloodlightChild->SetActorTickEnabled(false);
						FloodlightChild->RegisterAllComponents();
						FloodlightChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						FloodlightChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  FLOODLIGHT CHILD: Spawned %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = FloodlightChild;
				}
			}
			else if (USFAutoConnectService::IsRegularConveyorPoleHologram(ParentHologram))
			{
				// #354: standard conveyor pole - two-step (base + HEIGHT) placement. Use
				// ASFConveyorPoleChildHologram (extends AFGConveyorPoleHologram) so the parent's chosen
				// height (mPoleVariationIndex) + build step sync to children via SyncMultiStepHologramProperties.
				// Gated on IsRegularConveyorPoleHologram so the STACKABLE pole (also AFGConveyorPoleHologram)
				// keeps its existing generic-child path.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFConveyorPoleChildHologram* PoleChild = SpawnWorld->SpawnActor<ASFConveyorPoleChildHologram>(
						ASFConveyorPoleChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (PoleChild)
					{
						PoleChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						PoleChild->SetRecipe(Recipe);
						PoleChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(PoleChild, ChildName);

						USFHologramDataService::DisableValidation(PoleChild);
						USFHologramDataService::MarkAsChild(PoleChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (PoleChild->IsHologramLocked())
						{
							PoleChild->LockHologramPosition(false);
						}
						PoleChild->SetActorHiddenInGame(false);
						PoleChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						PoleChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						PoleChild->SetActorTickEnabled(false);
						PoleChild->RegisterAllComponents();
						PoleChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						PoleChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  #354 CONVEYOR POLE CHILD: Spawned %s at %s"),
							*ChildName.ToString(), *SpawnLocation.ToString());
					}
					ChildHologram = PoleChild;
				}
			}
			else if (USFAutoConnectService::IsRegularPipelinePoleHologram(ParentHologram))
			{
				// #364: standard pipeline support - two-step (base + HEIGHT) placement PLUS a vertical
				// ANGLE on the top piece (sloped pipe runs; conveyor poles have no angle). Use
				// ASFPipelinePoleChildHologram (extends AFGPipelinePoleHologram) so the parent's height
				// (mPoleVariationIndex), build step, and mVerticalAngle sync to children via
				// SyncMultiStepHologramProperties. Gated so the STACKABLE and WALL supports keep their
				// existing paths.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFPipelinePoleChildHologram* PipePoleChild = SpawnWorld->SpawnActor<ASFPipelinePoleChildHologram>(
						ASFPipelinePoleChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (PipePoleChild)
					{
						PipePoleChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						PipePoleChild->SetRecipe(Recipe);
						PipePoleChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(PipePoleChild, ChildName);

						USFHologramDataService::DisableValidation(PipePoleChild);
						USFHologramDataService::MarkAsChild(PipePoleChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (PipePoleChild->IsHologramLocked())
						{
							PipePoleChild->LockHologramPosition(false);
						}
						PipePoleChild->SetActorHiddenInGame(false);
						PipePoleChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> PipePolePrimitives;
						PipePoleChild->GetComponents<UPrimitiveComponent>(PipePolePrimitives);
						for (UPrimitiveComponent* PrimComp : PipePolePrimitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						PipePoleChild->SetActorTickEnabled(false);
						PipePoleChild->RegisterAllComponents();
						PipePoleChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						PipePoleChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  #364 PIPELINE POLE CHILD: Spawned %s at %s"),
							*ChildName.ToString(), *SpawnLocation.ToString());
					}
					ChildHologram = PipePoleChild;
				}
			}
			else if (ParentHologram->IsA(AFGStandaloneSignHologram::StaticClass()))
			{
				// Issue #192: Standalone signs/billboards have multi-step builds (pole height).
				// Use ASFStandaloneSignChildHologram (extends AFGStandaloneSignHologram) so
				// mBuildStep sync works. Children skip pole creation (SpawnChildren override).
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFStandaloneSignChildHologram* SignChild = SpawnWorld->SpawnActor<ASFStandaloneSignChildHologram>(
						ASFStandaloneSignChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (SignChild)
					{
						SignChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						SignChild->SetRecipe(Recipe);
						SignChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

						// Issue #192: Sign pole/stand for children — KNOWN LIMITATION
						// Children do not get pole holograms. Built buildings also don't get poles
						// from vanilla's construction system for grid children.
						//
						// Approaches attempted:
						// 1. Let vanilla SpawnChildren run (removed override) — vanilla's internal
						//    snap-state checks prevent pole creation on grid children
						// 2. Copy mDefaultSignSupportRecipe + call SpawnChildren manually — recipe
						//    copies correctly but vanilla still skips pole creation (internal state)
						// 3. Manually spawn FGSignPoleHologram with deferred construction, copy
						//    mBuildClass/mRecipe from parent's pole — pole spawns but:
						//    a. AddChild causes vanilla positioning to fight Smart!'s grid (children hop)
						//    b. AttachToActor doesn't resolve positioning
						//    c. Built buildings still don't get poles from construction system
						//
						// Future fix: may require a custom sign pole child hologram class
						// (like ASFFloodlightChildHologram) or post-construction pole spawning
						// via OnActorSpawned hook.

						ParentHologram->AddChild(SignChild, ChildName);

						USFHologramDataService::DisableValidation(SignChild);
						USFHologramDataService::MarkAsChild(SignChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (SignChild->IsHologramLocked())
						{
							SignChild->LockHologramPosition(false);
						}
						SignChild->SetActorHiddenInGame(false);
						SignChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						SignChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						SignChild->SetActorTickEnabled(false);
						SignChild->RegisterAllComponents();
						SignChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						SignChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  SIGN CHILD: Spawned %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = SignChild;
				}
			}
			else if (ParentHologram->IsA(AFGWaterPumpHologram::StaticClass()))
			{
				// Issue #197: Water pumps need custom child hologram for water validation.
				// Unlike ceiling lights/passthroughs, we do NOT disable validation here —
				// ASFWaterPumpChildHologram::CheckValidPlacement() runs our own water volume
				// check (EncompassesPoint) to ensure children are over water.
				// Tick is left ENABLED so per-frame validation runs when parent is unlocked.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFWaterPumpChildHologram* WaterPumpChild = SpawnWorld->SpawnActor<ASFWaterPumpChildHologram>(
						ASFWaterPumpChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (WaterPumpChild)
					{
						WaterPumpChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						WaterPumpChild->SetRecipe(Recipe);
						WaterPumpChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(WaterPumpChild, ChildName);

						// DO NOT call DisableValidation — we WANT CheckValidPlacement() to run
						USFHologramDataService::MarkAsChild(WaterPumpChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (WaterPumpChild->IsHologramLocked())
						{
							WaterPumpChild->LockHologramPosition(false);
						}
						WaterPumpChild->SetActorHiddenInGame(false);
						WaterPumpChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						WaterPumpChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						// DO NOT disable tick — CheckValidPlacement() must run per-frame for water validation
						WaterPumpChild->RegisterAllComponents();
						// DO NOT force HMS_OK — let CheckValidPlacement() determine material state
						WaterPumpChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  WATER PUMP CHILD: Spawned %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = WaterPumpChild;
				}
			}
			else if (bVanillaDelegateChildren)
			{
				// Tier 3 vanilla-delegate: STACKABLE conveyor/pipe/hypertube supports keep the
				// recipe's own vanilla child hologram - it carries the stack/connection behavior
				// the stackable AC preview builds against (#341/#354/#364). Routing these through
				// the generic Tier-1 child is possible future work, gated on validating series-run
				// wiring against ASFBuildableChildHologram children.
				ChildHologram = SpawnChildHologram(ParentHologram, ChildName, SpawnLocation, FRotator::ZeroRotator);
			}
			else
			{
				// #418 Tier 1: every remaining parent type gets the generic drift-proof child
				// (foundations, walls, ceiling lights, wall attachments, machines, storage, ...).
				// Consolidates the former per-type branches (#200 ceiling, #268 wall, #418
				// foundation) and replaces the raw-vanilla default that drifted to origin.
				// See SpawnBuildableChildHologram for the full configuration.
				ChildHologram = SpawnBuildableChildHologram(ParentHologram, ChildName, SpawnLocation);
			}

			if (ChildHologram)
			{
				// #418 Tier true-up: every Smart child class is fully configured during its spawn
				// branch (tag, collision, validation, data service, tick). Only vanilla-delegate
				// children (stackable supports) still need the legacy generic setup here — the
				// Smart branches must NOT get it (it would re-enable collision and override
				// material state).
				if (bVanillaDelegateChildren)
				{
					// Tag for Smart! ownership to aid future resync/cleanup
					ChildHologram->Tags.AddUnique(FName(TEXT("SF_GridChild")));
					// Ensure visibility and initial material state immediately after spawn
					ChildHologram->SetActorHiddenInGame(false);
					ChildHologram->SetActorEnableCollision(true);
					ChildHologram->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
					// Force components to register and become visible
					ChildHologram->RegisterAllComponents();
				}

				// Phase 4 CRITICAL FIX: Disable ticking for locked children to eliminate per-frame validation overhead
				// With 3000+ children, per-frame Tick() causes FPS to drop from 60 to 3-4 FPS
				// Locked holograms don't need validation, so we can safely disable their tick
				if (bParentLocked)
				{
					ChildHologram->SetActorTickEnabled(false);
				}

				SpawnedChildren.Add(ChildHologram);

				// #418 coordinate keying: bind the child to its grid cell (its identity from here
				// on). Applies to every tier - Smart classes and vanilla-delegate stackables alike.
				if (NextFreeCell < FreeCells.Num())
				{
					USFGridCoordComponent::AssignCell(ChildHologram, FreeCells[NextFreeCell++]);
				}
				else
				{
					UE_LOG(LogSmartFoundations, Verbose,
						TEXT("RegenerateChildHologramGrid: no free cell for spawned child %s (freeCells=%d, toSpawn=%d) - child stays unassigned until next regen"),
						*ChildName.ToString(), FreeCells.Num(), ToSpawn);
				}

				// Log child hologram lock state for Task 38 diagnostics
				const bool bChildLocked = ChildHologram->IsHologramLocked();
				const bool bChildCanLock = ChildHologram->CanLockHologram();

				if (bIsWaterExtractor)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  [WATER EXTRACTOR] Spawned child %s at %s - Type: %s | Parent Locked=%s, Child Locked=%s, Child CanLock=%s"),
						*ChildName.ToString(),
						*SpawnLocation.ToString(),
						*ChildHologram->GetClass()->GetName(),
						bParentLocked ? TEXT("YES") : TEXT("NO"),
						bChildLocked ? TEXT("YES") : TEXT("NO"),
						bChildCanLock ? TEXT("YES") : TEXT("NO"));
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  Spawned child %s | Parent Locked=%s, Child Locked=%s, Child CanLock=%s"),
						*ChildName.ToString(),
						bParentLocked ? TEXT("YES") : TEXT("NO"),
						bChildLocked ? TEXT("YES") : TEXT("NO"),
						bChildCanLock ? TEXT("YES") : TEXT("NO"));
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  FAILED to spawn child %s!"), *ChildName.ToString());
			}
		}

		// PERFORMANCE PROFILING: End spawn tracking
		FSFHologramPerformanceProfiler::EndSpawnProfile();

		// Log component breakdown for first child to understand overhead
		if (SpawnedChildren.Num() > 0 && SpawnedChildren[0].IsValid())
		{
			FSFHologramPerformanceProfiler::LogHologramComponents(SpawnedChildren[0].Get());
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Spawned %d children, total now: %d"),
			ToSpawn, SpawnedChildren.Num());

		// [MP-SLICE0] TEMP multiplayer instrumentation — remove before release.
		// Client-side preview signal: how many child holograms exist after a grid regen,
		// and on which net side. NetMode: 0=Standalone 1=DedicatedServer 2=ListenServer 3=Client.
		{
			const int32 NetMode = ParentHologram->GetWorld() ? (int32)ParentHologram->GetWorld()->GetNetMode() : -1;
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("[MP-SLICE0] GridRegen: parent=%s NetMode=%d HasAuthority=%d grid=%dx%dx%d previewChildren=%d"),
				*ParentHologram->GetName(), NetMode, ParentHologram->HasAuthority() ? 1 : 0,
				GridCounters.X, GridCounters.Y, GridCounters.Z, SpawnedChildren.Num());
		}
	}
	else if (ChildrenNeeded < CurrentChildren)
	{
		// Need to remove excess children
		ToRemove = CurrentChildren - ChildrenNeeded;

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Removing %d excess children..."), ToRemove);

		// #418: the reconcile pass above already evicted every out-of-bounds cell, so reaching
		// this branch means cell bookkeeping is inconsistent (e.g. duplicate cells). Trim LIFO -
		// Pop FIRST, then queue (QueueChildForDestroy's internal Remove becomes a no-op) - so
		// the child count converges; cell identity self-heals on the next regen.
		for (int32 i = 0; i < ToRemove; ++i)
		{
			if (SpawnedChildren.Num() > 0)
			{
				TWeakObjectPtr<AFGHologram> ChildToRemove = SpawnedChildren.Pop();
				if (ChildToRemove.IsValid())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Queueing child for destroy (LIFO trim): %s"), *ChildToRemove->GetName());
					QueueChildForDestroy(ChildToRemove.Get());
				}
			}
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Removed %d children, total now: %d"),
			ToRemove, SpawnedChildren.Num());

		// Clean up cached belt costs for removed children and trigger parent HUD update
		if (USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld()))
		{
			if (USFAutoConnectService* AutoConnect = Subsystem->GetAutoConnectService())
			{
				// Remove cached costs for children that were just destroyed
				// (They're queued for destroy, their costs are now stale)
				for (const TWeakObjectPtr<AFGHologram>& RemovedChild : PendingDestroyChildren)
				{
					if (RemovedChild.IsValid())
					{
						AutoConnect->ClearBeltCostsForDistributor(RemovedChild.Get());
					}
				}

				// Force parent to re-aggregate costs without the removed children
				// This updates the HUD to reflect the reduced belt costs
				if (AFGConveyorAttachmentHologram* ParentDistributor = Cast<AFGConveyorAttachmentHologram>(ParentHologram))
				{
					// Trigger re-aggregation by clearing parent's cache and recalculating
					AutoConnect->ClearBeltCostsForDistributor(ParentDistributor);

					// Re-store belt previews for parent (will aggregate from remaining children)
					const TArray<TSharedPtr<FBeltPreviewHelper>>* ParentPreviews = AutoConnect->GetBeltPreviews(ParentDistributor);
					if (ParentPreviews && ParentPreviews->Num() > 0)
					{
						// CRITICAL FIX: Create a local copy before calling StoreBeltPreviews!
						// ParentPreviews is a pointer to the value INSIDE the map.
						// StoreBeltPreviews calls Emplace(), which modifies the map.
						// Passing a reference to map internals while modifying the map is undefined behavior
						// and causes crashes (Access Violation) if the map reallocates or invalidates the reference.
						TArray<TSharedPtr<FBeltPreviewHelper>> PreviewsCopy = *ParentPreviews;
						AutoConnect->StoreBeltPreviews(ParentDistributor, PreviewsCopy);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   💰 HUD updated: Re-aggregated costs after removing %d children"), ToRemove);
					}
				}
			}
		}
	}

	// CRITICAL: Notify orchestrator of grid change (Refactor: Orchestrator)
	// This triggers full re-evaluation with shared input reservation
	if (ToSpawn > 0 || ToRemove > 0)
	{
		if (USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld()))
		{
			if (USFAutoConnectOrchestrator* Orchestrator = Subsystem->GetOrCreateOrchestrator(ParentHologram))
			{
				// Defer orchestration to after children are positioned to avoid evaluating with stale transforms.
				// GridSpawnerService::RegenerateChildHologramGrid will trigger OnGridChanged() after UpdateChildPositions().
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 Orchestrator: Grid changed detected (%d spawned, %d removed) - deferring evaluation until after positioning"),
					ToSpawn, ToRemove);
			}
		}
	}

	// Update positions immediately so build gun validation sees correct state
	if (UpdateChildPositionsCallback)
	{
		UpdateChildPositionsCallback();
	}

	// The progressive positioning callback handles locked child tick/material/visibility
	// as each child is placed. Avoid sweeping every child here; large grids were paying
	// for this O(n) pass immediately after spawning, before the batch had settled.
	if (bParentLocked)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose,
			TEXT("RegenerateChildHologramGrid: locked parent; skipping immediate full child visibility sweep for %d children"),
			SpawnedChildren.Num());
	}
	else
	{
		// #418 Tier true-up: Smart child classes stub validation (or, for water pumps, run their
		// own) — ticking them buys nothing and cost real frame time at 40K+ children. Keep tick
		// OFF for everything except water pump children (their per-frame water-volume check needs
		// tick) and vanilla-delegate stackable children (preserve vanilla dynamic validation).
		const bool bKeepTickDisabled = !bVanillaDelegateChildren
			&& !ParentHologram->IsA(AFGWaterPumpHologram::StaticClass());
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid())
			{
				AFGHologram* Child = ChildPtr.Get();
				if (bKeepTickDisabled)
				{
					Child->SetActorTickEnabled(false);
					Child->ResetConstructDisqualifiers();
					Child->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
				}
				else
				{
					Child->SetActorTickEnabled(true);
				}
			}
		}
	}

	// Force a placement/cost/material refresh only when no progressive
	// positioning is pending. Active batches validate in their completion callback,
	// after every child has reached its final transform.
	if (!bProgressiveBatchActive && LastController && IsValid(LastController))
	{
		if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(LastController->GetPawn()))
		{
			if (UFGInventoryComponent* Inventory = Character->GetInventory())
			{
				ParentHologram->ValidatePlacementAndCost(Inventory);
			}
		}
	}

	// CRITICAL: Update hologram registry recipes for all existing children
	// This ensures recipe inheritance works correctly when RegenerateChildHologramGrid
	// is triggered by recipe changes (not just scaling)
	TSubclassOf<UFGRecipe> ParentStoredRecipe = USFHologramDataService::GetStoredRecipe(ParentHologram);
	if (ParentStoredRecipe)
	{
		// Update children with parent's recipe
		int32 UpdatedChildren = 0;
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid())
			{
				AFGHologram* Child = ChildPtr.Get();
				USFHologramDataService::StoreRecipe(Child, ParentStoredRecipe);
				UpdatedChildren++;
			}
		}

		if (UpdatedChildren > 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Updated hologram registry recipes for %d children with %s"),
				UpdatedChildren, *ParentStoredRecipe->GetName());
		}
	}
	else
	{
		// Clear recipes from children when parent recipe is null
		int32 ClearedChildren = 0;
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid())
			{
				AFGHologram* Child = ChildPtr.Get();
				USFHologramDataService::StoreRecipe(Child, nullptr);
				ClearedChildren++;
			}
		}

		if (ClearedChildren > 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Cleared hologram registry recipes for %d children (recipe cleared)"),
				ClearedChildren);
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SF_SCALE_REGEN] grid=%dx%dx%d total=%d neededChildren=%d spawnedChildren=%d parentChildren=%d pendingDestroy=%d trackedTransforms=%d locked=%d"),
		GridCounters.X, GridCounters.Y, GridCounters.Z,
		TotalItems,
		ChildrenNeeded,
		SpawnedChildren.Num(),
		ParentHologram->GetHologramChildren().Num(),
		PendingDestroyChildren.Num(),
		ScalingChildIntendedTransforms.Num(),
		bParentLocked ? 1 : 0);
}
