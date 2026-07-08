// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - core: subsystem lifecycle (ctor/Init/Deinit), accessors, power-connection mgmt, Get() + input setup/scaling.
 * Implementation split across SFSubsystem.cpp + SFSubsystem_*.cpp (each <2k); shared includes in
 * SFSubsystemImpl.h. No behavior change from the monolith.
 */

#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFSubsystemImpl.h"
#include "Features/Walk/SFWalkService.h"
#include "UI/SFWalkPanelWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"  // FInputModeGameAndUI/GameOnly for the Walk panel show/hide
#include "Hologram/FGBuildableHologram.h"    // [#296] IsInZoopBuildMode / GetDefaultBuildGunMode

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

	UE_LOG(LogSmartFoundations, Verbose, TEXT("SFSubsystem: Phase 0 modules and recipe service initialized"));
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

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
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

// ========================================
// Power Connection Management (moved from header - PIMPL pattern)
// ========================================

void USFSubsystem::CommitBuildingConnections()
{
	// Commit building connections (overwrite is fine for these)
	UE_LOG(LogSmartFoundations, Verbose, TEXT("⚡ CommitBuildingConnections: Copying %d planned building connections to committed"),
		PlannedBuildingConnections.Num());
	CommittedBuildingConnections = PlannedBuildingConnections;
	UE_LOG(LogSmartFoundations, Verbose, TEXT("⚡ CommitBuildingConnections: CommittedBuildingConnections now has %d entries"),
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

// ========================================
// MP spec-based scaling construction - server-side per-player spec staging
// ========================================

void USFSubsystem::StageScalingSpecForPlayer(APlayerController* PC, const FSFScalingSpec& Spec)
{
	if (!PC)
	{
		return;
	}
	if (Spec.bValid)
	{
		StagedScalingSpecs.Add(PC, Spec);
	}
	else
	{
		// An invalid spec is an explicit CLEAR (sent on every non-grid fire) - overwrite semantics
		// guarantee a stale spec from an earlier failed construct cannot leak into a later fire.
		StagedScalingSpecs.Remove(PC);
	}
}

bool USFSubsystem::PeekScalingSpecForInstigator(APawn* Instigator, UClass* BuildClass, FSFScalingSpec& OutSpec) const
{
	if (!Instigator || !BuildClass)
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
	if (!PC)
	{
		return false;
	}
	const FSFScalingSpec* Staged = StagedScalingSpecs.Find(PC);
	if (!Staged || !Staged->bValid || Staged->BuildClass != BuildClass)
	{
		return false;
	}
	OutSpec = *Staged;
	return true;
}

bool USFSubsystem::ConsumeScalingSpecForInstigator(APawn* Instigator, UClass* BuildClass, FSFScalingSpec& OutSpec)
{
	if (!PeekScalingSpecForInstigator(Instigator, BuildClass, OutSpec))
	{
		return false;
	}
	StagedScalingSpecs.Remove(Cast<APlayerController>(Instigator->GetController()));
	return true;
}

// [EXTEND-MP] Extend commit staging - identical model to the scaling spec above.

void USFSubsystem::StageExtendCommitForPlayer(APlayerController* PC, const FSFExtendCommitSpec& Spec)
{
	if (!PC)
	{
		return;
	}
	if (Spec.bValid)
	{
		StagedExtendCommits.Add(PC, Spec);
		StagedExtendCommitTimes.Add(PC, FPlatformTime::Seconds());
	}
	else
	{
		StagedExtendCommits.Remove(PC);
		StagedExtendCommitTimes.Remove(PC);
	}
}

bool USFSubsystem::PeekExtendCommitForInstigator(APawn* Instigator, UClass* BuildClass, FSFExtendCommitSpec& OutSpec) const
{
	if (!Instigator || !BuildClass)
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
	if (!PC)
	{
		return false;
	}
	const FSFExtendCommitSpec* Staged = StagedExtendCommits.Find(PC);
	if (!Staged || !Staged->bValid || Staged->BuildClass != BuildClass)
	{
		return false;
	}
	// Freshness TTL: a live session pre-stages every ~250ms; an entry older than this belongs to
	// an abandoned session whose CLEAR was lost - never consume it into an unrelated construct.
	const double* StagedAt = StagedExtendCommitTimes.Find(PC);
	if (!StagedAt || FPlatformTime::Seconds() - *StagedAt > 10.0)
	{
		return false;
	}
	OutSpec = *Staged;
	return true;
}

bool USFSubsystem::ConsumeExtendCommitForInstigator(APawn* Instigator, UClass* BuildClass, FSFExtendCommitSpec& OutSpec)
{
	if (!PeekExtendCommitForInstigator(Instigator, BuildClass, OutSpec))
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
	StagedExtendCommits.Remove(PC);
	StagedExtendCommitTimes.Remove(PC);
	return true;
}

// Smart Walking (#356 Slice 3): walk commit staging - identical model to the Extend commit above.

void USFSubsystem::StageWalkCommitForPlayer(APlayerController* PC, const FSFWalkCommitSpec& Spec)
{
	if (!PC)
	{
		return;
	}
	if (Spec.bValid)
	{
		StagedWalkCommits.Add(PC, Spec);
		StagedWalkCommitTimes.Add(PC, FPlatformTime::Seconds());
	}
	else
	{
		StagedWalkCommits.Remove(PC);
		StagedWalkCommitTimes.Remove(PC);
	}
}

bool USFSubsystem::PeekWalkCommitForInstigator(APawn* Instigator, UClass* BuildClass, FSFWalkCommitSpec& OutSpec) const
{
	if (!Instigator || !BuildClass)
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
	if (!PC)
	{
		return false;
	}
	const FSFWalkCommitSpec* Staged = StagedWalkCommits.Find(PC);
	if (!Staged || !Staged->bValid || Staged->BuildClass != BuildClass)
	{
		return false;
	}
	// Freshness TTL: the walk stages its commit at fire time and the construct consumes it in the same
	// fire, so anything older than this belongs to an abandoned commit - never consume it.
	const double* StagedAt = StagedWalkCommitTimes.Find(PC);
	if (!StagedAt || FPlatformTime::Seconds() - *StagedAt > 10.0)
	{
		return false;
	}
	OutSpec = *Staged;
	return true;
}

bool USFSubsystem::ConsumeWalkCommitForInstigator(APawn* Instigator, UClass* BuildClass, FSFWalkCommitSpec& OutSpec)
{
	if (!PeekWalkCommitForInstigator(Instigator, BuildClass, OutSpec))
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
	StagedWalkCommits.Remove(PC);
	StagedWalkCommitTimes.Remove(PC);
	return true;
}

void USFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("Smart! Subsystem: Initialize() called"));

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

	UE_LOG(LogSmartFoundations, Verbose, TEXT("Smart! Subsystem: World found, starting timers"));

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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Auto-Connect Service initialized"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Radar Pulse Service initialized"));
	}

	// Initialize Smart Walking service (#356)
	WalkService = NewObject<USFWalkService>(this);
	if (WalkService)
	{
		WalkService->Initialize(this);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Smart Walking Service initialized"));
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

	// [Track E / #367] The deferred post-load chain diagnostic timer was removed along with the
	// load-time repair sweep (see USFChainActorService::Initialize). Smart! does not scan or mutate
	// chain actors at load; runtime guards and the post-build hygiene pass remain.

	UE_LOG(LogSmartFoundations, Verbose, TEXT("Smart! Subsystem: Timers started (player controller + hologram polling) + actor spawn delegate bound"));
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

	// Shutdown Smart Walking service (#356)
	if (WalkService)
	{
		WalkService->ExitWalk(false);
		WalkService = nullptr;
	}
	bWalkModeActive = false;

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

		// #418: continue a budget-truncated spawn burst. RegenerateChildHologramGrid clears the
		// flag on entry and its reconcile pass re-derives exactly the missing cells, so this
		// converges (at least one child spawns per slice) and self-cancels when the hologram
		// changes or the grid completes.
		if (HologramHelper->IsSpawnContinuationPending() && ActiveHologram.IsValid())
		{
			if (USFGridSpawnerService* Spawner = GetGridSpawnerService())
			{
				Spawner->RegenerateChildHologramGrid();
			}
		}

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

	// Smart Walking: keep the standalone preview holograms cyan. They are deliberately NOT build-gun-ticked and NOT
	// AddChild'd (AddChild crashes the build gun's per-tick ResetConstructDisqualifiers recursion), so the
	// FGCDInitializing flag added during init would otherwise persist and paint them red. Re-clear it + force HMS_OK
	// every frame — the same per-frame refresh the grid/Extend use to keep their un-ticked clone previews valid.
	if (WalkService && WalkService->IsActive())
	{
		WalkService->RefreshWalkValidity();
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
		FString DbgRawHit = TEXT("(no-pc/cam)"); // [EXTEND-MP] raw trace result for diagnostics
		FVector DbgDir = FVector::ZeroVector;

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
				// [EXTEND-MP] Use the player view point for the aim ray (robust on clients) instead of the
				// camera-manager actor's forward vector, which can be stale/wrong on a remote client.
				FVector ViewLoc; FRotator ViewRot;
				PC->GetPlayerViewPoint(ViewLoc, ViewRot);
				FVector Start = ViewLoc;
				DbgDir = ViewRot.Vector();
				FVector End = Start + DbgDir * 5000.0f;  // 50m range

				FHitResult HitResult;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(PC->GetPawn());
				Params.AddIgnoredActor(ActiveHologram.Get());

				// Use WorldStatic channel which hits buildings, not just visibility
				const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, Params);
				DbgRawHit = bHit ? (HitResult.GetActor() ? HitResult.GetActor()->GetClass()->GetName() : TEXT("hit(no-actor)")) : TEXT("MISS");
				if (bHit)
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
				SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose, TEXT("🔄 EXTEND: No PlayerController available for line trace"));
				LastWarnTime = CurrentTime;
			}
		}

		// [EXTEND-MP] TEMP diagnostic: why does Extend never activate on a client? Log the detection state
		// once/sec (Display so it shows in-game; remove before release).
		{
			static double LastExtMpLog = 0;
			const double NowExt = FPlatformTime::Seconds();
			if (NowExt - LastExtMpLog > 1.0)
			{
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[EXTEND-MP] NetMode=%d PC=%s rawHit=%s dir=(%.2f,%.2f,%.2f) LookedAt=%s holoBuildClass=%s"),
					GetWorld() ? (int32)GetWorld()->GetNetMode() : -1,
					PC ? TEXT("ok") : TEXT("NULL"),
					*DbgRawHit, DbgDir.X, DbgDir.Y, DbgDir.Z,
					LookedAtBuilding ? *LookedAtBuilding->GetName() : TEXT("none"),
					(ActiveHologram.IsValid() && ActiveHologram->GetBuildClass()) ? *ActiveHologram->GetBuildClass()->GetName() : TEXT("-"));
				LastExtMpLog = NowExt;
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
			UE_LOG(LogSmartFoundations, Verbose, TEXT("Started hologram auto-detection polling"));
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

void USFSubsystem::EnterWalkMode()
{
	UE_LOG(LogSmartFoundations, Log, TEXT(">>> [Walk] EnterWalkMode ENTER: bWalkModeActive=%d active=%s"),
		bWalkModeActive ? 1 : 0, *GetNameSafe(ActiveHologram.Get()));
	if (bWalkModeActive)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("<<< [Walk] EnterWalkMode EXIT: already active"));
		return;
	}

	AFGHologram* Seed = ActiveHologram.Get();
	if (!Seed)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("<<< [Walk] EnterWalkMode EXIT: no active hologram to seed a Path"));
		return;
	}
	UE_LOG(LogSmartFoundations, Log, TEXT("  [Walk] EnterWalkMode: seed=%s world=%s yaw=%.1f locked=%d | counter before reset=%s"),
		*GetNameSafe(Seed), *Seed->GetActorLocation().ToString(), Seed->GetActorRotation().Yaw,
		Seed->IsHologramLocked() ? 1 : 0, *GetCounterState().GridCounters.ToString());

	// Switching from grid scaling to walking: clear any scaled grid children (else they linger as orphan
	// previews) and LOCK the seed in place — an unlocked parent that follows the cursor would force rebuilding
	// the whole path on every move.
	if (FSFHologramHelperService* Helper = GetHologramHelper())
	{
		Helper->DestroyAllChildren();
	}
	// Reset the grid counter to 1x1x1 too: DestroyAllChildren only empties Smart's tracking array, so a later
	// grid resync would otherwise pull the scaling clones back from the parent's mChildren and re-spawn them at
	// the stale scaled size. With the counter at 1x1x1 there is nothing to regenerate.
	{
		FSFCounterState CounterReset = GetCounterState();
		CounterReset.GridCounters = FIntVector(1, 1, 1);
		UpdateCounterState(CounterReset);
	}
	bLockedByModifier = true;
	Seed->LockHologramPosition(true);

	if (WalkService && WalkService->EnterWalk(Seed))
	{
		bWalkModeActive = true;
		UpdateCounterDisplay();   // surface the walk's segment state in the build HUD immediately
		UE_LOG(LogSmartFoundations, Log, TEXT("Smart Walking: entered (seed locked, grid children cleared)"));
	}
	else
	{
		// Walk failed to start — undo the lock so we don't strand a locked hologram.
		bLockedByModifier = false;
		Seed->LockHologramPosition(false);
	}
}

