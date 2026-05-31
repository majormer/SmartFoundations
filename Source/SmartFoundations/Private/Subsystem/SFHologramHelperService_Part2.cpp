// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * FSFHologramHelperService implementation (part 2). Split out of SFHologramHelperService.cpp
 * (slice T1d, pure impl-split, one class across .cpp) to keep each file <2k. No behavior change.
 */

#include "Subsystem/SFHologramHelperServiceImpl.h"

void FSFHologramHelperService::ApplyScalingDelta(
	AFGHologram* Hologram,
	const FVector& ScalingDelta,
	FVector& CurrentScalingOffset,
	TFunction<void()> RegenerateGridCallback
)
{
	// Extracted from SFSubsystem::ApplyScalingToHologram (Refactor: Phase 1)
	// Applies scaling delta and triggers child grid regeneration

	if (!Hologram || !IsValid(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyScalingDelta: Invalid hologram"));
		return;
	}

	// Store old transform for logging
	const FTransform OldTransform = Hologram->GetTransform();
	const FVector OldLocation = OldTransform.GetLocation();

	// Update scaling offset (diagnostic tracking only)
	CurrentScalingOffset += ScalingDelta;

	// Let the Build Gun own the parent transform; only regenerate Smart! children
	const FVector NewLocation = OldLocation; // unchanged parent location for logging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SCALING APPLIED: Delta=%s | Old=%s | New=%s | TotalOffset=%s"),
		*ScalingDelta.ToString(), *OldLocation.ToString(), *NewLocation.ToString(), *CurrentScalingOffset.ToString());

	// Trigger child hologram grid regeneration via callback
	if (RegenerateGridCallback)
	{
		RegenerateGridCallback();
	}
}

void FSFHologramHelperService::QueueChildForDestroy(AFGHologram* Child)
{
	// Extracted from SFSubsystem::QueueChildForDestroy (Phase 2 - Task #61.6)
	// Deferred destruction to avoid mid-validation invalidation

	if (!Child || !IsValid(Child))
	{
		return;
	}

	// Mark as pending removal for resync filters
	Child->Tags.AddUnique(FName(TEXT("SF_GridChild_PendingDestroy")));

	// Clean up auto-connect belt previews for this child BEFORE destruction
	if (USFSubsystem* Subsystem = USFSubsystem::Get(Child->GetWorld()))
	{
		if (USFAutoConnectService* AutoConnectService = Subsystem->GetAutoConnectService())
		{
			AutoConnectService->CleanupDistributorPreviews(Child);
		}
	}

	// Remove from active children tracking
	SpawnedChildren.Remove(Child);
	ScalingChildIntendedTransforms.Remove(Child);

	// CRITICAL: Remove from parent's mChildren array IMMEDIATELY (not deferred)
	// This prevents desync between HologramHelper and parent's array
	// Build Gun iterates parent's array → must stay in sync with our tracking
	if (AFGHologram* Parent = Child->GetParentHologram())
	{
		if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
		{
			TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
			if (ChildrenArray)
			{
				ChildrenArray->Remove(Child);
			}
		}
	}

	PendingDestroyChildren.AddUnique(Child);

	if (!bPendingDestroyScheduled && WorldContext.IsValid())
	{
		bPendingDestroyScheduled = true;
		if (UWorld* World = WorldContext.Get())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HologramHelper: Scheduling FlushPendingDestroy for next tick (%d queued)"), PendingDestroyChildren.Num());
			FTimerDelegate D;
			D.BindLambda([this]() { this->FlushPendingDestroy(); });
			World->GetTimerManager().SetTimerForNextTick(D);
		}
	}
}

