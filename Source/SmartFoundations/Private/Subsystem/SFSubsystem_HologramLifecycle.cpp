// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - active-hologram lifecycle (Register/Unregister/Poll) + multi-step property sync.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Holograms/Logistics/SFPipelinePoleChildHologram.h"


// Hologram management with enhanced logging
void USFSubsystem::RegisterActiveHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM REGISTRATION FAILED: Null hologram pointer"));
		return;
	}

	bool bRestoredExtendActiveAtRegister = IsRestoredExtendModeActive();

	// Lazy initialization on first hologram registration (Task #58)
	if (!bSubsystemInitialized)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Subsystem: Lazy initialization on first hologram registration"));

			// Load configuration (deferred from Initialize() to ensure ConfigManager is available)
			if (!bConfigLoaded)
			{
				LoadConfiguration();
			}

			// Initialize HUD widgets
			InitializeWidgets();

#if SMART_ARROWS_ENABLED
			// Initialize StaticMeshComponent-based arrow visualization (Task 17)
			// Always initialize so Num1 toggle works, but respect config for initial visibility
			if (ArrowModule && ArrowModule->Initialize(World, this, this))
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("✅ Arrow visualization initialized (StaticMeshComponent) - Config: bShowArrows=%s, Runtime: %s"),
					CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"),
					bArrowsRuntimeVisible ? TEXT("true") : TEXT("false"));

				// Start arrow drawing timer (every frame = ~16ms at 60fps)
				World->GetTimerManager().SetTimer(
					ArrowTickTimer,
					this,
					&USFSubsystem::TickArrows,
					0.016f,  // ~60fps
					true     // Repeat
				);
				UE_LOG(LogSmartFoundations, Log, TEXT("Started arrow tick timer"));
			}
			else
			{
				UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to initialize arrow visualization"));
			}
#else
			UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrow visualization disabled (SMART_ARROWS_ENABLED=0)"));