void USFSubsystem::ExitWalkMode()
{
	UE_LOG(LogSmartFoundations, Log, TEXT(">>> [Walk] ExitWalkMode ENTER: bWalkModeActive=%d active=%s"),
		bWalkModeActive ? 1 : 0, *GetNameSafe(ActiveHologram.Get()));
	if (!bWalkModeActive)
	{
		return;
	}

	if (WalkService)
	{
		WalkService->ExitWalk(false);
	}
	// Tear down the segment-list overlay on every exit path (holster, cancel, etc.).
	if (WalkPanelWidget.IsValid())
	{
		WalkPanelWidget->RemoveFromParent();
		WalkPanelWidget.Reset();
	}
	// Release the seed lock the walk took on enter, so the parent follows the cursor again.
	if (bLockedByModifier && ActiveHologram.IsValid())
	{
		bLockedByModifier = false;
		ActiveHologram->LockHologramPosition(false);
	}
	bWalkModeActive = false;
	UpdateCounterDisplay();   // walk is gone → HUD falls back to the normal grid counter
	UE_LOG(LogSmartFoundations, Log, TEXT("Smart Walking: exited"));
}

void USFSubsystem::ToggleWalkMode()
{
	if (bWalkModeActive)
	{
		ExitWalkMode();
	}
	else
	{
		EnterWalkMode();
	}
}

