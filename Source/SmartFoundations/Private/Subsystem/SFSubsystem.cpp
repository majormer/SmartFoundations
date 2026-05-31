// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFSubsystem - core: subsystem lifecycle (ctor/Init/Deinit), accessors, power-connection mgmt, Get() + input setup/scaling.
 * Implementation split across SFSubsystem.cpp + SFSubsystem_*.cpp (each <2k); shared includes in
 * SFSubsystemImpl.h. No behavior change from the monolith.
 */

#include "Subsystem/SFSubsystemImpl.h"

USFSubsystem::USFSubsystem() : Super()
{
	#if SMART_ARROWS_ENABLED
	ArrowModule = MakeUnique<FSFArrowModule_StaticMesh>();
	#endif

	// Phase 0: Create extracted service modules (Task #61.6)
	InputHandler = MakeUnique<FSFInputHandler>();
	InputHandler->Initialize(this);

	PositionCalculator = MakeUnique<FSFPositionCalculator>();
	// Note: PositionCalculator is stateless, no Initialize() needed

	ValidationService = MakeUnique<FSFValidationService>();
	// Note: ValidationService is stateless, no Initialize() needed

	HologramHelper = MakeUnique<FSFHologramHelperService>();
	// Note: HologramHelper::Initialize() takes UWorld*, called during subsystem Init

	// Create recipe management service as default subobject
	RecipeManagementService = CreateDefaultSubobject<USFRecipeManagementService>(TEXT("RecipeManagementService"));
	if (RecipeManagementService)
	{
		RecipeManagementService->Initialize(this);
	}

	// Create upgrade audit service as default subobject (Mass Upgrade feature)
	UpgradeAuditService = CreateDefaultSubobject<USFUpgradeAuditService>(TEXT("UpgradeAuditService"));
	if (UpgradeAuditService)
	{
		UpgradeAuditService->Initialize(this);
	}

	// Create upgrade execution service as default subobject (Mass Upgrade feature)
	UpgradeExecutionService = CreateDefaultSubobject<USFUpgradeExecutionService>(TEXT("UpgradeExecutionService"));
	if (UpgradeExecutionService)
	{
		UpgradeExecutionService->Initialize(this);
	}

	// Create HUD service as default subobject (Phase A extraction)
	HudService = CreateDefaultSubobject<USFHudService>(TEXT("HudService"));
	if (HudService)
	{
		HudService->Initialize(this);
	}

	// Create Hint Bar service (Issue #281)
	HintBarService = CreateDefaultSubobject<USFHintBarService>(TEXT("HintBarService"));
	if (HintBarService)
	{
		HintBarService->Initialize(this);
	}

	// Create Chain Actor service (v29.2.2 — canonical chain invalidation path used by
	// Mass Upgrade and Extend; see Services/SFChainActorService.h for context)
	ChainActorService = CreateDefaultSubobject<USFChainActorService>(TEXT("ChainActorService"));
	if (ChainActorService)
	{
		ChainActorService->Initialize(this);
	}

	// NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()
	// NOTE: RecipeCostInjector removed - child holograms automatically aggregate costs via GetCost()

	// Create Grid state service as default subobject (Grid Phase 1)
	GridStateService = CreateDefaultSubobject<USFGridStateService>(TEXT("GridStateService"));
	if (GridStateService)
	{
		GridStateService->Initialize(this);
	}

	// Create Grid transform service as default subobject (Grid Phase 3)
	GridTransformService = CreateDefaultSubobject<USFGridTransformService>(TEXT("GridTransformService"));
	if (GridTransformService)
	{
		GridTransformService->Initialize(this);
	}

	// Create Restore service as default subobject (Smart Restore Enhanced)
	RestoreService = CreateDefaultSubobject<USFRestoreService>(TEXT("RestoreService"));
	if (RestoreService)
	{
		RestoreService->Initialize(this);
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("SFSubsystem: Phase 0 modules and recipe service initialized"));
}

// ========================================
// Accessor Functions (declared in header, implemented here due to forward declarations)
// ========================================

const FSFCounterState& USFSubsystem::GetCounterState() const
{
	if (GridStateService)
	{
		return GridStateService->GetCounterState();
	}
	return CounterState;
}

USFGridSpawnerService* USFSubsystem::GetGridSpawnerService()
{
	if (!GridSpawnerService)
	{
		GridSpawnerService = NewObject<USFGridSpawnerService>(this, TEXT("GridSpawnerService"));
		if (GridSpawnerService)
		{
			GridSpawnerService->Initialize(this);
		}
	}
	return GridSpawnerService;
}

bool USFSubsystem::IsSpacingModeActive() const
{
	return bSpacingModeActive;
}

bool USFSubsystem::IsStepsModeActive() const
{
	return bStepsModeActive;
}

bool USFSubsystem::IsStaggerModeActive() const
{
	return bStaggerModeActive;
}

bool USFSubsystem::IsRotationModeActive() const
{
	return bRotationModeActive;
}

bool USFSubsystem::IsExtendModeActive() const
{
	if (ExtendService)
	{
		return ExtendService->IsExtendModeActive();
	}
	return false;
}

bool USFSubsystem::IsRestoredExtendModeActive() const
{
	if (ExtendService)
	{
		return ExtendService->IsRestoredCloneTopologyActive();
	}
	return false;
}

bool USFSubsystem::ShouldSuppressNormalGridChildren() const
{
	return IsExtendModeActive() || IsRestoredExtendModeActive();
}

void USFSubsystem::ClearNormalGridChildrenForExtendSuppression(const TCHAR* Context)
{
	if (!HologramHelper)
	{
		return;
	}

	const TArray<TWeakObjectPtr<AFGHologram>> CurrentChildren = HologramHelper->GetSpawnedChildren();
	if (CurrentChildren.Num() == 0)
	{
		return;
	}

	int32 QueuedGridChildren = 0;
	int32 PreservedExtendChildren = 0;
	int32 SkippedOtherChildren = 0;
	for (const TWeakObjectPtr<AFGHologram>& Child : CurrentChildren)
	{
		if (!Child.IsValid())
		{
			SkippedOtherChildren++;
			continue;
		}

		const FSFHologramData* ChildData = USFHologramDataRegistry::GetData(Child.Get());
		const bool bIsExtendChild = Child->Tags.Contains(FName(TEXT("SF_ExtendChild")))
			|| (ChildData && ChildData->ChildType == ESFChildHologramType::ExtendClone);
		if (bIsExtendChild)
		{
			PreservedExtendChildren++;
			continue;
		}

		const bool bIsScalingGridChild = Child->ActorHasTag(FName(TEXT("SF_GridChild")))
			|| (ChildData && ChildData->ChildType == ESFChildHologramType::ScalingGrid);
		if (!bIsScalingGridChild)
		{
			SkippedOtherChildren++;
			continue;
		}

		QueueChildForDestroy(Child.Get());
		QueuedGridChildren++;
	}

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore][Extend] Suppressing normal Smart grid spawn/update: context=%s parent=%s helperChildren=%d queuedGrid=%d preservedExtend=%d skippedOther=%d liveExtend=%d restoredExtend=%d"),
		Context ? Context : TEXT("Unknown"),
		*GetNameSafe(GetActiveHologram()),
		CurrentChildren.Num(),
		QueuedGridChildren,
		PreservedExtendChildren,
		SkippedOtherChildren,
		IsExtendModeActive() ? 1 : 0,
		IsRestoredExtendModeActive() ? 1 : 0);

	if (QueuedGridChildren > 0)
	{
		FlushPendingDestroy();
	}
}