void FSFHologramHelperService::FlushPendingDestroy()
{
	// Extracted from SFSubsystem::FlushPendingDestroy (Phase 3 - Task #61.6)
	// Deferred destruction implementation with partial/full destroy logic

	// PERFORMANCE PROFILING: Track destroy performance
	const int32 PendingCount = PendingDestroyChildren.Num();
	if (PendingCount > 0)
	{
		FSFHologramPerformanceProfiler::BeginDestroyProfile("FlushPendingDestroy", PendingCount);
	}

	const UWorld* World = WorldContext.IsValid() ? WorldContext.Get() : nullptr;
	const double TS = World ? World->GetTimeSeconds() : 0.0;
	int32 DestroyedCount = 0;

	for (int32 i = PendingDestroyChildren.Num() - 1; i >= 0; --i)
	{
		TWeakObjectPtr<AFGHologram> Entry = PendingDestroyChildren[i];
		if (!Entry.IsValid())
		{
			PendingDestroyChildren.RemoveAtSwap(i);
			continue;
		}

		AFGHologram* H = Entry.Get();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Destroying queued child: %s"),
			(unsigned long long)GFrameCounter, TS, *H->GetName());
		ScalingChildIntendedTransforms.Remove(H);

		// NOTE: Child already removed from parent's mChildren in QueueChildForDestroy
		// This Remove call is redundant but harmless (TArray::Remove handles not-found gracefully)
		// Kept for safety in case child was added back to parent's array somehow
		if (AFGHologram* Parent = H->GetParentHologram())
		{
			if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
			{
				TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
				if (ChildrenArray)
				{
					ChildrenArray->Remove(H);  // Safe even if already removed
				}
			}
		}

		H->Destroy();
		DestroyedCount++;
		PendingDestroyChildren.RemoveAtSwap(i);
	}

	if (DestroyedCount > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Flushed queued destroys: %d"), DestroyedCount);
	}
	// PERFORMANCE PROFILING: End destroy tracking
	if (PendingCount > 0 && DestroyedCount > 0)
	{
		FSFHologramPerformanceProfiler::EndDestroyProfile();
	}

	bPendingDestroyScheduled = false;
	bSuppressChildUpdates = false;
}

void FSFHologramHelperService::ForceDestroyPendingChildren()
{
	// Extracted from SFSubsystem::ForceDestroyPendingChildren (Phase 3 - Task #61.6)
	// Emergency force-destroy when can't defer anymore

	const UWorld* World = WorldContext.IsValid() ? WorldContext.Get() : nullptr;
	const double TS = World ? World->GetTimeSeconds() : 0.0;
	int32 DestroyedCount = 0;

	for (int32 i = PendingDestroyChildren.Num() - 1; i >= 0; --i)
	{
		TWeakObjectPtr<AFGHologram> Entry = PendingDestroyChildren[i];
		if (!Entry.IsValid())
		{
			PendingDestroyChildren.RemoveAtSwap(i);
			continue;
		}
		AFGHologram* H = Entry.Get();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Force-destroying pending child: %s"),
			(unsigned long long)GFrameCounter, TS, *H->GetName());
		ScalingChildIntendedTransforms.Remove(H);

		// CRITICAL: Remove from parent's mChildren array before destroying
		if (AFGHologram* Parent = H->GetParentHologram())
		{
			// Access mChildren via reflection since GetHologramChildren() returns by value
			if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
			{
				TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
				if (ChildrenArray)
				{
					ChildrenArray->Remove(H);
				}
			}
		}

		H->Destroy();
		DestroyedCount++;
		PendingDestroyChildren.RemoveAtSwap(i);
	}
	if (DestroyedCount > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Force destroyed pending children: %d"), DestroyedCount);
	}
	bSuppressChildUpdates = false;
}

bool FSFHologramHelperService::CanSafelyDestroyChildren() const
{
	// It is safe to destroy queued children on the next tick since they have already been
	// removed from parent arrays and tracking, preventing UObject accumulation/leaks during active scaling.
	return true;
}

bool FSFHologramHelperService::CanSafelyDestroyChildren(const AFGHologram* HologramToCheck) const
{
	(void)HologramToCheck;
	return true;
}

bool FSFHologramHelperService::OnChildHologramDestroyed(AActor* DestroyedActor, TFunction<void()> UpdateChildrenCallback)
{
	// Extracted from SFSubsystem::OnChildHologramDestroyed (Phase 3 - Task #61.6)
	// Child destruction callback with mass destruction detection

	AFGHologram* DestroyedHologram = Cast<AFGHologram>(DestroyedActor);
	if (!DestroyedHologram)
	{
		return false;
	}

	SpawnedChildren.RemoveAll([DestroyedHologram](const TWeakObjectPtr<AFGHologram>& Child)
	{
		return !Child.IsValid() || Child.Get() == DestroyedHologram;
	});
	ScalingChildIntendedTransforms.Remove(DestroyedHologram);

	// CRITICAL FIX: Persistent mass destruction detection
	// Once we detect large grid destruction (100+ children), suppress updates
	// for the ENTIRE destruction sequence, not just while count >= 100
	//
	// Problem: Previous approach only suppressed while count >= 100
	// When count dropped to 99, updates resumed with 2M+ UObjects already created → crash
	//
	// Solution: Set persistent flag on first detection, clear only when all children gone
	const bool bLargeGridDestruction = SpawnedChildren.Num() >= LARGE_GRID_WARNING_THRESHOLD;

	// Detect start of mass destruction
	if (bLargeGridDestruction && !bInMassDestruction)
	{
		bInMassDestruction = true;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Mass destruction started (%d children) - suppressing updates until complete"), SpawnedChildren.Num());
	}

	// Clear flag when all children destroyed
	if (SpawnedChildren.Num() == 0 && bInMassDestruction)
	{
		bInMassDestruction = false;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Mass destruction complete - updates re-enabled"));
	}

	// Suppress updates during entire mass destruction sequence
	if (!bSuppressChildUpdates && !bInMassDestruction)
	{
		if (UpdateChildrenCallback)
		{
			UpdateChildrenCallback();
			return true; // Callback was invoked
		}
	}

	return false; // Callback was not invoked
}

