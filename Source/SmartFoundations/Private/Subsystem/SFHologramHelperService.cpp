#include "Subsystem/SFHologramHelperService.h"
#include "SFSubsystem.h"
#include "FGHologram.h"
#include "FactoryGame/Public/Hologram/FGFoundationHologram.h"
#include "FactoryGame/Public/Hologram/FGFactoryHologram.h"
#include "FactoryGame/Public/Hologram/FGBuildableHologram.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Components/BoxComponent.h"
#include "Holograms/Core/SFSmartChildHologram.h"
#include "Holograms/Core/SFSmartFactoryChildHologram.h"
#include "Holograms/Core/SFSmartLogisticsChildHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "Holograms/Core/SFFloodlightChildHologram.h"
#include "Holograms/Core/SFStandaloneSignChildHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Hologram/FGSignPoleHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFHologramDataService.h"
#include "Logging/LogMacros.h"
#include "Kismet/GameplayStatics.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"

// Module dependencies (Phase 2)
#include "SFValidationService.h"
#include "SFHologramPerformanceProfiler.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"

// Hologram adapters
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Holograms/Adapters/SFGenericAdapter.h"
#include "Holograms/Adapters/SFWallAdapter.h"
#include "Holograms/Adapters/SFPillarAdapter.h"
#include "Holograms/Adapters/SFWaterExtractorAdapter.h"

// Smart hologram base classes
#include "Holograms/Core/SFSmartHologram.h"
#include "Holograms/Adapters/SFResourceExtractorAdapter.h"
#include "Holograms/Adapters/SFFactoryAdapter.h"
#include "Holograms/Adapters/SFElevatorAdapter.h"
#include "Holograms/Adapters/SFRampAdapter.h"
#include "Holograms/Adapters/SFJumpPadAdapter.h"
#include "Holograms/Adapters/SFUnsupportedAdapter.h"

// Satisfactory hologram types for adapter factory
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGWallHologram.h"
#include "Hologram/FGPillarHologram.h"
#include "Hologram/FGStackableStorageHologram.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGPipeHyperAttachmentHologram.h"
#include "Hologram/FGCeilingLightHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Hologram/FGWallAttachmentHologram.h"
#include "Hologram/FGElevatorHologram.h"
#include "Hologram/FGStairHologram.h"
#include "Hologram/FGJumpPadHologram.h"
#include "Hologram/FGFactoryBuildingHologram.h"  // Issue #160: Zoop detection
#include "Hologram/FGWheeledVehicleHologram.h"
#include "Hologram/FGSpaceElevatorHologram.h"

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
			if (IsValid(P))
			{
				// Only consider Smart-owned children (tagged or delegate-bound) and not pending removal
				const bool bMarkedSmart = (P->ActorHasTag(FName(TEXT("SF_GridChild"))) ||
					P->OnDestroyed.IsBound());
				const bool bPendingRemoval = P->ActorHasTag(FName(TEXT("SF_GridChild_PendingDestroy")));
				if (bMarkedSmart && !bPendingRemoval)
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
				if (IsValid(P))
				{
					const bool bMarkedSmart = (P->ActorHasTag(FName(TEXT("SF_GridChild"))) ||
						P->OnDestroyed.IsBound());
					const bool bPendingRemoval = P->ActorHasTag(FName(TEXT("SF_GridChild_PendingDestroy")));
					if (!bMarkedSmart || bPendingRemoval) { continue; }

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
					UE_LOG(LogSmartFoundations, Verbose, TEXT("  Spawned child %s | Parent Locked=%s, Child Locked=%s, Child CanLock=%s"),
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

	// CRITICAL: Force visibility when parent is locked
	// When parent is locked, engine may hide children automatically
	// Explicitly unlock children to make them visible
	if (bParentLocked)
	{
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid())
			{
				AFGHologram* Child = ChildPtr.Get();
				// Force unlock to ensure visibility
				if (Child->IsHologramLocked())
				{
					Child->LockHologramPosition(false);
				}
				Child->SetActorHiddenInGame(false);

				// Phase 4 FIX: DO NOT re-enable ticking when parent is locked!
				// Ticking is disabled during spawn (line 306) to eliminate per-frame validation overhead
				// Re-enabling here would undo the performance fix and cause 3-4 FPS with large grids
				// Child->SetActorTickEnabled(true);  // REMOVED - causes massive FPS drop

				Child->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
			}
		}
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

	// Force a placement/cost/material refresh now that children exist
	if (LastController && IsValid(LastController))
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
}

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
	int32 DeferredCount = 0;

	const bool bSafeNow = CanSafelyDestroyChildren();

	for (int32 i = PendingDestroyChildren.Num() - 1; i >= 0; --i)
	{
		TWeakObjectPtr<AFGHologram> Entry = PendingDestroyChildren[i];
		if (!Entry.IsValid())
		{
			PendingDestroyChildren.RemoveAtSwap(i);
			continue;
		}

		AFGHologram* H = Entry.Get();
		if (bSafeNow)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Destroying queued child: %s"),
				(unsigned long long)GFrameCounter, TS, *H->GetName());

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
		else
		{
			// Defer actual destruction; make it inert and invisible in the meantime
			H->SetDisabled(true);
			H->SetActorHiddenInGame(true);
			H->SetActorEnableCollision(false);
			DeferredCount++;
			// Keep it in the queue for later ForceDestroy
		}
	}

	if (DestroyedCount > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Flushed queued destroys: %d"), DestroyedCount);
	}
	if (DeferredCount > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Deferred destroys due to active hologram: %d"), DeferredCount);
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
	// Conservative: only destroy when no active hologram is present
	return !ActiveHologram.IsValid();
}