bool USFSubsystem::IsModifierScaleXActive() const
{
	return bModifierScaleXActive;
}

bool USFSubsystem::IsModifierScaleYActive() const
{
	return bModifierScaleYActive;
}

void USFSubsystem::RebindAfterDelay()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->RebindAfterDelay();
	}
}

void USFSubsystem::RunPostLoadChainRepair()
{
	if (ChainActorService)
	{
		ChainActorService->RunPostLoadRepair();
	}
}

void USFSubsystem::OnDebugPrimaryFire()
{
	// Debug: Analyze nearby pipe splines to determine vanilla tangent formulas
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Debug Primary Fire: Analyzing nearby pipe splines..."));
	AnalyzeNearbyPipeSplines(5000.0f);  // 50m radius
}

// ========================================
// Power Connection Management (moved from header - PIMPL pattern)
// ========================================

void USFSubsystem::CommitBuildingConnections()
{
	// Commit building connections (overwrite is fine for these)
	UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ CommitBuildingConnections: Copying %d planned building connections to committed"),
		PlannedBuildingConnections.Num());
	CommittedBuildingConnections = PlannedBuildingConnections;
	UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ CommitBuildingConnections: CommittedBuildingConnections now has %d entries"),
		CommittedBuildingConnections.Num());

	// For pole connections, ADD to the deferred queue instead of overwriting!
	// This prevents race conditions when multiple builds happen quickly.
	if (PlannedPoleConnections.Num() > 0)
	{
		for (const auto& Connection : PlannedPoleConnections)
		{
			// Avoid duplicates
			bool bExists = false;
			for (const auto& Existing : DeferredPoleConnections)
			{
				if ((Existing.Key.Equals(Connection.Key, 10.0f) && Existing.Value.Equals(Connection.Value, 10.0f)) ||
					(Existing.Key.Equals(Connection.Value, 10.0f) && Existing.Value.Equals(Connection.Key, 10.0f)))
				{
					bExists = true;
					break;
				}
			}
			if (!bExists)
			{
				DeferredPoleConnections.Add(Connection);
			}
		}
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ CommitBuildingConnections: Added %d pole connections to deferred queue (total: %d)"),
			PlannedPoleConnections.Num(), DeferredPoleConnections.Num());
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ CommitBuildingConnections: No new pole connections (deferred queue: %d)"),
			DeferredPoleConnections.Num());
	}
}

void USFSubsystem::RemoveDeferredPoleConnection(const FVector& PoleA, const FVector& PoleB)
{
	for (int32 i = DeferredPoleConnections.Num() - 1; i >= 0; i--)
	{
		const auto& Conn = DeferredPoleConnections[i];
		if ((Conn.Key.Equals(PoleA, 50.0f) && Conn.Value.Equals(PoleB, 50.0f)) ||
			(Conn.Key.Equals(PoleB, 50.0f) && Conn.Value.Equals(PoleA, 50.0f)))
		{
			DeferredPoleConnections.RemoveAt(i);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ RemoveDeferredPoleConnection: Removed used connection, %d remaining"),
				DeferredPoleConnections.Num());
			return;
		}
	}
}

void USFSubsystem::ClearDeferredPoleConnections()
{
	if (DeferredPoleConnections.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ClearDeferredPoleConnections: Cleared %d deferred connections"),
			DeferredPoleConnections.Num());
		DeferredPoleConnections.Empty();
	}
}

int32 USFSubsystem::GetUniqueDeferredPoleConnectionCount() const
{
	TArray<TPair<FVector, FVector>> UniqueConnections;
	for (const auto& Conn : DeferredPoleConnections)
	{
		// Normalize pair so we always store (smaller, larger) by comparing X, then Y, then Z
		FVector First = Conn.Key;
		FVector Second = Conn.Value;
		if (First.X > Second.X ||
			(FMath::IsNearlyEqual(First.X, Second.X, 10.0f) && First.Y > Second.Y) ||
			(FMath::IsNearlyEqual(First.X, Second.X, 10.0f) && FMath::IsNearlyEqual(First.Y, Second.Y, 10.0f) && First.Z > Second.Z))
		{
			Swap(First, Second);
		}

		// Check if this normalized pair already exists
		bool bExists = false;
		for (const auto& Existing : UniqueConnections)
		{
			if (Existing.Key.Equals(First, 50.0f) && Existing.Value.Equals(Second, 50.0f))
			{
				bExists = true;
				break;
			}
		}

		if (!bExists)
		{
			UniqueConnections.Add(TPair<FVector, FVector>(First, Second));
		}
	}
	return UniqueConnections.Num();
}

void USFSubsystem::AddPlannedPoleConnection(const FVector& PoleA, const FVector& PoleB)
{
	// Check if connection already exists (either direction)
	for (const auto& Existing : PlannedPoleConnections)
	{
		if ((Existing.Key.Equals(PoleA, 10.0f) && Existing.Value.Equals(PoleB, 10.0f)) ||
			(Existing.Key.Equals(PoleB, 10.0f) && Existing.Value.Equals(PoleA, 10.0f)))
		{
			return; // Already tracked
		}
	}
	PlannedPoleConnections.Add(TPair<FVector, FVector>(PoleA, PoleB));
}

void USFSubsystem::UpdateCounterState(const FSFCounterState& NewState)
{
	// Push to service if available
	if (GridStateService)
	{
		GridStateService->UpdateCounterState(NewState);
	}

	// Keep local state in sync (Phase 1 source for getters and HUD)
	CounterState = NewState;

	// Mirror GridCounters for legacy API compatibility (non-const ref accessor required)
	GridCounters = CounterState.GridCounters;  // Sync deprecated mirror

	// Refresh HUD immediately
	UpdateCounterDisplay();

	// Scaled Extend (Issue #265): Notify extend service when modal values change
	// This handles spacing, steps, rotation adjustments via DispatchValueAdjust path
	if (IsExtendModeActive() && ExtendService)
	{
		ExtendService->OnScaledExtendStateChanged();
	}
	else if (ExtendService)
	{
		ExtendService->OnRestoredCloneTopologyStateChanged();
	}

	// Ensure grid children reposition immediately on counter updates
	if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
	{
		Spawner->UpdateChildPositions();
	}
}

void USFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Subsystem: Initialize() called"));

	// Reset recipe sampling subscription flag for safety
	bHasSubscribedToRecipeSampled = false;

	// NOTE: Config loading deferred until first hologram registration
	// ConfigManager subsystem may not be available during Initialize()

	// Task #58: Start lightweight timers
	// Heavy initialization (arrows, assets) happens in RegisterActiveHologram()
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Smart! Subsystem: No world in Initialize() - cannot start timers"));
		return;
	}

	// Initialize deterministic hologram lifecycle management (Layer 2) - bind once to prevent accumulation
	InitializeHologramCleanup();

	UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Subsystem: World found, starting timers"));

	// Phase 0: Initialize extracted modules with world context (Task #61.6)
	if (InputHandler)
	{
		InputHandler->Initialize(this);
	}
	if (HologramHelper)
	{
		HologramHelper->Initialize(World);
	}

	// Initialize auto-connect service (Refactor: Auto-Connect Service)
	AutoConnectService = NewObject<USFAutoConnectService>(this);
	if (AutoConnectService)
	{
		AutoConnectService->Init(this);
		UE_LOG(LogSmartFoundations, Log, TEXT("Auto-Connect Service initialized"));
	}

	// Initialize EXTEND service (Issue #219: Factory topology cloning)
	ExtendService = NewObject<USFExtendService>(this);
	if (ExtendService)
	{
		ExtendService->Initialize(this);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("EXTEND Service initialized"));
	}

	// Initialize Radar Pulse diagnostic service
	RadarPulseService = NewObject<USFRadarPulseService>(this);
	if (RadarPulseService)
	{
		RadarPulseService->Initialize(this);
		UE_LOG(LogSmartFoundations, Log, TEXT("Radar Pulse Service initialized"));
	}

	// Initialize Pipe Auto-Connect manager (feature-level coordinator)
	PipeAutoConnectManager = MakeUnique<FSFPipeAutoConnectManager>();
	if (PipeAutoConnectManager.IsValid())
	{
		PipeAutoConnectManager->Initialize(this, AutoConnectService);
	}

	// Initialize Power Auto-Connect manager (feature-level coordinator)
	PowerAutoConnectManager = MakeUnique<FSFPowerAutoConnectManager>();
	if (PowerAutoConnectManager.IsValid())
	{
		PowerAutoConnectManager->Initialize(this, AutoConnectService);
	}

	// Start player controller detection timer (CRITICAL for input binding!)
	World->GetTimerManager().SetTimer(
		PlayerControllerCheckTimer,
		this,
		&USFSubsystem::CheckForPlayerController,
		1.0f,  // Check every second
		true   // Repeat
	);

	// Start hologram polling timer (lightweight - just checks for holograms)
	World->GetTimerManager().SetTimer(
		HologramPollTimer,
		this,
		&USFSubsystem::PollForActiveHologram,
		0.1f,  // Poll every 100ms
		true   // Repeat
	);

	// Bind to actor spawn delegate for recipe inheritance
	World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &USFSubsystem::OnActorSpawned));

	// Schedule one-shot post-load chain diagnostics. Deferred by 8 seconds to ensure all
	// buildable actors (and their chain actors) are fully spawned and initialized.
	// This is intentionally diagnostic-only: mutating chain actors during normal load can
	// race vanilla conveyor Factory_Tick worker threads.
	if (ChainActorService)
	{
		World->GetTimerManager().SetTimer(
			PostLoadChainRepairTimer,
			this,
			&USFSubsystem::RunPostLoadChainRepair,
			8.0f,
			false  // One-shot
		);
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Subsystem: Timers started (player controller + hologram polling) + actor spawn delegate bound"));
}

#if SMART_ARROWS_ENABLED
void USFSubsystem::TickArrows()
{
	// Update arrows only when hologram moves or input changes (optimization)
	if (bArrowsRuntimeVisible && ActiveHologram.IsValid() && CurrentAdapter && CurrentAdapter->IsValid())
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FTransform CurrentTransform = CurrentAdapter->GetBaseTransform();

	// Check if transform, axis input, or grid structure changed
	const bool bTransformChanged = !CurrentTransform.Equals(LastHologramTransform);
	const bool bAxisInputChanged = (LastAxisInput != LastKnownAxisInput);

	int32 CurrentChildCount = 0;
	if (AFGHologram* Hologram = GetActiveHologram())
	{
		CurrentChildCount = Hologram->GetHologramChildren().Num();
	}
	const bool bChildCountChanged = (CurrentChildCount != LastChildCount);

	// Only update full arrows if something changed (optimization)
	if (bTransformChanged || bAxisInputChanged || bChildCountChanged)
	{
		ArrowModule->UpdateArrows(World, CurrentTransform, LastAxisInput, true);

		// Update cache
		LastHologramTransform = CurrentTransform;
		LastKnownAxisInput = LastAxisInput;
		LastChildCount = CurrentChildCount;

			// Log updates (only when changes occur)
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("Arrows updated: Transform=%d Axis=%d ChildCount=%d (%d children) at %s"),
			bTransformChanged, bAxisInputChanged, bChildCountChanged, CurrentChildCount,
			*CurrentTransform.GetLocation().ToString());
		}

		// Read arrow config per-frame (no world reload required — Issue #179)
		FSmart_ConfigStruct ArrowConfig = FSmart_ConfigStruct::GetActiveConfig(this);
		static bool bLastOrbit = true;
		static bool bLastLabels = true;
		if (ArrowConfig.bShowArrowOrbit != bLastOrbit || ArrowConfig.bShowArrowLabels != bLastLabels)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Arrow config changed: Orbit=%s Labels=%s"),
			ArrowConfig.bShowArrowOrbit ? TEXT("ON") : TEXT("OFF"),
			ArrowConfig.bShowArrowLabels ? TEXT("ON") : TEXT("OFF"));
			bLastOrbit = ArrowConfig.bShowArrowOrbit;
			bLastLabels = ArrowConfig.bShowArrowLabels;
		}
		ArrowModule->SetOrbitEnabled(ArrowConfig.bShowArrowOrbit);
		ArrowModule->SetLabelsVisible(ArrowConfig.bShowArrowLabels);

		// Issue #213: Tick label orbits every frame (lightweight — 3 sin/cos + billboard)
		ArrowModule->TickLabelOrbits(World);
	}
}
#else
void USFSubsystem::TickArrows()
{
	// Feature disabled - no-op
}