void FSFHologramHelperService::OnParentHologramDestroyed(AActor* DestroyedActor)
{
	// Extracted from SFSubsystem::OnParentHologramDestroyed (Phase 3 - Task #61.6)
	// Parent hologram destruction cleanup

	AFGHologram* DestroyedHologram = Cast<AFGHologram>(DestroyedActor);
	if (!DestroyedHologram)
	{
		return;
	}

	// If the destroyed hologram is our active hologram, clean up
	if (ActiveHologram.Get() == DestroyedHologram)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Parent hologram destroyed (building placed): %s - Clearing children"),
			*DestroyedHologram->GetName());

		// ========================================
		// Recipe Application System - Apply stored recipes to all constructed buildings
		// ========================================

		// TODO: Building registration needs to happen in ConfigureActor() override
		// See original Smart: ASFFactoryHologram::ConfigureActor stores inBuildable reference
		// For now, we're missing building references so recipe application won't work yet
		//
		// Next steps:
		// 1. Create custom hologram classes (e.g., ASFFactoryHologram)
		// 2. Override ConfigureActor() to call Subsystem->RegisterSmartBuilding(inBuildable, Index, bIsParent)
		// 3. This hook will capture building references as they're constructed
		// 4. Then ApplyRecipesToCurrentPlacement() will work correctly

		USFSubsystem* Subsystem = USFSubsystem::Get(DestroyedHologram->GetWorld());
		if (Subsystem && Subsystem->bHasStoredProductionRecipe)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("RECIPE APPLICATION: Parent hologram destroyed - attempting to apply recipes to placement group %d"),
				Subsystem->CurrentPlacementGroupID);

			// Apply recipes to all buildings registered during this placement
			Subsystem->ApplyRecipesToCurrentPlacement();
		}

		// CRITICAL: Suppress updates during cleanup to prevent UObject exhaustion
		bSuppressChildUpdates = true;

		// Clean up all children without calling UnregisterActiveHologram to avoid accessing destroyed hologram
		SpawnedChildren.Empty();
		ScalingChildIntendedTransforms.Empty();

		// Re-enable updates and clear mass destruction flag
		bSuppressChildUpdates = false;
		bInMassDestruction = false;

		ActiveHologram.Reset();
	}
}