#endif

			bSubsystemInitialized = true;
		}
	}

	if (bRestoredExtendActiveAtRegister && ExtendService && !ExtendService->IsHologramCompatibleWithRestoredCloneTopology(Hologram))
	{
		ExtendService->ClearRestoredCloneTopologySession(TEXT("RegisterActiveHologram build class mismatch"));
		if (RestoreService)
		{
			RestoreService->ClearActiveRestoreSession(TEXT("Active buildable changed away from restored Extend source"));
		}
		bRestoredExtendActiveAtRegister = false;

		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore][Extend] Dropped restored topology for incompatible active hologram: hologram=%s buildClass=%s"),
			*GetNameSafe(Hologram),
			*GetNameSafe(Hologram->GetBuildClass()));
	}

	// Reset auto-connect runtime settings when hologram changes
	ResetAutoConnectRuntimeSettings();

	// Issue #198: Reset one-shot Smart disable flag when hologram changes
	ResetSmartDisableFlag();

	// Reset current auto-connect setting based on hologram type
	// This ensures the HUD shows appropriate defaults for the new hologram type
	// NOTE: We determine the setting AFTER ActiveHologram is assigned so detection works
	// Each hologram type has its own "first" setting in its cycle
	if (AutoConnectService)
	{
		if (AutoConnectService->IsPowerPoleHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
		}
		else if (AutoConnectService->IsPipelineJunctionHologram(Hologram) ||
		         AutoConnectService->IsStackablePipelineSupportHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::Enabled; // Pipe context
		}
		else if (USFAutoConnectService::IsBeltSupportHologram(Hologram))
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
		}
		else
		{
			CurrentAutoConnectSetting = EAutoConnectSetting::Enabled; // Belt context (distributors)
		}
	}
	else
	{
		CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
	}

	// NOTE: Hologram swapping is DISABLED to fix the infinite re-registration loop
	// The build gun holds the original hologram reference, and swapping creates a mismatch:
	// - BuildState->GetHologram() returns vanilla hologram
	// - ActiveHologram is our swapped custom hologram
	// - PollForActiveHologram sees mismatch → unregister/re-register → infinite loop
	//
	// EXTEND and Scaling work with vanilla holograms - no swap needed.
	// (The MP spec-construction path is hologram-class-agnostic: it rides SML hooks on the vanilla
	// virtual bodies + the per-hologram data registry - see RegisterSpecConstructionHooks in
	// SFGameInstanceModule. An earlier iteration swapped to ASF spec-parent classes here; that was
	// live-validated but replaced by the hook path for total coverage incl. BP hologram wrappers.)
	ActiveHologram = Hologram;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RegisterActiveHologram: %s"), *Hologram->GetName());

	// Issue #281: Notify hint bar service
	if (HintBarService)
	{
		HintBarService->OnHologramRegistered();
	}

	// NOTE: Pipe previews are now handled by the Auto-Connect Orchestrator (created below)
	// Don't call PipeAutoConnectManager directly here to avoid duplicate managers

	CurrentScalingOffset = FVector::ZeroVector;

	// Clear recipe cache when switching to new hologram type
	if (SortedFilteredRecipes.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Clearing recipe cache for new hologram type (had %d recipes)"),
			SortedFilteredRecipes.Num());
		SortedFilteredRecipes.Empty();
		CurrentRecipeIndex = 0;
	}

	// Clear active recipe when switching to new hologram type to prevent stale display
	if (ActiveRecipe != nullptr)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Clearing active recipe for new hologram type"));
		ActiveRecipe = nullptr;
		ActiveRecipeSource = ERecipeSource::None;
	}

	// Clear any pending recipe regeneration timer when switching building types
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
	}

	// Reset all counters for new hologram (centralized - Task 51).
	// Restored Extend presets are edited as a sticky staged graph; vanilla may recreate
	// the parent hologram while aiming, so preserve Smart transform state across that swap.
	if (!bRestoredExtendActiveAtRegister)
	{
		ResetCounters();
	}
	else
	{
		if (GridStateService)
		{
			GridStateService->UpdateCounterState(CounterState);
		}
		UpdateCounterDisplay();
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
			TEXT("[SmartRestore][Extend] Preserved staged counters across hologram registration: grid=[%d,%d,%d] spacing=(%d,%d,%d) steps=(%d,%d) rotation=%.1f"),
			CounterState.GridCounters.X,
			CounterState.GridCounters.Y,
			CounterState.GridCounters.Z,
			CounterState.SpacingX,
			CounterState.SpacingY,
			CounterState.SpacingZ,
			CounterState.StepsX,
			CounterState.StepsY,
			CounterState.RotationZ);
	}

	// Reset HUD state to prevent stale display from previous hologram
	if (HudService)
	{
		HudService->ResetState();
	}
	if (bRestoredExtendActiveAtRegister)
	{
		UpdateCounterDisplay();
	}

	// Reset Zoop flag in HologramHelper (Issue #160)
	if (HologramHelper)
	{
		HologramHelper->ClearZoopState();
	}

	// Log counter state after reset to verify proper initialization
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" COUNTER STATE AFTER RESET:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Grid: [%d, %d, %d] = %d items | ValidChildren=%d"),
		CounterState.GridCounters.X, CounterState.GridCounters.Y, CounterState.GridCounters.Z,
		CounterState.GridCounters.X * CounterState.GridCounters.Y * CounterState.GridCounters.Z,
		CounterState.ValidChildCount);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Spacing: Mode=%d Axis=%d | X=%d Y=%d Z=%d (cm)"),
		(int32)CounterState.SpacingMode, (int32)CounterState.SpacingAxis,
		CounterState.SpacingX, CounterState.SpacingY, CounterState.SpacingZ);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Steps: Axis=%d | X=%d Y=%d (cm)"),
		(int32)CounterState.StepsAxis, CounterState.StepsX, CounterState.StepsY);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Stagger: Axis=%d | X=%d Y=%d ZX=%d ZY=%d (cm)"),
		(int32)CounterState.StaggerAxis, CounterState.StaggerX, CounterState.StaggerY,
		CounterState.StaggerZX, CounterState.StaggerZY);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Summary: GridCounters=[%d,%d,%d] SpacingMode=%d SpacingX=%d StepsX=%d StaggerX=%d"),
        CounterState.GridCounters.X, CounterState.GridCounters.Y, CounterState.GridCounters.Z,
        (int32)CounterState.SpacingMode, CounterState.SpacingX, CounterState.StepsX, CounterState.StaggerX);

	// Bind to hologram destruction to clean up when building is placed
	Hologram->OnDestroyed.AddUniqueDynamic(this, &USFSubsystem::OnParentHologramDestroyed);

	// Initialize transform cache via transform service (Phase 3 extraction)
	if (GridTransformService)
	{
		GridTransformService->DetectAndPropagateTransformChange(Hologram); // Initialize cache
	}

	// Create adapter to connect feature modules to this hologram
	CurrentAdapter = CreateHologramAdapter(Hologram);

	// Identify what we're trying to build (recipe/build class) for troubleshooting
	UClass* RecipeUClass = Hologram->GetRecipe();
	UClass* BuildUClass = Hologram->GetBuildClass();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BUILD SELECTION: Recipe=%s BuildClass=%s Hologram=%s"),
		*GetNameSafe(RecipeUClass), *GetNameSafe(BuildUClass), *Hologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("My log is running."));

	// Apply default ramp stepping if this is a ramp hologram (Task 76.3)
	// This sets a sensible default X-axis step based on the ramp's slope
	// Only applied once when ramp is first selected; user adjustments override this
	// NOTE: We check build class name instead of hologram type because ramps inherit from
	// AFGFoundationHologram and get detected as foundations in adapter creation
	if (BuildUClass && USFBuildableSizeRegistry::HasProfile(BuildUClass))
	{
		FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildUClass);
		float RampUnitHeight = USFBuildableSizeRegistry::GetRampUnitHeight(Profile);

		// Only apply default if this is a ramp (RampUnitHeight != 0) and StepsX is still unset (0)
		// Note: RampUnitHeight can be negative for descending structures (walkway ramps, catwalk stairs)
		if (RampUnitHeight != 0.0f && CounterState.StepsX == 0)
		{
			// Convert unit height to integer cm (rounding to nearest 100cm for clean values)
			int32 DefaultStepX = FMath::RoundToInt(RampUnitHeight / 100.0f) * 100;
			CounterState.StepsX = DefaultStepX;

			// Sync the updated state back to GridStateService if it exists
			if (GridStateService)
			{
				GridStateService->UpdateCounterState(CounterState);
			}

			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("🔧 RAMP DEFAULT STEPPING: Applied default StepsX=%dcm for %s (UnitHeight=%.1fcm)"),
				DefaultStepX, *Profile.BuildableClassName, RampUnitHeight);

			// Trigger HUD refresh to show the new stepping value
			UpdateCounterDisplay();
		}
	}

	// Apply default spacing for belt/pipe support poles (User Request, Issue #268)
	// Sets 54m (5400cm) spacing to facilitate bus building
	// Wall conveyor poles scale along Y, so default spacing goes on Y axis for them
	if (USFAutoConnectService::IsStackableSupportHologram(Hologram) || USFAutoConnectService::IsBeltSupportHologram(Hologram))
	{
		const bool bWallPole = USFAutoConnectService::IsWallConveyorPoleHologram(Hologram);
		int32& SpacingAxis = bWallPole ? CounterState.SpacingY : CounterState.SpacingX;

		if (SpacingAxis == 0)
		{
			SpacingAxis = 5400; // 54m (belts vanish at 55m due to belt length limit)

			// Sync the updated state back to GridStateService if it exists
			if (GridStateService)
			{
				GridStateService->UpdateCounterState(CounterState);
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 SUPPORT POLE: Applied default Spacing%s=5400cm for bus layout"),
				bWallPole ? TEXT("Y") : TEXT("X"));
			UpdateCounterDisplay();
		}
	}

	// Smart Auto-Connect: Check if this is a distributor hologram for auto-connect
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: Checking hologram type for auto-connect"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: AutoConnectService available: %s"), AutoConnectService ? TEXT("YES") : TEXT("NO"));

	if (AutoConnectService && AutoConnectService->IsDistributorHologram(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Distributor hologram detected - enabling auto-connect"));
		OnDistributorHologramUpdated(Hologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 DEBUG: Not a distributor hologram - auto-connect disabled"));
	}

	if (CurrentAdapter && CurrentAdapter->IsValid())
	{
		// Log hologram lock state for diagnostics (Task 38)
		const bool bIsLocked = Hologram->IsHologramLocked();
		const bool bCanLock = Hologram->CanLockHologram();
		const bool bCanNudge = Hologram->CanNudgeHologram();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HOLOGRAM REGISTERED: %s (%s adapter) - Ready for scaling | LOCK STATE: Locked=%s, CanLock=%s, CanNudge=%s"),
			*Hologram->GetName(), *CurrentAdapter->GetAdapterTypeName(),
			bIsLocked ? TEXT("YES") : TEXT("NO"),
			bCanLock ? TEXT("YES") : TEXT("NO"),
			bCanNudge ? TEXT("YES") : TEXT("NO"));

		// Debug: Check recipe cache initialization conditions
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ DEBUG: Cache init check - bHasStored=%d, Recipe=%s, CacheSize=%d"),
			bHasStoredProductionRecipe, StoredProductionRecipe ? TEXT("Valid") : TEXT("Null"), SortedFilteredRecipes.Num());

		// Initialize recipe cache now that hologram is registered
		// Cache should initialize for ANY factory building, not just when we have a stored recipe
		if (SortedFilteredRecipes.Num() == 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Initializing recipe cache after hologram registration"));
			SortedFilteredRecipes = GetFilteredRecipesForCurrentHologram();
			if (SortedFilteredRecipes.Num() > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe cache populated: %d recipes available"), SortedFilteredRecipes.Num());

				// Restore active recipe from stored recipe if available and no manual selection is active
				if (bHasStoredProductionRecipe && StoredProductionRecipe && ActiveRecipeSource != ERecipeSource::ManuallySelected)
				{
					// Find the stored recipe in the new cache
					for (int32 i = 0; i < SortedFilteredRecipes.Num(); i++)
					{
						if (SortedFilteredRecipes[i] == StoredProductionRecipe)
						{
							ActiveRecipe = StoredProductionRecipe;
							CurrentRecipeIndex = i;
							ActiveRecipeSource = ERecipeSource::Copied;
							StoredRecipeDisplayName = GetRecipeDisplayName(ActiveRecipe);

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Restored active recipe from stored: %s [%d/%d]"),
								*StoredRecipeDisplayName, i + 1, SortedFilteredRecipes.Num());
							break;
						}
					}
				}

				UpdateCounterDisplay(); // Refresh display with populated cache
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ DEBUG: Cache already exists with %d recipes"), SortedFilteredRecipes.Num());
		}

		// ========================================
		// Recipe Copying System - Use existing stored recipe
		// ========================================

		// Recipe storage should only come from existing buildings via StoreProductionRecipeFromBuilding
		// Holograms only have build recipes, not production recipes
		// We preserve any existing stored production recipe for inheritance

		if (!Hologram->GetParentHologram()) // Only log for parent holograms
		{
			if (bHasStoredProductionRecipe)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: Using stored production recipe %s for child inheritance"),
					*StoredProductionRecipe->GetName());
			}
			else
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: No stored production recipe - middle-click an existing building to copy its recipe"));
			}
		}

		// Calculate and cache building size once at registration
		// This size is reused for all child positioning to avoid repeated adapter queries
		// Priority: Use validated registry dimensions if available, otherwise fall back to adapter bounds
		bool bBuildingSupportsScaling = true;  // Default to true for unknown buildings (safe fallback)

		if (USFBuildableSizeRegistry::HasProfile(BuildUClass))
		{
			// Use validated dimensions from registry
			AFGBuildableHologram* BuildableHologram = Cast<AFGBuildableHologram>(Hologram);
			if (BuildableHologram)
			{
				FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildUClass);
				CachedBuildingSize = USFBuildableSizeRegistry::GetSizeForHologram(BuildableHologram);
				CachedAnchorOffset = Profile.AnchorOffset;
				bBuildingSupportsScaling = Profile.bSupportsScaling;  // Issue #164: Check if building is scalable

				if (!CachedAnchorOffset.IsZero())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ CACHED BUILDING SIZE: %s | ANCHOR OFFSET: %s | Source: %s | Scalable: %s"),
						*CachedBuildingSize.ToString(), *CachedAnchorOffset.ToString(), *Profile.SourceFile,
						bBuildingSupportsScaling ? TEXT("YES") : TEXT("NO"));
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ CACHED BUILDING SIZE: %s | Source: %s | Scalable: %s"),
						*CachedBuildingSize.ToString(), *Profile.SourceFile,
						bBuildingSupportsScaling ? TEXT("YES") : TEXT("NO"));
				}
			}
			else
			{
				// Shouldn't happen - HasProfile returned true but hologram isn't buildable
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ Registry has profile but hologram is not AFGBuildableHologram - using adapter fallback"));
				const FBoxSphereBounds AdapterBounds = CurrentAdapter->GetBuildingBounds();
				CachedBuildingSize = AdapterBounds.BoxExtent * 2.0f;
				CachedAnchorOffset = FVector::ZeroVector;
			}
		}
		else
		{
			// Fallback: Use adapter bounds for unknown/modded buildables
			const FBoxSphereBounds AdapterBounds = CurrentAdapter->GetBuildingBounds();
			CachedBuildingSize = AdapterBounds.BoxExtent * 2.0f;
			CachedAnchorOffset = FVector::ZeroVector;

			// Validate and clamp to reasonable range
			const float MaxReasonableSize = 2000.0f; // 20 meters
			if (CachedBuildingSize.X > 1.f && CachedBuildingSize.Y > 1.f && CachedBuildingSize.Z > 1.f)
			{
				CachedBuildingSize.X = FMath::Min(CachedBuildingSize.X, MaxReasonableSize);
				CachedBuildingSize.Y = FMath::Min(CachedBuildingSize.Y, MaxReasonableSize);
				CachedBuildingSize.Z = FMath::Min(CachedBuildingSize.Z, MaxReasonableSize);
			}
			else
			{
				// Invalid size, use default
				CachedBuildingSize = FVector(800.0f, 800.0f, 100.0f);
			}

			UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ CACHED BUILDING SIZE: %s via %s fallback (unknown buildable - consider adding to registry)"),
				*CachedBuildingSize.ToString(), *CurrentAdapter->GetAdapterTypeName());
		}

		// Task 52 FIX: Reset lock ownership for new hologram
		// When building while holding modifiers, vanilla releases the lock on hologram transition.
		// We need to reset our ownership flag and re-acquire if modifiers are still held.
		bLockedByModifier = false;

		// If any modal features are still active (modifiers held during build), re-acquire lock
		if (IsAnyModalFeatureActive())
		{
			TryAcquireHologramLock();
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔒 Lock re-acquired for new hologram (modifiers still held)"));
		}