void USFSubsystem::LoadConfiguration()
{
	// Configuration loading will be implemented here
}

void USFSubsystem::InitializeWidgets()
{
	// BUG FIX (Issue #148): Clear static input caches during world cleanup
	// Must be done FIRST before any other cleanup to prevent accessing stale cached objects
	USFInputRegistry::ClearInputCache();

	// Rest of the InitializeWidgets function remains the same
}

#endif

void USFSubsystem::Deinitialize()
{
	// BUG FIX (Issue #148): Clear static input caches during world cleanup
	// Must be done FIRST before any other cleanup to prevent accessing stale cached objects
	USFInputRegistry::ClearInputCache();

	// Clean up recipe inheritance state for world transition
	CleanupStateForWorldTransition();

	// Clean up widgets
	CleanupWidgets();

#if SMART_ARROWS_ENABLED
	// Clean up arrow visualization
	if (ArrowModule)
	{
		ArrowModule->Cleanup();
	}
#endif

	// Phase 0: Shutdown extracted modules (Task #61.6)
	if (InputHandler)
	{
		InputHandler->Shutdown();
	}
	InputHandler.Reset();

	if (HologramHelper)
	{
		HologramHelper->Shutdown();
	}
	HologramHelper.Reset();

	// Reset other TUniquePtr members
	PositionCalculator.Reset();
	ValidationService.Reset();

#if SMART_ARROWS_ENABLED
	ArrowModule.Reset();
#endif

	// Reset Pipe/Power Auto-Connect managers (TUniquePtr members)
	PipeAutoConnectManager.Reset();
	PowerAutoConnectManager.Reset();

	// Shutdown auto-connect service (Refactor: Auto-Connect Service)
	if (AutoConnectService)
	{
		AutoConnectService->Shutdown();
		AutoConnectService = nullptr;
	}

	// Shutdown EXTEND service (Issue #219)
	if (ExtendService)
	{
		ExtendService->Shutdown();
		ExtendService = nullptr;
	}

	// Shutdown Radar Pulse diagnostic service
	if (RadarPulseService)
	{
		RadarPulseService->Shutdown();
		RadarPulseService = nullptr;
	}

	// Shutdown Upgrade Audit service (Mass Upgrade feature)
	if (UpgradeAuditService)
	{
		UpgradeAuditService->Cleanup();
		UpgradeAuditService = nullptr;
	}

	// Shutdown Upgrade Execution service (Mass Upgrade feature)
	if (UpgradeExecutionService)
	{
		UpgradeExecutionService->Cleanup();
		UpgradeExecutionService = nullptr;
	}

	// NOTE: RecipeCostInjector removed - child holograms automatically aggregate costs via GetCost()

	// Cleanup all orchestrators (Refactor: Auto-Connect Orchestrator)
	for (auto& Pair : AutoConnectOrchestrators)
	{
		if (Pair.Value)
		{
			Pair.Value->Cleanup();
		}
	}
	AutoConnectOrchestrators.Empty();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 All Auto-Connect Orchestrators cleaned up"));

	// Clean up timers
	StopPeriodicCleanup();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerControllerCheckTimer);
		World->GetTimerManager().ClearTimer(DeferredRebindTimer);
		World->GetTimerManager().ClearTimer(ContextMonitorTimer);
		World->GetTimerManager().ClearTimer(HologramPollTimer);
		World->GetTimerManager().ClearTimer(PowerPoleDeferredTimer);
		World->GetTimerManager().ClearTimer(ChainRebuildTimerHandle);
#if SMART_ARROWS_ENABLED
		World->GetTimerManager().ClearTimer(ArrowTickTimer);
#endif
	}

	// Clean up deferred power pole connections
	PendingPowerPoleConnections.Empty();

	// Clean up any resources
	ActiveHologram.Reset();

	// Reset lazy initialization flag (Task #58) for menu → game → menu → game cycles
	bSubsystemInitialized = false;

	Super::Deinitialize();

}
// Tick (Phase 4)
// ========================================

void USFSubsystem::Tick(float DeltaTime)
{
	// Phase 4 Performance Optimization: Progressive Batch Reposition
	// Process progressive batch if active
	if (HologramHelper)
	{
		HologramHelper->TickProgressiveBatchReposition(DeltaTime);
		if (!HologramHelper->IsProgressiveBatchActive() && ActiveHologram.IsValid())
		{
			HologramHelper->TickTrackedScalingChildTransformRefresh(ActiveHologram.Get());
		}
	}

	// Tick the upgrade audit service for batch processing
	if (UpgradeAuditService)
	{
		UpgradeAuditService->Tick(DeltaTime);
	}

	// Tick the upgrade execution service for batch processing
	if (UpgradeExecutionService)
	{
		UpgradeExecutionService->Tick(DeltaTime);
	}

	if (ExtendService)
	{
		ExtendService->TickRestoredCloneTopology(DeltaTime);
	}

	// Issue #160: Continuous Zoop detection
	// When Zoop is activated mid-placement (click-and-drag), we need to detect it
	// and force grid to 1x1x1 even if no Smart! scaling input occurred.
	if (ActiveHologram.IsValid() && HologramHelper)
	{
		if (AFGFactoryBuildingHologram* FactoryBuildingHolo = Cast<AFGFactoryBuildingHologram>(ActiveHologram.Get()))
		{
			const TArray<FTransform>& ZoopTransforms = FactoryBuildingHolo->GetZoopInstanceTransforms();
			const bool bZoopNowActive = ZoopTransforms.Num() > 0;
			const bool bWasZoopActive = HologramHelper->IsZoopActive();

			// Zoop just became active - force grid regeneration to clear Smart! children
			if (bZoopNowActive && !bWasZoopActive)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚠️ Zoop activated mid-placement - triggering grid regeneration"));
				if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
				{
					Spawner->RegenerateChildHologramGrid();
				}
			}
			// Zoop just became inactive - could restore scaling, but user would need to re-input
			else if (!bZoopNowActive && bWasZoopActive)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Zoop deactivated - Smart! scaling available again"));
			}
		}
	}

	// Issue #200: Multi-step hologram property sync (e.g. floodlight fixture angle)
	// Detects changes in parent's multi-step properties and propagates to children
	if (ActiveHologram.IsValid() && HologramHelper)
	{
		SyncMultiStepHologramProperties();
	}

	// EXTEND Mode Processing (Issue #219)
	// AUTOMATIC: When holding a factory hologram and pointing at a matching building,
	// EXTEND activates automatically (no toggle needed - matches original Smart! behavior)
	if (ExtendService && ActiveHologram.IsValid())
	{
		// Get the building the player is looking at via line trace
		AFGBuildable* LookedAtBuilding = nullptr;

		AFGPlayerController* PC = Cast<AFGPlayerController>(LastController.Get());
		if (!PC)
		{
			// Try to get controller from world
			if (UWorld* World = GetWorld())
			{
				PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
			}
		}

		if (PC)
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				FVector Start = CameraManager->GetCameraLocation();
				FVector End = Start + CameraManager->GetActorForwardVector() * 5000.0f;  // 50m range

				FHitResult HitResult;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(PC->GetPawn());
				Params.AddIgnoredActor(ActiveHologram.Get());

				// Use WorldStatic channel which hits buildings, not just visibility
				if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, Params))
				{
					AActor* HitActor = HitResult.GetActor();
					LookedAtBuilding = Cast<AFGBuildable>(HitActor);

					// Debug: Log what we're hitting (throttled to once per second)
					static double LastLogTime = 0;
					double CurrentTime = FPlatformTime::Seconds();
					if (CurrentTime - LastLogTime > 1.0)
					{
						if (HitActor)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND trace: %s (Buildable: %s)"),
								*HitActor->GetClass()->GetName(),
								LookedAtBuilding ? TEXT("YES") : TEXT("NO"));
						}
						LastLogTime = CurrentTime;
					}
				}
			}
		}
		else
		{
			// Debug: Log why we can't do line trace
			static double LastWarnTime = 0;
			double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - LastWarnTime > 5.0)  // Log every 5 seconds max
			{
				SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND: No PlayerController available for line trace"));
				LastWarnTime = CurrentTime;
			}
		}

		// TryExtendFromBuilding handles the automatic activation/deactivation
		ExtendService->TryExtendFromBuilding(LookedAtBuilding, ActiveHologram.Get());
	}
}