void FSFHologramHelperService::UpdateChildrenForParentTransform(
	AFGHologram* ParentHologram,
	const FTransform& OldTransform,
	const FTransform& NewTransform,
	float BaselineHeightZ,
	TFunction<void()> UpdateChildPositionsCallback,
	TFunction<void()> ValidateCallback
)
{
	// Extracted from SFSubsystem::UpdateChildrenForCurrentTransform (Phase 3 - Task #61.6)
	// Transform change detection and child update coordination

	if (!ParentHologram || !IsValid(ParentHologram))
	{
		return;
	}

	const float DeltaZ = NewTransform.GetLocation().Z - OldTransform.GetLocation().Z;
	const float CurrentHeight = NewTransform.GetLocation().Z;
	const float DeltaFromBaseline = CurrentHeight - BaselineHeightZ;

	// Check if parent has nudge offset (vanilla vertical nudge system)
	const FVector ParentNudgeOffset = ParentHologram->GetHologramNudgeOffset();

	// CRITICAL FIX: Clean up invalid children before counting/repositioning
	// Optimization passes left stale weak pointers in SpawnedChildren array
	// This caused "2 total → 0 active (removed 0 disabled, 2 invalid)" filtering
	int32 BeforeCleanup = SpawnedChildren.Num();
	SpawnedChildren.RemoveAll([](const TWeakObjectPtr<AFGHologram>& Child)
	{
		return !Child.IsValid() || (Child.IsValid() && Child->IsDisabled());
	});
	int32 AfterCleanup = SpawnedChildren.Num();

	if (BeforeCleanup != AfterCleanup)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Cleaned up stale children: %d → %d valid"),
			BeforeCleanup, AfterCleanup);
	}

	// Only log transform changes with meaningful movement (>1cm threshold) for tester diagnostics
	// Prevents log spam from sub-centimeter floating point drift
	const bool bMeaningfulChange = FMath::Abs(DeltaZ) > 1.0f || FMath::Abs(DeltaFromBaseline) > 1.0f;

	if (bMeaningfulChange)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 PARENT NUDGED: DeltaZ=%.1f cm, Baseline=%.1f cm, Children=%d"),
			DeltaZ, DeltaFromBaseline, AfterCleanup);

		// CRITICAL: Update belt previews during parent movement for dynamic tracking
		// Belt previews need to update their spline endpoints as parent hologram moves
		if (USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld()))
		{
			Subsystem->OnDistributorHologramUpdated(ParentHologram);
		}

		// Pipe previews need to update when junction hologram moves
		FString ClassName = ParentHologram->GetClass()->GetName();
		if (ClassName.Contains(TEXT("PipelineJunction")))
		{
			if (USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld()))
			{
				// Use orchestrator for pipe junction updates (replaces legacy OnPipeJunctionHologramUpdated)
				if (USFAutoConnectOrchestrator* Orchestrator = Subsystem->GetOrCreateOrchestrator(ParentHologram))
				{
					Orchestrator->OnPipeJunctionsMoved();
				}
			}
		}
	}
	else
	{
		// Log detailed transform data at Verbose for debugging
		UE_LOG(LogSmartFoundations, Verbose, TEXT("🔄 TRANSFORM CHANGE DETECTED - Parent moved/rotated (likely nudge)"));
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Old Loc: %s"), *OldTransform.GetLocation().ToString());
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   New Loc: %s"), *NewTransform.GetLocation().ToString());
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Parent Nudge Offset: %s"), *ParentNudgeOffset.ToString());
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Delta Z: %.2f cm (this change)"), DeltaZ);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Height from baseline: %.2f cm (total change since children spawned)"), DeltaFromBaseline);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Absolute height: %.2f cm (world Z)"), CurrentHeight);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   Children to reposition: %d"), AfterCleanup);
	}

	// Call UpdateChildPositions callback to reposition children
	if (UpdateChildPositionsCallback)
	{
		UpdateChildPositionsCallback();
	}

	// CRITICAL FIX: Validate children after repositioning (Bug: nudge invalidation)
	// When parent is nudged vertically, children are repositioned but not validated
	// Build Gun then finds children in "invalid" state → "Surface is too uneven" error
	// Must call ValidatePlacementAndCost() after repositioning, same as RegenerateChildHologramGrid()
	if (ValidateCallback && AfterCleanup > 0)
	{
		ValidateCallback();
	}
	else if (AfterCleanup == 0 && bMeaningfulChange)
	{
		// Only log when there was meaningful movement (not just floating point drift)
		UE_LOG(LogSmartFoundations, Verbose, TEXT("   No children to validate"));
	}
}