#if SMART_ARROWS_ENABLED
		// Issue #164: Only attach arrows to scalable buildings
		// Non-scalable buildings (belts, pipes, extractors) should not show scaling arrows
		if (bBuildingSupportsScaling)
		{
			// Attach arrows to hologram
			if (ArrowModule && ArrowModule->IsInitialized())
			{
				USceneComponent* RootComponent = Hologram->GetRootComponent();

				if (RootComponent && ArrowModule->AttachToHologram(RootComponent))
				{
					// Reset arrow state for new hologram - should show all 3 arrows initially
					LastAxisInput = ELastAxisInput::None;
					ArrowModule->SetHighlightedAxis(ELastAxisInput::None);

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ ARROWS ATTACHED TO HOLOGRAM - State reset to show all 3 arrows"));
				}
				else
				{
					// AttachToHologram returns false when assets are still loading (async)
					// The deferred attachment system will complete the attachment when assets are ready
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏳ DEFERRED ARROW ATTACHMENT - Waiting for assets to load (arrows will appear shortly)"));
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, Error, TEXT("❌ ARROW MODULE NOT READY"));
			}
		}
		else
		{
			// Building does not support scaling - skip arrow attachment
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏭️ ARROWS SKIPPED - Building does not support scaling (Issue #164)"));
		}
#endif
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM REGISTERED: %s - Adapter creation failed"), *Hologram->GetName());
	}

	// Initial auto-connect check for distributor holograms (fixes missing previews on first selection)
	if (Hologram && AutoConnectService && AutoConnectService->IsDistributorHologram(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT: Initial check for newly selected distributor hologram: %s"), *Hologram->GetName());
		OnDistributorHologramUpdated(Hologram);

		// Issue #269: Post-registration refresh with FORCE RECREATE on next tick
		// At 1x1x1 (no children), OnGridChanged never fires so the forced initial evaluation
		// is the only opportunity for a clean slate. OnDistributorsMoved (non-forced) was insufficient.
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AFGHologram> WeakHolo = Hologram;
			FTimerDelegate D;
			D.BindLambda([this, WeakHolo]()
			{
				if (!WeakHolo.IsValid()) return;
				if (USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(WeakHolo.Get()))
				{
					Orchestrator->OnGridChanged();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Post-registration forced refresh (next tick) for %s"), *WeakHolo->GetName());
				}
			});
			World->GetTimerManager().SetTimerForNextTick(D);
		}
	}
	ActiveHologram = Hologram;

	if (bRestoredExtendActiveAtRegister && ExtendService)
	{
		TSharedPtr<FSFCloneTopology> RestoredTemplate = ExtendService->GetLastCloneTopology();
		if (RestoredTemplate.IsValid() && RestoredTemplate->ChildHolograms.Num() > 0)
		{
			ExtendService->ReplayRestoreCloneTopology(Hologram, *RestoredTemplate);
			SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore][Extend] Reattached staged topology to new active hologram: parent=%s templateChildren=%d"),
				*GetNameSafe(Hologram),
				RestoredTemplate->ChildHolograms.Num());
		}
	}

	// Issue #208/#209: Notify recipe service of build class change
	// OnNewBuildSession will bump session ID only if the build class differs from the shard source
	if (RecipeManagementService)
	{
		RecipeManagementService->OnNewBuildSession(Hologram->GetBuildClass());
	}

	// [#358] Smart!'s input mapping context is scoped to hologram-active so its bindings
	// (e.g. Scale X on X) only shadow vanilla keys while actually building - 1.2's
	// Customizer key (X) must reach vanilla when no hologram is in hand.
	if (InputHandler)
	{
		InputHandler->SetSmartContextActive(true);
	}

	// Notify external systems (like SmartCamera)
	if (OnHologramCreated.IsBound())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎥 Broadcasting OnHologramCreated to external systems"));
		OnHologramCreated.Broadcast(Hologram);
	}
}