TStatId USFSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USFSubsystem, STATGROUP_Tickables);
}

// ========================================
// Static Access
// ========================================

USFSubsystem* USFSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull))
	{
		return World->GetSubsystem<USFSubsystem>();
	}
	return nullptr;
}

// Input event handlers - Enhanced with detailed logging for testing
void USFSubsystem::SetupPlayerInput(AFGPlayerController* PlayerController)
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->SetupPlayerInput(PlayerController);

		// Start hologram polling (every 0.1 seconds for responsive detection)
		// Note: Timer management stays in subsystem for UObject lifecycle
		if (UWorld* WorldForDelay = GetWorld())
		{
			WorldForDelay->GetTimerManager().SetTimer(
				HologramPollTimer,
				this,
				&USFSubsystem::PollForActiveHologram,
				0.1f,  // Poll every 100ms
				true   // Repeat
			);
			UE_LOG(LogSmartFoundations, Log, TEXT("Started hologram auto-detection polling"));
		}

		// Store controller reference for subsystem use
		LastController = PlayerController;

		// Log active contexts after setup
		LogActiveInputContexts(TEXT("AfterContextAdded"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("SetupPlayerInput: InputHandler module not initialized"));
	}
}

void USFSubsystem::CheckForPlayerController()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->CheckForPlayerController();

		// Sync input setup state from module
		if (InputHandler->IsInputSetupCompleted())
		{
			bInputSetupCompleted = true;

			// Clear the timer since we're done
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(PlayerControllerCheckTimer);
			}
		}
	}
}

