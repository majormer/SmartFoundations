#include "Subsystem/SFHologramHelperServiceImpl.h"

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

	UE_LOG(LogSmartFoundations, Log, TEXT("HologramHelperService: Shutdown complete"));
}

void FSFHologramHelperService::RegisterActiveHologram(AFGHologram* Hologram)
{
	// TODO: Extract from SFSubsystem::RegisterActiveHologram

	if (!Hologram || !IsValid(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HologramHelperService: Cannot register invalid hologram"));
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

	// Issue #160: Detect vanilla Zoop and force 1x1x1 grid to prevent overlapping holograms
	// When Zoop is active (mDesiredZoop != 0), both Smart! and Zoop would create children,
	// resulting in duplicate buildings at the same location.
	if (AFGFactoryBuildingHologram* FactoryBuildingHolo = Cast<AFGFactoryBuildingHologram>(ParentHologram))
	{
		// Access mDesiredZoop - non-zero means Zoop is active
		// Note: mDesiredZoop is protected, but we can check via GetZoopInstanceTransforms()
		const TArray<FTransform>& ZoopTransforms = FactoryBuildingHolo->GetZoopInstanceTransforms();
		if (ZoopTransforms.Num() > 0)
		{
			// Zoop is active - set flag for HUD display
			if (!bZoopActive)
			{
				bZoopActive = true;
				UE_LOG(LogSmartFoundations, Warning,
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
		// Not a factory building hologram - clear Zoop flag
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
		UE_LOG(LogSmartFoundations, Error, TEXT("ValidationService module not initialized!"));
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
			UE_LOG(LogSmartFoundations, Error,
				TEXT("🛑 CRITICAL: Grid size capped from %d to %d children to prevent engine crash!"),
				ChildrenNeeded, GRID_CHILDREN_HARD_CAP);

			// Cap the grid size by proportionally reducing all dimensions
			const float ScaleFactor = FMath::Sqrt(static_cast<float>(GRID_CHILDREN_HARD_CAP) / ChildrenNeeded);
			GridCounters.X = FMath::Max(1, FMath::RoundToInt(GridCounters.X * ScaleFactor));
			GridCounters.Y = FMath::Max(1, FMath::RoundToInt(GridCounters.Y * ScaleFactor));
			GridCounters.Z = FMath::Max(1, FMath::RoundToInt(GridCounters.Z * ScaleFactor));

			// Recalculate children needed with capped dimensions
			ChildrenNeeded = FMath::Max(0, (FMath::Abs(GridCounters.X) * FMath::Abs(GridCounters.Y) * FMath::Abs(GridCounters.Z)) - 1);

			UE_LOG(LogSmartFoundations, Warning,
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
			UE_LOG(LogSmartFoundations, Error, TEXT("RegenerateChildHologramGrid: Parent hologram has no recipe!"));
			return;
		}

		AActor* HologramOwner = ParentHologram->GetOwner();
		UWorld* World = WorldContext.Get();
		if (!World)
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("RegenerateChildHologramGrid: No world context!"));
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

		for (int32 i = 0; i < ToSpawn; ++i)
		{
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
			else if (ParentHologram->IsA(AFGCeilingLightHologram::StaticClass()))
			{
				// Issue #200: Ceiling lights check ceiling snapping in CheckValidPlacement.
				// Children can't satisfy this. Use ASFBuildableChildHologram which always passes.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFBuildableChildHologram* BuildableChild = SpawnWorld->SpawnActor<ASFBuildableChildHologram>(
						ASFBuildableChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (BuildableChild)
					{
						BuildableChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						BuildableChild->SetRecipe(Recipe);
						BuildableChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(BuildableChild, ChildName);

						USFHologramDataService::DisableValidation(BuildableChild);
						USFHologramDataService::MarkAsChild(BuildableChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (BuildableChild->IsHologramLocked())
						{
							BuildableChild->LockHologramPosition(false);
						}
						BuildableChild->SetActorHiddenInGame(false);
						BuildableChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						BuildableChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						BuildableChild->SetActorTickEnabled(false);
						BuildableChild->RegisterAllComponents();
						BuildableChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						BuildableChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  BUILDABLE CHILD: Spawned %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = BuildableChild;
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
			else if (ParentHologram->IsA(AFGWallAttachmentHologram::StaticClass()))
			{
				// Issue #268: Wall attachments (conveyor ceiling supports, wall conveyor poles)
				// check wall/ceiling snapping in CheckValidPlacement.
				// Children can't satisfy this. Use ASFBuildableChildHologram which always passes.
				UWorld* SpawnWorld = WorldContext.Get();
				if (SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Name = ChildName;
					SpawnParams.Owner = ParentHologram->GetOwner();
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFBuildableChildHologram* BuildableChild = SpawnWorld->SpawnActor<ASFBuildableChildHologram>(
						ASFBuildableChildHologram::StaticClass(),
						SpawnLocation,
						FRotator::ZeroRotator,
						SpawnParams);

					if (BuildableChild)
					{
						BuildableChild->SetChildBuildClass(ParentHologram->GetBuildClass());
						BuildableChild->SetRecipe(Recipe);
						BuildableChild->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
						ParentHologram->AddChild(BuildableChild, ChildName);

						USFHologramDataService::DisableValidation(BuildableChild);
						USFHologramDataService::MarkAsChild(BuildableChild, ParentHologram, ESFChildHologramType::ScalingGrid);

						if (BuildableChild->IsHologramLocked())
						{
							BuildableChild->LockHologramPosition(false);
						}
						BuildableChild->SetActorHiddenInGame(false);
						BuildableChild->SetActorEnableCollision(false);

						TArray<UPrimitiveComponent*> Primitives;
						BuildableChild->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* PrimComp : Primitives)
						{
							PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}

						BuildableChild->SetActorTickEnabled(false);
						BuildableChild->RegisterAllComponents();
						BuildableChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
						BuildableChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  WALL ATTACHMENT CHILD: Spawned %s at %s (recipe=%s, buildClass=%s)"),
							*ChildName.ToString(), *SpawnLocation.ToString(),
							*Recipe->GetName(), *ParentHologram->GetBuildClass()->GetName());
					}
					ChildHologram = BuildableChild;
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
			else
			{
				// Normal spawn for non-passthrough holograms
				ChildHologram = SpawnChildHologram(ParentHologram, ChildName, SpawnLocation, FRotator::ZeroRotator);
			}

			if (ChildHologram)
			{
				// Issue #187/#200/#197: Passthrough, buildable, and water pump children are fully configured during
				// their custom spawn paths above (tag, collision, validation, data service, tick).
				// Skip generic setup for them — it would re-enable collision and override material state.
				const bool bIsCustomChild = ParentHologram->IsA(AFGPassthroughHologram::StaticClass())
					|| ParentHologram->IsA(AFGCeilingLightHologram::StaticClass())
					|| ParentHologram->IsA(AFGFloodlightHologram::StaticClass())
					|| ParentHologram->IsA(AFGWallAttachmentHologram::StaticClass())
					|| ParentHologram->IsA(AFGWaterPumpHologram::StaticClass());

				if (!bIsCustomChild)
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
				UE_LOG(LogSmartFoundations, Error, TEXT("  FAILED to spawn child %s!"), *ChildName.ToString());
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
	}
	else if (ChildrenNeeded < CurrentChildren)
	{
		// Need to remove excess children
		ToRemove = CurrentChildren - ChildrenNeeded;

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegenerateChildHologramGrid: Removing %d excess children..."), ToRemove);

		for (int32 i = 0; i < ToRemove; ++i)
		{
			if (SpawnedChildren.Num() > 0)
			{
				TWeakObjectPtr<AFGHologram> ChildToRemove = SpawnedChildren.Pop();
				if (ChildToRemove.IsValid())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Queueing child for destroy: %s"), *ChildToRemove->GetName());
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
		// Parent is unlocked - ensure children are ticking for dynamic validation
		// Issue #200: Ceiling lights and wall floodlights have CheckValidPlacement() overrides
		// that check ceiling/wall snapping. Children can't satisfy these, so keep tick disabled.
		// Note: Water pumps are NOT included here — they need tick for water volume validation.
		const bool bKeepTickDisabled = ParentHologram->IsA(AFGCeilingLightHologram::StaticClass())
			|| ParentHologram->IsA(AFGFloodlightHologram::StaticClass())
			|| ParentHologram->IsA(AFGWallAttachmentHologram::StaticClass());
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