void USFSubsystem::UnregisterActiveHologram(AFGHologram* Hologram)
{
	if (Hologram && ActiveHologram.Get() == Hologram)
	{
		// Notify external systems before cleanup
		if (OnHologramDestroyed.IsBound())
		{
			OnHologramDestroyed.Broadcast(Hologram);
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Unregistering active hologram: %s"), *Hologram->GetName());

		// [#358] Return Smart!'s keys to vanilla as soon as the player stops building
		if (InputHandler)
		{
			InputHandler->SetSmartContextActive(false);
		}

		// Issue #281: Remove Smart! hints from hint bar
		if (HintBarService)
		{
			HintBarService->OnHologramUnregistered();
		}

		// CRITICAL: Suppress child updates during mass destruction to prevent UObject exhaustion
		// When destroying 700+ children, each OnChildHologramDestroyed would trigger expensive updates
		bSuppressChildUpdates = true;

		// Clean up spawned children from HologramHelper (Phase 3 fix)
		if (HologramHelper)
		{
			TArray<TWeakObjectPtr<AFGHologram>> CurrentChildren = HologramHelper->GetSpawnedChildren();
			for (TWeakObjectPtr<AFGHologram>& Child : CurrentChildren)
			{
				if (Child.IsValid())
				{
					QueueChildForDestroy(Child.Get());
				}
			}
		}

		// Re-enable updates and clear mass destruction flag after manual cleanup complete
		bSuppressChildUpdates = false;
		bInMassDestruction = false;

		ActiveHologram.Reset();
		CurrentScalingOffset = FVector::ZeroVector;

		// Clear Smart Dismantle blueprint proxy for this build session (Issue #166)
		if (CurrentBuildProxy.IsValid())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🏗️ SMART DISMANTLE: Build session ended - clearing proxy %s"), *CurrentBuildProxy->GetName());
			CurrentBuildProxy.Reset();
			CurrentProxyOwner.Reset();
		}

		// Clear transform cache via transform service (Phase 3 extraction)
		if (GridTransformService)
		{
			GridTransformService->ClearCache();
		}

		// Reset multi-step hologram property cache (Issue #200)
		CachedParentFixtureAngle = INT_MIN;
		CachedParentBuildStep = 0;

		// Reset auto-hold state (Issue #273)
		bAutoHoldActive = false;
		bAutoHoldUserOverrode = false;

		// Clear adapter
		CurrentAdapter.Reset();

		// Clear auto-connect belt preview helpers when hologram is cancelled
		// This ensures belt holograms don't persist after build cancellation
		if (AutoConnectService)
		{
			AutoConnectService->ClearBeltPreviewHelpers();
			// Clear all pipe managers (previews are managed by auto-connect service now)
			AutoConnectService->ClearAllPipeManagers();
		}

		// Clean up EXTEND service children when hologram is cancelled
		// Always call cleanup - it's safe even if EXTEND wasn't active (just clears empty arrays)
		// This ensures stale child holograms are destroyed when switching away from EXTEND
		if (ExtendService)
		{
			ExtendService->CleanupExtension(Hologram);
		}

#if SMART_ARROWS_ENABLED
		// Detach arrows from hologram and reset state
		if (ArrowModule)
		{
			ArrowModule->DetachFromHologram();
			LastAxisInput = ELastAxisInput::None;
			ArrowModule->SetHighlightedAxis(ELastAxisInput::None);
		}
#endif

		// Reset all counters (fixes persistence bug - Task 51), except while a
		// restored Extend topology is staged for editing across transient aim gaps.
		if (!IsRestoredExtendModeActive())
		{
			ResetCounters();
		}
		else
		{
			UpdateCounterDisplay();
			SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore][Extend] Preserved staged counters across hologram unregister: grid=[%d,%d,%d]"),
				CounterState.GridCounters.X,
				CounterState.GridCounters.Y,
				CounterState.GridCounters.Z);
		}

		// CRITICAL FIX: Reset auto-connect runtime settings so next build session loads from global config
		// Without this, runtime settings persist between build sessions, ignoring global config changes
		AutoConnectRuntimeSettings.bInitialized = false;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Reset auto-connect runtime settings (will reload from config on next hologram)"));
	}
	else
	{
		if (Hologram)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM UNREGISTER FAILED: %s not currently active"), *Hologram->GetName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("HOLOGRAM UNREGISTER FAILED: Null hologram pointer"));
		}
	}
}

void USFSubsystem::PollForActiveHologram()
{
	// Get the player controller
	AFGPlayerController* PC = LastController.IsValid() ? LastController.Get() : nullptr;
	if (!PC)
	{
		// Try to find player controller if we don't have one cached
		UWorld* World = GetWorld();
		if (World && World->GetFirstPlayerController())
		{
			PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
			if (PC)
			{
				LastController = PC;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("PollForActiveHologram: Found player controller"));
			}
		}

		if (!PC)
		{
			return; // Still no controller - early return
		}
	}

	// Get the player character
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Player character unavailable"));

		// Clear hologram if character is not available
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}
		return;
	}

	// Get the build gun
	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Build gun unavailable"));
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}
		return;
	}

	// Subscribe to recipe sampling delegate for recipe copying (only once!)
	if (BuildGun && IsValid(BuildGun) && !bHasSubscribedToRecipeSampled)
	{
		BuildGun->mOnRecipeSampled.AddDynamic(this, &USFSubsystem::OnBuildGunRecipeSampled);
		bHasSubscribedToRecipeSampled = true;
		UE_LOG(LogSmartFoundations, Log, TEXT("RECIPE COPYING: Subscribed to build gun's OnRecipeSampled delegate"));
	}
	else if (!bHasSubscribedToRecipeSampled)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("RECIPE COPYING: BuildGun is invalid, cannot subscribe to recipe sampling delegate"));
	}

	// Check if we're in build state
	if (!BuildGun->IsInState(EBuildGunState::BGS_BUILD))
	{
		const bool bAbortedRestore = AbortRestoreSession(TEXT("Build gun left build state"));

		// Clear hologram if not in build state
		if (ActiveHologram.IsValid())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Clearing hologram: Build gun not in build state"));
			UnregisterActiveHologram(ActiveHologram.Get());
		}
		else if (bAbortedRestore)
		{
			ResetCounters();
		}

		// CRITICAL: Final cleanup for EXTEND when build gun leaves build mode
		// This catches any orphaned preview holograms that weren't cleaned up normally
		if (ExtendService)
		{
			ExtendService->CheckAndPerformFinalCleanup();
		}
		return;
	}

	// Get the build state
	UFGBuildGunState* CurrentState = BuildGun->GetCurrentState();
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(CurrentState);
	if (!BuildState)
	{
		return;
	}

	UClass* CurrentRecipe = BuildState->GetActiveRecipe();
	bool bAbortedRestoreForRecipeClear = false;
	if (!CurrentRecipe)
	{
		bAbortedRestoreForRecipeClear = AbortRestoreSession(TEXT("Build gun active recipe cleared"));
	}

	// Log recipe changes (identify what we're trying to build)
	{
		if (CurrentRecipe != LastLoggedRecipeClass)
		{
			LastLoggedRecipeClass = CurrentRecipe;
			AFGHologram* H = BuildState->GetHologram();
			UClass* InnerBuildClass = H ? H->GetBuildClass() : nullptr;
			const FText FriendlyName = CurrentRecipe ? UFGItemDescriptor::GetItemName(CurrentRecipe) : FText::GetEmpty();
			const FBox HoloBox = H ? H->GetComponentsBoundingBox(/*bNonColliding=*/true) : FBox(ForceInit);
			const FString HoloSizeStr = (H && HoloBox.IsValid) ? HoloBox.GetSize().ToString() : FString(TEXT("<n/a>"));
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BUILD RECIPE CHANGED: Recipe=%s Name=\"%s\" BuildClass=%s Hologram=%s HoloSize=%s"),
				*GetNameSafe(CurrentRecipe), *FriendlyName.ToString(), *GetNameSafe(InnerBuildClass), H ? *H->GetName() : TEXT("<none>"), *HoloSizeStr);
		}
	}

	// Get the hologram from the build state
	AFGHologram* CurrentHologram = BuildState->GetHologram();
	if (bAbortedRestoreForRecipeClear)
	{
		ResetCounters();
	}

	// Update registration if hologram changed
	if (CurrentHologram != ActiveHologram.Get())
	{
		// Unregister old hologram if exists
		if (ActiveHologram.IsValid())
		{
			UnregisterActiveHologram(ActiveHologram.Get());
		}

		// Register new hologram if exists
		if (CurrentHologram)
		{
			RegisterActiveHologram(CurrentHologram);
		}
	}

	// Auto-Hold user override detection (Issue #273)
	// If we set the auto-hold lock but the hologram is now unlocked, the user pressed Hold
	// to release it manually. Set override flag so we don't re-lock until next grid change.
	if (bAutoHoldActive && !bAutoHoldUserOverrode && ActiveHologram.IsValid())
	{
		if (!ActiveHologram->IsHologramLocked())
		{
			bAutoHoldActive = false;
			bLockedByModifier = false;
			bAutoHoldUserOverrode = true;
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔓 AUTO-HOLD: User released lock manually — suppressing re-lock until next grid change"));
		}
	}

	// #342: the manual Extend hold ("pin") toggle is detected inside USFExtendService::RefreshExtension
	// (at the per-frame "ensure locked" point), not here — Extend re-locks every frame, which masks the
	// player's unlock from this once-per-frame poll.

	// Detect and propagate transform changes via transform service (Phase 3 extraction)
	if (CurrentHologram && GridTransformService)
	{
		GridTransformService->DetectAndPropagateTransformChange(CurrentHologram);
	}

	// Update lift height on HUD for conveyor lift holograms
	if (CurrentHologram && HudService)
	{
		if (AFGConveyorLiftHologram* LiftHologram = Cast<AFGConveyorLiftHologram>(CurrentHologram))
		{
			// Access mTopTransform via reflection (it's protected in base class)
			float LiftHeight = 0.0f;
			if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
			{
				const FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(LiftHologram);
				if (TopTransformPtr)
				{
					LiftHeight = TopTransformPtr->GetTranslation().Z;
				}
			}

			float WorldHeight = LiftHologram->GetActorLocation().Z + LiftHeight;

			// Update HUD (HudService handles change detection internally)
			HudService->UpdateLiftHeight(LiftHeight, WorldHeight);
		}
		else
		{
			// Clear lift height when not placing a lift
			HudService->ClearLiftHeight();
		}
	}
}