void USFSubsystem::WalkAdvance()
{
	if (bWalkModeActive && WalkService)
	{
		WalkService->CommitActiveAndAdvance();
	}
}

void USFSubsystem::WalkBackUp()
{
	if (bWalkModeActive && WalkService)
	{
		WalkService->BackUp();
	}
}

void USFSubsystem::WalkNudgeActive(float DeltaAdvanceCm, float DeltaTurnDeg, float DeltaRiseCm, float DeltaShiftCm)
{
	if (bWalkModeActive && WalkService)
	{
		WalkService->NudgeActive(DeltaAdvanceCm, DeltaTurnDeg, DeltaRiseCm, DeltaShiftCm);
	}
}

void USFSubsystem::OpenWalkPanel()
{
	APlayerController* PC = GetLastController();
	if (!PC)
	{
		PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	}
	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Walk Panel: no PlayerController"));
		return;
	}

	// Already open → just refresh it.
	if (WalkPanelWidget.IsValid() && WalkPanelWidget->IsInViewport())
	{
		WalkPanelWidget->Refresh();
		return;
	}

	const FString WidgetPath = TEXT("/SmartFoundations/SmartFoundations/UI/Smart_WalkPanel_Widget.Smart_WalkPanel_Widget_C");
	FSoftClassPath WidgetClassPath(WidgetPath);
	UClass* WidgetClass = WidgetClassPath.TryLoadClass<UUserWidget>();
	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Walk Panel: failed to load widget class at %s"), *WidgetPath);
		return;
	}

	USFWalkPanelWidget* Widget = CreateWidget<USFWalkPanelWidget>(PC, WidgetClass);
	if (!Widget)
	{
		return;
	}
	WalkPanelWidget = Widget;
	// #356 two-mode design: steer mode (Collapsed — HUD badge + in-world controls) vs edit mode (visible cursor panel),
	// toggled with K. Create it Collapsed + add to viewport, then immediately ToggleWalkPanel() to OPEN it in edit mode:
	// entering a walk needs a visible cue — the Smart Panel vanishing into a bare steer screen read as "nothing happened".
	// (ToggleWalkPanel's create-guard sees the widget already in-viewport, so this doesn't recurse.)
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	Widget->AddToViewport(100);
	ToggleWalkPanel();   // show the panel right away; K then hides it to steer
}