TSharedPtr<ISFHologramAdapter> FSFHologramHelperService::CreateHologramAdapter(AFGHologram* Hologram)
{
	// TODO: Extract from SFSubsystem::CreateHologramAdapter

	if (!Hologram || !IsValid(Hologram))
	{
		return nullptr;
	}

	// Factory pattern: detect hologram type and create appropriate adapter

	if (Cast<AFGFoundationHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFGenericAdapter>(Hologram, TEXT("Foundation")));
	}
	else if (Cast<AFGWallHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFWallAdapter>(Hologram));
	}
	else if (Cast<AFGPillarHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFPillarAdapter>(Hologram));
	}
	else if (Cast<AFGStackableStorageHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFGenericAdapter>(Hologram, TEXT("Storage")));
	}
	else if (Cast<AFGWaterPumpHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFWaterExtractorAdapter>(Hologram));
	}
	else if (Cast<AFGResourceExtractorHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFResourceExtractorAdapter>(Hologram));
	}
	else if (Cast<AFGConveyorAttachmentHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFGenericAdapter>(Hologram, TEXT("ConveyorAttachment")));
	}
	else if (Cast<AFGPipeHyperAttachmentHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFGenericAdapter>(Hologram, TEXT("HypertubeAttachment")));
	}
	else if (Cast<AFGFactoryHologram>(Hologram))
	{
		// Check registry first - some factory holograms have scaling disabled
		UClass* BuildClass = Hologram->GetBuildClass();
		if (BuildClass && USFBuildableSizeRegistry::HasProfile(BuildClass))
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildClass);
			if (!Profile.bSupportsScaling)
			{
				// Registry disables scaling for this buildable
				FString TypeName = BuildClass->GetName();
				return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFUnsupportedAdapter>(Hologram, TypeName));
			}
		}
		// Registry allows scaling or no profile - use Factory adapter
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFFactoryAdapter>(Hologram));
	}
	else if (Cast<AFGElevatorHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFElevatorAdapter>(Hologram));
	}
	else if (Cast<AFGStairHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFRampAdapter>(Hologram));
	}
	else if (Cast<AFGJumpPadHologram>(Hologram))
	{
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFJumpPadAdapter>(Hologram));
	}
	else if (Cast<AFGWheeledVehicleHologram>(Hologram) || Cast<AFGSpaceElevatorHologram>(Hologram))
	{
		FString TypeName = Hologram->GetClass()->GetName();
		return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFUnsupportedAdapter>(Hologram, TypeName));
	}

	// Default: unsupported
	FString TypeName = Hologram->GetClass()->GetName();
	return TSharedPtr<ISFHologramAdapter>(MakeShared<FSFUnsupportedAdapter>(Hologram, TypeName));
}

// ========================================
// Lock Management (Phase 1 - Task #61.6)
// ========================================

bool FSFHologramHelperService::TemporarilyUnlockChild(AFGHologram* ChildHologram, bool bParentWasLocked)
{
	(void)ChildHologram;
	(void)bParentWasLocked;
	// Bypassed: With SetActorLocation/SetActorRotation used for positioning,
	// children do not need to be unlocked during movement. This avoids creating UI widgets
	// and UObjects on every position update tick.
	return false;
}

void FSFHologramHelperService::RestoreChildLock(AFGHologram* ChildHologram, bool bParentWasLocked)
{
	// Bypassed with TemporarilyUnlockChild(): calling LockHologramPosition() for
	// scaling children creates build-gun UI widgets and drives UObject growth.
	// Use direct private-field access so locked-parent children keep vanilla's expected
	// lock inheritance without entering the UI/widget code path.
	SetChildLockStateWithoutWidgets(ChildHologram, bParentWasLocked);
}

void FSFHologramHelperService::SetChildLockStateWithoutWidgets(AFGHologram* ChildHologram, bool bLocked) const
{
	if (!ChildHologram || !IsValid(ChildHologram))
	{
		return;
	}

	ChildHologram->mHologramIsLocked = bLocked;
	if (bLocked)
	{
		ChildHologram->mHologramLockLocation = ChildHologram->GetActorLocation();
	}
}

// ========================================
// Performance Optimization (Phase 2)
// ========================================

void FSFHologramHelperService::BeginRepositionChildren()
{
	// Phase 2 Performance Optimization: Transform-Ignore Guard
	// Suppress cascading validation during batch reposition to eliminate O(n²) scaling
	// Each child's SetActorLocation/Rotation would normally trigger validation cascades
	// By setting this flag, we signal that validation should be deferred until batch complete

	bInBatchReposition = true;
	BatchRepositionStartTime = FPlatformTime::Seconds();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔒 BeginRepositionChildren: Transform guard ENABLED"));
}

void FSFHologramHelperService::EndRepositionChildren()
{
	// Phase 2 Performance Optimization: Transform-Ignore Guard
	// Restore normal transform behavior after batch complete
	// Log elapsed time for performance profiling

	if (!bInBatchReposition)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("EndRepositionChildren called without matching Begin"));
		return;
	}

	const double ElapsedSeconds = FPlatformTime::Seconds() - BatchRepositionStartTime;
	const double ElapsedMs = ElapsedSeconds * 1000.0;

	bInBatchReposition = false;
	BatchRepositionStartTime = 0.0;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔓 EndRepositionChildren: Transform guard DISABLED (elapsed: %.2f ms)"), ElapsedMs);
}

// ========================================
// Progressive Batch Reposition (Phase 4)
// ========================================