void USFSubsystem::ApplyAxisScaling(ESFScaleAxis Axis, int32 StepDelta, const TCHAR* DebugLabel)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[INPUT] ApplyAxisScaling: Axis=%d Delta=%d Label=%s Extend=%d"),
		static_cast<int32>(Axis), StepDelta, DebugLabel, IsExtendModeActive());

	// Refactored (Phase 1): Thin orchestrator delegating to services
	// - Counter mutations → GridStateService
	// - Hologram scaling → HologramHelper
	// - Validation & logging → Subsystem (keeps all diagnostic output centralized)

	// ========================================
	// 1. Validation
	// ========================================

	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ApplyAxisScaling: No active hologram registered"));
		return;
	}

	AFGHologram* Hologram = ActiveHologram.Get();
	const bool bIsLocked = Hologram->IsHologramLocked();
	if (bIsLocked)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ApplyAxisScaling: Hologram is LOCKED - Attempting %s (Delta=%d)"), DebugLabel, StepDelta);
	}

	// Check if hologram supports scaling features
	if (CurrentAdapter && !CurrentAdapter->SupportsFeature(ESFFeature::ScaleX))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("Scaling not supported for %s - ignoring input"),
			*CurrentAdapter->GetAdapterTypeName());
		return;
	}

	// ========================================
	// 2. Update Counters via GridStateService
	// ========================================

	int32 PreviousValue = 0;
	if (GridStateService)
	{
		PreviousValue = GridStateService->ApplyAxisScaling(CounterState, Axis, StepDelta);
		GridCounters = CounterState.GridCounters;  // Sync deprecated mirror
	}

	// ========================================
	// 3. Diagnostic Logging (Subsystem keeps all logs)
	// ========================================

	if (StepDelta < 0)
	{
		const UWorld* World = GetWorld();
		const double TS = World ? World->GetTimeSeconds() : 0.0;
		const int32 NewValue = (Axis == ESFScaleAxis::X) ? CounterState.GridCounters.X :
		                       (Axis == ESFScaleAxis::Y) ? CounterState.GridCounters.Y :
		                       CounterState.GridCounters.Z;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Axis negative step: Axis=%d Prev=%d New=%d"),
			(unsigned long long)GFrameCounter, TS, static_cast<int32>(Axis), PreviousValue, NewValue);
	}

	// ========================================
	// 4. Apply Scaling to Hologram via HologramHelper
	// ========================================

	// Scaled Extend (Issue #265): When extend is active, don't apply normal grid scaling.
	// UpdateCounterState (below) will notify ExtendService->OnScaledExtendStateChanged().
	const bool bRestoredExtendActive = IsRestoredExtendModeActive();
	if (ShouldSuppressNormalGridChildren())
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore][Extend] %s axis changed, grid=[%d,%d,%d], liveExtend=%d restoredExtend=%d"),
			DebugLabel,
			CounterState.GridCounters.X,
			CounterState.GridCounters.Y,
			CounterState.GridCounters.Z,
			IsExtendModeActive() ? 1 : 0,
			bRestoredExtendActive ? 1 : 0);
	}
	else if (HologramHelper)
	{
		// Calculate scaling delta
		const float StepSize = ScaleStepSize * StepDelta;
		FVector Delta(0.0f);
		switch (Axis)
		{
		case ESFScaleAxis::X: Delta.X = StepSize; break;
		case ESFScaleAxis::Y: Delta.Y = StepSize; break;
		case ESFScaleAxis::Z: Delta.Z = StepSize; break;
		}

		// Apply via service with regenerate callback
		auto RegenerateCallback = [this]() { RegenerateChildHologramGrid(); };
		HologramHelper->ApplyScalingDelta(Hologram, Delta, CurrentScalingOffset, RegenerateCallback);
	}

	// ========================================
	// 5. Post-Scaling Logging
	// ========================================

	if (ActiveHologram.IsValid())
	{
		const UWorld* World = GetWorld();
		const double TS = World ? World->GetTimeSeconds() : 0.0;
		const TArray<AFGHologram*> ParentChildren = ActiveHologram->GetHologramChildren();
		const int32 HelperCount = HologramHelper ? HologramHelper->GetSpawnedChildren().Num() : 0;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Frame=%llu t=%.3f] Post-axis counts: helper=%d parent=%d"),
			(unsigned long long)GFrameCounter, TS, HelperCount, ParentChildren.Num());
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: %s - Grid[%d,%d,%d]"), DebugLabel,
        CounterState.GridCounters.X, CounterState.GridCounters.Y, CounterState.GridCounters.Z);

	// ========================================
	// 6. Update UI and Trigger Child Repositioning
	// ========================================

	UpdateCounterState(CounterState);

	// ========================================
	// 7. Auto-Hold after Grid Modification (Issue #273)
	// ========================================
	// When bAutoHoldOnGridChange is enabled: lock the hologram whenever grid > 1x1x1.
	// Unlike Scaled Extend, this lock is user-overridable — pressing vanilla Hold will release it.
	// Each new grid change resets the override flag and re-engages auto-hold.
	{
		FSmart_ConfigStruct Config = FSmart_ConfigStruct::GetActiveConfig(this);
		if (Config.bAutoHoldOnGridChange && ActiveHologram.IsValid() && !IsExtendModeActive() && !bRestoredExtendActive)
		{
			const FIntVector& Grid = CounterState.GridCounters;
			const bool bGridExpanded = (FMath::Abs(Grid.X) > 1 || FMath::Abs(Grid.Y) > 1 || FMath::Abs(Grid.Z) > 1);

			if (bGridExpanded)
			{
				// A new grid change always resets the user override so auto-hold re-engages
				bAutoHoldUserOverrode = false;

				if (!ActiveHologram->IsHologramLocked())
				{
					bLockedByModifier = true;
					ActiveHologram->LockHologramPosition(true);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔒 AUTO-HOLD: Locked hologram after grid change [%d,%d,%d]"),
						Grid.X, Grid.Y, Grid.Z);
				}

				// Issue #282: Always claim auto-hold ownership when grid is expanded,
				// even if already locked by a modifier. This prevents modifier release
				// from unlocking — TryReleaseHologramLock checks !bAutoHoldActive.
				if (!bAutoHoldActive)
				{
					bAutoHoldActive = true;
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔒 AUTO-HOLD: Claimed ownership (grid expanded [%d,%d,%d], wasAlreadyLocked=%d)"),
						Grid.X, Grid.Y, Grid.Z, ActiveHologram->IsHologramLocked());
				}
			}
			else
			{
				// Grid returned to 1x1x1 — release auto-hold if we own the lock
				if (bAutoHoldActive && bLockedByModifier && ActiveHologram->IsHologramLocked() && !IsAnyModalFeatureActive())
				{
					bAutoHoldActive = false;
					bLockedByModifier = false;
					bAutoHoldUserOverrode = false;
					ActiveHologram->LockHologramPosition(false);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔓 AUTO-HOLD: Released lock (grid returned to 1x1x1)"));
				}
				if (!IsAnyModalFeatureActive())
				{
					bAutoHoldActive = false;
				}
				bAutoHoldUserOverrode = false;
			}
		}
	}
}

void USFSubsystem::OnScaleXChanged(const FInputActionValue& Value)
{
	// Scaled Extend (Issue #265): Allow X scaling during extend mode.
	// ApplyAxisScaling routes to ExtendService when extend is active.

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnScaleXChanged(Value);
	}

	const float AxisValue = Value.Get<float>();

	// DIAGNOSTIC: Log at Display level to debug scaling input regression
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[INPUT] OnScaleXChanged: Value=%.3f, ModX=%d, ModY=%d, HasHolo=%d, Spacing=%d, Steps=%d, Stagger=%d, Recipe=%d, Extend=%d"),
		AxisValue, bModifierScaleXActive, bModifierScaleYActive, ActiveHologram.IsValid(),
		bSpacingModeActive, bStepsModeActive, bStaggerModeActive, bRecipeModeActive, IsExtendModeActive());

	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[INPUT] OnScaleXChanged: BLOCKED - No active hologram"));
		return;
	}

	// CRITICAL FIX: Chord binding doesn't properly block the callback - it fires with Value=0.0
	// when prerequisites aren't met. We must manually gate the input here.
	if (FMath::Abs(AxisValue) < 0.01f)
	{
		if (!bModifierScaleXActive && !bModifierScaleYActive)
		{
			return;
		}
		return;
	}

	// NumPad8/5 keys may also be bound to this axis callback. To avoid double-processing
	// when a modal feature is active, let the dedicated handlers manage modal adjustments.
	if (!bModifierScaleXActive && !bModifierScaleYActive &&
		(bSpacingModeActive || bStepsModeActive || bStaggerModeActive || bRecipeModeActive))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[INPUT] OnScaleXChanged: BLOCKED - Modal active (spacing=%d steps=%d stagger=%d recipe=%d)"),
			bSpacingModeActive, bStepsModeActive, bStaggerModeActive, bRecipeModeActive);
		return; // Modal handling is performed in OnValueIncreased/OnValueDecreased/OnMouseWheelChanged
	}

	// These become universal increment/decrement when modifiers are held
	const int32 Direction = AxisValue > 0.0f ? +1 : -1;

	// Capture rotation BEFORE scaling to detect if vanilla wheel rotation interfered
	const FRotator RotationBefore = ActiveHologram->GetActorRotation();

	// MODIFIER PRIORITY: Check modifiers first to determine which axis to scale
	if (bModifierScaleXActive && bModifierScaleYActive)
	{
		// Both modifiers held → Scale Z-axis
		LastAxisInput = ELastAxisInput::Z;  // Both modifiers = Z axis
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Z: %.2f (direction: %d) [X+Z modifiers]"), AxisValue, Direction);
		ApplyAxisScaling(ESFScaleAxis::Z, Direction, TEXT("Scale Z"));
	}
	else if (bModifierScaleXActive)
	{
		// X modifier held → Scale X-axis
		LastAxisInput = ELastAxisInput::X;  // X modifier only = X axis
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale X: %.2f (direction: %d) [X modifier]"), AxisValue, Direction);
		ApplyAxisScaling(ESFScaleAxis::X, Direction, TEXT("Scale X"));
	}
	else if (bModifierScaleYActive)
	{
		// Z modifier held → Scale Y-axis
		LastAxisInput = ELastAxisInput::Y;  // Y modifier still held = Y axis
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Y: %.2f (direction: %d) [Z modifier]"), AxisValue, Direction);
		ApplyAxisScaling(ESFScaleAxis::Y, Direction, TEXT("Scale Y"));
	}
	else
	{
		// No modifiers → Default X-axis behavior
		LastAxisInput = ELastAxisInput::X;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale X: %.2f (direction: %d)"), AxisValue, Direction);
		ApplyAxisScaling(ESFScaleAxis::X, Direction, TEXT("Scale X"));
	}

	// Fix rotation leak: undo any vanilla rotation that leaked through during scaling.
	// This handles cases where external mods interfere with lock
	// state, allowing vanilla scroll wheel rotation to apply during Smart!'s modifier use.
	const FRotator RotationAfter = ActiveHologram->GetActorRotation();
	if (!RotationBefore.Equals(RotationAfter, 0.01f))
	{
		ActiveHologram->SetActorRotation(RotationBefore);
		UE_LOG(LogSmartFoundations, Warning, TEXT("\U0001f504 Rotation leak corrected during scaling. Before: %s, After: %s, Delta: Yaw=%.2f"),
			*RotationBefore.ToCompactString(), *RotationAfter.ToCompactString(),
			RotationAfter.Yaw - RotationBefore.Yaw);
	}
}