bool USFSubsystem::IsWalkPanelVisible() const
{
	return WalkPanelWidget.IsValid() && WalkPanelWidget->IsInViewport()
		&& WalkPanelWidget->GetVisibility() == ESlateVisibility::Visible;
}

void USFSubsystem::ToggleWalkPanel()
{
	// Create it (Collapsed) if this is the first K press of the walk, then fall through to show it.
	if (!WalkPanelWidget.IsValid() || !WalkPanelWidget->IsInViewport())
	{
		OpenWalkPanel();
	}
	if (!WalkPanelWidget.IsValid())
	{
		return;
	}

	APlayerController* PC = GetLastController();
	if (!PC)
	{
		PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	}

	const bool bHidden = WalkPanelWidget->GetVisibility() == ESlateVisibility::Collapsed;
	if (bHidden)
	{
		// Restore → interactive EDIT mode: Visible + cursor + UIOnly. UIOnly makes the panel MODAL — clicks outside it
		// hit no widget and are swallowed (no accidental world building, like the Smart Panel) and steering pauses
		// (panel-up = edit). Because UIOnly also blocks the game K action, the widget handles K/Escape itself (its
		// NativeOnKeyDown), so it's focused here. Refresh() so it reflects any steering done while hidden.
		WalkPanelWidget->SetVisibility(ESlateVisibility::Visible);
		WalkPanelWidget->Refresh();
		if (PC)
		{
			PC->bShowMouseCursor = true;
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(WalkPanelWidget->TakeWidget());
			PC->SetInputMode(Mode);
			// Explicit keyboard focus so the panel's NativeOnKeyDown reliably gets K/Escape. SetWidgetToFocus alone
			// routes MOUSE, not keyboard — without this, K only worked after clicking a widget (mirrors the Upgrade panel).
			WalkPanelWidget->SetIsFocusable(true);
			WalkPanelWidget->SetKeyboardFocus();
		}
	}
	else
	{
		// Hide → STEER mode: restore game input + hide the cursor (HUD badge only).
		WalkPanelWidget->SetVisibility(ESlateVisibility::Collapsed);
		if (PC)
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
}

bool USFSubsystem::RouteWalkValueAdjust(int32 AccumulatedSteps, int32 Direction)
{
	// Authoritative gate: whenever a walk is in progress, the walk OWNS all value-adjust input (Num8/Num5,
	// modifier scroll). Gate on the service's own IsActive() — never let the grid-scaling path run while a walk
	// exists, even if the bWalkModeActive flag and the service ever disagree. (#356: Num8/Num5 were leaking to
	// grid Y scaling, which blocked auto-connect.)
	if (!WalkService || !WalkService->IsActive())
	{
		return false;
	}

	// #356 reframe: the active transform modal acts on the ACTIVE segment, not the grid.
	// [#217] Walk maps 1:1 to the grid transforms and SHARES the same configured increments
	// (Advance=Spacing, Turn=Rotation, Rise=Steps, Shift=Stagger). Defaults 0.5 m / 5°.
	const float Steps = static_cast<float>(FMath::Max(1, AccumulatedSteps)) * static_cast<float>(Direction);
	float dAdvance = 0.0f, dTurn = 0.0f, dRise = 0.0f, dShift = 0.0f;
	if (bSpacingModeActive)       { dAdvance = Steps * CachedScrollIncrements.SpacingCm;   }  // Spacing  → segment gap  (cm)
	else if (bRotationModeActive) { dTurn    = Steps * CachedScrollIncrements.RotationDeg; }  // Rotation → segment turn (deg)
	else if (bStepsModeActive)    { dRise    = Steps * CachedScrollIncrements.StepsCm;     }  // Steps    → segment rise  (cm)
	else if (bStaggerModeActive)  { dShift   = Steps * CachedScrollIncrements.StaggerCm;   }  // Stagger  → segment shift (cm)
	else
	{
		// No transform modal → a plain Scale-X value-adjust ADVANCES (up) / BACKS UP (down) the walk, segment by
		// segment (the in-world "scaling" reframe). Single advance entry point for value-adjust input so the grid
		// counter is never touched while walking.
		if (Direction >= 0) { WalkService->CommitActiveAndAdvance(); }
		else                { WalkService->BackUp(); }
		if (WalkPanelWidget.IsValid()) { WalkPanelWidget->Refresh(); }
		UpdateCounterDisplay();
		UE_LOG(LogSmartFoundations, Log, TEXT("[Walk] value-adjust (steps=%d dir=%d) -> %s"),
			AccumulatedSteps, Direction, Direction >= 0 ? TEXT("advance") : TEXT("back-up"));
		return true;
	}

	WalkService->NudgeActive(dAdvance, dTurn, dRise, dShift);
	if (WalkPanelWidget.IsValid())
	{
		WalkPanelWidget->Refresh();
	}
	UpdateCounterDisplay();
	return true;
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

	// Smart Walking (#356): walk mode owns scale input. Scale-X commits the active segment and advances;
	// scale-X-down backs up. The grid path NEVER runs while walking (lane separation by construction).
	if (WalkService && WalkService->IsActive())
	{
		// Walk owns scale input by construction (authoritative IsActive gate). Scale-X advances/backs-up segment by
		// segment; Scale-Y adds/removes a parallel bus LANE; Scale-Z adds/removes a vertical STACK level.
		switch (Axis)
		{
		case ESFScaleAxis::X:
			if (StepDelta > 0)      { WalkService->CommitActiveAndAdvance(); }
			else if (StepDelta < 0) { WalkService->BackUp(); }
			break;
		case ESFScaleAxis::Y:
			WalkService->AdjustCrossSection(StepDelta, 0);   // lanes (perpendicular to the heading)
			break;
		case ESFScaleAxis::Z:
			WalkService->AdjustCrossSection(0, StepDelta);   // vertical stack levels
			break;
		default:
			break;
		}
		if (WalkPanelWidget.IsValid()) { WalkPanelWidget->Refresh(); }
		UpdateCounterDisplay();
		UE_LOG(LogSmartFoundations, Log, TEXT("[Walk] ApplyAxisScaling axis=%d delta=%d (X=advance/back, Y=lanes, Z=stacks)"),
			static_cast<int32>(Axis), StepDelta);
		return;
	}

	if (!ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyAxisScaling: No active hologram registered"));
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
		UE_LOG(LogSmartFoundations, Verbose,
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
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose, TEXT("[SmartRestore][Extend] %s axis changed, grid=[%d,%d,%d], liveExtend=%d restoredExtend=%d"),
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
	// 6.5 Zoop/Vertical Build-Mode Reset (Issue #296)
	// ========================================
	// In a Zoop/Vertical Zoop build mode, primary fire doesn't construct the scaled grid - it
	// silently discards it (the reported "it just removes everything"). The mode is meaningless
	// while Smart! scaling drives the multi-placement, so snap back to the hologram's default
	// mode the moment the grid expands. Deliberately NOT gated by the auto-hold config below:
	// zoop + scaled grid is broken in every configuration.
	{
		const FIntVector& ZoopCheckGrid = CounterState.GridCounters;
		if (FMath::Abs(ZoopCheckGrid.X) > 1 || FMath::Abs(ZoopCheckGrid.Y) > 1 || FMath::Abs(ZoopCheckGrid.Z) > 1)
		{
			ResetZoopBuildModeForScaling();
		}
	}

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

void USFSubsystem::ResetZoopBuildModeForScaling()
{
	// [#296] Only interfere when there is a REAL mode conflict: the active hologram reports it is
	// currently in a zoop build mode (IsInZoopBuildMode is the vanilla predicate; the
	// AFGFoundationHologram override also covers Vertical Zoop). A player in Default - or any
	// other non-zoop mode - is never touched, and vanilla zoop without Smart! scaling is unaffected.
	AFGBuildableHologram* BuildableHolo = Cast<AFGBuildableHologram>(ActiveHologram.Get());
	if (!BuildableHolo || !BuildableHolo->IsInZoopBuildMode())
	{
		return;
	}

	TSubclassOf<UFGHologramBuildModeDescriptor> DefaultMode = BuildableHolo->GetDefaultBuildGunMode();
	if (!DefaultMode)
	{
		return;
	}

	// SetCurrentBuildGunMode is the same public entry the vanilla mode-select UI uses - it fires
	// OnBuildGunModeChanged (clearing zoop instances) and handles the server RPC in MP.
	AFGPlayerController* PC = GetLastController();
	AFGCharacterPlayer* Character = PC ? Cast<AFGCharacterPlayer>(PC->GetCharacter()) : nullptr;
	AFGBuildGun* BuildGun = Character ? Character->GetBuildGun() : nullptr;
	if (!BuildGun)
	{
		return;
	}

	BuildGun->SetCurrentBuildGunMode(DefaultMode);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("[#296] Zoop build mode reset to default (%s) - Smart! grid scaling owns placement"),
		*DefaultMode->GetName());
}

// [#209] Player Relative Controls - FIRST-CUT resolver (SPIKE, feel-pending).
// Maps the player's facing (relative to the held building) to a grid axis + sign, quantized to the
// nearest cardinal, so the primary scale grows toward where the player looks. This is the minimal
// wiring the #209 scope doc calls for: a facing -> (axis, sign) resolver in front of ApplyAxisScaling,
// gated on the opt-in setting, leaving the classic modifiers working. NOT YET DONE and to be settled
// in-game per the design: (1) the cardinal->axis/sign handedness ("away = right") - the sign choices
// below are a reasonable first guess to be confirmed or flipped once it's felt; (2) latch-on-mode-enter
// vs per-input resolution (this resolves per-input - simplest, may need a latch); (3) a 45-degree
// hysteresis band so the axis doesn't twitch at the boundary. Returns false if inputs are unusable.
static bool SF_ResolvePlayerRelativeAxis(float ViewYawDeg, float BuildingYawDeg, ESFScaleAxis& OutAxis, int32& OutSign)
{
	// Relative facing in [-180, 180]: 0 = looking along the building's forward, +90 = to its right.
	const float Rel = FMath::UnwindDegrees(ViewYawDeg - BuildingYawDeg);
	const float A = FMath::Abs(Rel);
	if (A <= 45.0f)        { OutAxis = ESFScaleAxis::X; OutSign = +1; }  // facing forward  -> grow +X (forward)
	else if (A >= 135.0f)  { OutAxis = ESFScaleAxis::X; OutSign = -1; }  // facing backward -> grow -X
	else if (Rel > 0.0f)   { OutAxis = ESFScaleAxis::Y; OutSign = +1; }  // facing right    -> grow +Y (verify sign)
	else                   { OutAxis = ESFScaleAxis::Y; OutSign = -1; }  // facing left     -> grow -Y (verify sign)
	return true;
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
		// X modifier held → Scale X-axis. [#209] When Player Relative is ON, this PRIMARY scale
		// instead grows toward the player's facing (resolver above). Non-destructive: only this
		// primary path remaps; the Y-modifier and Z (both-modifier) paths are untouched. Opt-in.
		ESFScaleAxis PrimaryAxis = ESFScaleAxis::X;
		int32 PrimarySign = +1;
		if (FSmart_ConfigStruct::GetActiveConfig(this).bPlayerRelativeControls)
		{
			if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				SF_ResolvePlayerRelativeAxis(PC->GetControlRotation().Yaw,
					ActiveHologram->GetActorRotation().Yaw, PrimaryAxis, PrimarySign);
			}
		}
		LastAxisInput = (PrimaryAxis == ESFScaleAxis::Y) ? ELastAxisInput::Y : ELastAxisInput::X;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Scale X: %.2f (dir: %d) [X modifier] PR axis=%d sign=%d"),
			AxisValue, Direction, (int32)PrimaryAxis, PrimarySign);
		ApplyAxisScaling(PrimaryAxis, Direction * PrimarySign, TEXT("Scale X (player-relative)"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("\U0001f504 Rotation leak corrected during scaling. Before: %s, After: %s, Delta: Yaw=%.2f"),
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

    // #356 walk: transform modals act on the active segment, not the grid.
    if (RouteWalkValueAdjust(1, +1)) { return; }

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
            bRotationModeActive,
            CachedScrollIncrements);  // [#217] config-driven per-notch increments

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

    // #356 walk: transform modals act on the active segment, not the grid.
    if (RouteWalkValueAdjust(AccumulatedSteps, Direction)) { return; }

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
            bRotationModeActive,
            CachedScrollIncrements);  // [#217] config-driven per-notch increments

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

    // #356 walk: transform modals act on the active segment, not the grid.
    if (RouteWalkValueAdjust(AccumulatedSteps, -1)) { return; }

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
            bRotationModeActive,
            CachedScrollIncrements);  // [#217] config-driven per-notch increments

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