// ========================================
// Multi-Step Hologram Property Sync (Issue #200)
// ========================================
// Syncs properties like fixture angle and build step from parent to children.
// Runs in Tick to detect changes from vanilla ScrollRotate during step 2.

void USFSubsystem::SyncMultiStepHologramProperties()
{
	AFGHologram* Parent = ActiveHologram.Get();
	if (!Parent) return;

	// === Floodlight sync (Issue #200) ===
	AFGFloodlightHologram* FloodlightParent = Cast<AFGFloodlightHologram>(Parent);
	if (FloodlightParent)
	{
		// Read mFixtureAngle via reflection (private member)
		int32 ParentAngle = 0;
		FIntProperty* AngleProp = FindFProperty<FIntProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mFixtureAngle"));
		if (AngleProp)
		{
			ParentAngle = AngleProp->GetPropertyValue_InContainer(FloodlightParent);
		}

		// Read mBuildStep via reflection (private member, CustomSerialization enum)
		uint8 ParentBuildStep = 0;
		FProperty* StepProp = FindFProperty<FProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mBuildStep"));
		if (StepProp)
		{
			StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(FloodlightParent));
		}

		if (ParentAngle != CachedParentFixtureAngle || ParentBuildStep != CachedParentBuildStep)
		{
			CachedParentFixtureAngle = ParentAngle;
			CachedParentBuildStep = ParentBuildStep;

			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			int32 SyncedCount = 0;
			for (const auto& ChildPtr : SpawnedChildren)
			{
				if (!ChildPtr.IsValid()) continue;
				AFGFloodlightHologram* FloodlightChild = Cast<AFGFloodlightHologram>(ChildPtr.Get());
				if (!FloodlightChild) continue;

				if (AngleProp) AngleProp->SetPropertyValue_InContainer(FloodlightChild, ParentAngle);
				if (StepProp) StepProp->CopyCompleteValue(StepProp->ContainerPtrToValuePtr<void>(FloodlightChild), &ParentBuildStep);
				if (UFunction* RepFunc = FloodlightChild->FindFunction(TEXT("OnRep_FixtureAngle")))
				{
					FloodlightChild->ProcessEvent(RepFunc, nullptr);
				}
				SyncedCount++;
			}
			if (SyncedCount > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Synced floodlight properties to %d children: Angle=%d, BuildStep=%d"),
					SyncedCount, ParentAngle, ParentBuildStep);
			}
		}
		return;
	}

	// === Standard conveyor pole HEIGHT sync (#354) ===
	// The regular conveyor pole is a two-step placement (base, then height). The chosen height is carried by
	// the private int32 mPoleVariationIndex (the value the player scrolls in step 2); mBuildStep is the step.
	// Sync both from parent to children via reflection so a scaled grid of poles all take the parent's height.
	// GATED to the REGULAR pole only - stackable/wall poles are also AFGConveyorPoleHologram and must not be
	// touched here (they don't use this height step and already work).
	//
	// KNOWN LIMITATION (#354): SCALING WHILE IN THE HEIGHT-ADJUST STEP drops the player out of height-adjust.
	// Re-evaluating the grid (spawning/removing child poles) during the parent's active vanilla build-step
	// disrupts that build-step state; gating our own parent refresh did not fix it, so the cause is the grid
	// re-eval itself, not this sync. The conveyor pole is the only build-*height* two-step buildable Smart
	// scales, so there's nothing to compare against - treat as a pole-specific quirk. Workflow workaround:
	// size the pole line first, THEN adjust the height (or adjust then build); just don't scale mid-drag.
	if (USFAutoConnectService::IsRegularConveyorPoleHologram(Parent))
	{
		AFGConveyorPoleHologram* PoleParent = Cast<AFGConveyorPoleHologram>(Parent);
		if (PoleParent)
		{
			int32 ParentVariation = -1;
			FIntProperty* VarProp = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex"));
			if (VarProp)
			{
				ParentVariation = VarProp->GetPropertyValue_InContainer(PoleParent);
			}

			uint8 ParentBuildStep = 0;
			FProperty* StepProp = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep"));
			if (StepProp)
			{
				StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(PoleParent));
			}

			// #354: also re-sync when the CHILD COUNT changes (a pole was added/removed by scaling), not just
			// on a height change - otherwise a newly-scaled pole keeps its base height (belt on the floor)
			// until the player nudges the height. Tracking the count reconciles new poles + their belts at
			// the current height on the next tick.
			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			const int32 ChildCount = SpawnedChildren.Num();
			if (ParentVariation != CachedParentPoleVariation || ParentBuildStep != CachedParentBuildStep || ChildCount != CachedPoleChildCount)
			{
				CachedParentPoleVariation = ParentVariation;
				CachedParentBuildStep = ParentBuildStep;
				CachedPoleChildCount = ChildCount;

				int32 SyncedCount = 0;
				for (const auto& ChildPtr : SpawnedChildren)
				{
					if (!ChildPtr.IsValid()) continue;
					AFGConveyorPoleHologram* PoleChild = Cast<AFGConveyorPoleHologram>(ChildPtr.Get());
					if (!PoleChild) continue;

					if (VarProp)  VarProp->SetPropertyValue_InContainer(PoleChild, ParentVariation);
					if (StepProp) StepProp->CopyCompleteValue(StepProp->ContainerPtrToValuePtr<void>(PoleChild), &ParentBuildStep);

					// Refresh the child's mesh/height from the new variation index. Prefer the reflected
					// OnRep; fall back to our public RefreshPoleMesh() shim if the OnRep does not refresh.
					if (UFunction* RepFunc = PoleChild->FindFunction(TEXT("OnRep_PoleVariationIndex")))
					{
						PoleChild->ProcessEvent(RepFunc, nullptr);
					}
					else if (ASFConveyorPoleChildHologram* SmartChild = Cast<ASFConveyorPoleChildHologram>(PoleChild))
					{
						SmartChild->RefreshPoleMesh();
					}
					SyncedCount++;
				}
				if (SyncedCount > 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("#354 Synced conveyor-pole height to %d children: VariationIndex=%d, BuildStep=%d"),
						SyncedCount, ParentVariation, ParentBuildStep);
				}

				// #354: refresh the PARENT's mesh/connector too - but ONLY in build step 1 (placement).
				// In step 1 vanilla hasn't run UpdatePoleMesh on the parent yet, so its belt connector is
				// still at the FLOOR while the children (OnRep'd above) sit at the synced height, leaving the
				// belt's parent end on the ground. In PHBS_AdjustHeight the player is actively dragging the
				// parent's height and vanilla moves its connector every frame - forcing OnRep there fights
				// the drag and kicks the player out of the height step (the #354 quirk). So skip it then.
				if (ParentBuildStep != static_cast<uint8>(EPoleHologramBuildStep::PHBS_AdjustHeight))
				{
					if (UFunction* ParentRep = PoleParent->FindFunction(TEXT("OnRep_PoleVariationIndex")))
					{
						PoleParent->ProcessEvent(ParentRep, nullptr);
					}
				}

				// #354: belt auto-connect otherwise only recomputes on GRID/spacing changes, so raising the
				// pole HEIGHT never moved the belts - they stayed at the initial (shortest, ~floor) height.
				// The pole's SnapOnly0 belt connector is at the TOP (height-aware), so re-running belt
				// auto-connect after refreshing both parent + children connectors re-routes the belts to the
				// poles' top connectors.
				if (USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(Parent))
				{
					Orchestrator->OnStackableConveyorPolesChanged();
				}
			}
		}
		return;
	}

	// === Standard pipeline support HEIGHT + VERTICAL ANGLE sync (#364) ===
	// The pipe analog of the #354 conveyor-pole branch, with one addition the conveyor pole
	// doesn't have: mVerticalAngle tilts the top piece + pipe connection for sloped runs
	// (public Get/SetVerticalAngle - no reflection needed). Height still rides the inherited
	// mPoleVariationIndex/mBuildStep pair via reflection. GATED to the regular ground support;
	// stackable/wall supports keep their existing paths.
	if (USFAutoConnectService::IsRegularPipelinePoleHologram(Parent))
	{
		AFGPipelinePoleHologram* PipePoleParent = Cast<AFGPipelinePoleHologram>(Parent);
		if (PipePoleParent)
		{
			int32 ParentVariation = -1;
			FIntProperty* VarProp = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex"));
			if (VarProp)
			{
				ParentVariation = VarProp->GetPropertyValue_InContainer(PipePoleParent);
			}

			uint8 ParentBuildStep = 0;
			FProperty* StepProp = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep"));
			if (StepProp)
			{
				StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(PipePoleParent));
			}

			const float ParentAngle = PipePoleParent->GetVerticalAngle();

			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			const int32 ChildCount = SpawnedChildren.Num();
			if (ParentVariation != CachedParentPipePoleVariation
				|| ParentBuildStep != CachedParentBuildStep
				|| !FMath::IsNearlyEqual(ParentAngle, CachedParentPipePoleAngle)
				|| ChildCount != CachedPipePoleChildCount)
			{
				CachedParentPipePoleVariation = ParentVariation;
				CachedParentBuildStep = ParentBuildStep;
				CachedParentPipePoleAngle = ParentAngle;
				CachedPipePoleChildCount = ChildCount;

				int32 SyncedCount = 0;
				for (const auto& ChildPtr : SpawnedChildren)
				{
					if (!ChildPtr.IsValid()) continue;
					ASFPipelinePoleChildHologram* PipePoleChild = Cast<ASFPipelinePoleChildHologram>(ChildPtr.Get());
					if (!PipePoleChild) continue;

					if (VarProp)  VarProp->SetPropertyValue_InContainer(PipePoleChild, ParentVariation);
					if (StepProp) StepProp->CopyCompleteValue(StepProp->ContainerPtrToValuePtr<void>(PipePoleChild), &ParentBuildStep);
					PipePoleChild->SetVerticalAngle(ParentAngle);

					// Refresh the child's mesh/height/angle. Prefer the reflected OnReps; fall back
					// to the public RefreshPoleMesh shim.
					if (UFunction* RepFunc = PipePoleChild->FindFunction(TEXT("OnRep_PoleVariationIndex")))
					{
						PipePoleChild->ProcessEvent(RepFunc, nullptr);
					}
					else
					{
						PipePoleChild->RefreshPoleMesh();
					}
					if (UFunction* AngleRep = PipePoleChild->FindFunction(TEXT("OnRep_VerticalAngle")))
					{
						PipePoleChild->ProcessEvent(AngleRep, nullptr);
					}
					SyncedCount++;
				}
				if (SyncedCount > 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("#364 Synced pipeline-support to %d children: VariationIndex=%d, BuildStep=%d, VerticalAngle=%.1f"),
						SyncedCount, ParentVariation, ParentBuildStep, ParentAngle);
				}

				// Mirror #354: refresh the PARENT's connector outside the active height-drag step.
				if (ParentBuildStep != static_cast<uint8>(EPoleHologramBuildStep::PHBS_AdjustHeight))
				{
					if (UFunction* ParentRep = PipePoleParent->FindFunction(TEXT("OnRep_PoleVariationIndex")))
					{
						PipePoleParent->ProcessEvent(ParentRep, nullptr);
					}
				}
			}
		}
		return;
	}

	// === Standalone sign/billboard sync (Issue #192) ===
	AFGStandaloneSignHologram* SignParent = Cast<AFGStandaloneSignHologram>(Parent);
	if (SignParent)
	{
		// Read mBuildStep via reflection (protected, CustomSerialization enum)
		uint8 ParentBuildStep = 0;
		FProperty* StepProp = FindFProperty<FProperty>(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep"));
		if (StepProp)
		{
			StepProp->CopyCompleteValue(&ParentBuildStep, StepProp->ContainerPtrToValuePtr<void>(SignParent));
		}

		if (ParentBuildStep != CachedParentBuildStep)
		{
			CachedParentBuildStep = ParentBuildStep;

			const auto& SpawnedChildren = HologramHelper->GetSpawnedChildren();
			int32 SyncedCount = 0;
			for (const auto& ChildPtr : SpawnedChildren)
			{
				if (!ChildPtr.IsValid()) continue;
				ASFStandaloneSignChildHologram* SignChild = Cast<ASFStandaloneSignChildHologram>(ChildPtr.Get());
				if (!SignChild) continue;

				// Use the public setter on our child class (no reflection needed)
				SignChild->SetBuildStep(static_cast<ESignHologramBuildStep>(ParentBuildStep));
				SyncedCount++;
			}
			if (SyncedCount > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Synced sign build step to %d children: BuildStep=%d"),
					SyncedCount, ParentBuildStep);
			}
		}
		return;
	}
}