void USFSubsystem::OnScaleYChanged(const FInputActionValue& Value)
{
	// Scaled Extend (Issue #265): Allow Y scaling during extend mode.
	// ApplyAxisScaling routes to ExtendService when extend is active.

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnScaleYChanged(Value);
	}

	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("OnScaleYChanged: No active hologram"));
		return;
	}

	const float AxisValue = Value.Get<float>();
	if (FMath::Abs(AxisValue) < 0.01f)
	{
		return;
	}

	// NumPad6/4 keys fire here
	// MODIFIER PRIORITY: Ignore if any modifier is active (use NumPad8/5 in modifier mode)
	if (bModifierScaleXActive || bModifierScaleYActive)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Y: Ignored (modifier mode active - use NumPad8/5)"));
		return;
	}

	// No modifiers → Default Y-axis behavior
	const int32 Direction = AxisValue > 0.0f ? +1 : -1;
	LastAxisInput = ELastAxisInput::Y;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Y: %.2f (direction: %d)"), AxisValue, Direction);
	ApplyAxisScaling(ESFScaleAxis::Y, Direction, TEXT("Scale Y"));
}

void USFSubsystem::OnScaleZChanged(const FInputActionValue& Value)
{
    // Block scaling while EXTEND is active
    if (IsExtendModeActive())
    {
        return;
    }

    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnScaleZChanged(Value);
    }

    if (!ActiveHologram.IsValid())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("OnScaleZChanged: No active hologram"));
        return;
    }

    const float AxisValue = Value.Get<float>();
    if (FMath::Abs(AxisValue) < 0.01f)
    {
        return;
    }

    // MODIFIER PRIORITY: Ignore if any modifier is active (use NumPad8/5 in modifier mode)
    if (bModifierScaleXActive || bModifierScaleYActive)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Z: Ignored (modifier mode active - use NumPad8/5)"));
        return;
    }

    // No modifiers → Default Z-axis behavior
    const int32 Direction = AxisValue > 0.0f ? +1 : -1;
    LastAxisInput = ELastAxisInput::Z;
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale Z: %.2f (direction: %d)"), AxisValue, Direction);
    ApplyAxisScaling(ESFScaleAxis::Z, Direction, TEXT("Scale Z"));
}

void USFSubsystem::OnValueIncreased(const FInputActionValue& Value)
{
    // Scaled Extend (Issue #265): Allow value increase during extend mode
    // for spacing, steps, and rotation adjustments.

    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnValueIncreased(Value);
    }

    const float AxisValue = Value.Get<float>();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[INPUT] OnValueIncreased: Value=%.3f, HasHolo=%d, Spacing=%d, Steps=%d, Stagger=%d, Recipe=%d, Rotation=%d, Extend=%d"),
        AxisValue, ActiveHologram.IsValid(), bSpacingModeActive, bStepsModeActive, bStaggerModeActive, bRecipeModeActive, bRotationModeActive, IsExtendModeActive());

    if (FMath::Abs(AxisValue) < 0.01f)
    {
        return;
    }

    // Support for fast input accumulation (e.g., high-speed mouse wheels)
    const int32 AccumulatedSteps = (int32)FMath::Abs(AxisValue);
    const int32 Direction = +1; // Always increase in OnValueIncreased

    // Auto-Connect Settings Mode takes priority
    if (bAutoConnectSettingsModeActive)
    {
        AdjustAutoConnectSetting(+1);
        return;
    }

    // Unified modal routing via GridStateService dispatcher
    if (GridStateService)
    {
        const auto Result = GridStateService->DispatchValueAdjust(
            CounterState,
            /*AccumulatedSteps*/ 1,
            /*Direction*/ +1,
            bRecipeModeActive,
            bSpacingModeActive,
            bStepsModeActive,
            bStaggerModeActive,
            bRotationModeActive);

        if (Result == USFGridStateService::EValueAdjustResult::CountersChanged)
        {
            UpdateCounterState(CounterState);
            return;
        }
        if (Result == USFGridStateService::EValueAdjustResult::RecipeChanged)
        {
            CycleRecipeForward(1);
            return;
        }
    }

    // Default: with no mode active, Num8/Num5 should adjust X scaling via OnScaleXChanged
    // Always forward the button value to OnScaleXChanged so default X behavior works
    {
        FInputActionValue ScaledValue(AxisValue);
        OnScaleXChanged(ScaledValue);
    }
}