void FSFHologramHelperService::BeginProgressiveBatchReposition(
	const TArray<FGridIndex>& GridIndices,
	TFunction<void(int32)> UpdateCallback,
	TFunction<void()> CompletionCallback,
	AFGHologram* ParentHologram
)
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Spread child positioning across multiple frames to eliminate freezes
	// Process 200 children per frame to maintain 60 FPS

	// Cancel any existing batch
	if (bProgressiveBatchActive)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BeginProgressiveBatchReposition: Cancelling existing batch"));
		CancelProgressiveBatchReposition();
	}

	if (GridIndices.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("BeginProgressiveBatchReposition: No children to position; running completion callback immediately"));
		if (CompletionCallback)
		{
			CompletionCallback();
		}
		return;
	}

	// Initialize batch state
	BatchState.Reset();
	BatchState.GridIndices = GridIndices;
	BatchState.TotalChildren = GridIndices.Num();
	BatchState.CurrentIndex = 0;
	BatchState.UpdateCallback = UpdateCallback;
	BatchState.CompletionCallback = CompletionCallback;
	BatchState.ParentHologram = ParentHologram;
	BatchState.StartTime = FPlatformTime::Seconds();
	BatchState.FrameCount = 0;

	bProgressiveBatchActive = true;

	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("🔄 BeginProgressiveBatchReposition: %d children, %d per frame (est. %d frames)"),
		BatchState.TotalChildren,
		BatchState.ChildrenPerFrame,
		FMath::CeilToInt((float)BatchState.TotalChildren / BatchState.ChildrenPerFrame));
}

void FSFHologramHelperService::TickProgressiveBatchReposition(float DeltaTime)
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Process one batch of children per frame

	if (!bProgressiveBatchActive)
	{
		return;
	}

	// Validate parent hologram still exists
	if (!BatchState.ParentHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("TickProgressiveBatchReposition: Parent hologram destroyed, cancelling batch"));
		CancelProgressiveBatchReposition();
		return;
	}

	BatchState.FrameCount++;

	const int32 StartIndex = BatchState.CurrentIndex;
	const int32 EndIndex = FMath::Min(
		StartIndex + BatchState.ChildrenPerFrame,
		BatchState.TotalChildren
	);

	// Process this frame's batch
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		if (BatchState.UpdateCallback)
		{
			BatchState.UpdateCallback(i);
		}
	}

	BatchState.CurrentIndex = EndIndex;

	// Log progress (verbose to avoid spam)
	const float Progress = (float)EndIndex / BatchState.TotalChildren * 100.0f;
	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("  📊 Batch Frame %d: Positioned %d-%d/%d (%.1f%%)"),
		BatchState.FrameCount, StartIndex, EndIndex, BatchState.TotalChildren, Progress);

	// Check if batch complete
	if (BatchState.CurrentIndex >= BatchState.TotalChildren)
	{
		CompleteBatchReposition();
	}
}

void FSFHologramHelperService::CancelProgressiveBatchReposition()
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Cancel batch operation (parent destroyed or error)

	if (!bProgressiveBatchActive)
	{
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("❌ ProgressiveBatchReposition CANCELLED at %d/%d children (frame %d)"),
		BatchState.CurrentIndex, BatchState.TotalChildren, BatchState.FrameCount);

	PendingTrackedScalingChildTransformRefreshTicks = 0;
	bProgressiveBatchActive = false;
	BatchState.Reset();
}

void FSFHologramHelperService::CompleteBatchReposition()
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Batch complete - log results and fire completion callback

	const double ElapsedSeconds = FPlatformTime::Seconds() - BatchState.StartTime;
	const double ElapsedMs = ElapsedSeconds * 1000.0;
	const double MsPerFrame = BatchState.FrameCount > 0 ? ElapsedMs / BatchState.FrameCount : 0.0;
	const double MsPerChild = BatchState.TotalChildren > 0 ? ElapsedMs / BatchState.TotalChildren : 0.0;

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("✅ ProgressiveBatchReposition COMPLETE: %d children in %.2f ms across %d frames (%.2f ms/frame avg, %.3f ms/child)"),
		BatchState.TotalChildren, ElapsedMs, BatchState.FrameCount, MsPerFrame, MsPerChild);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SF_SCALE_BATCH] complete children=%d elapsedMs=%.2f frames=%d msPerFrame=%.2f msPerChild=%.3f trackedTransforms=%d"),
		BatchState.TotalChildren,
		ElapsedMs,
		BatchState.FrameCount,
		MsPerFrame,
		MsPerChild,
		ScalingChildIntendedTransforms.Num());

	// Fire completion callback
	if (BatchState.CompletionCallback)
	{
		BatchState.CompletionCallback();
	}

	if (BatchState.ParentHologram.IsValid() && BatchState.ParentHologram->IsHologramLocked() && ScalingChildIntendedTransforms.Num() > 0)
	{
		PendingTrackedScalingChildTransformRefreshTicks = 3;
	}

	// Cleanup
	bProgressiveBatchActive = false;
	BatchState.Reset();
}