// Enhanced scaling application with child hologram generation (POC)
void USFSubsystem::ApplyScalingToHologram(const FVector& ScalingDelta)
{
	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("SCALING FAILED: No active hologram"));
		return;
	}

	AFGHologram* Hologram = ActiveHologram.Get();

	// Store old transform for logging
	const FTransform OldTransform = Hologram->GetTransform();
	const FVector OldLocation = OldTransform.GetLocation();

	// Update our scaling offset (diagnostic only). Do NOT move the parent here.
	CurrentScalingOffset += ScalingDelta;

	// Let the Build Gun own the parent transform; only regenerate Smart! children
	const FVector NewLocation = OldLocation; // unchanged parent location for logging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SCALING APPLIED: Delta=%s | Old=%s | New=%s | TotalOffset=%s"),
		*ScalingDelta.ToString(), *OldLocation.ToString(), *NewLocation.ToString(), *CurrentScalingOffset.ToString());

	// POC: Regenerate child hologram grid based on counters
	RegenerateChildHologramGrid();
}

// Full child hologram grid management with positioning (delegated)
void USFSubsystem::RegenerateChildHologramGrid()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->RegenerateChildHologramGrid();
    }
}

void USFSubsystem::QueueChildForDestroy(AFGHologram* Child)
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->QueueChildForDestroy(Child);
    }
}

// Destroy any queued children now (runs on next tick)
void USFSubsystem::FlushPendingDestroy()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->FlushPendingDestroy();
    }
}

bool USFSubsystem::CanSafelyDestroyChildren() const
{
    if (USFGridSpawnerService* Spawner = const_cast<USFSubsystem*>(this)->GetGridSpawnerService())
    {
        return Spawner->CanSafelyDestroyChildren();
    }
    return true;
}

void USFSubsystem::ForceDestroyPendingChildren()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->ForceDestroyPendingChildren();
    }
}

// Update positions of all child holograms in the grid
void USFSubsystem::UpdateChildPositions()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->UpdateChildPositions();
    }
}

void USFSubsystem::UpdateChildrenForCurrentTransform()
{
    if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
    {
        Spawner->UpdateChildrenForCurrentTransform();
    }
}

void USFSubsystem::OnChildHologramDestroyed(AActor* DestroyedActor)
{
	// Phase 3: Delegate to HologramHelperService (Task #61.6)
	if (HologramHelper)
	{
		// Create callback for transform updates
		auto UpdateCallback = [this]()
		{
			this->UpdateChildrenForCurrentTransform();
		};

		HologramHelper->OnChildHologramDestroyed(DestroyedActor, UpdateCallback);
	}
}