bool FSFHologramHelperService::CanSafelyDestroyChildren(const AFGHologram* HologramToCheck) const
{
	// Conservative: only destroy when no active hologram is present
	return !HologramToCheck || !IsValid(HologramToCheck);
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
	// Extracted from SFSubsystem.cpp lines 2076-2080 (Phase 1 extraction)
	// CRITICAL FIX for Task 38: Temporarily unlock children during positioning
	// Parent lock blocks child transform updates, causing "move away and back" requirement
	// Unlock child, position it, then restore parent's lock state to children
	//
	// CRITICAL FIX for UObject exhaustion: Skip locking during mass updates
	// LockHologramPosition() creates UI widgets. With 700+ children, each lock
	// creates widgets, hitting UObject limit. Only lock when not suppressed.

	if (!ChildHologram || !IsValid(ChildHologram))
	{
		return false;
	}

	// Only unlock if:
	// 1. Parent is locked (lock state needs management)
	// 2. Child updates not suppressed (avoid UI widget creation)
	// 3. Child is currently locked (needs unlocking)
	if (!bSuppressChildUpdates && bParentWasLocked && ChildHologram->IsHologramLocked())
	{
		ChildHologram->LockHologramPosition(false);
		return true;  // Unlocked - needs restore later
	}

	return false;  // Not unlocked - no restore needed
}

void FSFHologramHelperService::RestoreChildLock(AFGHologram* ChildHologram, bool bParentWasLocked, bool bSuppressUpdates)
{
	// Extracted from SFSubsystem.cpp lines 2113-2117 (Phase 1 extraction)
	// Restore lock state if parent is locked (skip if suppressed)

	if (!ChildHologram || !IsValid(ChildHologram))
	{
		return;
	}

	// CRITICAL FIX: Children MUST match parent's lock state for visibility.
	// Empirical testing shows Satisfactory hides children whose lock state differs from
	// their parent. Do not remove this call unless the engine behavior changes and the
	// documentation in docs/Architecture/CRITICAL_Child_Lock_Inheritance_REQUIRED.md is
	// updated to match.
	//
	// Only restore lock if:
	// 1. Child updates not suppressed (avoid cascading updates during mass operations)
	// 2. Parent is locked (children should inherit lock state for rendering)
	if (!bSuppressUpdates && bParentWasLocked)
	{
		ChildHologram->LockHologramPosition(true);
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

	// Fire completion callback
	if (BatchState.CompletionCallback)
	{
		BatchState.CompletionCallback();
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
// Future Feature Stubs (Auto-Connect/Extend)
// ========================================

bool FSFHologramHelperService::CanAutoConnect(const AFGHologram* Source, const AFGHologram* Target) const
{
	// STUB: To be implemented for Auto-Connect feature
	// See docs/Future_Features_Analysis.md for design details
	return false;
}

void FSFHologramHelperService::ApplyAutoConnect(AFGHologram* Source, AFGHologram* Target)
{
	// STUB: To be implemented for Auto-Connect feature
	// See docs/Future_Features_Analysis.md for design details
}

bool FSFHologramHelperService::CanExtend(const AFGHologram* Hologram) const
{
	// STUB: To be implemented for Extend feature
	// See docs/Future_Features_Analysis.md for design details
	return false;
}

void FSFHologramHelperService::ApplyExtend(AFGHologram* Hologram)
{
	// STUB: To be implemented for Extend feature
	// See docs/Future_Features_Analysis.md for design details
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

	bInMassDestruction = false;
	bSuppressChildUpdates = false;
}