float FSFHologramHelperService::GetProgressiveBatchProgress() const
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Return progress from 0.0 to 1.0

	if (!bProgressiveBatchActive || BatchState.TotalChildren == 0)
	{
		return 0.0f;
	}

	return (float)BatchState.CurrentIndex / BatchState.TotalChildren;
}

const FSFHologramHelperService::FGridIndex& FSFHologramHelperService::GetBatchGridIndex(int32 IndexInBatch) const
{
	// Phase 4 Performance Optimization: Progressive Batching
	// Access grid index during batch callback

	check(IndexInBatch >= 0 && IndexInBatch < BatchState.GridIndices.Num());
	return BatchState.GridIndices[IndexInBatch];
}

// ========================================
// UObject Warning System (Phase 5)
// ========================================

FSFHologramHelperService::EUObjectWarningLevel FSFHologramHelperService::CheckUObjectUtilization(int32 ChildCount, const FIntVector& GridCounters)
{
	// Phase 5: Progressive UObject Warning System
	// Check if we're approaching Unreal Engine's UObject limit
	// Engine crashes at 2,162,688 UObjects (hardcoded in FChunkedFixedUObjectArray)

	// UObject thresholds (based on crash analysis)
	const int32 ENGINE_LIMIT = 2162688;
	const float YELLOW_THRESHOLD = 0.50f;   // 50% headroom used
	const float ORANGE_THRESHOLD = 0.75f;   // 75% headroom used
	const float RED_THRESHOLD = 0.90f;      // 90% headroom used
	const float CRITICAL_THRESHOLD = 0.95f; // 95% headroom used

	// Get current UObject count
	const int32 CurrentUObjects = GUObjectArray.GetObjectArrayNum();

	// Calculate headroom utilization
	// Note: We assume the game starts with ~200k UObjects, so available headroom is ENGINE_LIMIT - starting count
	const float Utilization = static_cast<float>(CurrentUObjects) / ENGINE_LIMIT;

	// Determine warning level
	EUObjectWarningLevel WarningLevel = EUObjectWarningLevel::None;
	if (Utilization >= CRITICAL_THRESHOLD)
	{
		WarningLevel = EUObjectWarningLevel::Critical;
	}
	else if (Utilization >= RED_THRESHOLD)
	{
		WarningLevel = EUObjectWarningLevel::Red;
	}
	else if (Utilization >= ORANGE_THRESHOLD)
	{
		WarningLevel = EUObjectWarningLevel::Orange;
	}
	else if (Utilization >= YELLOW_THRESHOLD)
	{
		WarningLevel = EUObjectWarningLevel::Yellow;
	}

	// Only show warning if:
	// 1. Warning level changed, OR
	// 2. Grid size increased significantly (50+ children) since last warning
	const bool bShouldShowWarning = (WarningLevel != CurrentWarningLevel) ||
	                                (ChildCount > LastWarningGridSize + 50);

	if (bShouldShowWarning && WarningLevel != EUObjectWarningLevel::None)
	{
		// Log warning with appropriate severity
		const FString WarningMessage = FString::Printf(
			TEXT("📊 UObject Warning [Grid %dx%dx%d (%d children)]: %d UObjects (%.1f%% of engine limit)"),
			GridCounters.X, GridCounters.Y, GridCounters.Z,
			ChildCount,
			CurrentUObjects,
			Utilization * 100.0f
		);

		switch (WarningLevel)
		{
			case EUObjectWarningLevel::Critical:
				UE_LOG(LogSmartFoundations, Error, TEXT("🛑 CRITICAL: %s - Grid capped to prevent crash!"), *WarningMessage);
				break;
			case EUObjectWarningLevel::Red:
				UE_LOG(LogSmartFoundations, Error, TEXT("🚨 %s - Build may crash! Recommend smaller grid."), *WarningMessage);
				break;
			case EUObjectWarningLevel::Orange:
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ %s - FPS may drop significantly."), *WarningMessage);
				break;
			case EUObjectWarningLevel::Yellow:
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ %s - Consider building in sections."), *WarningMessage);
				break;
			default:
				break;
		}

		// Update tracking state
		CurrentWarningLevel = WarningLevel;
		LastWarningGridSize = ChildCount;
	}

	return WarningLevel;
}