void USFSubsystem::OnParentHologramDestroyed(AActor* DestroyedActor)
{
	// Phase 3: Delegate to HologramHelperService (Task #61.6)
	if (HologramHelper)
	{
		HologramHelper->OnParentHologramDestroyed(DestroyedActor);
	}

	// Check if this was our active hologram for local state cleanup
	AFGHologram* DestroyedHologram = Cast<AFGHologram>(DestroyedActor);
	if (DestroyedHologram && ActiveHologram.Get() == DestroyedHologram)
	{
		// ========================================
		// STACKABLE PIPE SUPPORT: Positions already cached in ProcessStackablePipelineSupports
		// ========================================
		// NOTE: We do NOT cache positions here because:
		// 1. Build actors spawn BEFORE OnParentHologramDestroyed is called
		// 2. Child holograms are already destroyed by this point
		// Positions are cached in ProcessStackablePipelineSupports where children are still valid
		if (AutoConnectService && AutoConnectService->IsStackablePipelineSupportHologram(DestroyedHologram))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE SUPPORT: OnParentHologramDestroyed - cache already set (Expected=%d, Pending=%d)"),
				StackablePipeSupportExpectedCount, bStackablePipeBuildPending);
		}

		// Clean up local SFSubsystem state
		CurrentScalingOffset = FVector::ZeroVector;

		// Clear transform cache via transform service (Phase 3 extraction)
		if (GridTransformService)
		{
			GridTransformService->ClearCache();
		}

		CurrentAdapter.Reset();

		// Reset all counters (fixes persistence bug - Task 51)
		ResetCounters();
	}
}

void USFSubsystem::OnBuildGunEquipped()
{
	if (!LastController.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Cannot log contexts: No player controller"));
		return;
	}

	AFGPlayerController* PC = LastController.Get();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Cannot log contexts: No Enhanced Input subsystem"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📋 ACTIVE INPUT CONTEXTS [BuildGunEquipped]"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	// Get all active mapping contexts using the correct API
	const TArray<FEnhancedActionKeyMapping>& CurrentMappings = InputSubsystem->GetAllPlayerMappableActionKeyMappings();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Total player mappable key mappings: %d"), CurrentMappings.Num());

	// Log Smart! specific context status
	UFGInputMappingContext* SmartContext = USFInputRegistry::GetSmartInputMappingContext();
	if (SmartContext)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Smart! Context exists: %s"), *SmartContext->GetName());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Display Name: %s"), *SmartContext->mDisplayName.ToString());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Priority: %.1f"), SmartContext->mMenuPriority);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Smart! Context NOT LOADED"));
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔫 BUILD GUN EQUIPPED - Smart! input context should now be active"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Numpad keys should trigger Smart! actions when hologram is present"));

	// Clean up any stale recipe inheritance state for safety
	// This ensures fresh state when equipping build gun after save loads
	CleanupStateForWorldTransition();

	// Start periodic cleanup of dead hologram entries
	StartPeriodicCleanup();

	// Note: Recipe delegate subscription now happens in MonitorActiveHologram() on first access

	// Log active contexts immediately after equip
	LogActiveInputContexts(TEXT("BuildGunEquipped"));

	// Start periodic monitoring during build mode
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ContextMonitorTimer,
			[this]()
			{
				LogActiveInputContexts(TEXT("PeriodicMonitor"));
			},
			5.0f,  // Log every 5 seconds
			true   // Repeat
		);
	}
}

void USFSubsystem::OnBuildGunUnequipped()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" BUILD GUN UNEQUIPPED - Smart! build context deactivated"));

	AbortRestoreSession(TEXT("Build gun unequipped"));

	// Reset recipe sampling subscription flag
	bHasSubscribedToRecipeSampled = false;

	// Log contexts at unequip
	LogActiveInputContexts(TEXT("BuildGunUnequipped"));

	// Stop periodic monitoring
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ContextMonitorTimer);
	}

	// Stop periodic cleanup of dead hologram entries
	StopPeriodicCleanup();

	// Clear any active hologram and reset counters
	if (ActiveHologram.IsValid())
	{
		UnregisterActiveHologram(ActiveHologram.Get());
	}

	// Force counter display to clear (in case hologram was already unregistered)
	ResetCounters();

	// Issue #198: Reset one-shot Smart disable flag when build gun is unequipped
	ResetSmartDisableFlag();

	// Issue #208/#209: User requested that sampled recipes and items (Shards/Somersloops)
	// should NOT persist across build gun sessions to avoid accidentally applying them
	// to unrelated builds later. We clear them on unequip.
	ClearStoredProductionRecipe();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("RECIPE COPYING: Cleared stored recipe and items on build gun unequip"));

	// Now it is safe to destroy any pending children we deferred while building
	ForceDestroyPendingChildren();
}

void USFSubsystem::OnBuildGunStateChanged(const FString& NewStateName, const FString& OldStateName)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 BUILD GUN STATE CHANGE: %s -> %s"), *OldStateName, *NewStateName);
	LastBuildGunState = NewStateName;

	// Log contexts at every state change
	LogActiveInputContexts(FString::Printf(TEXT("StateChange_%s"), *NewStateName));
}

void USFSubsystem::LogActiveInputContexts(const FString& CallerContext)
{
	if (!LastController.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[%s] Cannot log contexts: No player controller"), *CallerContext);
		return;
	}

	AFGPlayerController* PC = LastController.Get();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("[%s] Cannot log contexts: No Enhanced Input subsystem"), *CallerContext);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📋 ACTIVE INPUT CONTEXTS [%s]"), *CallerContext);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));

	// Get all active mapping contexts using the correct API
	const TArray<FEnhancedActionKeyMapping>& CurrentMappings = InputSubsystem->GetAllPlayerMappableActionKeyMappings();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Total player mappable key mappings: %d"), CurrentMappings.Num());

	// Log Smart! specific context status
	UFGInputMappingContext* SmartContext = USFInputRegistry::GetSmartInputMappingContext();
	if (SmartContext)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Smart! Context exists: %s"), *SmartContext->GetName());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Display Name: %s"), *SmartContext->mDisplayName.ToString());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Priority: %.1f"), SmartContext->mMenuPriority);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Smart! Context NOT LOADED"));
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("═══════════════════════════════════════════════════════════"));
}