void USFSubsystem::OnMouseWheelChanged(const FInputActionValue& Value)
{
    const float AxisValue = Value.Get<float>();
    if (FMath::Abs(AxisValue) < 0.01f)
    {
        return;
    }

    // Support for fast input accumulation (e.g., high-speed mouse wheels)
    const int32 AccumulatedSteps = (int32)FMath::Abs(AxisValue);
    const int32 Direction = AxisValue > 0.0f ? +1 : -1;

    // Auto-Connect Settings Mode takes priority
    if (bAutoConnectSettingsModeActive)
    {
        AdjustAutoConnectSetting(Direction);
        return;
    }

    // EXTEND mode: Cycle extend direction (Forward→Right→Backward→Left)
    if (ExtendService && ExtendService->IsExtendModeActive())
    {
        // Check if any modal feature or modifier is active
        bool bAnyModalActive = bSpacingModeActive || bStepsModeActive || bRotationModeActive ||
                               bModifierScaleXActive || bModifierScaleYActive;

        if (!bAnyModalActive)
        {
            // No modifiers/modes - cycle extend direction
            ExtendService->CycleExtendDirection(Direction);
            return;
        }
        // Fall through to modal routing below
    }

    // Unified modal routing via GridStateService dispatcher
    if (GridStateService)
    {
        const auto Result = GridStateService->DispatchValueAdjust(
            CounterState,
            AccumulatedSteps,
            Direction,
            bRecipeModeActive,
            bSpacingModeActive,
            bStepsModeActive,
            bStaggerModeActive,
            bRotationModeActive);

        if (Result == USFGridStateService::EValueAdjustResult::CountersChanged)
        {
            UpdateCounterState(CounterState);
            return;
        }
        if (Result == USFGridStateService::EValueAdjustResult::RecipeChanged)
        {
            if (Direction > 0) { CycleRecipeForward(AccumulatedSteps); }
            else { CycleRecipeBackward(AccumulatedSteps); }
            return;
        }
    }

    // Default: Only handle mouse wheel if modifiers are active (X or Z modifier held)
    // Otherwise, leave it alone for vanilla rotation
    if (bModifierScaleXActive || bModifierScaleYActive)
    {
        // Modifier active - continuous scaling (use accumulated value directly)
        FInputActionValue ScaledValue(AxisValue);
        OnScaleXChanged(ScaledValue);
    }
    // No modifiers or modes active - ignore input, let vanilla handle it
}

void USFSubsystem::OnValueDecreased(const FInputActionValue& Value)
{
    // Scaled Extend (Issue #265): Allow value decrease during extend mode
    // for spacing, steps, and rotation adjustments.

    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnValueDecreased(Value);
    }

    const float AxisValue = Value.Get<float>();
    if (FMath::Abs(AxisValue) < 0.01f)
    {
        return;
    }

    // Support for fast input accumulation (e.g., high-speed mouse wheels)
    const int32 AccumulatedSteps = (int32)FMath::Abs(AxisValue);
    const int32 Direction = -1; // Always decrease in OnValueDecreased

    // Auto-Connect Settings Mode takes priority
    if (bAutoConnectSettingsModeActive)
    {
        AdjustAutoConnectSetting(-1);
        return;
    }

    // Unified modal routing via GridStateService dispatcher
    if (GridStateService)
    {
        const auto Result = GridStateService->DispatchValueAdjust(
            CounterState,
            /*AccumulatedSteps*/ 1,
            /*Direction*/ -1,
            bRecipeModeActive,
            bSpacingModeActive,
            bStepsModeActive,
            bStaggerModeActive,
            bRotationModeActive);

        if (Result == USFGridStateService::EValueAdjustResult::CountersChanged)
        {
            UpdateCounterState(CounterState);
            return;
        }
        if (Result == USFGridStateService::EValueAdjustResult::RecipeChanged)
        {
            CycleRecipeBackward(1);
            return;
        }
        // If Result == None, fall through to default scaling fallback
    }

    // Default - continuous scaling (not discrete steps)
    // Negate the value so OnScaleXChanged sees negative and decreases
    FInputActionValue NegatedValue(-Value.Get<float>());
    OnScaleXChanged(NegatedValue);
}

void USFSubsystem::OnModifierScaleXPressed(const FInputActionValue& Value)
{
    // Scaled Extend (Issue #265): Allow X modifier during extend mode for Scaled Extend.

    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleXPressed(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleXActive = InputHandler->IsModifierScaleXActive();

        // Update arrow highlighting immediately based on modifier combination
        if (InputHandler->IsModifierScaleXActive() && InputHandler->IsModifierScaleYActive())
        {
            LastAxisInput = ELastAxisInput::Z;  // Both modifiers = Z axis
        }
        else if (InputHandler->IsModifierScaleXActive())
        {
            LastAxisInput = ELastAxisInput::X;  // X modifier only = X axis
        }

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
}

void USFSubsystem::OnModifierScaleXReleased(const FInputActionValue& Value)
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleXReleased(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleXActive = InputHandler->IsModifierScaleXActive();
    }

    // Update arrow highlighting based on remaining modifiers
    if (InputHandler && InputHandler->IsModifierScaleYActive())
    {
        LastAxisInput = ELastAxisInput::Y;  // Y modifier still held = Y axis
    }
    else
    {
        LastAxisInput = ELastAxisInput::None;  // No modifiers = show all arrows
    }

    // Try to release lock (Task 52 - centralized)
    TryReleaseHologramLock();
}

void USFSubsystem::OnModifierScaleYPressed(const FInputActionValue& Value)
{
    // Scaled Extend (Issue #265): Allow Y modifier during extend mode for Scaled Extend.

    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleYPressed(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleYActive = InputHandler->IsModifierScaleYActive();
    }

    // Update arrow highlighting immediately based on modifier combination
    if (InputHandler && InputHandler->IsModifierScaleXActive() && InputHandler->IsModifierScaleYActive())
    {
        LastAxisInput = ELastAxisInput::Z;  // Both modifiers = Z axis
    }
    else if (bModifierScaleYActive)
    {
        LastAxisInput = ELastAxisInput::Y;  // Y modifier only = Y axis
    }

    // Try to acquire lock (Task 52 - centralized)
    TryAcquireHologramLock();
}

void USFSubsystem::OnModifierScaleYReleased(const FInputActionValue& Value)
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnModifierScaleYReleased(Value);

        // Sync state from module (temporary during migration)
        bModifierScaleYActive = InputHandler->IsModifierScaleYActive();
    }

    // Update arrow highlighting based on remaining modifiers
    if (InputHandler && InputHandler->IsModifierScaleXActive())
    {
        LastAxisInput = ELastAxisInput::X;  // X modifier still held = X axis
    }
    else
    {
        LastAxisInput = ELastAxisInput::None;  // No modifiers = show all arrows
    }

    // Try to release lock (Task 52 - centralized)
    TryReleaseHologramLock();
}