// ========================================
// Private Helpers
// ========================================

AFGHologram* FSFHologramHelperService::SpawnChildHologram(
	AFGHologram* ParentHologram,
	FName ChildName,
	const FVector& Position,
	const FRotator& Rotation
)
{
	// Extracted from SFSubsystem::RegenerateChildHologramGrid (Phase 2 - Task #61.6)
	// Spawn a single child hologram from parent's recipe

	if (!ParentHologram || !IsValid(ParentHologram))
	{
		return nullptr;
	}

	TSubclassOf<UFGRecipe> Recipe = ParentHologram->GetRecipe();
	if (!Recipe)
	{
		return nullptr;
	}

	AActor* HologramOwner = ParentHologram->GetOwner();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SpawnChildHologram: Creating child %s for parent %s"),
		*ChildName.ToString(), *ParentHologram->GetName());

	// ALEX'S ORIGINAL APPROACH: Use vanilla holograms + data structure control
	// This works reliably - children may appear red but still place correctly
	AFGHologram* ChildHologram = AFGHologram::SpawnChildHologramFromRecipe(
		ParentHologram,                                 // Parent hologram reference
		ChildName,                                      // Child name (FName parameter)
		Recipe,                                         // Recipe to spawn from
		HologramOwner ? HologramOwner : ParentHologram->GetOwner(),  // Owner actor
		Position,                                       // Spawn location
		nullptr                                         // Callback - not needed
	);

	if (!ChildHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("SpawnChildHologram: Failed to spawn child from recipe %s"),
			*Recipe->GetName());
		return nullptr;
	}

	// Apply Smart data structure control to vanilla child
	// Note: Children may appear red (validation failed) but still place correctly
	USFHologramDataService::DisableValidation(ChildHologram);
	USFHologramDataService::MarkAsChild(ChildHologram, ParentHologram, ESFChildHologramType::ScalingGrid);

	// Copy parent's STORED recipe to child (not parent's current recipe)
	// Get stored recipe from subsystem (where StoreProductionRecipeFromBuilding stores it)
	USFSubsystem* Subsystem = USFSubsystem::Get(ParentHologram->GetWorld());
	TSubclassOf<UFGRecipe> ParentStoredRecipe = nullptr;
	if (Subsystem && Subsystem->bHasStoredProductionRecipe)
	{
		ParentStoredRecipe = Subsystem->StoredProductionRecipe;
	}

	if (ParentStoredRecipe)
	{
		USFHologramDataService::StoreRecipe(ChildHologram, ParentStoredRecipe);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SpawnChildHologram: Copied parent's stored recipe %s to child %s"),
			*ParentStoredRecipe->GetName(), *ChildHologram->GetName());
	}
	else
	{
		// Fallback: use parent's current recipe if no stored recipe exists
		USFHologramDataService::StoreRecipe(ChildHologram, ParentHologram->GetRecipe());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SpawnChildHologram: No stored recipe found, used parent's current recipe %s for child %s"),
			*ParentHologram->GetRecipe()->GetName(), *ChildHologram->GetName());
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SpawnChildHologram: Created vanilla child %s with data structure control"),
		*ChildName.ToString());

	// Final check: Log material state before returning
	if (ChildHologram)
	{
		EHologramMaterialState FinalState = ChildHologram->GetHologramMaterialState();
		const TCHAR* FinalStateStr = (FinalState == EHologramMaterialState::HMS_OK) ? TEXT("OK") :
		                            (FinalState == EHologramMaterialState::HMS_WARNING) ? TEXT("WARNING") :
		                            TEXT("ERROR");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SpawnChildHologram: Final material state for child %s: %s"),
			*ChildHologram->GetName(), FinalStateStr);
	}

	return ChildHologram;
}

void FSFHologramHelperService::DestroyAllChildren()
{
	bSuppressChildUpdates = true;

	// Detect mass destruction
	if (SpawnedChildren.Num() > LARGE_GRID_WARNING_THRESHOLD)
	{
		bInMassDestruction = true;
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("HologramHelperService: Mass destruction of %d children detected"),
			SpawnedChildren.Num()
		);
	}

	for (TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (AFGHologram* Child = ChildPtr.Get())
		{
			if (IsValid(Child))
			{
				Child->Destroy();
			}
		}
	}

	SpawnedChildren.Empty();
	PendingDestroyChildren.Empty();
	ScalingChildIntendedTransforms.Empty();

	bInMassDestruction = false;
	bSuppressChildUpdates = false;
}