TSharedPtr<ISFHologramAdapter> USFSubsystem::CreateHologramAdapter(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CreateHologramAdapter: Null hologram"));
		return nullptr;
	}

	const FString HologramTypeName = Hologram->GetClass()->GetName();

	// Log inheritance chain for ALL holograms to diagnose misclassification issues
	TArray<FString> InheritanceChain;
	UClass* CurrentClass = Hologram->GetClass();
	while (CurrentClass)
	{
		InheritanceChain.Add(CurrentClass->GetName());
		CurrentClass = CurrentClass->GetSuperClass();

		if (CurrentClass && CurrentClass->GetName() == TEXT("Object"))
		{
			InheritanceChain.Add(TEXT("UObject"));
			break;
		}
	}

	FString InheritanceStr;
	for (int32 i = 0; i < InheritanceChain.Num(); i++)
	{
		InheritanceStr += InheritanceChain[i];
		if (i < InheritanceChain.Num() - 1)
		{
			InheritanceStr += TEXT(" -> ");
		}
	}

	// Also log the BuildClass to help identify resource extractors
	UClass* BuildClass = Hologram->GetBuildClass();
	FString BuildClassName = BuildClass ? BuildClass->GetName() : TEXT("NONE");

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 Hologram inheritance: %s"), *InheritanceStr);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 BuildClass: %s"), *BuildClassName);

	// ========================================
	// PHASE 4: RUNTIME HOLOGRAM SWAPPING (Smart Custom Holograms)
	// ========================================
	// NOTE: Parent hologram swapping removed - only swap child holograms
	// Parent holograms are already valid and don't need recipe copying
	// Child holograms are swapped in SpawnChildHologram function

	// ========================================
	// CRITICAL: Detect vanilla blueprint holograms FIRST (Issue #166)
	// Blueprint placement must not be scaled - it would break the blueprint system
	// ========================================
	if (Cast<AFGBlueprintHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected Blueprint hologram (%s) - Smart! features disabled (blueprint placement)"),
			*Hologram->GetName());
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Blueprint"));
	}

	// Check for custom logistics holograms first (most specific)
	if (ASFLogisticsHologram* LogisticsHolo = Cast<ASFLogisticsHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Smart Logistics adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFSmartLogisticsAdapter>(LogisticsHolo);
	}

	// Check for custom factory holograms
	if (ASFFactoryHologram* FactoryHolo = Cast<ASFFactoryHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Factory adapter for custom factory hologram %s"), *Hologram->GetName());
		return MakeShared<FSFFactoryAdapter>(FactoryHolo);
	}

	// Check for custom foundation holograms
	if (ASFFoundationHologram* FoundationHolo = Cast<ASFFoundationHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Foundation adapter for custom foundation hologram %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Foundation"));
	}

	// Check for custom buildable base class (catch-all for other custom holograms)
	if (ASFBuildableHologram* BuildableHolo = Cast<ASFBuildableHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Creating Smart Buildable adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFSmartBuildableAdapter>(BuildableHolo);
	}

	// ========================================
	// VANILLA HOLOGRAMS (Fallback for mod compatibility)
	// ========================================

	// CRITICAL: Check resource extractors BEFORE foundations!
	// Some miners use Holo_Snappable_C which inherits from AFGFoundationHologram for grid snapping
	// but are actually resource extractors that must be on resource nodes
	if (Cast<AFGWaterPumpHologram>(Hologram))
	{
		// Water extractors CAN be scaled - they check water depth per-position, not resource nodes
		// Children validate independently via CheckMinimumDepth()
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		FString InnerBuildClassName = InnerBuildClass ? InnerBuildClass->GetName() : TEXT("NONE");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating WaterExtractor adapter for %s (scaling enabled - validates per-child) BuildClass=%s"),
			*Hologram->GetName(), *InnerBuildClassName);
		return MakeShared<FSFWaterExtractorAdapter>(Hologram);
	}
	else if (Cast<AFGResourceExtractorHologram>(Hologram))
	{
		// Other resource extractors (miners, oil extractors) CANNOT be scaled
		// They must be placed on specific resource nodes - children fail validation
		UE_LOG(LogSmartFoundations, Warning, TEXT("Creating ResourceExtractor adapter for %s (scaling disabled - must be on resource node)"), *Hologram->GetName());
		return MakeShared<FSFResourceExtractorAdapter>(Hologram);
	}
	else if (Cast<AFGFoundationHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Foundation adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Foundation"));
	}
	else if (Cast<AFGStackableStorageHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Storage adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("Storage"));
	}
	else if (Cast<AFGPipelineJunctionHologram>(Hologram))
	{
		// CRITICAL: Check PipelineJunction BEFORE ConveyorAttachment and Factory!
		// AFGPipelineJunctionHologram inherits from AFGFactoryHologram
		// Uses registry AnchorOffset for elevated pivot compensation
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating PipelineJunction adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("PipelineJunction"));
	}
	else if (Cast<AFGConveyorAttachmentHologram>(Hologram))
	{
		// CRITICAL: Check ConveyorAttachment BEFORE Factory!
		// AFGConveyorAttachmentHologram inherits from AFGFactoryHologram
		// Uses registry AnchorOffset for center-pivot compensation + unique floor validation
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating ConveyorAttachment adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("ConveyorAttachment"));
	}
	else if (Cast<AFGPipeHyperAttachmentHologram>(Hologram))
	{
		// CRITICAL: Check PipeHyperAttachment BEFORE Factory!
		// AFGPipeHyperAttachmentHologram inherits from AFGPipeAttachmentHologram -> AFGFactoryHologram
		// Uses registry AnchorOffset for elevated pivot compensation (7/8 height ratio)
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating HypertubeAttachment adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFGenericAdapter>(Hologram, TEXT("HypertubeAttachment"));
	}
	else if (Cast<AFGPassthroughHologram>(Hologram))
	{
		// Issue #187: Check Passthrough BEFORE Factory!
		// AFGPassthroughHologram inherits from AFGFactoryHologram
		// Floor holes for conveyors and pipes - children snap to foundations independently
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Passthrough adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFPassthroughAdapter>(Hologram);
	}
	else if (Cast<AFGFactoryHologram>(Hologram))
	{
		// Check registry first - some factory holograms have scaling disabled
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		if (InnerBuildClass && USFBuildableSizeRegistry::HasProfile(InnerBuildClass))
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(InnerBuildClass);
			if (!Profile.bSupportsScaling)
			{
				// Registry disables scaling for this buildable
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("Detected registry buildable with scaling disabled (%s) - Smart! features disabled"),
					*InnerBuildClass->GetName());
				return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("RegistryDisabled"));
			}
		}
		// Registry allows scaling or no profile - use Factory adapter
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Factory adapter for %s"), *Hologram->GetName());
		return MakeShared<FSFFactoryAdapter>(Hologram);
	}

	// ========================================
	// PARTIAL SUPPORT (Stub Adapters - Features Disabled)
	// ========================================

	else if (Cast<AFGWallHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Wall adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFWallAdapter>(Hologram);
	}
	else if (Cast<AFGPillarHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Pillar adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFPillarAdapter>(Hologram);
	}
	else if (Cast<AFGElevatorHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Elevator adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFElevatorAdapter>(Hologram);
	}
	else if (Cast<AFGStairHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Ramp adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFRampAdapter>(Hologram);
	}
	else if (Cast<AFGJumpPadHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating JumpPad adapter for %s (features currently disabled)"), *Hologram->GetName());
		return MakeShared<FSFJumpPadAdapter>(Hologram);
	}

	// ========================================
	// EXPLICITLY UNSUPPORTED TYPES
	// ========================================

	else if (Cast<AFGWheeledVehicleHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected vehicle hologram (%s) - Smart! features disabled (vehicles not supported)"),
			*HologramTypeName);
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Vehicle"));
	}
	else if (Cast<AFGSpaceElevatorHologram>(Hologram))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected Space Elevator hologram (%s) - Smart! features disabled (unique building)"),
			*HologramTypeName);
		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("SpaceElevator"));
	}

	// ========================================
	// REGISTRY-BASED ADAPTER SELECTION
	// ========================================

	// Check if this buildable has a registry profile with scaling enabled
	// This allows the registry to drive adapter selection for special buildables
	else if (Cast<AFGBuildableHologram>(Hologram))
	{
		UClass* InnerBuildClass = Hologram->GetBuildClass();
		if (InnerBuildClass && USFBuildableSizeRegistry::HasProfile(InnerBuildClass))
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(InnerBuildClass);
			if (Profile.bSupportsScaling)
			{
				// Registry says this buildable supports scaling - use Factory adapter
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating Factory adapter for %s (registry-enabled: %s)"),
					*Hologram->GetName(), *InnerBuildClass->GetName());
				return MakeShared<FSFFactoryAdapter>(Hologram);
			}
			else
			{
				// Registry says scaling is disabled (extractors, unique buildings, etc.)
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("Detected registry buildable with scaling disabled (%s) - Smart! features disabled"),
					*InnerBuildClass->GetName());
				return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("RegistryDisabled"));
			}
		}
		// If no registry profile, fall through to unknown handling
	}

	// ========================================
	// UNKNOWN TYPES (Default to Unsupported)
	// ========================================

	// No registry profile and no specific adapter - unsupported
	{
		// Log full inheritance chain to help identify modded buildables
		TArray<FString> InnerInheritanceChain;
		UClass* CurrentClassForChain = Hologram->GetClass();
		while (CurrentClassForChain)
		{
			InnerInheritanceChain.Add(CurrentClassForChain->GetName());
			CurrentClassForChain = CurrentClassForChain->GetSuperClass();

			// Stop at UObject to avoid cluttering the log
			if (CurrentClassForChain && CurrentClassForChain->GetName() == TEXT("Object"))
			{
				InnerInheritanceChain.Add(TEXT("UObject"));
				break;
			}
		}

		// Build inheritance string (e.g., "Holo_X -> AFGBuildableHologram -> AActor -> UObject")
		FString InnerInheritanceStr;
		for (int32 i = 0; i < InnerInheritanceChain.Num(); i++)
		{
			InnerInheritanceStr += InnerInheritanceChain[i];
			if (i < InnerInheritanceChain.Num() - 1)
			{
				InnerInheritanceStr += TEXT(" -> ");
			}
		}

		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Detected unknown hologram type (%s) - Smart! features disabled. Inheritance: %s"),
			*HologramTypeName, *InnerInheritanceStr);

		return MakeShared<FSFUnsupportedAdapter>(Hologram, TEXT("Unknown"));
	}
}
