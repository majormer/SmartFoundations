#include "Subsystem/SFSubsystem.h"
#include "SmartFoundations.h"
#include "FGHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "FGRecipeManager.h"
#include "FGBuildingDescriptor.h"
#include "Data/SFHologramData.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"
#include "Debug/SFSplineAnalyzer.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SmartUpgradePanel.h"

// Phase 0 Extracted Modules (Task #61.6)
#include "Subsystem/SFInputHandler.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFValidationService.h"
#include "Subsystem/SFHologramHelperService.h"

// Service includes (moved from header for PIMPL pattern)
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"
#include "Services/RadarPulse/SFRadarPulseService.h"
#include "Services/SFHudService.h"
#include "Services/SFHintBarService.h"
#include "Services/SFChainActorService.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Services/SFGridStateService.h"
#include "Services/SFGridSpawnerService.h"
#include "Services/SFGridTransformService.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Features/Restore/SFRestoreService.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "Logging/SFLogMacros.h"
#include "Hologram/FGFactoryBuildingHologram.h"  // Issue #160: Zoop detection
#include "SFHologramPerformanceProfiler.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
#include "Config/Smart_ConfigStruct.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Input/FGEnhancedInputComponent.h"
#include "Input/FGInputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Resources/FGBuildingDescriptor.h"
#include "FGCentralStorageSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"
#include "FGGameState.h"
#include "FGHUDBase.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "Components/SplineComponent.h"

// Blueprint proxy for Smart Dismantle grouping (Issue #166)
#include "FGBlueprintProxy.h"
#include "FGBlueprintHologram.h"

// Build gun integration (Phase 4)
// Hologram adapters
#include "Holograms/Adapters/ISFHologramAdapter.h"

// Smart custom hologram classes (Phase 3)
#include "Holograms/Core/SFBuildableHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Core/ASFLogisticsHologram.h"
#include "Holograms/Core/SFFoundationHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Holograms/Core/SFStandaloneSignChildHologram.h"

// Auto-Connect belt building state (file scope for cross-function access)
static bool bProcessingGridPlacement = false;

// Smart custom hologram adapters (Phase 3)
#include "Holograms/Adapters/SFSmartBuildableAdapter.h"
#include "Holograms/Adapters/SFSmartFactoryAdapter.h"
#include "Holograms/Adapters/SFSmartLogisticsAdapter.h"
#include "Holograms/Adapters/SFSmartFoundationAdapter.h"

// Vanilla hologram adapters (fallback)
#include "Holograms/Adapters/SFGenericAdapter.h"
#include "Holograms/Adapters/SFWallAdapter.h"
#include "Holograms/Adapters/SFPillarAdapter.h"
#include "Holograms/Adapters/SFWaterExtractorAdapter.h"
#include "Holograms/Adapters/SFResourceExtractorAdapter.h"
#include "Holograms/Adapters/SFFactoryAdapter.h"
#include "Holograms/Adapters/SFElevatorAdapter.h"
#include "Holograms/Adapters/SFRampAdapter.h"
#include "Holograms/Adapters/SFJumpPadAdapter.h"
#include "Holograms/Adapters/SFUnsupportedAdapter.h"
#include "Holograms/Adapters/SFPassthroughAdapter.h"

// Smart custom hologram classes (Phase 3)
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"

// Input registry (needed for ClearInputCache and GetSmartInputMappingContext)
#include "Input/SFInputRegistry.h"

// Feature modules
#include "Features/Scaling/SFScalingModule.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/Spacing/SFSpacingModule.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGBuildablePolePipe.h"  // For stackable pipeline support auto-connect

// Satisfactory hologram types for adapter factory
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGWallHologram.h"
#include "Hologram/FGPillarHologram.h"
#include "Hologram/FGStackableStorageHologram.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "Hologram/FGPipeHyperAttachmentHologram.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Hologram/FGElevatorHologram.h"
#include "Hologram/FGStairHologram.h"
#include "Hologram/FGJumpPadHologram.h"
#include "Hologram/FGWheeledVehicleHologram.h"
#include "Hologram/FGSpaceElevatorHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Buildables/FGBuildablePassthrough.h"

// Recipe copying system includes
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGRecipe.h"

// ========================================
// File-scope static cache for stackable belt build data (Issue #220)
// Shared between CacheStackableBeltPreviewsForBuild() and OnActorSpawned()
// ========================================
namespace
{
	struct FStackableBeltBuildData
	{
		TArray<FSplinePointData> SplineData;
		TWeakObjectPtr<UFGFactoryConnectionComponent> OutputConnector;
		TWeakObjectPtr<UFGFactoryConnectionComponent> InputConnector;
		int32 BeltTier = 0;
	};

	TArray<FStackableBeltBuildData> GCachedStackableBeltData;
	bool bGStackableBeltDataCached = false;
}

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

	UE_LOG(LogSmartFoundations, Log,
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
				UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND: No PlayerController available for line trace"));
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
		UE_LOG(LogSmartFoundations, Log, TEXT("[SmartRestore][Extend] %s axis changed, grid=[%d,%d,%d], liveExtend=%d restoredExtend=%d"),
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

// ========================================
// Hologram Lock Ownership Helpers (Task 52)
// ========================================

bool USFSubsystem::TryAcquireHologramLock()
{
    if (!ActiveHologram.IsValid())
    {
        return false;
    }

    // Only lock if not already locked by vanilla hold system
    if (!ActiveHologram->IsHologramLocked())
    {
        bLockedByModifier = true;
        ActiveHologram->LockHologramPosition(true);
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔒 Smart! acquired hologram lock"));
        return true;
    }
    else
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔒 Hologram already locked (vanilla hold owns it)"));
        return false;
    }
}

void USFSubsystem::TryReleaseHologramLock()
{
    // Only unlock if WE locked it AND no other Smart! modes are active
    // Issue #273: Don't release if auto-hold is active — that lock is managed by
    // ApplyAxisScaling (grid changes) and PollForActiveHologram (user override detection)
    if (bLockedByModifier && !bAutoHoldActive && !IsAnyModalFeatureActive() && ActiveHologram.IsValid())
    {
        bLockedByModifier = false;
        ActiveHologram->LockHologramPosition(false);
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔓 Smart! released hologram lock"));
    }
}

bool USFSubsystem::IsAnyModalFeatureActive() const
{
    // Phase 0: Forward to InputHandler module (Task #61.6)
    if (InputHandler)
    {
        // Also check EXTEND mode which is managed by the ExtendService
        return InputHandler->IsAnyModalFeatureActive() || IsExtendModeActive();
    }

    // Fallback if module not initialized
    return bModifierScaleXActive || bModifierScaleYActive ||
           bSpacingModeActive || bStepsModeActive || bStaggerModeActive ||
           bRecipeModeActive || bAutoConnectSettingsModeActive || bRotationModeActive ||
           IsExtendModeActive();
}

FVector USFSubsystem::GetFurthestTopHologramPosition() const
{
    // If no active hologram, return zero vector
    if (!ActiveHologram.IsValid())
    {
        return FVector::ZeroVector;
    }

    // Get current grid counters
    const FIntVector& GridCounters = GetGridCounters();

    // If grid is 1x1x1 (no extension), return parent position
    if (GridCounters.X == 1 && GridCounters.Y == 1 && GridCounters.Z == 1)
    {
        return ActiveHologram->GetActorLocation();
    }

    // Calculate furthest grid position based on direction
    // The "furthest top" is at the corner furthest from the parent (X,Y)
    // and at the TOP Z level (highest)
    int32 FurthestX, FurthestY, TopZ;

    // Issue #275: Scaled Extend axis mapping
    // Extend clones are placed along the building's Y axis (via RightVector).
    // GridCounters.X = clone count (always positive, from FMath::Abs).
    // GridCounters.Y = row count (perpendicular to extend, along building's X axis).
    // CalculateChildPosition maps X param → building X, Y param → building Y.
    // So for Scaled Extend we must swap: clone count → FurthestY, row count → FurthestX.
    const bool bScaledExtend = ExtendService && ExtendService->IsScaledExtendActive();

    if (bScaledExtend)
    {
        ESFExtendDirection ExtendDir = ExtendService->GetExtendDirection();
        int32 CloneCount = FMath::Abs(GridCounters.X);

        // Clones extend along building's Y axis (RightVector)
        FurthestY = CloneCount - 1;
        if (ExtendDir == ESFExtendDirection::Left)
        {
            FurthestY = -FurthestY;  // Left = negative Y
        }

        // Rows extend along building's X axis (perpendicular to extend)
        FurthestX = GridCounters.Y > 0 ? GridCounters.Y - 1 : 0;

        // Scaled Extend doesn't use Z scaling
        TopZ = 0;
    }
    else
    {
        // Normal grid mode: counters directly map to axes
        // X direction
        if (GridCounters.X > 0)
        {
            FurthestX = GridCounters.X - 1;
        }
        else if (GridCounters.X < 0)
        {
            FurthestX = -(FMath::Abs(GridCounters.X) - 1);
        }
        else
        {
            FurthestX = 0;
        }

        // Y direction
        if (GridCounters.Y > 0)
        {
            FurthestY = GridCounters.Y - 1;
        }
        else if (GridCounters.Y < 0)
        {
            FurthestY = -(FMath::Abs(GridCounters.Y) - 1);
        }
        else
        {
            FurthestY = 0;
        }

        // Z direction
        if (GridCounters.Z > 0)
        {
            TopZ = GridCounters.Z - 1;
        }
        else if (GridCounters.Z < 0)
        {
            TopZ = -(FMath::Abs(GridCounters.Z) - 1);
        }
        else
        {
            TopZ = 0;
        }
    }

    // Get parent transform and item size
    FVector ParentLocation = ActiveHologram->GetActorLocation();
    FRotator ParentRotation = ActiveHologram->GetActorRotation();
    FVector ItemSize = GetCachedBuildingSize();

    // Calculate world position using PositionCalculator
    if (FSFPositionCalculator* PosCalc = GetPositionCalculator())
    {
        return PosCalc->CalculateChildPosition(
            FurthestX,
            FurthestY,
            TopZ,
            ParentLocation,
            ParentRotation,
            ItemSize,
            GetCounterState(),
            0, // Grid index not needed for position calculation
            FVector::ZeroVector // No anchor offset for position query
        );
    }

    // Fallback: return parent position if PositionCalculator is unavailable
    return ActiveHologram->GetActorLocation();
}

// Note: ToggleExtendMode() removed - EXTEND is now AUTOMATIC
// Activates when pointing at a compatible building of the same type

void USFSubsystem::OnSpacingModeChanged(const FInputActionValue& Value)
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnSpacingModeChanged(Value);
        // Sync state from module
        bSpacingModeActive = InputHandler->IsSpacingModeActive();
    }
    else
    {
        bSpacingModeActive = Value.Get<bool>();
    }
    if (bSpacingModeActive)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("\U0001F527 Spacing Mode: ACTIVE (Semicolon held) - Current axis: %s"),
            *UEnum::GetValueAsString(CounterState.SpacingAxis));
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Counters: X=%d cm, Y=%d cm, Z=%d cm"),
            CounterState.SpacingX, CounterState.SpacingY, CounterState.SpacingZ);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("\U0001F527 Spacing Mode: Inactive (Semicolon released)"));

        // Try to release lock (Task 52 - centralized)
        TryReleaseHologramLock();
    }
}

void USFSubsystem::OnSpacingCycleAxis()
{
    // Legacy handler - redirects to generic context-aware handler
    // Kept for backward compatibility with existing blueprint bindings
    OnCycleAxis();
}

void USFSubsystem::OnStepsModeChanged(const FInputActionValue& Value)
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnStepsModeChanged(Value);
        // Sync state from module
        bStepsModeActive = InputHandler->IsStepsModeActive();
    }
    else
    {
        bStepsModeActive = Value.Get<bool>();
    }
    if (bStepsModeActive)
    {
        // X-axis: Columns (constant X) step up based on X position
        // Y-axis: Rows (constant Y) step up based on Y position
        const TCHAR* AxisName = (CounterState.StepsAxis == ESFScaleAxis::X) ? TEXT("X (columns)") : TEXT("Y (rows)");
        const int32 CurrentCounter = (CounterState.StepsAxis == ESFScaleAxis::X) ? CounterState.StepsX : CounterState.StepsY;

        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 Steps Mode: ACTIVE (I held) - Axis: %s"), AxisName);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current counter: %d units (Num0 to toggle X/Y)"), CurrentCounter);

        // Try to acquire lock (Task 52 - centralized)
        TryAcquireHologramLock();
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 Steps Mode: Inactive (I released)"));

        // Try to release lock (Task 52 - centralized)
        TryReleaseHologramLock();
    }
}

void USFSubsystem::OnCycleAxis()
{
    // Phase 0: Delegate input processing to InputHandler (Task #61.6)
    if (InputHandler)
    {
        InputHandler->OnCycleAxis();
    }

    // Context-aware mode swapper - dispatches based on active mode
    // Allows Num0 to work intelligently across all modal features

    if (bAutoConnectSettingsModeActive)
    {
        // Auto-Connect Settings mode: Cycle to next setting
        CycleAutoConnectSetting();
    }
    else if (bSpacingModeActive)
    {
        // Spacing mode: Cycle spacing axis X → Y → Z via service
        if (GridStateService)
        {
            GridStateService->CycleSpacingAxis(CounterState);
        }

        const TCHAR* AxisName = TEXT("");
        switch (CounterState.SpacingAxis)
        {
        case ESFScaleAxis::X: AxisName = TEXT("X"); break;
        case ESFScaleAxis::Y: AxisName = TEXT("Y"); break;
        case ESFScaleAxis::Z: AxisName = TEXT("Z"); break;
        default: break;
        }
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Spacing Axis Cycled: Now adjusting %s-axis"), AxisName);
        UpdateCounterState(CounterState);
    }
    else if (bStepsModeActive)
    {
        // Steps mode: Toggle between X and Y only via service
        if (GridStateService)
        {
            GridStateService->ToggleStepsAxis(CounterState);
        }

        const TCHAR* AxisName = (CounterState.StepsAxis == ESFScaleAxis::X) ? TEXT("X") : TEXT("Y");
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Steps Axis Toggled: Now adjusting %s-axis (columns/rows)"), AxisName);
        UpdateCounterState(CounterState);
    }
    else if (bStaggerModeActive)
    {
        // Stagger mode: Cycle X → Y → ZX → ZY via service
        if (GridStateService)
        {
            GridStateService->CycleStaggerAxis(CounterState);
        }

		const TCHAR* AxisName = TEXT("Unknown");
		const TCHAR* AxisDescription = TEXT("");

		switch (CounterState.StaggerAxis)
		{
		case ESFScaleAxis::X:
			AxisName = TEXT("X");
			AxisDescription = TEXT("horizontal diagonal - sideways offset");
			break;
		case ESFScaleAxis::Y:
			AxisName = TEXT("Y");
			AxisDescription = TEXT("horizontal diagonal - forward/back offset");
			break;
		case ESFScaleAxis::ZX:
			AxisName = TEXT("ZX");
			AxisDescription = TEXT("vertical lean - forward/back");
			break;
		case ESFScaleAxis::ZY:
			AxisName = TEXT("ZY");
			AxisDescription = TEXT("vertical lean - sideways");
			break;
		default:
			break;
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Stagger Axis Toggled: Now adjusting %s-axis (%s)"), AxisName, AxisDescription);
		UpdateCounterState(CounterState);
	}
	else if (bRecipeModeActive)
	{
		// Recipe mode: Clear manual selection completely
		if (ActiveRecipeSource == ERecipeSource::ManuallySelected)
		{
			// Clear manual selection
			ActiveRecipeSource = ERecipeSource::None;
			ActiveRecipe = nullptr;
			CurrentRecipeIndex = 0;

			// Apply recipe clear to parent hologram
			ApplyRecipeToParentHologram();

			// Trigger debounced regeneration to propagate clear to children
			if (ActiveHologram.IsValid() && ActiveHologram->GetHologramChildren().Num() > 0)
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
					World->GetTimerManager().SetTimer(
						RecipeRegenerationTimer,
						this,
						&USFSubsystem::RegenerateChildHologramGrid,
						0.2f,  // 200ms debounce
						false
					);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe cleared - scheduled child regeneration in 200ms"));
				}
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe selection cleared - no recipe will be applied to parent or children"));
		}
		else
		{
			// No manual selection to clear, just clear everything
			ClearAllRecipes();

			// Apply clear to hologram registry and trigger regeneration
			ApplyRecipeToParentHologram();
			if (ActiveHologram.IsValid() && ActiveHologram->GetHologramChildren().Num() > 0)
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(RecipeRegenerationTimer);
					World->GetTimerManager().SetTimer(
						RecipeRegenerationTimer,
						this,
						&USFSubsystem::RegenerateChildHologramGrid,
						0.2f,  // 200ms debounce
						false
					);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ All recipes cleared - scheduled child regeneration in 200ms"));
				}
			}
		}

		UpdateCounterDisplay();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe selection cleared via Num0"));
	}
	else
	{
		// No mode active - check for double-tap to toggle Smart disable (Issue #198)
		// Only works when no modifier keys are held
		if (!bModifierScaleXActive && !bModifierScaleYActive)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			const double TimeSinceLastTap = CurrentTime - LastCycleAxisTapTime;

			if (TimeSinceLastTap < DoubleTapWindow && LastCycleAxisTapTime > 0.0)
			{
				// Double-tap detected - toggle the disable flags
				bDisableSmartForNextAction = !bDisableSmartForNextAction;
				bExtendDisabledForSession = !bExtendDisabledForSession;  // Issue #257: toggle Extend too

				if (bDisableSmartForNextAction)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("⏸️ Smart! DISABLED for session (Auto-Connect + Extend) (double-tap detected)"));

					// Issue #198: Immediately clear all auto-connect previews when disabling
					if (ActiveHologram.IsValid() && AutoConnectService)
					{
						USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
						if (Orchestrator)
						{
							Orchestrator->ForceRefresh();
						}
					}

					// Issue #257: Clear active Extend state when disabling
					if (ExtendService)
					{
						ExtendService->ClearExtendState();
					}
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("▶️ Smart! RE-ENABLED (Auto-Connect + Extend) (double-tap detected)"));

					// Re-enable: trigger refresh to recreate previews
					if (ActiveHologram.IsValid() && AutoConnectService)
					{
						USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
						if (Orchestrator)
						{
							Orchestrator->ForceRefresh();
						}
					}
				}

				// Reset timing to prevent triple-tap from toggling again
				LastCycleAxisTapTime = 0.0;

				// Update HUD to show the change
				UpdateCounterDisplay();
			}
			else
			{
				// First tap - record time for potential double-tap
				LastCycleAxisTapTime = CurrentTime;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Cycle axis tap recorded (no mode active) - waiting for potential double-tap"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Cycle axis ignored - modifier keys held"));
		}
	}
}

void USFSubsystem::OnStaggerModeChanged(const FInputActionValue& Value)
{
	// Block stagger mode while EXTEND is active
	if (IsExtendModeActive())
	{
		return;
	}

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnStaggerModeChanged(Value);
		// Sync state from module
		bStaggerModeActive = InputHandler->IsStaggerModeActive();
	}
	else
	{
		bStaggerModeActive = Value.Get<bool>();
	}
	if (bStaggerModeActive)
	{
		// Stagger only uses X and Y axes (lateral grid offset)
		const TCHAR* AxisName = (CounterState.StaggerAxis == ESFScaleAxis::X) ? TEXT("X (sideways curve)") : TEXT("Y (forward curve)");
		const int32 CurrentCounter = (CounterState.StaggerAxis == ESFScaleAxis::X) ? CounterState.StaggerX : CounterState.StaggerY;

		UE_LOG(LogSmartFoundations, Warning, TEXT(" Stagger Mode: ACTIVE (Y held) - Axis: %s"), AxisName);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current counter: %d units (Num0 to toggle X/Y)"), CurrentCounter);

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		// Try to release lock (Task 52 - centralized)
		TryReleaseHologramLock();
	}
}

void USFSubsystem::OnRotationModeChanged(const FInputActionValue& Value)
{
	// Scaled Extend (Issue #265): Allow rotation mode during extend.

	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnRotationModeChanged(Value);
		// Sync state from module
		bRotationModeActive = InputHandler->IsRotationModeActive();
	}
	else
	{
		bRotationModeActive = Value.Get<bool>();
	}
	if (bRotationModeActive)
	{
		// Rotation currently only uses Z axis (horizontal arc)
		// Phase 2 will add X and Y axes for vertical arches
		UE_LOG(LogSmartFoundations, Warning, TEXT(" Rotation Mode: ACTIVE (Comma held) - Axis: Z (horizontal arc)"));
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current rotation: %.1f° (Num0 to cycle axis when Phase 2 adds X/Y)"), CounterState.RotationZ);

		// Try to acquire lock (Task 52 - centralized)
		TryAcquireHologramLock();
	}
	else
	{
		// Try to release lock (Task 52 - centralized)
		TryReleaseHologramLock();
	}
}

void USFSubsystem::OnToggleArrows()
{
	// Phase 0: Delegate input processing to InputHandler (Task #61.6)
	if (InputHandler)
	{
		InputHandler->OnToggleArrows();
	}

#if SMART_ARROWS_ENABLED
	// Toggle runtime visibility flag (doesn't change config)
	bArrowsRuntimeVisible = !bArrowsRuntimeVisible;
	bArrowsVisible = bArrowsRuntimeVisible; // Sync deprecated flag
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Arrows TOGGLE - Visible: %s (Config default: %s)"),
		bArrowsRuntimeVisible ? TEXT("ON") : TEXT("OFF"),
		CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"));

	// Force immediate arrow update to handle visibility change
	// This ensures arrows appear/disappear instantly without waiting for hologram movement
	if (ActiveHologram.IsValid() && CurrentAdapter && CurrentAdapter->IsValid() && ArrowModule)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FTransform CurrentTransform = CurrentAdapter->GetBaseTransform();

			if (bArrowsRuntimeVisible)
			{
				// Force redraw when enabling - show arrows immediately
				ArrowModule->UpdateArrows(World, CurrentTransform, LastAxisInput, true);
				UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrows: Forced redraw on visibility enable"));
			}
			else
			{
				// Force clear when disabling - hide arrows immediately by calling with bVisible=false
				ArrowModule->UpdateArrows(World, CurrentTransform, LastAxisInput, false);
				UE_LOG(LogSmartFoundations, Verbose, TEXT("Arrows: Forced clear on visibility disable"));
			}

			// Update cache to prevent immediate redraw in next Tick
			LastHologramTransform = CurrentTransform;
			LastKnownAxisInput = LastAxisInput;
			LastChildCount = 0;  // Reset child count when toggling visibility
		}
	}
#else
	UE_LOG(LogSmartFoundations, Warning, TEXT("Arrows feature is currently disabled (SMART_ARROWS_ENABLED=0)"));
#endif

	// DEBUG: Comprehensive manifold scan within 50m radius
	// Captures chain actors, connections, splines, and all properties for research
	if (RadarPulseService)
	{
		const float ScanRadius = 5000.0f;   // 50m = 5000cm
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📡 [Arrow Toggle Debug] Comprehensive scan within 50m..."));

		// 1. HOLOGRAM SCAN - Inspect all holograms in range (captures mesh paths!)
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📡 Scanning for holograms..."));
		RadarPulseService->InspectAllHologramsInRadius(ScanRadius);

		// 2. Chain actor scan (chain boundaries, members, connections)
		RadarPulseService->ScanChainActors(ScanRadius);

		// 3. Full radar pulse snapshot (all buildables with properties)
		// Filter to logistics and factory categories for manifold research
		TArray<FString> ManifoldCategories;
		ManifoldCategories.Add(TEXT("Belt"));
		ManifoldCategories.Add(TEXT("Lift"));
		ManifoldCategories.Add(TEXT("Distributor"));
		ManifoldCategories.Add(TEXT("Factory"));
		ManifoldCategories.Add(TEXT("Pipe"));
		ManifoldCategories.Add(TEXT("Junction"));
		ManifoldCategories.Add(TEXT("StackablePipePole"));
		ManifoldCategories.Add(TEXT("StackableBeltPole"));

		FSFRadarPulseSnapshot Snapshot = RadarPulseService->CaptureSnapshotFiltered(
			ScanRadius,
			TEXT("MANIFOLD_RESEARCH"),
			ManifoldCategories
		);

		// Log with verbose details and enumerate all categories
		RadarPulseService->LogSnapshotFiltered(Snapshot, true, ManifoldCategories);
	}
}

void USFSubsystem::OnToggleSettingsForm()
{
	UE_LOG(LogSmartFoundations, Warning, TEXT("!!! K KEY PRESSED !!! (OnToggleSettingsForm)"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Settings Form Toggle (Phase 0 validation)"));

	// Route to Upgrade Panel if holding an upgrade-capable hologram (belt/lift/pipe/pump/power pole/wall outlet)
	if (IsUpgradeCapableContext())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Routing to Upgrade Panel (upgrade-capable context)"));
		OnToggleUpgradePanel();
		return;
	}

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: No valid PlayerController available"));
		return;
	}

	// Toggle behavior: if widget is open, close it (with cancel/revert)
	if (SettingsFormWidget.IsValid() && SettingsFormWidget->IsInViewport())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Closing via toggle key (canceling unapplied changes)"));

		// Use CancelAndClose to revert unapplied changes before closing
		SettingsFormWidget->CancelAndClose();
		SettingsFormWidget.Reset();
		return;
	}

	// Load the Blueprint Widget class
	FString WidgetPath = TEXT("/SmartFoundations/SmartFoundations/UI/Smart_SettingsForm_Widget.Smart_SettingsForm_Widget_C");
	FSoftClassPath WidgetClassPath(WidgetPath);
	UClass* WidgetClass = WidgetClassPath.TryLoadClass<UUserWidget>();

	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance and add to viewport
	USmartSettingsFormWidget* WidgetInstance = CreateWidget<USmartSettingsFormWidget>(PC, WidgetClass);

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: Failed to create widget instance"));
		return;
	}

	// Store reference for toggle behavior
	SettingsFormWidget = WidgetInstance;

	// Suppress HUD while settings form is open
	if (HudService)
	{
		HudService->SetHUDSuppressed(true);
	}

	// Populate form with current counter state values
	WidgetInstance->PopulateFromCounterState(this);

	// Set visibility and add to viewport
	WidgetInstance->SetVisibility(ESlateVisibility::Visible);
	WidgetInstance->AddToViewport(100);

	// Enable mouse cursor and UI-only input mode (blocks game input to prevent click-through)
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(WidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}

bool USFSubsystem::IsUpgradeCapableContext() const
{
    AFGHologram* Hologram = ActiveHologram.Get();

    // Fallback: If the 100ms poll hasn't fired yet or cache is stale, check the Build Gun directly
    if (!Hologram)
    {
        AFGPlayerController* PC = GetLastController();
        if (PC)
        {
            if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetCharacter()))
            {
                if (AFGBuildGun* BuildGun = Character->GetBuildGun())
                {
                    if (BuildGun->IsInState(EBuildGunState::BGS_BUILD))
                    {
                        if (UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState()))
                        {
                            Hologram = BuildState->GetHologram();
                        }
                    }
                }
            }
        }
    }

    if (!Hologram)
    {
        return false;
    }

    FString ClassName = Hologram->GetClass()->GetName();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("IsUpgradeCapableContext: Checking hologram class: %s"), *ClassName);

    // Belt holograms (conveyor belts) - e.g. Holo_ConveyorBelt_C
    if (ClassName.Contains(TEXT("ConveyorBelt")) && !ClassName.Contains(TEXT("Lift")))
    {
        return true;
    }

    // Lift holograms (conveyor lifts) - e.g. Holo_ConveyorLift_C
    if (ClassName.Contains(TEXT("ConveyorLift")) || ClassName.Contains(TEXT("Lift")))
    {
        return true;
    }

    // Pipeline holograms (pipes)
    if (ClassName.Contains(TEXT("Pipeline")) && !ClassName.Contains(TEXT("Junction")) && !ClassName.Contains(TEXT("Pump")) && !ClassName.Contains(TEXT("Support")))
    {
        return true;
    }

    // Power line/wire holograms - use for power network upgrades instead of power poles
    // This avoids conflict with power pole (and wall outlet) scaling feature
    if (ClassName.Contains(TEXT("Wire")) || ClassName.Contains(TEXT("PowerLine")))
    {
        return true;
    }

    // Note: Pipeline Pumps, Power Poles, and Wall Outlets intentionally return false here
    // so that the Smart! Panel opens instead, allowing players to use grid scaling.

    return false;
}

void USFSubsystem::OnToggleUpgradePanel()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("INPUT EVENT: Upgrade Panel Toggle"));

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: No valid PlayerController available"));
		return;
	}

	// Toggle behavior: if widget is open, close it
	if (UpgradePanelWidget.IsValid() && UpgradePanelWidget->IsInViewport())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Closing via toggle key"));
		UpgradePanelWidget->RemoveFromParent();
		UpgradePanelWidget.Reset();

		// Restore game input mode
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		// Re-enable HUD
		if (HudService)
		{
			HudService->SetHUDSuppressed(false);
		}
		return;
	}

	// Load the Upgrade Panel Blueprint Widget class
	FString WidgetPath = TEXT("/SmartFoundations/SmartFoundations/UI/Smart_UpgradePanel_Widget.Smart_UpgradePanel_Widget_C");
	FSoftClassPath WidgetClassPath(WidgetPath);
	UClass* WidgetClass = WidgetClassPath.TryLoadClass<UUserWidget>();

	if (!WidgetClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Failed to load Blueprint widget class at path: %s"), *WidgetPath);
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Loaded widget class: %s"), *WidgetClass->GetName());

	// Create widget instance - try our C++ class first, fall back to UUserWidget if Blueprint not reparented
	USmartUpgradePanel* SmartPanelInstance = CreateWidget<USmartUpgradePanel>(PC, WidgetClass);
	UUserWidget* WidgetInstance = SmartPanelInstance;

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: CreateWidget<USmartUpgradePanel> failed, trying UUserWidget fallback"));
		// Blueprint may not be reparented yet - fall back to base UUserWidget
		WidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	}

	if (!WidgetInstance)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Failed to create widget instance - Blueprint may need to be reparented to UserWidget in Unreal Editor"));
		UE_LOG(LogSmartFoundations, Error, TEXT("Upgrade Panel: Open Smart_UpgradePanel_Widget in UE Editor, go to Class Settings, and set Parent Class to UserWidget or SmartUpgradePanel"));
		return;
	}

	// Store reference for toggle behavior
	UpgradePanelWidget = WidgetInstance;

	// If using fallback UUserWidget, manually bind the close button
	if (!SmartPanelInstance)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Using fallback mode - manually binding close button"));
		if (UButton* CloseButton = Cast<UButton>(WidgetInstance->GetWidgetFromName(TEXT("CloseButton"))))
		{
			CloseButton->OnClicked.AddDynamic(this, &USFSubsystem::OnUpgradePanelCloseClicked);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: CloseButton bound successfully"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: CloseButton not found in widget"));
		}
	}

	// Suppress HUD while upgrade panel is open
	if (HudService)
	{
		HudService->SetHUDSuppressed(true);
	}

	// Set visibility and add to viewport
	WidgetInstance->SetVisibility(ESlateVisibility::Visible);
	WidgetInstance->AddToViewport(100);

	// Enable mouse cursor and UI-only input mode (blocks game input to prevent click-through)
	PC->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(WidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	// Set widget to be focusable so it can receive keyboard input
	WidgetInstance->SetIsFocusable(true);
	WidgetInstance->SetKeyboardFocus();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Opened (ESC or Close button to close)"));
}

void USFSubsystem::OnUpgradePanelCloseClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Close button clicked (fallback handler)"));

	if (!UpgradePanelWidget.IsValid())
	{
		return;
	}

	// Get player controller
	AFGPlayerController* PC = GetLastController();
	if (!PC)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			PC = World->GetFirstPlayerController<AFGPlayerController>();
		}
	}

	// Remove widget from viewport
	UpgradePanelWidget->RemoveFromParent();
	UpgradePanelWidget.Reset();

	// Restore game input mode
	if (PC)
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	// Re-enable HUD
	if (HudService)
	{
		HudService->SetHUDSuppressed(false);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Closed via close button"));
}

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

		UE_LOG(LogSmartFoundations, Log,
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
	// If custom hologram behavior is needed in the future, use Blueprint class remapping at module load.
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
		UE_LOG(LogSmartFoundations, Log,
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
			UE_LOG(LogSmartFoundations, Log,
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
			UE_LOG(LogSmartFoundations, Log,
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Parent: %s"), SmartContext->mParentContext.IsValid() ? *SmartContext->mParentContext->GetName() : TEXT("None"));
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Parent: %s"), SmartContext->mParentContext.IsValid() ? *SmartContext->mParentContext->GetName() : TEXT("None"));
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

// ========================================
// RPC Handler Methods (Called by SFRCO)
// ========================================

void USFSubsystem::ApplyScalingFromRPC(AFGHologram* HologramActor, uint8 Axis, int32 Delta, int32 NewCounter)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Axis=%d, Delta=%d, Counter=%d, Hologram=%s"),
		Axis, Delta, NewCounter, *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[RPC] ApplyScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Validate axis
	if (Axis > 2)
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[RPC] ApplyScalingFromRPC: Invalid axis %d"), Axis);
		return;
	}

	// Calculate scaling delta from counter value
	// Counter represents cumulative steps; Delta is the increment direction
	const float StepSize = ScaleStepSize * Delta;
	FVector ScalingDelta = FVector::ZeroVector;

	switch (Axis)
	{
		case 0: // X axis
			ScalingDelta.X = StepSize;
			break;
		case 1: // Y axis
			ScalingDelta.Y = StepSize;
			break;
		case 2: // Z axis
			ScalingDelta.Z = StepSize;
			break;
	}

	// Apply the scaling
	ApplyScalingToHologram(ScalingDelta);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Applied delta %s, TotalOffset now %s"),
		*ScalingDelta.ToString(), *CurrentScalingOffset.ToString());

	// TODO Task #21: Replicate to other clients via multicast or property replication
}

void USFSubsystem::ResetScalingFromRPC(AFGHologram* HologramActor)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Hologram=%s"), *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[RPC] ResetScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Reset scaling offset
	CurrentScalingOffset = FVector::ZeroVector;

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Scaling offset cleared"));

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetSpacingModeFromRPC(ESFSpacingMode NewMode)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: NewMode=%d"), static_cast<uint8>(NewMode));

	// Update spacing mode in CounterState
	CounterState.SpacingMode = NewMode;

	FString ModeName = FSFSpacingModule::GetSpacingModeName(CounterState.SpacingMode);
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: Mode set to %s"), *ModeName);

	// Sync via centralized update
	UpdateCounterState(CounterState);

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetArrowVisibilityFromRPC(bool bVisible)
{
#if SMART_ARROWS_ENABLED
    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Visible=%d"), bVisible);

    // Update arrow runtime visibility state - Tick() will handle drawing
    bArrowsRuntimeVisible = bVisible;
    bArrowsVisible = bVisible; // Sync deprecated flag

    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Arrows visibility set to %s"),
        bVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
#else
    UE_LOG(LogSmartFoundations, Warning,
        TEXT("[RPC] SetArrowVisibilityFromRPC ignored - arrows feature disabled"));
#endif

    // TODO Task #21: Replicate to other clients
}

FString USFSubsystem::GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipeDisplayName(Recipe);
	}
	return TEXT("None");
}

UTexture2D* USFSubsystem::GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipePrimaryProductIcon(Recipe);
	}
	return nullptr;
}

FString USFSubsystem::GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithInputsOutputs(Recipe);
    }
    return TEXT("None");
}

void USFSubsystem::GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const
{
    if (RecipeManagementService)
    {
        RecipeManagementService->GetRecipeDisplayInfo(OutCurrentIndex, OutTotalRecipes);
    }
    else
    {
        OutCurrentIndex = 0;
        OutTotalRecipes = 0;
    }
}

TSubclassOf<UFGRecipe> USFSubsystem::GetActiveRecipe() const
{
    return RecipeManagementService ? RecipeManagementService->GetActiveRecipe() : nullptr;
}

FString USFSubsystem::GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithIngredient(Recipe);
	}
	return TEXT("None");
}

void USFSubsystem::UpdateCounterDisplay()
{
    // Build formatted strings (Phase B): route through HUD service
    TPair<FString, FString> DisplayLines = HudService
        ? HudService->BuildCounterDisplayLines()
        : TPair<FString, FString>(FString(), FString());
    const FString& FirstLine = DisplayLines.Key;
    const FString& SecondLine = DisplayLines.Value;

    // Log for debugging
    if (FirstLine.Len() > 0 || SecondLine.Len() > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Counter Display] Line1: '%s' Line2: '%s'"),
            *FirstLine, *SecondLine);
    }

    // Update HUD via service
    if (HudService)
    {
        HudService->UpdateWidgetDisplay(FirstLine, SecondLine);
    }
}

// ========================================
// Configuration System (Task 69)
// ========================================

void USFSubsystem::LoadConfiguration()
{
	// Try to load configuration from SML config system
	// Note: ConfigManager might not be available during early initialization
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: No world context, using defaults"));
		// Use defaults - config will be loaded later if needed
		bArrowsRuntimeVisible = true;
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: No game instance, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ LoadConfiguration: ConfigManager not available yet, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	// ConfigManager is available, load config
	CachedConfig = FSmart_ConfigStruct::GetActiveConfig(this);

	// Initialize runtime arrow visibility from config (Issue #146)
	// This can be toggled during runtime with Num1, but starts with config value
	bArrowsRuntimeVisible = CachedConfig.bShowArrows;

	// Initialize arrow orbit and label toggles from config (Issue #179)
	if (ArrowModule)
	{
		ArrowModule->SetOrbitEnabled(CachedConfig.bShowArrowOrbit);
		ArrowModule->SetLabelsVisible(CachedConfig.bShowArrowLabels);
	}

	// Issue #257: Initialize Extend enabled state from config
	bExtendEnabledByConfig = CachedConfig.bExtendEnabled;

	bConfigLoaded = true;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Smart! Configuration loaded:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowHUD: %s"), CachedConfig.bShowHUD ? TEXT("true") : TEXT("false"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowArrows: %s (runtime: %s, orbit: %s, labels: %s)"),
		CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"),
		bArrowsRuntimeVisible ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowOrbit ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowLabels ? TEXT("true") : TEXT("false"));
}

void USFSubsystem::CleanupStateForWorldTransition()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Resetting recipe inheritance state"));

	// Clear stored recipe and cache
	StoredProductionRecipe = nullptr;
	StoredRecipeDisplayName = TEXT("");
	bHasStoredProductionRecipe = false;

	// Clear hologram registry to prevent stale entries
	USFHologramDataRegistry::ClearRegistry();

	// Update HUD to reflect cleared state
	UpdateCounterDisplay();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Recipe inheritance state reset complete"));
}

bool USFSubsystem::AbortRestoreSession(const TCHAR* Reason)
{
	const bool bHadRestoredTopology = ExtendService && ExtendService->IsRestoredCloneTopologyActive();
	const bool bHadRestoreSession = RestoreService && RestoreService->IsRestoreSessionActive();
	if (!bHadRestoredTopology && !bHadRestoreSession)
	{
		return false;
	}

	if (bHadRestoredTopology)
	{
		ExtendService->ClearRestoredCloneTopologySession(Reason);
	}

	if (bHadRestoreSession)
	{
		RestoreService->ClearActiveRestoreSession(Reason);
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("[SmartRestore] Aborted restore session: reason=%s topology=%d session=%d"),
		Reason ? Reason : TEXT("Unknown"),
		bHadRestoredTopology ? 1 : 0,
		bHadRestoreSession ? 1 : 0);

	UpdateCounterDisplay();
	return true;
}

void USFSubsystem::InitializeHologramCleanup()
{
	if (UWorld* World = GetWorld())
	{
		// Bind to OnActorDestroyed delegate to automatically clean up hologram data
		// This ensures deterministic cleanup for any hologram destruction scenario
		World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &USFSubsystem::OnActorDestroyed));
		UE_LOG(LogSmartFoundations, Log, TEXT("✅ Initialized Hologram Cleanup: Bound to OnActorDestroyed delegate."));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT(" Failed to Initialize Hologram Cleanup: World is null."));
	}
}

void USFSubsystem::OnActorDestroyed(AActor* DestroyedActor)
{
	// Filter for holograms only (deterministic cleanup)
	if (AFGHologram* Hologram = Cast<AFGHologram>(DestroyedActor))
	{
		USFHologramDataRegistry::ClearData(Hologram);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("OnActorDestroyed: Auto-cleared hologram %s from registry"),
			*GetNameSafe(Hologram));

		// Clean up auto-connect previews when hologram is destroyed (covers Escape, right-click, etc.)
		if (ActiveHologram.IsValid() && ActiveHologram.Get() == Hologram)
		{
			if (AutoConnectService)
			{
				// Only clear the appropriate preview type based on hologram type
				if (AutoConnectService->IsDistributorHologram(Hologram))
				{
					AutoConnectService->ClearBeltPreviewHelpers();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared belt previews for distributor (OnActorDestroyed)"));
				}
				else if (AutoConnectService->IsPipelineJunctionHologram(Hologram))
				{
					// CRITICAL FIX: Only clear previews for THIS specific junction, not all junctions
					// This prevents child hologram destruction from clearing parent previews (fixes flickering)
					AutoConnectService->ClearPipePreviews(Hologram);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared pipe previews for junction %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
				else if (AutoConnectService->IsPowerPoleHologram(Hologram))
				{
					// Clear power line previews when power pole hologram is destroyed
					AutoConnectService->ClearAllPowerPreviews();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared power line previews for pole %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
			}

			// NOTE: Don't reset bProcessingGridPlacement here!
			// The hologram is destroyed both on successful build AND on cancel.
			// The belt building lambda will reset the flag after completion.
			// If user cancels, OnActorSpawned never runs, so flag is never set.
		}
	}
}

void USFSubsystem::StartPeriodicCleanup()
{
	// Start periodic cleanup timer during build mode
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PeriodicCleanupTimer,
			this,
			&USFSubsystem::OnPeriodicCleanup,
			10.0f,  // Every 10 seconds
			true    // Looping
		);

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StartPeriodicCleanup: Started periodic hologram registry cleanup"));
	}
}

void USFSubsystem::StopPeriodicCleanup()
{
	// Stop periodic cleanup timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicCleanupTimer);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StopPeriodicCleanup: Stopped periodic hologram registry cleanup"));
	}
}

void USFSubsystem::OnPeriodicCleanup()
{
	// Perform deterministic cleanup of dead entries
	USFHologramDataRegistry::CleanupDeadEntries();
}

// ========================================
// Build HUD Widget Management
// ========================================

void USFSubsystem::InitializeWidgets()
{
	if (HudService)
	{
		HudService->InitializeWidgets();
	}
}

void USFSubsystem::EnsureHUDBinding()
{
	if (HudService)
	{
		HudService->EnsureHUDBinding();
	}
}

void USFSubsystem::CleanupWidgets()
{
    if (HudService)
    {
        HudService->CleanupWidgets();
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Widgets cleaned up"));
}

void USFSubsystem::DisableVanillaBuildGunContext()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->DisableVanillaBuildGunContext();
	}
}

void USFSubsystem::EnableVanillaBuildGunContext()
{
	// Phase 0: Forward to InputHandler module (Task #61.6)
	if (InputHandler)
	{
		InputHandler->EnableVanillaBuildGunContext();
	}
}

// ========================================
// Recipe Copying System Implementation
// ========================================

void USFSubsystem::StoreProductionRecipeFromBuilding(AFGBuildable* SourceBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->StoreProductionRecipeFromBuilding(SourceBuilding);
	}
}

void USFSubsystem::ApplyStoredProductionRecipeToBuilding(AFGBuildable* TargetBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyStoredProductionRecipeToBuilding(TargetBuilding);
	}
}

bool USFSubsystem::IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass)
{
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsRecipeCompatibleWithHologram(Recipe, HologramBuildClass);
	}
	return false;
}

bool USFSubsystem::IsProductionBuilding(AFGBuildable* Building) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsProductionBuilding(Building);
	}
	return false;
}

void USFSubsystem::ClearStoredProductionRecipe()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearStoredProductionRecipe();
	}
}

void USFSubsystem::OnBuildGunRecipeSampled(TSubclassOf<UFGRecipe> SampledRecipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->OnBuildGunRecipeSampled(SampledRecipe);
	}
}

// Recipe Selector System Implementation
// ========================================
void USFSubsystem::OnRecipeModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();

	// Context-aware mode switching: U button behaves differently based on hologram type
	// - Distributors: Auto-Connect Settings Mode (navigate belt settings)
	// - Pipe Junctions: Auto-Connect Settings Mode (navigate pipe settings)
	// - Power Poles: Auto-Connect Settings Mode (navigate power settings)
	// - Factories: Recipe Mode (select recipes)

	bool bIsAutoConnectHologram = false;
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		bool bIsDistributor = AutoConnectService->IsDistributorHologram(ActiveHologram.Get());
		bool bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
		bool bIsStackablePipe = AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get());
		bool bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
		bool bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
		bool bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get());

		bIsAutoConnectHologram = bIsDistributor || bIsPipeJunction || bIsStackablePipe || bIsStackableBelt || bIsPowerPole || bIsPassthroughPipe;

		FString BuildClassName = ActiveHologram->GetBuildClass() ? ActiveHologram->GetBuildClass()->GetName() : TEXT("NULL");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnRecipeModeChanged: Hologram=%s, BuildClass=%s, Distributor=%d, PipeJunction=%d, StackablePipe=%d, BeltSupport=%d, PowerPole=%d, PassthroughPipe=%d, IsAutoConnect=%d"),
			*ActiveHologram->GetClass()->GetName(), *BuildClassName, bIsDistributor, bIsPipeJunction, bIsStackablePipe, bIsStackableBelt, bIsPowerPole, bIsPassthroughPipe, bIsAutoConnectHologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT(" OnRecipeModeChanged: ActiveHologram=%d, AutoConnectService=%d"),
			ActiveHologram.IsValid(), AutoConnectService != nullptr);
	}

	if (bIsAutoConnectHologram)
	{
		// Auto-Connect Settings Mode for distributors, pipe junctions, and power poles
		bAutoConnectSettingsModeActive = bPressed;
		bRecipeModeActive = false;  // Ensure recipe mode is off

		if (bAutoConnectSettingsModeActive)
		{
			// Initialize runtime settings from config only if not already initialized
			// This preserves user modifications when re-entering settings mode
			if (bConfigLoaded && !AutoConnectRuntimeSettings.bInitialized)
			{
				AutoConnectRuntimeSettings.InitFromConfig(CachedConfig);
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect runtime settings initialized from config"));
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect Settings Mode: Active (U held on auto-connect hologram)"));
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current Setting: %s"), *GetAutoConnectSettingDisplayString());

			// Try to acquire lock
			TryAcquireHologramLock();

			// Update HUD to show current active setting
			UpdateCounterDisplay();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚙️ Auto-Connect Settings Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();

			// Update HUD to refresh display (remove active indicator)
			UpdateCounterDisplay();
		}
	}
	else
	{
		// Recipe Mode for factory buildings
		bRecipeModeActive = bPressed;
		bAutoConnectSettingsModeActive = false;  // Ensure auto-connect settings mode is off

		if (bRecipeModeActive)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe Mode: Active (U held on factory)"));

			// Try to acquire lock
			TryAcquireHologramLock();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("🍽️ Recipe Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();
		}

		// Delegate to recipe management service (only for factories)
		if (RecipeManagementService)
		{
			RecipeManagementService->OnRecipeModeChanged(Value);
		}
	}
}

void USFSubsystem::AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->AddRecipeToUnlocked(Recipe);
	}
}

TArray<TSubclassOf<UFGRecipe>> USFSubsystem::GetFilteredRecipesForCurrentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetFilteredRecipesForCurrentHologram();
	}
	return TArray<TSubclassOf<UFGRecipe>>();
}


void USFSubsystem::SetActiveRecipeByIndex(int32 Index)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->SetActiveRecipeByIndex(Index);
	}
}

// ========================================
// Smart Restore Enhanced — New Subsystem Methods
// ========================================

void USFSubsystem::SetAutoConnectRuntimeSettingsFromPreset(const FSFRestoreAutoConnectState& PresetState)
{
	AutoConnectRuntimeSettings.bEnabled = PresetState.bBeltEnabled;
	AutoConnectRuntimeSettings.BeltTierMain = PresetState.BeltTierMain;
	AutoConnectRuntimeSettings.BeltTierToBuilding = PresetState.BeltTierToBuilding;
	AutoConnectRuntimeSettings.bChainDistributors = PresetState.bChainDistributors;
	AutoConnectRuntimeSettings.BeltRoutingMode = PresetState.BeltRoutingMode;
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = PresetState.bPipeEnabled;
	AutoConnectRuntimeSettings.PipeTierMain = PresetState.PipeTierMain;
	AutoConnectRuntimeSettings.PipeTierToBuilding = PresetState.PipeTierToBuilding;
	AutoConnectRuntimeSettings.bPipeIndicator = PresetState.bPipeIndicator;
	AutoConnectRuntimeSettings.PipeRoutingMode = PresetState.PipeRoutingMode;
	AutoConnectRuntimeSettings.bConnectPower = PresetState.bPowerEnabled;
	AutoConnectRuntimeSettings.PowerGridAxis = PresetState.PowerGridAxis;
	AutoConnectRuntimeSettings.PowerReserved = PresetState.PowerReserved;
	AutoConnectRuntimeSettings.bInitialized = true;

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[SmartRestore] Applied auto-connect settings from preset"));
}

bool USFSubsystem::SetBuildGunByRecipeName(const FString& RecipeClassName)
{
	if (RecipeClassName.IsEmpty())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Find the recipe by class name in available recipes
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No recipe manager"));
		return false;
	}

	TArray<TSubclassOf<UFGRecipe>> AllRecipes;
	RecipeManager->GetAllAvailableRecipes(AllRecipes);

	TSubclassOf<UFGRecipe> TargetRecipe = nullptr;
	for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
	{
		if (Recipe && Recipe->GetName() == RecipeClassName)
		{
			TargetRecipe = Recipe;
			break;
		}
	}

	if (!TargetRecipe)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: Recipe '%s' not found in available recipes"),
			*RecipeClassName);
		return false;
	}

	// Get the player's build gun and set the recipe
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		return false;
	}

	// The build gun is equipment — get it from the character
	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun found"));
		return false;
	}

	// Set the active recipe on the build gun's build state
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(
		BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun build state"));
		return false;
	}

	BuildState->SetActiveRecipe(TargetRecipe);

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[SmartRestore] Switched build gun to recipe '%s'"), *RecipeClassName);
	return true;
}

void USFSubsystem::ApplyRecipeToParentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipeToParentHologram();
	}
}

void USFSubsystem::CycleRecipeForward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeForward(AccumulatedSteps);
	}
}

void USFSubsystem::CycleRecipeBackward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeBackward(AccumulatedSteps);
	}
}

void USFSubsystem::ClearAllRecipes()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearAllRecipes();
	}
}

// ========================================
// Pipe Tier Configuration Helpers
// ========================================

UClass* USFSubsystem::GetPipeClassForTier(int32 Tier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe class path based on tier and style
	FString PipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMK2/Build_PipelineMK2_NoIndicator.Build_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassForTier: Failed to load pipe class for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	// Check if player has unlocked this pipe tier
	if (PlayerController)
	{
		UWorld* World = PlayerController->GetWorld();
		if (World)
		{
			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
			if (RecipeManager)
			{
				// Cast to AFGBuildable to check availability
				TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
				if (!RecipeManager->IsBuildingAvailable(PipeBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetPipeClassForTier: Pipe tier Mk%d (%s) not unlocked yet"),
					Tier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
					return nullptr;  // Pipe tier not unlocked - prevents cheating
				}
			}
		}
	}

	return PipeClass;
}

TSubclassOf<UFGRecipe> USFSubsystem::GetBeltRecipeForTier(int32 Tier)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 6)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltRecipeForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt recipe path based on tier
	FString RecipePath;
	switch (Tier)
	{
		case 1: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"); break;
		case 2: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk2.Recipe_ConveyorBeltMk2_C"); break;
		case 3: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk3.Recipe_ConveyorBeltMk3_C"); break;
		case 4: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"); break;
		case 5: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk5.Recipe_ConveyorBeltMk5_C"); break;
		case 6: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk6.Recipe_ConveyorBeltMk6_C"); break;
		default: return nullptr;
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltRecipeForTier: Failed to load recipe for Mk%d belt"), Tier);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

TSubclassOf<UFGRecipe> USFSubsystem::GetPipeRecipeForTier(int32 Tier, bool bWithIndicator)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeRecipeForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe recipe path based on tier and style
	FString RecipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline_NoIndicator.Recipe_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2.Recipe_PipelineMK2_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2_NoIndicator.Recipe_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeRecipeForTier: Failed to load recipe for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

int32 USFSubsystem::GetHighestUnlockedPipeTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No recipe manager, defaulting to Mk1"));
		return 1;
	}

	// Check pipe tiers from highest (Mk2) to lowest (Mk1) to find highest unlocked
	// Check "Normal" variant (with indicators) as the canonical unlock check
	for (int32 Tier = 2; Tier >= 1; Tier--)
	{
		FString PipePath;
		if (Tier == 1)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else  // Tier == 2
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}

		UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);
		if (PipeClass)
		{
			TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
			if (RecipeManager->IsBuildingAvailable(PipeBuildableClass))
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHighestUnlockedPipeTier: Highest unlocked tier is Mk%d"), Tier);
				return Tier;  // Found highest unlocked tier
			}
		}
	}

	// Fallback: If nothing is unlocked (shouldn't happen), return Mk1
	UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedPipeTier: No pipes unlocked, defaulting to Mk1"));
	return 1;
}

bool USFSubsystem::AreCleanPipesUnlocked(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return false;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return false;
	}

	// Check if Mk1 clean pipe is unlocked (NoIndicator variant)
	FString CleanPipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
	UClass* CleanPipeClass = LoadObject<UClass>(nullptr, *CleanPipePath);

	if (CleanPipeClass)
	{
		TSubclassOf<AFGBuildable> CleanPipeBuildableClass = CleanPipeClass;
		if (RecipeManager->IsBuildingAvailable(CleanPipeBuildableClass))
		{
			return true;
		}
	}

	return false;
}

UClass* USFSubsystem::GetPipeClassFromConfig(int32 ConfigTier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	int32 ActualTier = ConfigTier;

	// Handle "Auto" mode (0 = use highest unlocked)
	if (ConfigTier == 0)
	{
		ActualTier = GetHighestUnlockedPipeTier(PlayerController);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Auto mode selected highest tier Mk%d"), ActualTier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Using configured tier Mk%d"), ActualTier);
	}

	// Get pipe class for the determined tier and style
	UClass* PipeClass = GetPipeClassForTier(ActualTier, bWithIndicator, PlayerController);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetPipeClassFromConfig: Pipe tier Mk%d (%s) unavailable or not unlocked - pipe category disabled"),
			ActualTier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
	}

	return PipeClass;
}

void USFSubsystem::CycleAutoConnectSetting()
{
    // Check context (Belt vs Pipe Junction vs Stackable Pipe vs Stackable Belt vs Power)
    bool bIsPipeJunction = false;
    bool bIsStackablePipe = false;
    bool bIsStackableBelt = false;
    bool bIsPowerPole = false;
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
        bIsStackablePipe = AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get());
        bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
        bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
    }

    if (bIsPowerPole)
    {
        // Power Pole cycle: Enabled -> Reserved -> Grid Axis -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::PowerEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerReserved;
            break;
        case EAutoConnectSetting::PowerReserved:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerGridAxis;
            break;
        case EAutoConnectSetting::PowerGridAxis:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        default:
            // If on unrelated setting, jump to first power setting
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        }
    }
    else if (bIsStackableBelt)
    {
        // Stackable Conveyor Pole cycle: Enabled -> Tier -> Routing Mode -> Direction -> Enabled
        // NOTE: Uses BeltTierMain for tier, StackableBeltDirection for direction
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::StackableBeltEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltTier;
            break;
        case EAutoConnectSetting::StackableBeltTier:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltDirection;
            break;
        case EAutoConnectSetting::StackableBeltDirection:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        default:
            // If on unrelated setting, jump to first stackable belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        }
    }
    else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
    {
        // Floor Hole Pipe cycle: Enabled -> To Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsStackablePipe)
    {
        // Stackable Pipe cycle: Enabled -> Main Tier -> Indicator -> Routing Mode -> Enabled
        // NOTE: No TierToBuilding - stackable pipes only connect to each other, not buildings
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsPipeJunction)
    {
        // Pipe Junction cycle: Enabled -> Main Tier -> Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else
    {
        // Belt cycle (Distributor): Enabled -> Main Tier -> Building Tier -> Routing Mode -> Chain -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierMain;
            break;
        case EAutoConnectSetting::BeltTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierToBuilding;
            break;
        case EAutoConnectSetting::BeltTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::ChainDistributors;
            break;
        case EAutoConnectSetting::ChainDistributors:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect Setting Cycled: %s"), *GetAutoConnectSettingDisplayString());
    UpdateCounterDisplay();
}

void USFSubsystem::AdjustAutoConnectSetting(int32 Delta)
{
    // Mark settings as modified (initialized) when user makes changes
    AutoConnectRuntimeSettings.bInitialized = true;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware toggle: belts on distributor, pipes on junction/stackable supports
        if (ActiveHologram.IsValid() && AutoConnectService)
        {
            if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
                AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()) ||
                USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
            {
                // Pipe hologram (junction, stackable support, or floor hole): toggle pipe auto-connect
                AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = !AutoConnectRuntimeSettings.bPipeAutoConnectEnabled;
            }
            else
            {
                // Distributor: toggle belt auto-connect
                AutoConnectRuntimeSettings.bEnabled = !AutoConnectRuntimeSettings.bEnabled;
            }
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierToBuilding + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        AutoConnectRuntimeSettings.bChainDistributors = !AutoConnectRuntimeSettings.bChainDistributors;
        break;


    case EAutoConnectSetting::PipeTierMain:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierMain + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierToBuilding + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeIndicator:
        // Toggle between Normal (with indicators) and Clean (no indicators)
        AutoConnectRuntimeSettings.bPipeIndicator = !AutoConnectRuntimeSettings.bPipeIndicator;
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        // Cycle through 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical
        AutoConnectRuntimeSettings.PipeRoutingMode = (AutoConnectRuntimeSettings.PipeRoutingMode + Delta) % 6;
        if (AutoConnectRuntimeSettings.PipeRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.PipeRoutingMode += 6;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        // Toggle stackable conveyor pole belt auto-connect
        AutoConnectRuntimeSettings.bStackableBeltEnabled = !AutoConnectRuntimeSettings.bStackableBeltEnabled;
        break;

    case EAutoConnectSetting::StackableBeltTier:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier - reuses BeltTierMain
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        // Toggle between 0 (Forward) and 1 (Backward)
        AutoConnectRuntimeSettings.StackableBeltDirection = (AutoConnectRuntimeSettings.StackableBeltDirection == 0) ? 1 : 0;
        break;

    case EAutoConnectSetting::PowerEnabled:
        AutoConnectRuntimeSettings.bConnectPower = !AutoConnectRuntimeSettings.bConnectPower;
        break;

    case EAutoConnectSetting::PowerReserved:
        AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(AutoConnectRuntimeSettings.PowerReserved + Delta, 0, 5);
        break;

    case EAutoConnectSetting::PowerGridAxis:
        // Cycle through 0 (Auto), 1 (X), 2 (Y), 3 (X+Y)
        AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(AutoConnectRuntimeSettings.PowerGridAxis + Delta, 0, 3);
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        // Cycle through 0=Default, 1=Curve, 2=Straight (matches vanilla belt build modes)
        AutoConnectRuntimeSettings.BeltRoutingMode = (AutoConnectRuntimeSettings.BeltRoutingMode + Delta) % 3;
        if (AutoConnectRuntimeSettings.BeltRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.BeltRoutingMode += 3;
        }
        break;
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect Setting Adjusted: %s"), *GetAutoConnectSettingDisplayString());

    // Trigger immediate re-evaluation of previews with new settings
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        if (AutoConnectService->IsDistributorHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this distributor
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all belt previews with new settings
                Orchestrator->EvaluateGrid(true);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated belt previews after settings change"));
            }
        }
        else if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this pipeline junction
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all pipe previews with new settings
                Orchestrator->OnPipeGridChanged();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated pipe previews after settings change"));
            }
        }
        else if (AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()))
        {
            // Stackable pipe supports: trigger re-processing of pipe previews
            AutoConnectService->ProcessStackablePipelineSupports(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Stackable Pipe: Force recreated pipe previews after settings change"));
        }
        else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
        {
            // Floor hole pipes: trigger re-processing of pipe previews
            AutoConnectService->ProcessFloorHolePipes(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Floor Hole Pipe: Force recreated pipe previews after settings change"));
        }
        else if (USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get()))
        {
            // Belt support poles (stackable, ceiling, wall): trigger re-processing of belt previews
            AutoConnectService->ProcessStackableConveyorPoles(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Belt Support: Force recreated belt previews after settings change"));
        }
        else if (AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this power pole
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all power previews with new settings
                Orchestrator->OnPowerGridChanged();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated power previews after settings change"));
            }
        }
    }

    UpdateCounterDisplay();
    AutoConnectRuntimeSettings.bInitialized = true;
}

FString USFSubsystem::GetAutoConnectSettingDisplayString() const
{
    FString SettingName;
    FString SettingValue;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware display: show belt or pipe auto-connect status
        if (ActiveHologram.IsValid() && AutoConnectService &&
            (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
             AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram.Get()) ||
             USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get())))
        {
            SettingName = TEXT("Pipe Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF");
        }
        else
        {
            SettingName = TEXT("Belt Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF");
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        SettingName = TEXT("Belt Tier (Main)");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        SettingName = TEXT("Belt Tier (To Building)");
        if (AutoConnectRuntimeSettings.BeltTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        SettingName = TEXT("Chain Distributors");
        SettingValue = AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        SettingName = TEXT("Belt Routing");
        switch (AutoConnectRuntimeSettings.BeltRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Default");
            break;
        case 1:
            SettingValue = TEXT("Curve");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        default:
            SettingValue = TEXT("Default");
            break;
        }
        break;

    case EAutoConnectSetting::PipeTierMain:
        SettingName = TEXT("Pipe Tier (Main)");
        if (AutoConnectRuntimeSettings.PipeTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
        }
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        SettingName = TEXT("Pipe Tier (To Building)");
        if (AutoConnectRuntimeSettings.PipeTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
        }
        break;

    case EAutoConnectSetting::PipeIndicator:
        SettingName = TEXT("Pipe Style");
        SettingValue = AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean");
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        SettingName = TEXT("Pipe Routing");
        switch (AutoConnectRuntimeSettings.PipeRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("Auto 2D");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        case 3:
            SettingValue = TEXT("Curve");
            break;
        case 4:
            SettingValue = TEXT("Noodle");
            break;
        case 5:
            SettingValue = TEXT("Horiz→Vert");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        SettingName = TEXT("Stackable Belt Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bStackableBeltEnabled ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::StackableBeltTier:
        SettingName = TEXT("Stackable Belt Tier");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        SettingName = TEXT("Stackable Belt Direction");
        SettingValue = AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward");
        break;

    case EAutoConnectSetting::PowerEnabled:
        SettingName = TEXT("Power Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::PowerGridAxis:
        SettingName = TEXT("Grid Axis");
        switch (AutoConnectRuntimeSettings.PowerGridAxis)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("X");
            break;
        case 2:
            SettingValue = TEXT("Y");
            break;
        case 3:
            SettingValue = TEXT("X+Y");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::PowerReserved:
        SettingName = TEXT("Reserved Slots");
        SettingValue = FString::Printf(TEXT("%d"), AutoConnectRuntimeSettings.PowerReserved);
        break;
    }

    return FString::Printf(TEXT("%s: %s"), *SettingName, *SettingValue);
}

bool USFSubsystem::IsCurrentHologramAutoConnectCapable() const
{
	if (!ActiveHologram.IsValid() || !AutoConnectService)
	{
		return false;
	}

	AFGHologram* Hologram = ActiveHologram.Get();
	return AutoConnectService->IsDistributorHologram(Hologram) ||
	       AutoConnectService->IsPipelineJunctionHologram(Hologram) ||
	       AutoConnectService->IsPowerPoleHologram(Hologram) ||
	       USFAutoConnectService::IsBeltSupportHologram(Hologram) ||
	       USFAutoConnectService::IsPassthroughPipeHologram(Hologram);
}

TArray<FString> USFSubsystem::GetDirtyAutoConnectSettings() const
{
	TArray<FString> DirtySettings;

	if (!bConfigLoaded)
	{
		return DirtySettings; // No config to compare against
	}

	// Determine hologram type to filter relevant settings
	bool bIsPipeJunction = false;
	bool bIsDistributor = false;
	bool bIsPowerPole = false;
	bool bIsStackableSupport = false;
	bool bIsPassthroughPipe = false;
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		AFGHologram* Hologram = ActiveHologram.Get();
		bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(Hologram);
		bIsDistributor = AutoConnectService->IsDistributorHologram(Hologram);
		bIsPowerPole = AutoConnectService->IsPowerPoleHologram(Hologram);
		bIsStackableSupport = USFAutoConnectService::IsBeltSupportHologram(Hologram);
		bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(Hologram);
	}

	// CRITICAL FIX: Only show settings relevant to current hologram type
	// Pipe junctions should only show pipe settings, distributors should only show belt settings

	if (bIsPipeJunction || bIsPassthroughPipe)
	{
		// Pipe junction or floor hole passthrough: Show pipe-related settings
		if (AutoConnectRuntimeSettings.bPipeAutoConnectEnabled != CachedConfig.bPipeAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PipeTierMain != CachedConfig.PipeLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.PipeTierToBuilding != CachedConfig.PipeLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bPipeIndicator != CachedConfig.PipeIndicator)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Style: %s"),
				AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean")));
		}
	}
	else if (bIsDistributor || bIsStackableSupport)
	{
		// Distributor: Show only belt-related settings
		if (AutoConnectRuntimeSettings.bEnabled != CachedConfig.bAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Belt Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.BeltTierMain != CachedConfig.BeltLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.BeltTierToBuilding != CachedConfig.BeltLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bChainDistributors != CachedConfig.bAutoConnectDistributors)
		{
			DirtySettings.Add(FString::Printf(TEXT("Chain Distributors: %s"),
				AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF")));
		}
	}
	else if (bIsPowerPole)
	{
		// Power pole: Show only power-related settings
		if (AutoConnectRuntimeSettings.bConnectPower != CachedConfig.bPowerAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Power Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PowerReserved != CachedConfig.PowerConnectReserved)
		{
			DirtySettings.Add(FString::Printf(TEXT("Reserved Slots: %d"), AutoConnectRuntimeSettings.PowerReserved));
		}

		if (AutoConnectRuntimeSettings.PowerGridAxis != CachedConfig.PowerConnectMode)
		{
			FString AxisText;
			switch (AutoConnectRuntimeSettings.PowerGridAxis)
			{
			case 0: AxisText = TEXT("Auto"); break;
			case 1: AxisText = TEXT("X"); break;
			case 2: AxisText = TEXT("Y"); break;
			case 3: AxisText = TEXT("X+Y"); break;
			default: AxisText = TEXT("Auto"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Grid Axis: %s"), *AxisText));
		}

		// Building range is config-only, but we can show if runtime value differs
		// Convert both to meters for comparison (runtime is in cm, config is in meters)
		int32 RuntimeRangeMeters = FMath::RoundToInt(AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
		if (RuntimeRangeMeters != CachedConfig.PowerConnectRange)
		{
			DirtySettings.Add(FString::Printf(TEXT("Building Range: %dm"), RuntimeRangeMeters));
		}
	}

	return DirtySettings;
}

void USFSubsystem::ResetAutoConnectRuntimeSettings()
{
	// Reset runtime settings to match FRESH config (not cached)
	// This ensures config changes made mid-session take effect when equipping a new hologram
	FSmart_ConfigStruct FreshConfig = FSmart_ConfigStruct::GetActiveConfig(this);
	AutoConnectRuntimeSettings.InitFromConfig(FreshConfig);

	// Issue #257: Refresh Extend enabled state from fresh config
	bExtendEnabledByConfig = FreshConfig.bExtendEnabled;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect runtime settings reset from config:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Belt Auto-Connect: %s"), AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Pipe Auto-Connect: %s"), AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Auto-Connect: %s"), AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Range: %.0fm"), AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Extend Enabled: %s"), bExtendEnabledByConfig ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::ResetSmartDisableFlag()
{
	if (bDisableSmartForNextAction || bExtendDisabledForSession)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("▶️ Smart! re-enabled (Auto-Connect + Extend flags reset)"));
	}
	bDisableSmartForNextAction = false;
	bExtendDisabledForSession = false;  // Issue #257: reset Extend session flag too
	LastCycleAxisTapTime = 0.0;
}

// ========================================
// Belt Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectBeltEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectBeltTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier main set to: %d"), AutoConnectRuntimeSettings.BeltTierMain);
}

void USFSubsystem::SetAutoConnectBeltTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier to building set to: %d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
}

void USFSubsystem::SetAutoConnectBeltChain(bool bEnabled)
{
	AutoConnectRuntimeSettings.bChainDistributors = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt chain distributors set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectStackableBeltDirection(int32 Direction)
{
	AutoConnectRuntimeSettings.StackableBeltDirection = FMath::Clamp(Direction, 0, 1);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Stackable belt direction set to: %d (%s)"),
		AutoConnectRuntimeSettings.StackableBeltDirection,
		AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward"));
}

void USFSubsystem::SetAutoConnectBeltRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.BeltRoutingMode = FMath::Clamp(Mode, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.BeltRoutingMode)
	{
	case 0: ModeName = TEXT("Default"); break;
	case 1: ModeName = TEXT("Curve"); break;
	case 2: ModeName = TEXT("Straight"); break;
	default: ModeName = TEXT("Default"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt routing mode set to: %d (%s)"), AutoConnectRuntimeSettings.BeltRoutingMode, *ModeName);
}

// ========================================
// Pipe Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPipeEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPipeTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier main set to: %d"), AutoConnectRuntimeSettings.PipeTierMain);
}

void USFSubsystem::SetAutoConnectPipeTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier to building set to: %d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
}

void USFSubsystem::SetAutoConnectPipeIndicator(bool bIndicator)
{
	AutoConnectRuntimeSettings.bPipeIndicator = bIndicator;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe indicator style set to: %s"), bIndicator ? TEXT("Normal") : TEXT("Clean"));
}

void USFSubsystem::SetAutoConnectPipeRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.PipeRoutingMode = FMath::Clamp(Mode, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.PipeRoutingMode)
	{
	case 0: ModeName = TEXT("Auto"); break;
	case 1: ModeName = TEXT("Auto 2D"); break;
	case 2: ModeName = TEXT("Straight"); break;
	case 3: ModeName = TEXT("Curve"); break;
	case 4: ModeName = TEXT("Noodle"); break;
	case 5: ModeName = TEXT("Horiz→Vert"); break;
	default: ModeName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe routing mode set to: %s (%d)"), *ModeName, AutoConnectRuntimeSettings.PipeRoutingMode);
}

// ========================================
// Power Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPowerEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bConnectPower = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPowerGridAxis(int32 Axis)
{
	AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(Axis, 0, 3);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString AxisName;
	switch (AutoConnectRuntimeSettings.PowerGridAxis)
	{
	case 0: AxisName = TEXT("Auto"); break;
	case 1: AxisName = TEXT("X"); break;
	case 2: AxisName = TEXT("Y"); break;
	case 3: AxisName = TEXT("X+Y"); break;
	default: AxisName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power grid axis set to: %d (%s)"), AutoConnectRuntimeSettings.PowerGridAxis, *AxisName);
}

void USFSubsystem::SetAutoConnectPowerReserved(int32 Reserved)
{
	AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(Reserved, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power reserved slots set to: %d"), AutoConnectRuntimeSettings.PowerReserved);
}

void USFSubsystem::TriggerAutoConnectRefresh()
{
	// Trigger auto-connect preview refresh by notifying all active orchestrators
	for (auto& Pair : AutoConnectOrchestrators)
	{
		if (Pair.Value)
		{
			Pair.Value->ForceRefresh();
		}
	}

	if (AutoConnectOrchestrators.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Auto-connect refresh triggered from settings change (%d orchestrators)"), AutoConnectOrchestrators.Num());
	}
}

// ========================================
// Building Registry System Implementation
// ========================================

void USFSubsystem::RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->RegisterSmartBuilding(Building, IndexInGroup, bIsParent);
	}
}

void USFSubsystem::ApplyRecipesToCurrentPlacement()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipesToCurrentPlacement();
	}
}

void USFSubsystem::ClearCurrentPlacement()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearCurrentPlacement();
	}
}

// ========================================
// Phase 4: Runtime Hologram Swapping Implementation
// ========================================

AFGHologram* USFSubsystem::TrySwapToSmartHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("TrySwapToSmartHologram: Invalid hologram"));
		return OriginalHologram;
	}

	// NOTE: Removed name-based check as Satisfactory renames holograms after creation
	// Child holograms are identified by the calling context in SpawnChildHologram
	// All holograms reaching this function are intended for swapping
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("TrySwapToSmartHologram: Proceeding with hologram swap for %s"), *OriginalHologram->GetName());

	// Get the build class to determine what type of building this is
	UClass* BuildClass = OriginalHologram->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("TrySwapToSmartHologram: No build class for %s"), *OriginalHologram->GetName());
		return OriginalHologram;
	}

	FString BuildClassName = BuildClass->GetName();
	FString OriginalHologramClass = OriginalHologram->GetClass()->GetName();

	UE_LOG(LogSmartFoundations, Verbose, TEXT("TrySwapToSmartHologram: %s -> BuildClass=%s"),
		*OriginalHologramClass, *BuildClassName);

	// For now, implement a simple check - if it's a foundation hologram, we could swap it
	// This is a placeholder for the full implementation
	// The actual swapping would involve:
	// 1. Creating a new custom hologram instance
	// 2. Copying properties from the original
	// 3. Replacing it in the build gun system
	// 4. Destroying the original

	// PHASE 4 FULL IMPLEMENTATION: Actually swap holograms
	if (OriginalHologramClass == TEXT("Holo_Foundation_C"))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SWAPPING: Foundation hologram -> ASFFoundationHologram"));
		ASFFoundationHologram* CustomHologram = CreateCustomFoundationHologram(OriginalHologram);
		if (CustomHologram)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Successfully created custom foundation hologram: %s"), *CustomHologram->GetName());
			return CustomHologram;
		}
		else
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to create custom foundation hologram, falling back to vanilla"));
		}
	}
	else if (Cast<AFGFactoryHologram>(OriginalHologram))
	{
		// NOTE: Runtime factory hologram swapping is DISABLED
		// The build gun holds the original hologram reference, and creating a new one
		// causes constant re-registration loops. Factory holograms need Blueprint class
		// remapping at module load time instead.
		// For now, EXTEND must work with vanilla holograms or use a different approach.
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Factory hologram %s - runtime swap disabled (use Blueprint class remapping)"),
			*OriginalHologramClass);
		// Return the original - don't create orphan custom holograms
		return OriginalHologram;
	}
	else if (OriginalHologramClass == TEXT("Holo_ConveyorBelt_C") ||
			 OriginalHologramClass == TEXT("Holo_ConveyorAttachmentSplitter_C") ||
			 OriginalHologramClass == TEXT("Holo_ConveyorAttachmentMerger_C"))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SWAPPING: Logistics hologram (%s) -> ASFLogisticsHologram"), *OriginalHologramClass);
		ASFLogisticsHologram* CustomHologram = CreateCustomLogisticsHologram(OriginalHologram);
		if (CustomHologram)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Successfully created custom logistics hologram: %s"), *CustomHologram->GetName());
			return CustomHologram;
		}
		else
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("❌ Failed to create custom logistics hologram, falling back to vanilla"));
		}
	}

	// No swap needed
	return OriginalHologram;
}

// ========================================
// Hologram Creation Functions Implementation
// ========================================

ASFFoundationHologram* USFSubsystem::CreateCustomFoundationHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: Invalid original hologram"));
		return nullptr;
	}

	// Get the world context from the original hologram
	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: No world context"));
		return nullptr;
	}

	// Configure spawn parameters with DEFERRED construction
	// This allows us to set properties before BeginPlay() is called
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFFoundationHologram* CustomHologram = World->SpawnActor<ASFFoundationHologram>(ASFFoundationHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFoundationHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFoundationHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	// NOTE: Material state should be inherited from parent once properly linked
	// The swapped hologram needs to be added to parent's children array to inherit parent's valid state

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFoundationHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

ASFFactoryHologram* USFSubsystem::CreateCustomFactoryHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFFactoryHologram* CustomHologram = World->SpawnActor<ASFFactoryHologram>(ASFFactoryHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomFactoryHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFactoryHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomFactoryHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

ASFLogisticsHologram* USFSubsystem::CreateCustomLogisticsHologram(AFGHologram* OriginalHologram)
{
	if (!OriginalHologram || !OriginalHologram->IsValidLowLevel())
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: Invalid original hologram"));
		return nullptr;
	}

	UWorld* World = OriginalHologram->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: No world context"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OriginalHologram->GetOwner();
	SpawnParams.Instigator = OriginalHologram->GetInstigator();
	SpawnParams.bDeferConstruction = true;  // CRITICAL: Defer construction to set build class first

	ASFLogisticsHologram* CustomHologram = World->SpawnActor<ASFLogisticsHologram>(ASFLogisticsHologram::StaticClass(), SpawnParams);
	if (!CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CreateCustomLogisticsHologram: Failed to spawn custom hologram"));
		return nullptr;
	}

	// CRITICAL: Set build class BEFORE finishing construction to prevent assertion failure
	if (OriginalHologram->GetBuildClass())
	{
		CustomHologram->SetBuildClass(OriginalHologram->GetBuildClass());
	}

	// Now finish construction which will trigger BeginPlay() with proper build class set
	CustomHologram->FinishSpawning(CustomHologram->GetActorTransform(), true);

	CopyHologramProperties(OriginalHologram, CustomHologram);

	// CRITICAL: Force material state to OK to ensure child holograms are placeable
	// This bypasses validation that would otherwise set them to ERROR
	CustomHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Log the forced state for debugging
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomLogisticsHologram: Forced material state to HMS_OK for %s"),
		*CustomHologram->GetName());

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateCustomLogisticsHologram: Successfully created %s (build gun replacement pending)"), *CustomHologram->GetName());
	return CustomHologram;
}

void USFSubsystem::CopyHologramProperties(AFGHologram* Source, AFGHologram* Destination)
{
	if (!Source || !Destination)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("CopyHologramProperties: Invalid source or destination"));
		return;
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyHologramProperties: Copying from %s to %s"),
		*Source->GetName(), *Destination->GetName());

	// Copy essential hologram properties
	Destination->SetActorLocationAndRotation(Source->GetActorLocation(), Source->GetActorRotation());

	// Note: Build class is already set in the creation function BEFORE BeginPlay() to prevent assertion failure

	// Copy recipe if available
	if (Source->GetRecipe())
	{
		Destination->SetRecipe(Source->GetRecipe());
	}

	// NOTE: Parent hologram reference and scroll mode cannot be directly copied
	// These properties are managed by the hologram system internally
	// The parent-child relationship must be established by replacing the child in the parent's children array

	// Copy basic hologram state (simplified for Phase 4 MVP)
	// Note: Advanced property copying will be added in later iteration

	UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyHologramProperties: Successfully copied basic properties (location, rotation, build class, recipe, parent)"));
}

bool USFSubsystem::ReplaceHologramInBuildGun(AFGHologram* OriginalHologram, AFGHologram* CustomHologram)
{
	// Phase 4 MVP: Simplified implementation that always succeeds
	// Full build gun replacement will be implemented in later iteration

	if (!OriginalHologram || !CustomHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("ReplaceHologramInBuildGun: Invalid holograms"));
		return false;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ReplaceHologramInBuildGun: MVP implementation - hologram replacement pending"));

	// For MVP, we just return true to indicate success
	// The actual replacement logic will be added later
	return true;
}

// Belt Tier Configuration Helpers
// ========================================

UClass* USFSubsystem::GetBeltClassForTier(int32 Tier, AFGPlayerController* PlayerController)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 6)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltClassForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt class path
	FString BeltPath = FString::Printf(
		TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
		Tier, Tier, Tier);

	UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);

	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltClassForTier: Failed to load belt class for tier %d"), Tier);
		return nullptr;
	}

	// Check if player has unlocked this belt tier
	if (PlayerController)
	{
		UWorld* World = PlayerController->GetWorld();
		if (World)
		{
			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
			if (RecipeManager)
			{
				// Cast to AFGBuildable to check availability
				TSubclassOf<AFGBuildable> BeltBuildableClass = BeltClass;
				if (!RecipeManager->IsBuildingAvailable(BeltBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBeltClassForTier: Belt tier Mk%d not unlocked yet"), Tier);
					return nullptr;  // Belt tier not unlocked - prevents cheating
				}
			}
		}
	}

	return BeltClass;
}

int32 USFSubsystem::GetHighestUnlockedBeltTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No recipe manager, defaulting to Mk1"));
		return 1;
	}

	// Check belt tiers from highest (Mk6) to lowest (Mk1) to find highest unlocked
	for (int32 Tier = 6; Tier >= 1; Tier--)
	{
		FString BeltPath = FString::Printf(
			TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
			Tier, Tier, Tier);

		UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);
		if (BeltClass)
		{
			TSubclassOf<AFGBuildable> BeltBuildableClass = BeltClass;
			if (RecipeManager->IsBuildingAvailable(BeltBuildableClass))
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHighestUnlockedBeltTier: Highest unlocked tier is Mk%d"), Tier);
				return Tier;  // Found highest unlocked tier
			}
		}
	}

	// Fallback: If nothing is unlocked (shouldn't happen), return Mk1
	UE_LOG(LogSmartFoundations, Warning, TEXT("GetHighestUnlockedBeltTier: No belts unlocked, defaulting to Mk1"));
	return 1;
}

int32 USFSubsystem::GetHighestUnlockedPowerPoleTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return 1;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return 1;
	}

	// Power pole paths: Mk1, Mk2, Mk3
	static const TCHAR* PowerPolePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1.Build_PowerPoleMk1_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk2/Build_PowerPoleMk2.Build_PowerPoleMk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3.Build_PowerPoleMk3_C"),
	};

	// Check from highest (Mk3) to lowest (Mk1)
	for (int32 Tier = 3; Tier >= 1; Tier--)
	{
		UClass* PoleClass = LoadObject<UClass>(nullptr, PowerPolePaths[Tier - 1]);
		if (PoleClass)
		{
			TSubclassOf<AFGBuildable> PoleBuildableClass = PoleClass;
			if (RecipeManager->IsBuildingAvailable(PoleBuildableClass))
			{
				return Tier;
			}
		}
	}

	return 1;
}

int32 USFSubsystem::GetHighestUnlockedWallOutletTier(AFGPlayerController* PlayerController, bool bDouble)
{
	if (!PlayerController)
	{
		return 1;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return 1;
	}

	// Wall outlet paths: Mk1, Mk2, Mk3. Single and double are independent asset families and
	// unlock separately via game progression (Issue #267), so probe whichever family the caller
	// asked for — mixing them under-reports availability for the unchecked family.
	// Note: Wall outlets use underscore naming (Build_PowerPoleWall_Mk2) in the same subfolder,
	// NOT separate subfolders like regular power poles (PowerPoleMk2/Build_PowerPoleMk2).
	static const TCHAR* WallOutletSinglePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall.Build_PowerPoleWall_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk2.Build_PowerPoleWall_Mk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall_Mk3.Build_PowerPoleWall_Mk3_C"),
	};
	static const TCHAR* WallOutletDoublePaths[] = {
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble.Build_PowerPoleWallDouble_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk2.Build_PowerPoleWallDouble_Mk2_C"),
		TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk3.Build_PowerPoleWallDouble_Mk3_C"),
	};
	const TCHAR* const * Paths = bDouble ? WallOutletDoublePaths : WallOutletSinglePaths;

	// Check from highest (Mk3) to lowest (Mk1)
	for (int32 Tier = 3; Tier >= 1; Tier--)
	{
		UClass* OutletClass = LoadObject<UClass>(nullptr, Paths[Tier - 1]);
		if (OutletClass)
		{
			TSubclassOf<AFGBuildable> OutletBuildableClass = OutletClass;
			if (RecipeManager->IsBuildingAvailable(OutletBuildableClass))
			{
				return Tier;
			}
		}
	}

	return 1;
}

UClass* USFSubsystem::GetBeltClassFromConfig(int32 ConfigTier, AFGPlayerController* PlayerController)
{
	int32 ActualTier = ConfigTier;

	// Handle "Auto" mode (0 = use highest unlocked)
	if (ConfigTier == 0)
	{
		ActualTier = GetHighestUnlockedBeltTier(PlayerController);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassFromConfig: Auto mode selected highest tier Mk%d"), ActualTier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltClassFromConfig: Using configured tier Mk%d"), ActualTier);
	}

	// Get belt class for the determined tier
	UClass* BeltClass = GetBeltClassForTier(ActualTier, PlayerController);

	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBeltClassFromConfig: Belt tier Mk%d unavailable or not unlocked - belt category disabled"), ActualTier);
	}

	return BeltClass;
}

bool USFSubsystem::ChargePlayerForBelt(UClass* BeltClass, AFGPlayerController* PlayerController, float BeltLengthCm)
{
	if (!BeltClass || !PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: Invalid belt class or player controller"));
		return false;
	}

	// Get the building descriptor for this belt class
	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: No world context"));
		return false;
	}

	// Check if "No Build Cost" cheat is enabled - skip deduction but still build
	// ONLY check GetCheatNoCost() - do NOT treat Creative Mode as free building
	AFGGameState* GameState = World->GetGameState<AFGGameState>();
	if (GameState && GameState->GetCheatNoCost())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ChargePlayerForBelt: No Build Cost cheat enabled - skipping deduction"));
		return true;  // Allow build without charging
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: No recipe manager"));
		return false;
	}

	// Find the building descriptor for this belt class
	TSubclassOf<AFGBuildable> BeltBuildableClass = BeltClass;
	TSubclassOf<UFGBuildingDescriptor> Descriptor = RecipeManager->FindBuildingDescriptorByClass(BeltBuildableClass);

	if (!Descriptor)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: Could not find building descriptor for %s"), *BeltClass->GetName());
		return false;
	}

	// Get the recipe from the descriptor (recipes contain the build cost)
	TArray<TSubclassOf<UFGRecipe>> Recipes = RecipeManager->FindRecipesByProduct(Descriptor, /*onlyAvailableRecipes*/ false, /*availableFirst*/ true);

	if (Recipes.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: No recipes found for descriptor %s"), *Descriptor->GetName());
		return false;
	}

	// Use the first recipe (there should only be one for buildings)
	TSubclassOf<UFGRecipe> Recipe = Recipes[0];
	const UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();

	if (!RecipeCDO)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: Could not get recipe CDO"));
		return false;
	}

	// Get the BASE build cost per meter (ingredients are for 1 meter)
	const TArray<FItemAmount>& BaseCost = RecipeCDO->GetIngredients();

	// Calculate actual cost based on belt length
	// Belt cost in Satisfactory is 0.5 materials per meter (per wiki)
	// Recipe ingredients represent cost for 2 meters of belt
	// (100cm = 1m in UE units)
	float LengthInMeters = BeltLengthCm / 100.0f;

	// Calculate scaled cost for this belt length
	TArray<FItemAmount> ActualCost;
	for (const FItemAmount& Cost : BaseCost)
	{
		FItemAmount ScaledCost = Cost;
		// Belt costs 0.5 materials per meter, so divide by 2
		// Round up to nearest integer as per vanilla behavior
		ScaledCost.Amount = FMath::CeilToInt(Cost.Amount * LengthInMeters * 0.5f);
		ActualCost.Add(ScaledCost);
	}

	// Get central storage subsystem (dimensional storage)
	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);

	// Get player's inventory
	AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(PlayerController->GetPawn());
	if (!Player)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: Player pawn not found"));
		return false;
	}

	UFGInventoryComponent* Inventory = Player->GetInventory();
	if (!Inventory)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForBelt: Player inventory not found"));
		return false;
	}

	// Check if player has enough materials (central storage + personal inventory)
	// Match vanilla behavior: check dimensional storage first, then personal inventory
	for (const FItemAmount& Cost : ActualCost)
	{
		int32 InCentralStorage = CentralStorage ? CentralStorage->GetNumItemsFromCentralStorage(Cost.ItemClass) : 0;
		int32 InPersonalInventory = Inventory->GetNumItems(Cost.ItemClass);
		int32 TotalAvailable = InCentralStorage + InPersonalInventory;

		if (TotalAvailable < Cost.Amount)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ChargePlayerForBelt: Player cannot afford %.1fm belt - missing %d %s (have %d in central + %d in inventory = %d total)"),
				LengthInMeters, Cost.Amount - TotalAvailable, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString(),
				InCentralStorage, InPersonalInventory, TotalAvailable);
			return false;  // Can't afford
		}
	}

	// Remove materials (central storage first, then personal inventory)
	// This matches vanilla building behavior
	for (const FItemAmount& Cost : ActualCost)
	{
		int32 Remaining = Cost.Amount;

		// Try central storage first
		if (CentralStorage && Remaining > 0)
		{
			int32 RemovedFromCentral = CentralStorage->TryRemoveItemsFromCentralStorage(Cost.ItemClass, Remaining);
			Remaining -= RemovedFromCentral;

			if (RemovedFromCentral > 0)
			{
				UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForBelt: Removed %d %s from central storage"),
					RemovedFromCentral, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString());
			}
		}

		// Remove remainder from personal inventory
		if (Remaining > 0)
		{
			Inventory->Remove(Cost.ItemClass, Remaining);
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForBelt: Removed %d %s from personal inventory"),
				Remaining, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString());
		}
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForBelt: Successfully charged player for %.1fm %s belt"),
		LengthInMeters, *BeltClass->GetName());
	return true;
}

bool USFSubsystem::ChargePlayerForPipe(UClass* PipeClass, AFGPlayerController* PlayerController, float PipeLengthCm)
{
	if (!PipeClass || !PlayerController)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: Invalid pipe class or player controller"));
		return false;
	}

	// Get the building descriptor for this pipe class
	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: No world context"));
		return false;
	}

	// Check if "No Build Cost" cheat is enabled - skip deduction but still build
	// ONLY check GetCheatNoCost() - do NOT treat Creative Mode as free building
	AFGGameState* GameState = World->GetGameState<AFGGameState>();
	if (GameState && GameState->GetCheatNoCost())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ChargePlayerForPipe: No Build Cost cheat enabled - skipping deduction"));
		return true;  // Allow build without charging
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: No recipe manager"));
		return false;
	}

	// Find the building descriptor for this pipe class
	TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
	TSubclassOf<UFGBuildingDescriptor> Descriptor = RecipeManager->FindBuildingDescriptorByClass(PipeBuildableClass);

	if (!Descriptor)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: Could not find building descriptor for %s"), *PipeClass->GetName());
		return false;
	}

	// Get the recipe from the descriptor (recipes contain the build cost)
	TArray<TSubclassOf<UFGRecipe>> Recipes = RecipeManager->FindRecipesByProduct(Descriptor, /*onlyAvailableRecipes*/ false, /*availableFirst*/ true);

	if (Recipes.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: No recipes found for descriptor %s"), *Descriptor->GetName());
		return false;
	}

	// Use the first recipe (there should only be one for buildings)
	TSubclassOf<UFGRecipe> Recipe = Recipes[0];
	const UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();

	if (!RecipeCDO)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: Could not get recipe CDO"));
		return false;
	}

	// Get the BASE build cost per segment (ingredients are for 4m segment)
	const TArray<FItemAmount>& BaseCost = RecipeCDO->GetIngredients();

	// Calculate actual cost based on pipe length
	// Pipes are charged per 4m segment (rounded up):
	// 0-3.99m = 1 segment, 4.00-7.99m = 2 segments, etc.
	float LengthInMeters = PipeLengthCm / 100.0f;
	int32 Segments = FMath::CeilToInt(LengthInMeters / 4.0f);

	// Calculate scaled cost for this pipe length
	TArray<FItemAmount> ActualCost;
	for (const FItemAmount& Cost : BaseCost)
	{
		FItemAmount ScaledCost = Cost;
		ScaledCost.Amount = Cost.Amount * Segments;
		ActualCost.Add(ScaledCost);
	}

	// Get central storage subsystem (dimensional storage)
	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);

	// Get player's inventory
	AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(PlayerController->GetPawn());
	if (!Player)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: Player pawn not found"));
		return false;
	}

	UFGInventoryComponent* Inventory = Player->GetInventory();
	if (!Inventory)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChargePlayerForPipe: Player inventory not found"));
		return false;
	}

	// Check if player has enough materials (central storage + personal inventory)
	// Match vanilla behavior: check dimensional storage first, then personal inventory
	for (const FItemAmount& Cost : ActualCost)
	{
		int32 InCentralStorage = CentralStorage ? CentralStorage->GetNumItemsFromCentralStorage(Cost.ItemClass) : 0;
		int32 InPersonalInventory = Inventory->GetNumItems(Cost.ItemClass);
		int32 TotalAvailable = InCentralStorage + InPersonalInventory;

		if (TotalAvailable < Cost.Amount)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ChargePlayerForPipe: Player cannot afford %.1fm pipe - missing %d %s (have %d in central + %d in inventory = %d total)"),
				LengthInMeters, Cost.Amount - TotalAvailable, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString(),
				InCentralStorage, InPersonalInventory, TotalAvailable);
			return false;  // Can't afford
		}
	}

	// Remove materials (central storage first, then personal inventory)
	// This matches vanilla building behavior
	for (const FItemAmount& Cost : ActualCost)
	{
		int32 Remaining = Cost.Amount;

		// Try central storage first
		if (CentralStorage && Remaining > 0)
		{
			int32 RemovedFromCentral = CentralStorage->TryRemoveItemsFromCentralStorage(Cost.ItemClass, Remaining);
			Remaining -= RemovedFromCentral;

			if (RemovedFromCentral > 0)
			{
				UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForPipe: Removed %d %s from central storage"),
					RemovedFromCentral, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString());
			}
		}

		// Remove remainder from personal inventory
		if (Remaining > 0)
		{
			Inventory->Remove(Cost.ItemClass, Remaining);
			UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForPipe: Removed %d %s from personal inventory"),
				Remaining, *UFGItemDescriptor::GetItemName(Cost.ItemClass).ToString());
		}
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("ChargePlayerForPipe: Successfully charged player for %.1fm %s pipe"),
		LengthInMeters, *PipeClass->GetName());
	return true;
}

void USFSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->OnActorSpawned(SpawnedActor);
	}

	// ========================================
	// Smart Dismantle: Blueprint Proxy Grouping (Issue #166, #270)
	// When placing a grid (>1x1x1) OR using Extend, group all spawned
	// buildings into an AFGBlueprintProxy so players can dismantle the
	// entire placement at once using vanilla's Blueprint Dismantle (R key).
	// ========================================
	if (AFGBuildable* Buildable = Cast<AFGBuildable>(SpawnedActor))
	{
		// Only group if we have an active Smart! hologram with a multi-building grid
		if (ActiveHologram.IsValid())
		{
			const FIntVector& Grid = GetGridCounters();
			const bool bIsMultiGrid = (FMath::Abs(Grid.X) > 1 || FMath::Abs(Grid.Y) > 1 || FMath::Abs(Grid.Z) > 1);
			const bool bIsExtendActive = ExtendService && ExtendService->IsExtendModeActive();  // Issue #270

			if ((bIsMultiGrid || bIsExtendActive) && !Buildable->GetBlueprintProxy())
			{
				// DIAGNOSTIC: Log what type of building is spawning
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SMART DISMANTLE: Building spawned: %s (class: %s)"),
					*Buildable->GetName(), *Buildable->GetClass()->GetName());

				// CRITICAL: Detect if this is a NEW build session (different hologram)
				// If the hologram changed, we're starting a fresh grid placement
				const bool bNewBuildSession = !CurrentProxyOwner.IsValid() || CurrentProxyOwner.Get() != ActiveHologram.Get();

				if (bNewBuildSession && CurrentBuildProxy.IsValid())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SMART DISMANTLE: New build session detected - clearing previous proxy %s"),
						*CurrentBuildProxy->GetName());
					CurrentBuildProxy.Reset();
					CurrentProxyOwner.Reset();
				}

				// Create proxy on first building of this grid session
				if (!CurrentBuildProxy.IsValid())
				{
					UWorld* World = GetWorld();
					if (World)
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						FTransform ProxyTransform = Buildable->GetActorTransform();
						AFGBlueprintProxy* NewProxy = World->SpawnActor<AFGBlueprintProxy>(
							AFGBlueprintProxy::StaticClass(),
							ProxyTransform,
							SpawnParams
						);

						if (NewProxy)
						{
							CurrentBuildProxy = NewProxy;
							CurrentProxyOwner = ActiveHologram.Get();

							// CRITICAL FIX (Issue #264): Clear the blueprint proxy flag that was set
							// by the nested OnActorSpawned during SpawnActor above. Smart-created
							// proxies (for grid grouping) must NOT block recipe application.
							// The flag is only meant to protect vanilla blueprint buildings.
							if (RecipeManagementService)
							{
								RecipeManagementService->ClearBlueprintProxyFlag();
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SMART DISMANTLE: Created BlueprintProxy %s for grid %dx%dx%d (first building: %s)"),
								*NewProxy->GetName(), Grid.X, Grid.Y, Grid.Z, *Buildable->GetClass()->GetName());
						}
					}
				}

				// Register this building with the proxy
				if (CurrentBuildProxy.IsValid())
				{
					Buildable->SetBlueprintProxy(CurrentBuildProxy.Get());
					CurrentBuildProxy->RegisterBuildable(Buildable);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SMART DISMANTLE: Registered %s (%s) with proxy %s (total: %d buildings)"),
						*Buildable->GetName(), *Buildable->GetClass()->GetName(), *CurrentBuildProxy->GetName(), CurrentBuildProxy->GetBuildables().Num());
				}
			}
		}
	}

	// Phase 3/4: Build EXTEND belts, lifts and pipes when a factory building is spawned
	// CRITICAL: Defer to next tick - factory components aren't ready at spawn time
	{
		if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(SpawnedActor))
		{
			if (ExtendService && ExtendService->HasPendingPostBuildWiring())
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Factory %s spawned - deferring connection wiring to next tick"), *Factory->GetName());

				// Capture weak reference to factory and service
				TWeakObjectPtr<AFGBuildableFactory> WeakFactory = Factory;
				TWeakObjectPtr<USFExtendService> WeakExtendService = ExtendService;

				GetWorld()->GetTimerManager().SetTimerForNextTick([WeakFactory, WeakExtendService]()
				{
					if (WeakFactory.IsValid() && WeakExtendService.IsValid())
					{
						if (!WeakExtendService->HasPendingPostBuildWiring())
						{
							return;
						}

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Deferred connection wiring executing for %s"), *WeakFactory->GetName());
						// NOTE: Child holograms (belts, lifts, pipes) are built by vanilla via AddChild
						// We just need to wire their connections here

						// Connect chain elements (for any that need inter-chain connections)
						WeakExtendService->ConnectAllChainElements(WeakFactory.Get());

						// Wire connections for child holograms that were built via AddChild
						// This uses the hologram→buildable mapping from USFHologramDataRegistry
						WeakExtendService->WireBuiltChildConnections(WeakFactory.Get());

						// DIAGNOSTIC: Capture post-build snapshot and log diff
						WeakExtendService->CapturePostBuildSnapshotAndLogDiff();
					}
					else
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: Deferred connection wiring cancelled - factory or service no longer valid"));
					}
				});
			}
		}
	}

	if (AutoConnectService && ActiveHologram.IsValid())
	{
		AFGBuildableConveyorAttachment* Attachment = Cast<AFGBuildableConveyorAttachment>(SpawnedActor);
		if (Attachment && AutoConnectService->IsDistributorHologram(ActiveHologram.Get()))
		{
			// CRITICAL: Use file-scope static flag to ensure we only process ONCE for entire grid placement
			if (bProcessingGridPlacement)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Skipping - already processing grid placement"));
				return;  // Already processing this grid, ignore subsequent spawns
			}

			if (TArray<TSharedPtr<FBeltPreviewHelper>>* PreviewsPtr = AutoConnectService->GetBeltPreviews(ActiveHologram.Get()))
			{
				if (PreviewsPtr->Num() > 0)
				{
					bProcessingGridPlacement = true;  // Lock to prevent re-entry

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Capturing %d preview(s) for deferred belt construction"), PreviewsPtr->Num());

					// CRITICAL: Extract all data from previews NOW before hologram is destroyed
					struct FBeltBuildData
					{
						TArray<FSplinePointData> SplineData;
						FString OutputConnectorName;  // Store connector name for exact lookup
						TWeakObjectPtr<UFGFactoryConnectionComponent> InputConnector;
						AFGHologram* SourceDistributorHologram;  // RAW pointer - persists as map key even after hologram destruction
						FString InputConnectorName;  // Store input connector name for manifold and merger building belts
						AFGHologram* TargetDistributorHologram = nullptr;  // For manifold belts (distributor→distributor)
						TWeakObjectPtr<UFGFactoryConnectionComponent> OutputConnector;  // For merger building belts (building output)
					};

					// Collect belt data from ALL distributors in the grid (parent + children)
					TArray<FBeltBuildData> BeltData;
					TArray<AFGHologram*> AllDistributorHolograms;  // For belt data collection only

					// Start with parent
					AllDistributorHolograms.Add(ActiveHologram.Get());

					// Add all children
					const TArray<AFGHologram*>& Children = ActiveHologram->GetHologramChildren();
					for (AFGHologram* Child : Children)
					{
						if (Child && AutoConnectService->IsDistributorHologram(Child))
						{
							AllDistributorHolograms.Add(Child);
						}
					}

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Collecting belt data from %d distributor(s) (parent + children)"), AllDistributorHolograms.Num());

					// Extract belt data from each distributor's previews
					for (AFGHologram* Distributor : AllDistributorHolograms)
					{
						if (TArray<TSharedPtr<FBeltPreviewHelper>>* DistributorPreviews = AutoConnectService->GetBeltPreviews(Distributor))
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Distributor %s has %d preview(s)"), *Distributor->GetName(), DistributorPreviews->Num());

							for (const TSharedPtr<FBeltPreviewHelper>& Preview : *DistributorPreviews)
							{
								if (!Preview.IsValid()) continue;

								UFGFactoryConnectionComponent* Conn0 = Preview->GetOutputConnector();  // Get Connection0
								UFGFactoryConnectionComponent* Conn1 = Preview->GetInputConnector();   // Get Connection1
								AFGSplineHologram* PreviewHolo = Preview->GetHologram();

								if (!Conn0 || !Conn1 || !PreviewHolo) continue;

								// CRITICAL: For manifolds, connectors are swapped (Connection0=INPUT, Connection1=OUTPUT)
								// Detect by checking connector TYPES, not positions
								bool bConn0IsOutput = (Conn0->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT);

								UFGFactoryConnectionComponent* HoloOutput = bConn0IsOutput ? Conn0 : Conn1;
								UFGFactoryConnectionComponent* BuildingInput = bConn0IsOutput ? Conn1 : Conn0;

								// Extract spline data from hologram's spline component
								USplineComponent* SplineComp = PreviewHolo->FindComponentByClass<USplineComponent>();
								if (!SplineComp) continue;

								FBeltBuildData Data;
								Data.OutputConnectorName = HoloOutput->GetName();  // Store exact connector name
								Data.InputConnector = BuildingInput;
								Data.SourceDistributorHologram = Distributor;  // Track which hologram owns this belt

								// Check if this is a manifold belt (distributor→DIFFERENT distributor)
								AActor* InputOwner = BuildingInput->GetOwner();
								AActor* OutputOwner = HoloOutput->GetOwner();

								// Check if input is on a distributor hologram (manifold or merger building belt)
								if (InputOwner && InputOwner->IsA(AFGHologram::StaticClass()))
								{
									AFGHologram* TargetHolo = Cast<AFGHologram>(InputOwner);
									if (TargetHolo && AutoConnectService->IsDistributorHologram(TargetHolo))
									{
										// Check if this is a manifold (DIFFERENT distributor) or merger building belt (SAME distributor)
										if (TargetHolo != Distributor)
										{
											// Manifold belt: Different distributor
											Data.InputConnectorName = BuildingInput->GetName();
											Data.OutputConnectorName = HoloOutput->GetName();
											Data.TargetDistributorHologram = TargetHolo;
											UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔗 MANIFOLD belt: %s → %s (Output=%s, Input=%s)"),
												*Distributor->GetName(), *TargetHolo->GetName(), *Data.OutputConnectorName, *Data.InputConnectorName);
										}
										else
										{
											// Merger building belt: Input is on the merger itself (same distributor), output is on building
											// Store input connector name AND building output connector
											Data.InputConnectorName = BuildingInput->GetName();
											Data.OutputConnector = HoloOutput;  // Store building output connector (persists until build)
											UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔗 MERGER building belt: Building %s → Merger %s (Input=%s)"),
												*HoloOutput->GetName(), *Data.InputConnectorName, *Data.InputConnectorName);
										}
									}
								}

								// Extract spline points
								int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
								for (int32 i = 0; i < NumPoints; i++)
								{
									FSplinePointData Point;
									Point.Location = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
									Point.ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
									Point.LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
									Data.SplineData.Add(Point);
								}

								BeltData.Add(Data);
							}
						}
					}

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Extracted data for %d belt(s) from %d distributor(s)"), BeltData.Num(), AllDistributorHolograms.Num());

					// CRITICAL: Only track PARENT distributor spawn!
					// Children are built as part of grid system but don't individually trigger OnActorSpawned
					// We'll find child distributors via parent's spawned children when building belts

					// Create persistent tracking structures (static to survive across OnActorSpawned calls)
					static TWeakObjectPtr<AFGBuildableConveyorAttachment> SpawnedParentDistributor;
					static AFGHologram* ParentHologram = nullptr;
					static TArray<AFGHologram*> ChildHolograms;  // CRITICAL: Store child holograms BEFORE they're destroyed
					static TArray<FBeltBuildData> PendingBeltData;
					static bool bWaitingForSpawn = false;

					// First distributor spawning - initialize tracking
					if (!bWaitingForSpawn)
					{
						SpawnedParentDistributor.Reset();
						ParentHologram = ActiveHologram.Get();  // Store parent hologram

						// CRITICAL: Store child holograms NOW before they're destroyed
						ChildHolograms.Empty();
						const TArray<AFGHologram*>& CurrentChildren = ActiveHologram->GetHologramChildren();
						for (AFGHologram* Child : CurrentChildren)
						{
							if (Child && AutoConnectService->IsDistributorHologram(Child))
							{
								ChildHolograms.Add(Child);
							}
						}

						PendingBeltData = BeltData;
						bWaitingForSpawn = true;

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Waiting for parent distributor to spawn (stored %d child holograms)"), ChildHolograms.Num());
					}

					// Store the parent distributor
					SpawnedParentDistributor = Attachment;
					bWaitingForSpawn = false;  // Parent has spawned!

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Parent distributor spawned! Building %d belt(s)"), PendingBeltData.Num());
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Skipping legacy post-build belt construction (child holograms handle belt builds)"));

					bProcessingGridPlacement = false;
					SpawnedParentDistributor.Reset();
					ParentHologram = nullptr;
					ChildHolograms.Empty();
					PendingBeltData.Empty();
					return;

					GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
					{
						UWorld* World = GetWorld();
						if (!World)
						{
							UE_LOG(LogSmartFoundations, Warning, TEXT("🎯 AUTO-CONNECT BUILD: No world context"));
							return;
						}

						// Use runtime settings (modified with U menu) - these override global config during placement
						const auto& RuntimeSettings = AutoConnectRuntimeSettings;

						// Check master switch - early exit if auto-connect disabled
						if (!RuntimeSettings.bEnabled)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Skipping - auto-connect disabled in runtime settings"));
							bProcessingGridPlacement = false;  // Reset flag
							SpawnedParentDistributor.Reset();
							ParentHologram = nullptr;
							ChildHolograms.Empty();
							PendingBeltData.Empty();
							return;
						}

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Building %d belt(s) NOW (ChainDistributors=%d, BeltTierMain=%d, BeltTierToBuilding=%d)"),
							PendingBeltData.Num(), RuntimeSettings.bChainDistributors, RuntimeSettings.BeltTierMain, RuntimeSettings.BeltTierToBuilding);

						// Build hologram->actor map from parent and its children
						TMap<AFGHologram*, AFGBuildableConveyorAttachment*> HologramToActorMap;

						if (SpawnedParentDistributor.IsValid())
						{
							AFGBuildableConveyorAttachment* ParentActor = SpawnedParentDistributor.Get();
							HologramToActorMap.Add(ParentHologram, ParentActor);

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Parent actor %s at %s"),
								*ParentActor->GetName(), *ParentActor->GetActorLocation().ToString());

							// Use stored child holograms (NOT GetHologramChildren - those are destroyed!)
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Looking for %d child distributor actors (from stored list)"), ChildHolograms.Num());

							// Match child holograms to spawned actors by proximity to EACH child position
							for (AFGHologram* ChildHolo : ChildHolograms)
							{
								if (!AutoConnectService->IsDistributorHologram(ChildHolo)) continue;

								FVector ChildHoloPos = ChildHolo->GetActorLocation();
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔍 Searching for distributor near child hologram %s at %s"),
									*ChildHolo->GetName(), *ChildHoloPos.ToString());

								// Search for distributor near THIS child hologram position
								AFGBuildableConveyorAttachment* BestMatch = nullptr;
								float BestDistance = 500.0f;  // Max 5m tolerance from child position

								for (TActorIterator<AFGBuildableConveyorAttachment> It(World); It; ++It)
								{
									AFGBuildableConveyorAttachment* PotentialChild = *It;
									if (PotentialChild == ParentActor) continue;  // Skip parent
									if (HologramToActorMap.FindKey(PotentialChild)) continue;  // Skip already matched

									float Dist = FVector::Dist(PotentialChild->GetActorLocation(), ChildHoloPos);
									if (Dist < BestDistance)
									{
										BestDistance = Dist;
										BestMatch = PotentialChild;
									}
								}

								if (BestMatch)
								{
									HologramToActorMap.Add(ChildHolo, BestMatch);
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Matched child hologram %s → actor %s (%.1f cm apart)"),
										*ChildHolo->GetName(), *BestMatch->GetName(), BestDistance);
								}
								else
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("   ❌ Could not find distributor within 500cm of child hologram %s"),
										*ChildHolo->GetName());
								}
							}
						}

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Mapped %d/%d distributors (holograms → actors)"),
							HologramToActorMap.Num(), 1 + ChildHolograms.Num());

						// Build each belt from extracted data
						int32 BuiltCount = 0;
						int32 SkippedManifolds = 0;
						int32 SkippedBuildings = 0;

						for (const FBeltBuildData& Data : PendingBeltData)
						{
							// Determine if this is a manifold belt (distributor→distributor)
							bool bIsManifoldBelt = (Data.TargetDistributorHologram != nullptr && !Data.InputConnectorName.IsEmpty());

							// Filter based on runtime settings toggles
							// Buildings are always built when auto-connect is enabled
							// Only distributor chaining can be toggled separately
							if (bIsManifoldBelt && !RuntimeSettings.bChainDistributors)
							{
								SkippedManifolds++;
								continue;  // Skip manifold belts if chaining disabled
							}

							// Get input connector (handle manifold vs regular belts)
							UFGFactoryConnectionComponent* BuildingInput = nullptr;

							// Check if this is a manifold belt (distributor→distributor)
							if (bIsManifoldBelt)
							{
								// Manifold belt: Look up target distributor and find input connector by name
								AFGBuildableConveyorAttachment** TargetDistributorPtr = HologramToActorMap.Find(Data.TargetDistributorHologram);

								if (TargetDistributorPtr && *TargetDistributorPtr)
								{
									AFGBuildableConveyorAttachment* TargetDistributor = *TargetDistributorPtr;

									// Connector names are stored correctly (no swapping needed)
									// InputConnectorName = target INPUT, OutputConnectorName = source OUTPUT
									FString TargetInputName = Data.InputConnectorName;

									// Get all connectors from target distributor
									TArray<UFGFactoryConnectionComponent*> TargetConns;
									TargetDistributor->GetComponents<UFGFactoryConnectionComponent>(TargetConns, false);

									// Find input connector by name
									for (UFGFactoryConnectionComponent* Conn : TargetConns)
									{
										if (Conn && Conn->GetDirection() == EFactoryConnectionDirection::FCD_INPUT &&
											Conn->GetName() == TargetInputName)
										{
											BuildingInput = Conn;
											UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 MANIFOLD belt: Found input %s on target %s"),
												*TargetInputName, *TargetDistributor->GetName());
											break;
										}
									}

									if (!BuildingInput)
									{
										UE_LOG(LogSmartFoundations, Warning, TEXT("Manifold belt: Could not find input connector '%s' on target distributor %s"),
											*TargetInputName, *TargetDistributor->GetName());
										continue;
									}
								}
								else
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("Manifold belt: Target distributor not found for hologram %s"),
										*GetNameSafe(Data.TargetDistributorHologram));
									continue;
								}
							}
							else if (!Data.InputConnectorName.IsEmpty() && !Data.TargetDistributorHologram)
							{
								// Merger building belt: Input connector is on the source merger (not a building)
								// Has InputConnectorName but NO TargetDistributorHologram (not a manifold)
								// Look up source distributor and find input connector by name
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔍 MERGER building belt detected: InputName='%s', TargetHolo=%s"),
									*Data.InputConnectorName, Data.TargetDistributorHologram ? TEXT("SET") : TEXT("NULL"));

								AFGBuildableConveyorAttachment** SourceDistributorPtr = HologramToActorMap.Find(Data.SourceDistributorHologram);

								if (SourceDistributorPtr && *SourceDistributorPtr)
								{
									AFGBuildableConveyorAttachment* SourceDistributor = *SourceDistributorPtr;

									// Get all connectors from source distributor (merger)
									TArray<UFGFactoryConnectionComponent*> SourceConns;
									SourceDistributor->GetComponents<UFGFactoryConnectionComponent>(SourceConns, false);

									// Find input connector by name
									for (UFGFactoryConnectionComponent* Conn : SourceConns)
									{
										if (Conn && Conn->GetDirection() == EFactoryConnectionDirection::FCD_INPUT &&
											Conn->GetName() == Data.InputConnectorName)
										{
											BuildingInput = Conn;
											UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 MERGER building belt: Found input %s on merger %s"),
												*Data.InputConnectorName, *SourceDistributor->GetName());
											break;
										}
									}

									if (!BuildingInput)
									{
										UE_LOG(LogSmartFoundations, Warning, TEXT("Merger building belt: Could not find input connector '%s' on merger %s"),
											*Data.InputConnectorName, *SourceDistributor->GetName());
										continue;
									}
								}
								else
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("Merger building belt: Source distributor not found for hologram %s"),
										*GetNameSafe(Data.SourceDistributorHologram));
									continue;
								}
							}
							else
							{
								// Regular splitter building belt: Use stored weak pointer
								if (!Data.InputConnector.IsValid())
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("Input connector is no longer valid"));
									continue;
								}
								BuildingInput = Data.InputConnector.Get();
							}

							// Find the correct spawned distributor for this belt
							AFGBuildableConveyorAttachment** SourceDistributorPtr = HologramToActorMap.Find(Data.SourceDistributorHologram);

							if (!SourceDistributorPtr || !*SourceDistributorPtr)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Spawned distributor not found for hologram %s"),
									*GetNameSafe(Data.SourceDistributorHologram));
								continue;
							}

							AFGBuildableConveyorAttachment* SourceDistributor = *SourceDistributorPtr;
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 Belt from distributor %s (hologram %s)"),
								*SourceDistributor->GetName(), *Data.SourceDistributorHologram->GetName());

							// CRITICAL: For mergers, output is on BUILDING; for splitters, output is on distributor
							UFGFactoryConnectionComponent* BestOutput = nullptr;
							bool bIsMerger = AutoConnectService->IsMergerHologram(Data.SourceDistributorHologram);

							if (!bIsManifoldBelt && bIsMerger)
							{
								// Merger building belt: Output is on the BUILDING (use stored connector)
								if (!Data.OutputConnector.IsValid())
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("Merger: Building output connector is no longer valid"));
									continue;
								}

								BestOutput = Data.OutputConnector.Get();
								AFGBuildable* BuildingActor = Cast<AFGBuildable>(BestOutput->GetOwner());

								if (BestOutput && BuildingActor)
								{
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Merger: Output %s on building %s"),
										*BestOutput->GetName(), *BuildingActor->GetName());
								}
								else
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("Merger: Could not get building from output connector"));
									continue;
								}
							}
							else
							{
								// Splitter building belt or manifold: Output is on distributor
								TArray<UFGFactoryConnectionComponent*> AllConns;
								SourceDistributor->GetComponents<UFGFactoryConnectionComponent>(AllConns, false);

								TArray<UFGFactoryConnectionComponent*> Outputs;
								for (UFGFactoryConnectionComponent* C : AllConns)
								{
									if (C && C->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
										Outputs.Add(C);
								}

								// Connector names are stored correctly (no swapping needed)
								// OutputConnectorName = source OUTPUT name
								FString SourceOutputName = Data.OutputConnectorName;

								for (UFGFactoryConnectionComponent* Candidate : Outputs)
								{
									if (Candidate && Candidate->GetName() == SourceOutputName)
									{
										BestOutput = Candidate;
										break;
									}
								}

								if (BestOutput)
								{
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Splitter/Manifold: Output %s on distributor %s"),
										*BestOutput->GetName(), *SourceDistributor->GetName());
								}
								else
								{
									UE_LOG(LogSmartFoundations, Warning, TEXT("No output '%s' on distributor %s"),
										*SourceOutputName, *SourceDistributor->GetName());
									continue;
								}
							}

							// Get connector positions and normals for routing
							FVector StartPos = BestOutput->GetComponentLocation();
							FVector EndPos = BuildingInput->GetComponentLocation();
							FVector StartNormal = BestOutput->GetConnectorNormal();
							FVector EndNormal = BuildingInput->GetConnectorNormal();

							// Spawn temporary hologram for spline routing
							FActorSpawnParameters HoloParams;
							HoloParams.bDeferConstruction = true;

							ASFConveyorBeltHologram* RoutingHologram = World->SpawnActor<ASFConveyorBeltHologram>(
								ASFConveyorBeltHologram::StaticClass(),
								StartPos,
								FRotator::ZeroRotator,
								HoloParams);

							if (!RoutingHologram)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to spawn routing hologram"));
								continue;
							}

							// Get belt class from runtime settings (respects U menu changes)
							int32 ConfiguredTier = bIsManifoldBelt ? RuntimeSettings.BeltTierMain : RuntimeSettings.BeltTierToBuilding;
							UClass* BeltClass = GetBeltClassFromConfig(ConfiguredTier, LastController.Get());

							if (!BeltClass)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to load belt class for tier %d (%s) - skipping belt"),
									ConfiguredTier, bIsManifoldBelt ? TEXT("Manifold") : TEXT("Building"));
								RoutingHologram->Destroy();
								continue;  // Belt tier unavailable or not unlocked - belt category disabled
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Using belt tier Mk%d from runtime settings for %s belt"),
								ConfiguredTier == 0 ? 5 : ConfiguredTier, bIsManifoldBelt ? TEXT("manifold") : TEXT("building"));

							RoutingHologram->SetBuildClass(BeltClass);
							RoutingHologram->FinishSpawning(FTransform(FRotator::ZeroRotator, StartPos));

							// Route spline using connector normals
							RoutingHologram->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);

							// Extract spline data from hologram
							TArray<FSplinePointData> RoutedSplineData = RoutingHologram->GetSplinePointData();

							if (RoutedSplineData.Num() == 0)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Hologram produced no spline data"));
								RoutingHologram->Destroy();
								continue;
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 BEFORE Respline: Hologram routed belt with %d spline points"), RoutedSplineData.Num());
							for (int32 i = 0; i < RoutedSplineData.Num(); i++)
							{
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Point %d: ArriveTangent=%.1f, LeaveTangent=%.1f"),
									i,
									RoutedSplineData[i].ArriveTangent.Size(),
									RoutedSplineData[i].LeaveTangent.Size());
							}

							// Approximate belt length from routed spline data (sum of segment lengths in cm)
							float BeltLengthCm = 0.0f;
							for (int32 PointIdx = 1; PointIdx < RoutedSplineData.Num(); ++PointIdx)
							{
								BeltLengthCm += FVector::Dist(RoutedSplineData[PointIdx - 1].Location, RoutedSplineData[PointIdx].Location);
							}

							// NOTE: Belt costs are now handled by child holograms via GetCost()
							// Vanilla deducts the combined cost when the hologram is built, so we skip
							// manual deduction here to avoid double-charging the player.
							// ChargePlayerForBelt is kept for legacy/fallback but not called in normal flow.

							// Spawn actual belt
							AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(
								BeltClass,
								StartPos,
								FRotator::ZeroRotator);

							if (!Belt)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to spawn belt actor"));
								RoutingHologram->Destroy();
								continue;
							}

							// Apply hologram's spline to belt
							AFGBuildableConveyorBelt* ResplinedBelt = AFGBuildableConveyorBelt::Respline(Belt, RoutedSplineData);

							// Cleanup hologram
							RoutingHologram->Destroy();

							if (!ResplinedBelt)
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("Respline failed"));
								Belt->Destroy();
								continue;
							}

							// Finalize belt
							Belt->OnBuildEffectFinished();

							// Connect the belt endpoints
							// AutoRouteSpline routes from StartPos (Output) to EndPos (Input)
							// Therefore Connection0 is at Output position, Connection1 is at Input position
							// ALL belts use the same connection order: Conn0=Output, Conn1=Input
							ResplinedBelt->GetConnection0()->SetConnection(BestOutput);     // Output end
							ResplinedBelt->GetConnection1()->SetConnection(BuildingInput);  // Input end

							if (bIsManifoldBelt)
							{
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 Manifold belt connected correctly: %s→%s"),
									*BestOutput->GetName(), *BuildingInput->GetName());
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Belt spawned and connected!"));
							BuiltCount++;
						}

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BUILD: Built %d belt(s), Skipped %d manifold(s), Skipped %d building(s)"),
							BuiltCount, SkippedManifolds, SkippedBuildings);

						// CRITICAL: Reset static variables for next placement
						SpawnedParentDistributor.Reset();
						ParentHologram = nullptr;
						ChildHolograms.Empty();  // Clear stored child holograms
						PendingBeltData.Empty();
						bWaitingForSpawn = false;
						bProcessingGridPlacement = false;  // Unlock for next placement

						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT BUILD: Reset tracking for next placement"));
					});
				}
			}
		}

		// ========================================
		// PIPE AUTO-CONNECT CHILD DEFERRED WIRING (Issue #235)
		// ========================================
		// With child holograms, vanilla builds the pipes automatically.
		// We just need deferred wiring to connect them after all junctions are built.
		if (AutoConnectService)
		{
			AFGBuildablePipeline* AutoConnectPipe = Cast<AFGBuildablePipeline>(SpawnedActor);
			if (AutoConnectPipe && SpawnedActor->Tags.Contains(FName(TEXT("SF_PipeAutoConnectChild"))))
			{
				// This is a pipe spawned by our child hologram system - track for deferred wiring
				static TArray<TWeakObjectPtr<AFGBuildablePipeline>> PendingAutoConnectPipes;
				static TWeakObjectPtr<AFGHologram> LastPipeAutoConnectHologram;
				static FTimerHandle PipeAutoConnectWiringTimerHandle;

				// Reset tracking when hologram changes (if available)
				if (ActiveHologram.IsValid())
				{
					if (ActiveHologram.Get() != LastPipeAutoConnectHologram.Get())
					{
						LastPipeAutoConnectHologram = ActiveHologram;
						PendingAutoConnectPipes.Empty();
					}
				}
				else
				{
					LastPipeAutoConnectHologram.Reset();
				}

				// Track this pipe for deferred wiring
				PendingAutoConnectPipes.Add(AutoConnectPipe);

				// Build tag list manually (TArray<FName> has no ToStringSimple)
				FString TagList;
				for (int32 TagIdx = 0; TagIdx < SpawnedActor->Tags.Num(); TagIdx++)
				{
					TagList += SpawnedActor->Tags[TagIdx].ToString();
					if (TagIdx + 1 < SpawnedActor->Tags.Num())
					{
						TagList += TEXT(",");
					}
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT SPAWN: Tracking %s for deferred wiring (%d pending). Tags=%s"),
					*AutoConnectPipe->GetName(), PendingAutoConnectPipes.Num(), *TagList);

				// Set/reset timer to wire pipes on next tick (after all junctions are spawned)
				GetWorld()->GetTimerManager().ClearTimer(PipeAutoConnectWiringTimerHandle);
				GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT WIRING: Processing %d pipe(s)..."), PendingAutoConnectPipes.Num());

					int32 ConnectionsMade = 0;
					const float ConnectionRadius = 100.0f;  // 1m tolerance for connector matching

					// Collect valid pipes
					TArray<AFGBuildablePipeline*> ValidPipes;
					for (const auto& WeakPipe : PendingAutoConnectPipes)
					{
						if (WeakPipe.IsValid())
						{
							ValidPipes.Add(WeakPipe.Get());
						}
					}

					// Wire each pipe's unconnected endpoints to nearby built connectors
					for (AFGBuildablePipeline* Pipe : ValidPipes)
					{
						UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
						UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();

						// Wire Conn0 to nearby unconnected pipe connector
						if (Conn0 && !Conn0->IsConnected())
						{
							FVector SearchLoc = Conn0->GetComponentLocation();
							UFGPipeConnectionComponent* BestMatch = nullptr;
							float BestDist = ConnectionRadius;

							for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
							{
								AFGBuildable* Buildable = *It;
								if (Buildable == Pipe) continue;

								TArray<UFGPipeConnectionComponent*> Connectors;
								Buildable->GetComponents<UFGPipeConnectionComponent>(Connectors);

								for (UFGPipeConnectionComponent* Conn : Connectors)
								{
									if (!Conn || Conn->IsConnected()) continue;

									float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
									if (Dist < BestDist)
									{
										BestDist = Dist;
										BestMatch = Conn;
									}
								}
							}

							if (BestMatch)
							{
								Conn0->SetConnection(BestMatch);
								ConnectionsMade++;
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT: Wired %s.Conn0 → %s.%s (dist=%.1f)"),
									*Pipe->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
							}
						}

						// Wire Conn1 to nearby unconnected pipe connector
						if (Conn1 && !Conn1->IsConnected())
						{
							FVector SearchLoc = Conn1->GetComponentLocation();
							UFGPipeConnectionComponent* BestMatch = nullptr;
							float BestDist = ConnectionRadius;

							for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
							{
								AFGBuildable* Buildable = *It;
								if (Buildable == Pipe) continue;

								TArray<UFGPipeConnectionComponent*> Connectors;
								Buildable->GetComponents<UFGPipeConnectionComponent>(Connectors);

								for (UFGPipeConnectionComponent* Conn : Connectors)
								{
									if (!Conn || Conn->IsConnected()) continue;

									float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
									if (Dist < BestDist)
									{
										BestDist = Dist;
										BestMatch = Conn;
									}
								}
							}

							if (BestMatch)
							{
								Conn1->SetConnection(BestMatch);
								ConnectionsMade++;
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT: Wired %s.Conn1 → %s.%s (dist=%.1f)"),
									*Pipe->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
							}
						}
					}

					// Merge pipe networks for connected pipes
					if (ConnectionsMade > 0)
					{
						AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(GetWorld());
						if (PipeSubsystem)
						{
							for (AFGBuildablePipeline* Pipe : ValidPipes)
							{
								if (UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0())
								{
									if (Conn0->IsConnected())
									{
										int32 NetworkID = Conn0->GetPipeNetworkID();
										if (AFGPipeNetwork* Network = PipeSubsystem->FindPipeNetwork(NetworkID))
										{
											Network->MarkForFullRebuild();
										}
									}
								}
							}
						}
					}

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT WIRING: Made %d connection(s)"), ConnectionsMade);
					PendingAutoConnectPipes.Empty();
				});
			}
		}

		// ========================================
		// PIPE JUNCTION AUTO-CONNECT BUILD (DISABLED - Issue #235)
		// ========================================
		// This legacy deferred build system is now disabled. Pipe children are built via the
		// vanilla child hologram Construct() mechanism with post-build wiring in SFPipelineHologram.cpp.
		// The two systems were competing, causing pipes to be built but not properly wired.
		AFGBuildablePipelineAttachment* PipeAttachment = Cast<AFGBuildablePipelineAttachment>(SpawnedActor);
		if (PipeAttachment && AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()))
		{
			// DISABLED: Vanilla child hologram system now handles pipe building via Construct()
			// The wiring is done in SFPipelineHologram::Construct() using FindCompatibleOverlappingConnection()
			// Child holograms are automatically cleaned up by vanilla when parent is destroyed.
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT BUILD: Pipeline junction spawned - using vanilla child hologram system"));
			return;  // Skip legacy deferred build system

			// === LEGACY CODE BELOW - KEPT FOR REFERENCE ===
			if (bProcessingGridPlacement)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT BUILD: Skipping - already processing grid"));
				return;
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT BUILD: Pipeline junction spawned - construction system active"));

			// Get pipe previews from the pipe auto-connect manager
			FSFPipeAutoConnectManager* PipeManager = AutoConnectService->GetPipeManager(ActiveHologram.Get());
			if (!PipeManager)
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE AUTO-CONNECT BUILD: No pipe manager found for junction"));
				return;
			}

			// Check if we have any pipe previews to build
			const auto& BuildingPreviews = PipeManager->GetBuildingPipePreviews();
			const auto& ManifoldPreviews = PipeManager->GetManifoldPipePreviews();
			int32 TotalPreviews = BuildingPreviews.Num() + ManifoldPreviews.Num();

			if (TotalPreviews == 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: No pipe previews to build"));
				return;
			}

			bProcessingGridPlacement = true;

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Capturing %d pipe preview(s) for deferred construction"), TotalPreviews);

			// CRITICAL: Extract all data from previews NOW before hologram is destroyed
			struct FPipeBuildData
			{
				TArray<FSplinePointData> SplineData;
				TWeakObjectPtr<UFGPipeConnectionComponent> StartConnector;
				TWeakObjectPtr<UFGPipeConnectionComponent> EndConnector;
				AFGHologram* SourceJunctionHologram;  // RAW pointer - persists as map key
				AFGHologram* TargetJunctionHologram = nullptr;  // For manifold pipes
				bool bIsManifold = false;  // True if junction→junction, false if junction→building
				int32 PipeTier = 1;  // Mk1 for now (will use PipeTierMain or PipeTierToBuilding later)

				// Connector reconstruction metadata (for when hologram-side connectors are destroyed)
				int32 StartConnectorIndex = INDEX_NONE;
				int32 EndConnectorIndex = INDEX_NONE;
				bool bStartIsOnJunction = false;
				bool bEndIsOnJunction = false;
				AFGHologram* StartConnectorOwner = nullptr;
				AFGHologram* EndConnectorOwner = nullptr;
			};

			// Collect pipe data from ALL junctions in the grid
			TArray<FPipeBuildData> PipeData;
			TArray<AFGHologram*> AllJunctionHolograms;

			// Start with parent
			AllJunctionHolograms.Add(ActiveHologram.Get());

			// Add all children
			const TArray<AFGHologram*>& Children = ActiveHologram->GetHologramChildren();
			for (AFGHologram* Child : Children)
			{
				if (Child && AutoConnectService->IsPipelineJunctionHologram(Child))
				{
					AllJunctionHolograms.Add(Child);
				}
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Collecting pipe data from %d junction(s)"), AllJunctionHolograms.Num());

			// Extract pipe data from each junction's previews
			for (AFGHologram* Junction : AllJunctionHolograms)
			{
				FSFPipeAutoConnectManager* JunctionManager = AutoConnectService->GetPipeManager(Junction);
				if (!JunctionManager) continue;

				// Extract building pipe data (junction→building)
				for (const auto& Pair : JunctionManager->GetBuildingPipePreviews())
				{
					if (!Pair.Value.IsValid()) continue;
					FPipePreviewHelper* PreviewHelper = Pair.Value.Get();

					if (!PreviewHelper->IsPreviewValid()) continue;

					AFGSplineHologram* PreviewHolo = PreviewHelper->GetHologram();
					if (!PreviewHolo) continue;

					FPipeBuildData Data;
					Data.SourceJunctionHologram = Junction;
					Data.bIsManifold = false;
					Data.PipeTier = PreviewHelper->GetPipeTier();

					UFGPipeConnectionComponent* StartConn = PreviewHelper->GetStartConnection();
					UFGPipeConnectionComponent* EndConn = PreviewHelper->GetEndConnection();
					Data.StartConnector = StartConn;
					Data.EndConnector = EndConn;

					// Record junction-side connector indices for reconstruction after hologram destruction
					if (StartConn)
					{
						if (AFGHologram* StartOwnerHolo = Cast<AFGHologram>(StartConn->GetOwner()))
						{
							Data.bStartIsOnJunction = true;
							Data.StartConnectorOwner = StartOwnerHolo;
							TArray<UFGPipeConnectionComponent*> JunctionConns;
							FSFPipeConnectorFinder::GetJunctionConnectors(StartOwnerHolo, JunctionConns);
							Data.StartConnectorIndex = JunctionConns.IndexOfByKey(StartConn);
						}
					}
					if (EndConn)
					{
						if (AFGHologram* EndOwnerHolo = Cast<AFGHologram>(EndConn->GetOwner()))
						{
							Data.bEndIsOnJunction = true;
							Data.EndConnectorOwner = EndOwnerHolo;
							TArray<UFGPipeConnectionComponent*> JunctionConns;
							FSFPipeConnectorFinder::GetJunctionConnectors(EndOwnerHolo, JunctionConns);
							Data.EndConnectorIndex = JunctionConns.IndexOfByKey(EndConn);
						}
					}

					// CRITICAL: Extract spline points - use 6-point BUILD spline, not 2-point PREVIEW!
					if (ASFPipelineHologram* SmartPipeHolo = Cast<ASFPipelineHologram>(PreviewHolo))
					{
						// Use the 6-point build spline from our Smart! hologram
						Data.SplineData = SmartPipeHolo->GetBuildSplineData();
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Using 6-point BUILD spline (%d points) for pipe construction"),
							Data.SplineData.Num());
					}
					else
					{
						// Fallback: Read from spline component (vanilla holograms)
						USplineComponent* SplineComp = PreviewHolo->FindComponentByClass<USplineComponent>();
						if (SplineComp)
						{
							int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
							for (int32 i = 0; i < NumPoints; i++)
							{
								FSplinePointData Point;
								Point.Location = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Point.ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Point.LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Data.SplineData.Add(Point);
							}
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Using spline component data (%d points) for vanilla hologram"),
								NumPoints);
						}
					}

					PipeData.Add(Data);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Building pipe: Junction %s → Building"), *Junction->GetName());
				}

				// Extract manifold pipe data (junction→junction)
				for (const auto& Pair : JunctionManager->GetManifoldPipePreviews())
				{
					if (!Pair.Value.IsValid()) continue;
					FPipePreviewHelper* PreviewHelper = Pair.Value.Get();

					if (!PreviewHelper->IsPreviewValid()) continue;

					AFGSplineHologram* PreviewHolo = PreviewHelper->GetHologram();
					if (!PreviewHolo) continue;

					FPipeBuildData Data;
					Data.SourceJunctionHologram = Junction;
					Data.TargetJunctionHologram = Pair.Key;  // Target junction key from map
					Data.bIsManifold = true;
					Data.PipeTier = PreviewHelper->GetPipeTier();

					UFGPipeConnectionComponent* StartConn = PreviewHelper->GetStartConnection();
					UFGPipeConnectionComponent* EndConn = PreviewHelper->GetEndConnection();
					Data.StartConnector = StartConn;
					Data.EndConnector = EndConn;

					// Record junction-side connector indices for reconstruction after hologram destruction
					if (StartConn)
					{
						if (AFGHologram* StartOwnerHolo = Cast<AFGHologram>(StartConn->GetOwner()))
						{
							Data.bStartIsOnJunction = true;
							Data.StartConnectorOwner = StartOwnerHolo;
							TArray<UFGPipeConnectionComponent*> JunctionConns;
							FSFPipeConnectorFinder::GetJunctionConnectors(StartOwnerHolo, JunctionConns);
							Data.StartConnectorIndex = JunctionConns.IndexOfByKey(StartConn);
						}
					}
					if (EndConn)
					{
						if (AFGHologram* EndOwnerHolo = Cast<AFGHologram>(EndConn->GetOwner()))
						{
							Data.bEndIsOnJunction = true;
							Data.EndConnectorOwner = EndOwnerHolo;
							TArray<UFGPipeConnectionComponent*> JunctionConns;
							FSFPipeConnectorFinder::GetJunctionConnectors(EndOwnerHolo, JunctionConns);
							Data.EndConnectorIndex = JunctionConns.IndexOfByKey(EndConn);
						}
					}

					// CRITICAL: Extract spline points - use 6-point BUILD spline, not 2-point PREVIEW!
					if (ASFPipelineHologram* SmartPipeHolo = Cast<ASFPipelineHologram>(PreviewHolo))
					{
						// Use the 6-point build spline from our Smart! hologram
						Data.SplineData = SmartPipeHolo->GetBuildSplineData();
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Using 6-point BUILD spline (%d points) for manifold pipe construction"),
							Data.SplineData.Num());
					}
					else
					{
						// Fallback: Read from spline component (vanilla holograms)
						USplineComponent* SplineComp = PreviewHolo->FindComponentByClass<USplineComponent>();
						if (SplineComp)
						{
							int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
							for (int32 i = 0; i < NumPoints; i++)
							{
								FSplinePointData Point;
								Point.Location = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Point.ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Point.LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
								Data.SplineData.Add(Point);
							}
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Using spline component data (%d points) for vanilla hologram"),
								NumPoints);
						}
					}

					PipeData.Add(Data);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Manifold pipe: Junction %s → Junction %s"),
						*Junction->GetName(), *Pair.Key->GetName());
				}
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" PIPE AUTO-CONNECT BUILD: Extracted data for %d pipe(s)"), PipeData.Num());

			// Create persistent tracking structures (static to survive across OnActorSpawned calls)
			static TWeakObjectPtr<AFGBuildablePipelineAttachment> SpawnedParentJunction;
			static AFGHologram* ParentJunctionHologram = nullptr;
			static TArray<AFGHologram*> ChildJunctionHolograms;
			static TArray<FPipeBuildData> PendingPipeData;
			static bool bWaitingForJunctionSpawn = false;

			// First junction spawning - initialize tracking
			if (!bWaitingForJunctionSpawn)
			{
				SpawnedParentJunction.Reset();
				ParentJunctionHologram = ActiveHologram.Get();

				// Store child holograms NOW before they're destroyed
				ChildJunctionHolograms.Empty();
				const TArray<AFGHologram*>& CurrentChildren = ActiveHologram->GetHologramChildren();
				for (AFGHologram* Child : CurrentChildren)
				{
					if (Child && AutoConnectService->IsPipelineJunctionHologram(Child))
					{
						ChildJunctionHolograms.Add(Child);
					}
				}

				PendingPipeData = PipeData;
				bWaitingForJunctionSpawn = true;

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Waiting for parent junction to spawn (stored %d children)"), ChildJunctionHolograms.Num());
			}

			// Store the parent junction
			SpawnedParentJunction = PipeAttachment;
			bWaitingForJunctionSpawn = false;

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Parent junction spawned! Building %d pipe(s)"), PendingPipeData.Num());

			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				UWorld* World = GetWorld();
				if (!World)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE AUTO-CONNECT BUILD: No world context"));
					bProcessingGridPlacement = false;
					return;
				}

				// Build hologram→actor map from parent and children
				TMap<AFGHologram*, AFGBuildablePipelineAttachment*> HologramToActorMap;

				if (SpawnedParentJunction.IsValid())
				{
					AFGBuildablePipelineAttachment* ParentActor = SpawnedParentJunction.Get();
					HologramToActorMap.Add(ParentJunctionHologram, ParentActor);

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Parent junction %s at %s"),
						*ParentActor->GetName(), *ParentActor->GetActorLocation().ToString());

					// Match child holograms to spawned actors by proximity
					for (AFGHologram* ChildHolo : ChildJunctionHolograms)
					{
						if (!AutoConnectService->IsPipelineJunctionHologram(ChildHolo)) continue;

						FVector ChildHoloPos = ChildHolo->GetActorLocation();

						AFGBuildablePipelineAttachment* BestMatch = nullptr;
						float BestDistance = 500.0f;  // Max 5m tolerance

						for (TActorIterator<AFGBuildablePipelineAttachment> It(World); It; ++It)
						{
							AFGBuildablePipelineAttachment* PotentialChild = *It;
							if (PotentialChild == ParentActor) continue;
							if (HologramToActorMap.FindKey(PotentialChild)) continue;

							float Dist = FVector::Dist(PotentialChild->GetActorLocation(), ChildHoloPos);
							if (Dist < BestDistance)
							{
								BestDistance = Dist;
								BestMatch = PotentialChild;
							}
						}

						if (BestMatch)
						{
							HologramToActorMap.Add(ChildHolo, BestMatch);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Matched child junction %s → %s (%.1f cm)"),
								*ChildHolo->GetName(), *BestMatch->GetName(), BestDistance);
						}
					}
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Mapped %d junctions"), HologramToActorMap.Num());

				// Build each pipe from extracted data
				int32 BuiltCount = 0;

				for (const FPipeBuildData& Data : PendingPipeData)
				{
					// Start with whatever pointers survived hologram destruction
					UFGPipeConnectionComponent* StartConn = Data.StartConnector.Get();
					UFGPipeConnectionComponent* EndConn = Data.EndConnector.Get();

					// Attempt to reconstruct junction-side connectors using stored owner/index
					if (!StartConn && Data.bStartIsOnJunction && Data.StartConnectorOwner)
					{
						if (AFGBuildablePipelineAttachment* RealActor = HologramToActorMap.FindRef(Data.StartConnectorOwner))
						{
							TArray<UFGPipeConnectionComponent*> RealConns;
							RealActor->GetComponents<UFGPipeConnectionComponent>(RealConns);
							if (RealConns.IsValidIndex(Data.StartConnectorIndex))
							{
								StartConn = RealConns[Data.StartConnectorIndex];
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Reconstructed StartConn from index %d on actor: %s"), Data.StartConnectorIndex, *StartConn->GetName());
							}
						}
					}
					if (!EndConn && Data.bEndIsOnJunction && Data.EndConnectorOwner)
					{
						if (AFGBuildablePipelineAttachment* RealActor = HologramToActorMap.FindRef(Data.EndConnectorOwner))
						{
							TArray<UFGPipeConnectionComponent*> RealConns;
							RealActor->GetComponents<UFGPipeConnectionComponent>(RealConns);
							if (RealConns.IsValidIndex(Data.EndConnectorIndex))
							{
								EndConn = RealConns[Data.EndConnectorIndex];
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Reconstructed EndConn from index %d on actor: %s"), Data.EndConnectorIndex, *EndConn->GetName());
							}
						}
					}

					if (!StartConn || !EndConn)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("Pipe connectors no longer valid after reconstruction attempt"));
						continue;
					}

					// IMPORTANT: If start/end is on a hologram, swap it for the spawned actor's connector
					AActor* StartOwner = StartConn->GetOwner();
					AActor* EndOwner = EndConn->GetOwner();

					// Get connector positions
					FVector StartPos = StartConn->GetComponentLocation();
					FVector EndPos = EndConn->GetComponentLocation();

					// Approximate pipe length
					float PipeLengthCm = 0.0f;
					if (Data.SplineData.Num() > 1)
					{
						for (int32 i = 1; i < Data.SplineData.Num(); i++)
						{
							PipeLengthCm += FVector::Dist(Data.SplineData[i-1].Location, Data.SplineData[i].Location);
						}
					}
					else
					{
						PipeLengthCm = FVector::Dist(StartPos, EndPos);
					}

					// Get pipe class from CURRENT runtime settings (not preview's stored tier)
					// This ensures settings changed after preview creation are respected
					int32 ConfigTier = Data.bIsManifold
						? AutoConnectRuntimeSettings.PipeTierMain
						: AutoConnectRuntimeSettings.PipeTierToBuilding;
					bool bWithIndicator = AutoConnectRuntimeSettings.bPipeIndicator;
					UClass* PipeClass = GetPipeClassFromConfig(ConfigTier, bWithIndicator, LastController.Get());

					if (!PipeClass)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to load pipe class for config tier=%d, manifold=%d, indicator=%d - skipping pipe"),
							ConfigTier, Data.bIsManifold, bWithIndicator);
						continue;
					}

					// NOTE: Pipe costs are now handled by child holograms via GetCost()
					// Vanilla deducts the combined cost when the hologram is built, so we skip
					// manual deduction here to avoid double-charging the player.
					// ChargePlayerForPipe is kept for legacy/fallback but not called in normal flow.

					// Spawn actual pipe with DEFERRED construction (set spline before BeginPlay)
					FActorSpawnParameters SpawnParams;
					SpawnParams.bDeferConstruction = true;

					AFGBuildablePipeline* Pipe = World->SpawnActor<AFGBuildablePipeline>(
						PipeClass,
						StartPos,
						FRotator::ZeroRotator,
						SpawnParams);

					if (!Pipe)
					{
						UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to spawn pipe actor"));
						continue;
					}

					// Apply spline data to pipe BEFORE FinishSpawning (BeginPlay checks mSplineData.Num() >= 2)
					// CRITICAL: Must populate mSplineData directly, not just USplineComponent
					TArray<FSplinePointData>* PipeSplineData = Pipe->GetMutableSplinePointData();
					if (PipeSplineData && Data.SplineData.Num() >= 2)
					{
						*PipeSplineData = Data.SplineData;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Set pipe mSplineData: %d points"), PipeSplineData->Num());
					}

					// Finish spawning (triggers BeginPlay with spline configured)
					FTransform PipeTransform(FRotator::ZeroRotator, StartPos);
					Pipe->FinishSpawning(PipeTransform);

					// Finalize pipe
					Pipe->OnBuildEffectFinished();

					// Connect the pipe endpoints
					Pipe->GetPipeConnection0()->SetConnection(StartConn);
					Pipe->GetPipeConnection1()->SetConnection(EndConn);

					// Determine actual tier from config for logging
					int32 LogTier = Data.bIsManifold
						? AutoConnectRuntimeSettings.PipeTierMain
						: AutoConnectRuntimeSettings.PipeTierToBuilding;
					if (LogTier == 0)  // Auto mode
					{
						LogTier = GetHighestUnlockedPipeTier(LastController.Get());
					}

					if (Data.bIsManifold)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 Manifold Mk%d pipe connected: %s→%s"),
							LogTier, *StartConn->GetName(), *EndConn->GetName());
					}
					else
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 Building Mk%d pipe connected: Junction→Building"),
							LogTier);
					}

					BuiltCount++;
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Built %d pipe(s)"), BuiltCount);

				// Reset static variables
				SpawnedParentJunction.Reset();
				ParentJunctionHologram = nullptr;
				ChildJunctionHolograms.Empty();
				PendingPipeData.Empty();
				bWaitingForJunctionSpawn = false;
				bProcessingGridPlacement = false;

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT BUILD: Reset tracking for next placement"));
			});
		}
	}

	// ========================================
	// STACKABLE PIPELINE SUPPORT AUTO-CONNECT BUILD (Issue #220)
	// ========================================
	// Vanilla doesn't build our dynamically added pipe children, so we must build them manually.
	// When a stackable pipe support is built, find any SF_StackableChild pipe holograms and build them.
	//
	// CRITICAL: This code runs for EVERY pole that gets built (parent + children).
	// We use a static flag to ensure pipes are only built ONCE per placement operation.
	// The flag is reset when the hologram changes.
	FString SpawnedClassName = SpawnedActor->GetClass()->GetName();

	bool bIsStackablePipeSupport = (SpawnedClassName.Contains(TEXT("PipeSupportStackable")) ||
	                                SpawnedClassName.Contains(TEXT("PipelineStackable"))) &&
	                               SpawnedClassName.StartsWith(TEXT("Build_"));

	// Track if we've already built pipes for this placement operation
	static TWeakObjectPtr<AFGHologram> LastPipeBuildHologram;
	static bool bPipesAlreadyBuiltForThisPlacement = false;

	// Reset tracking when hologram changes
	if (ActiveHologram.Get() != LastPipeBuildHologram.Get())
	{
		LastPipeBuildHologram = ActiveHologram;
		bPipesAlreadyBuiltForThisPlacement = false;
	}

	if (bIsStackablePipeSupport && ActiveHologram.IsValid() && !bPipesAlreadyBuiltForThisPlacement)
	{
		// Check if parent hologram has SF_StackableChild pipe children to build
		if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
		{
			TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ActiveHologram.Get());
			if (ChildrenArray)
			{
				// PHASE 1: Build all pipes first (collect them for connection phase)
				TArray<AFGBuildablePipeline*> BuiltPipes;

				for (AFGHologram* Child : *ChildrenArray)
				{
					if (Child && Child->Tags.Contains(FName(TEXT("SF_StackableChild"))))
					{
						// This is one of our pipe children - build it manually
						TArray<AActor*> ChildActors;
						FNetConstructionID DummyID;
						AActor* BuiltPipe = Child->Construct(ChildActors, DummyID);

						if (BuiltPipe)
						{
							if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(BuiltPipe))
							{
								BuiltPipes.Add(Pipeline);
							}
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE BUILD: Manually built pipe %s"), *BuiltPipe->GetName());
						}
						else
						{
							UE_LOG(LogSmartFoundations, Warning, TEXT("STACKABLE PIPE BUILD: Failed to build pipe child %s"), *Child->GetName());
						}
					}
				}

				// PHASE 2: Connect pipes to each other at shared pole locations
				// Now that all pipes exist, we can find neighbors
				if (BuiltPipes.Num() > 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE BUILD: Built %d pipe(s), now connecting..."), BuiltPipes.Num());

					int32 ConnectionsMade = 0;

					// Helper to check if an actor is in our built list
					auto IsNewlyBuilt = [&](AActor* Actor) -> bool
					{
						for (AFGBuildablePipeline* P : BuiltPipes) if (P == Actor) return true;
						return false;
					};

					for (AFGBuildablePipeline* Pipe : BuiltPipes)
					{
						UFGPipeConnectionComponent* Conns[] = { Pipe->GetPipeConnection0(), Pipe->GetPipeConnection1() };

						for (UFGPipeConnectionComponent* Conn : Conns)
						{
							if (!Conn || Conn->IsConnected()) continue;

							bool bConnected = false;

							// 1. Try to connect to other newly built pipes (Manual distance check, bypasses physics)
							for (AFGBuildablePipeline* OtherPipe : BuiltPipes)
							{
								if (OtherPipe == Pipe) continue;

								UFGPipeConnectionComponent* OtherConns[] = { OtherPipe->GetPipeConnection0(), OtherPipe->GetPipeConnection1() };
								for (UFGPipeConnectionComponent* OtherConn : OtherConns)
								{
									if (OtherConn && !OtherConn->IsConnected())
									{
										// 50cm tolerance (squared)
										if (FVector::DistSquared(Conn->GetComponentLocation(), OtherConn->GetComponentLocation()) < 2500.0f)
										{
											Conn->SetConnection(OtherConn);
											ConnectionsMade++;
											bConnected = true;
											UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE: Connected %s to %s (Internal)"),
												*Pipe->GetName(), *OtherPipe->GetName());
											break;
										}
									}
								}
								if (bConnected) break;
							}

							// 2. If not connected, try external world pipes (Physics check for existing pipes)
							if (!bConnected)
							{
								FVector SearchLoc = Conn->GetComponentLocation();
								UFGPipeConnectionComponentBase* Neighbor = UFGPipeConnectionComponentBase::FindCompatibleOverlappingConnection(
									Conn, SearchLoc, Pipe, 50.0f, {});

								// Ensure we don't reconnect to something we just built (though loop above should handle it)
								// and ensure it's not the pipe itself
								if (Neighbor && !Neighbor->IsConnected() && Neighbor->GetOwner() != Pipe && !IsNewlyBuilt(Neighbor->GetOwner()))
								{
									Conn->SetConnection(Neighbor);
									ConnectionsMade++;
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE: Connected %s to %s (External)"),
										*Pipe->GetName(), *Neighbor->GetOwner()->GetName());
								}
							}
						}
					}

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE BUILD: Made %d connection(s)"), ConnectionsMade);

					// PHASE 3: Merge pipe networks
					if (ConnectionsMade > 0)
					{
						AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(GetWorld());
						if (PipeSubsystem)
						{
							TSet<int32> NetworksToRebuild;
							for (AFGBuildablePipeline* Pipe : BuiltPipes)
							{
								UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
								UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();
								if (Conn0) NetworksToRebuild.Add(Conn0->GetPipeNetworkID());
								if (Conn1) NetworksToRebuild.Add(Conn1->GetPipeNetworkID());
							}

							for (int32 NetworkID : NetworksToRebuild)
							{
								if (NetworkID != INDEX_NONE)
								{
									if (AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(NetworkID))
									{
										Net->MarkForFullRebuild();
									}
								}
							}
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE PIPE BUILD: Marked %d network(s) for rebuild"), NetworksToRebuild.Num());
						}
					}

					bPipesAlreadyBuiltForThisPlacement = true;  // Prevent duplicate builds for subsequent poles
				}
			}
		}
	}

	// ========================================
	// STACKABLE CONVEYOR POLE AUTO-CONNECT BUILD (Issue #220)
	// ========================================
	// Check if this is a stackable conveyor pole AND we have belt previews cached
	// Detection by class name since we don't have a specific buildable base class
	// NOTE: SpawnedClassName already declared above for pipe support detection
	// CRITICAL: Must exclude holograms - only detect actual built actors (Build_* prefix)

	bool bIsStackableConveyorPole = (SpawnedClassName.Contains(TEXT("ConveyorPoleStackable")) ||
	                                SpawnedClassName.Contains(TEXT("ConveyorCeilingAttachment")) ||
	                                SpawnedClassName.Contains(TEXT("ConveyorPoleWall"))) &&
	                                SpawnedClassName.StartsWith(TEXT("Build_"));

	// DISABLED: Manual belt spawning causes crashes (array index -1 in Factory_UpdateRadioactivity)
	// Belts spawned via SpawnActor bypass vanilla's proper initialization.
	// Belt creation should be handled by the hologram system through preview holograms.
	// See Issue #220 for investigation details.
	if (false && bIsStackableConveyorPole && bGStackableBeltDataCached && GCachedStackableBeltData.Num() > 0 && !bProcessingGridPlacement)
	{
		bProcessingGridPlacement = true;

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("STACKABLE CONVEYOR POLE AUTO-CONNECT BUILD: Using %d cached belt(s)"), GCachedStackableBeltData.Num());

		// Move cached data to local for deferred processing
		TArray<FStackableBeltBuildData> BeltData = MoveTemp(GCachedStackableBeltData);
		bGStackableBeltDataCached = false;

		// Build belts on next tick (after all poles are spawned)
		GetWorld()->GetTimerManager().SetTimerForNextTick([this, BeltData]()
		{
			UWorld* World = GetWorld();
			if (!World)
			{
				bProcessingGridPlacement = false;
				return;
			}

			int32 BuiltCount = 0;

			for (const auto& Data : BeltData)
			{
				UFGFactoryConnectionComponent* OutputConn = Data.OutputConnector.Get();
				UFGFactoryConnectionComponent* InputConn = Data.InputConnector.Get();

				if (!OutputConn || !InputConn)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 STACKABLE CONVEYOR POLE: Belt connectors no longer valid"));
					continue;
				}

				FVector StartPos = OutputConn->GetComponentLocation();

				// Get belt class from settings
				int32 ConfigTier = AutoConnectRuntimeSettings.BeltTierMain;
				if (ConfigTier == 0)
				{
					ConfigTier = GetHighestUnlockedBeltTier(LastController.Get());
				}
				UClass* BeltClass = GetBeltClassForTier(ConfigTier, LastController.Get());

				if (!BeltClass)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 STACKABLE CONVEYOR POLE: Failed to get belt class for tier %d"), ConfigTier);
					continue;
				}

				// Spawn actual belt with DEFERRED construction
				FActorSpawnParameters SpawnParams;
				SpawnParams.bDeferConstruction = true;

				AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(
					BeltClass,
					StartPos,
					FRotator::ZeroRotator,
					SpawnParams);

				if (!Belt)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 STACKABLE CONVEYOR POLE: Failed to spawn belt actor"));
					continue;
				}

				// Apply spline data BEFORE FinishSpawning
				TArray<FSplinePointData>* BeltSplineData = Belt->GetMutableSplinePointData();
				if (BeltSplineData && Data.SplineData.Num() >= 2)
				{
					*BeltSplineData = Data.SplineData;
				}

				// Finish spawning
				FTransform BeltTransform(FRotator::ZeroRotator, StartPos);
				Belt->FinishSpawning(BeltTransform);

				// Finalize belt
				Belt->OnBuildEffectFinished();

				// Connect the belt endpoints
				Belt->GetConnection0()->SetConnection(OutputConn);
				Belt->GetConnection1()->SetConnection(InputConn);

				// NOTE: Do NOT call QueueChainRebuild here!
				// Vanilla automatically handles chain creation for spawned belts.
				// Manual chain manipulation causes crashes in Factory_Tick.

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE CONVEYOR POLE: ✅ Built Mk%d belt between poles"), ConfigTier);
				BuiltCount++;
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE CONVEYOR POLE AUTO-CONNECT BUILD: Built %d belt(s)"), BuiltCount);

			bProcessingGridPlacement = false;
		});
	}

	// ========================================
	// STACKABLE BELT CHILD: Deferred wiring + chain rebuild
	// ========================================
	// Wires nearby belt endpoints together, then invalidates chain actors
	// so vanilla rebuilds them with correct topology next frame.
	// Uses chain-level API (RemoveConveyorChainActor) — safe from timers.
	// Bucket-level APIs (AddConveyor/RemoveConveyor) are NOT safe.
	// See RESEARCH_MassUpgrade_ChainActorSafety.md (Archengius insight).
	AFGBuildableConveyorBelt* StackableBelt = Cast<AFGBuildableConveyorBelt>(SpawnedActor);
	if (StackableBelt && SpawnedClassName.Contains(TEXT("ConveyorBelt")))
	{
		// Static array tracks belts spawned in the current build operation
		static TArray<TWeakObjectPtr<AFGBuildableConveyorBelt>> PendingStackableBelts;
		static TWeakObjectPtr<AFGHologram> LastBeltBuildHologram;
		static FTimerHandle BeltWiringTimerHandle;

		// Reset tracking when hologram changes
		if (ActiveHologram.Get() != LastBeltBuildHologram.Get())
		{
			LastBeltBuildHologram = ActiveHologram;
			PendingStackableBelts.Empty();
		}

		// Check if this belt is from our stackable pole system
		// Stackable belts are spawned during pole hologram construction
		if (ActiveHologram.IsValid() && AutoConnectService &&
		    USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get()))
		{
			PendingStackableBelts.Add(StackableBelt);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT SPAWN: Tracking %s for deferred wiring (%d pending)"),
				*StackableBelt->GetName(), PendingStackableBelts.Num());

			// Set/reset timer to wire belts on next tick (after all are spawned)
			GetWorld()->GetTimerManager().ClearTimer(BeltWiringTimerHandle);
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (PendingStackableBelts.Num() == 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: No pending belts"));
					return;
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: Processing %d belts..."), PendingStackableBelts.Num());

				// ========================================
				// STEP 1: Collect valid belts
				// ========================================
				TArray<AFGBuildableConveyorBelt*> ValidBelts;
				for (const auto& WeakBelt : PendingStackableBelts)
				{
					if (WeakBelt.IsValid())
					{
						ValidBelts.Add(WeakBelt.Get());
					}
				}

				if (ValidBelts.Num() == 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: No valid belts remain"));
					PendingStackableBelts.Empty();
					return;
				}

				// ========================================
				// STEP 2: Wire belts together (proximity-based)
				// ========================================
				int32 ConnectionsMade = 0;
				const float ConnectionRadius = 100.0f;

				for (AFGBuildableConveyorBelt* Belt : ValidBelts)
				{
					UFGFactoryConnectionComponent* Conn0 = Belt->GetConnection0();
					UFGFactoryConnectionComponent* Conn1 = Belt->GetConnection1();

					// Connect this belt's Conn0 (input) to nearest unconnected Conn1 (output)
					if (Conn0 && !Conn0->IsConnected())
					{
						FVector SearchLoc = Conn0->GetComponentLocation();
						for (AFGBuildableConveyorBelt* OtherBelt : ValidBelts)
						{
							if (OtherBelt == Belt) continue;
							UFGFactoryConnectionComponent* OtherConn1 = OtherBelt->GetConnection1();
							if (OtherConn1 && !OtherConn1->IsConnected())
							{
								float Dist = FVector::Dist(SearchLoc, OtherConn1->GetComponentLocation());
								if (Dist < ConnectionRadius)
								{
									OtherConn1->SetConnection(Conn0);
									ConnectionsMade++;
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: Connected %s.Conn1 -> %s.Conn0 (dist=%.1f)"),
										*OtherBelt->GetName(), *Belt->GetName(), Dist);
									break;
								}
							}
						}
					}

					// Connect this belt's Conn1 (output) to nearest unconnected Conn0 (input)
					if (Conn1 && !Conn1->IsConnected())
					{
						FVector SearchLoc = Conn1->GetComponentLocation();
						for (AFGBuildableConveyorBelt* OtherBelt : ValidBelts)
						{
							if (OtherBelt == Belt) continue;
							UFGFactoryConnectionComponent* OtherConn0 = OtherBelt->GetConnection0();
							if (OtherConn0 && !OtherConn0->IsConnected())
							{
								float Dist = FVector::Dist(SearchLoc, OtherConn0->GetComponentLocation());
								if (Dist < ConnectionRadius)
								{
									Conn1->SetConnection(OtherConn0);
									ConnectionsMade++;
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: Connected %s.Conn1 -> %s.Conn0 (dist=%.1f)"),
										*Belt->GetName(), *OtherBelt->GetName(), Dist);
									break;
								}
							}
						}
					}
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: Made %d connection(s)"), ConnectionsMade);

				// Invalidate chain actors so vanilla rebuilds them with correct
				// topology next frame. Uses chain-level API (RemoveConveyorChainActor)
				// which is safe from timers — unlike bucket-level APIs:
				//   AddConveyor → double-add → chain mismatch crash
				//   RemoveConveyor → bucket index corruption crash
				//   Respline → Dismantle → RemoveConveyor → bucket index -1 crash
				// See RESEARCH_MassUpgrade_ChainActorSafety.md (Archengius insight).
				AFGBuildableSubsystem* BuildableSub = AFGBuildableSubsystem::Get(GetWorld());
				if (BuildableSub)
				{
					TSet<AFGConveyorChainActor*> ChainsToRebuild;
					for (AFGBuildableConveyorBelt* Belt : ValidBelts)
					{
						if (Belt)
						{
							if (AFGConveyorChainActor* Chain = Belt->GetConveyorChainActor())
								ChainsToRebuild.Add(Chain);
						}
					}

					for (AFGConveyorChainActor* Chain : ChainsToRebuild)
					{
						if (IsValid(Chain))
						{
							BuildableSub->RemoveConveyorChainActor(Chain);
						}
					}

					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CHAIN FIX: Invalidated %d chain(s) — vanilla rebuilds next frame"),
						ChainsToRebuild.Num());
				}

				PendingStackableBelts.Empty();
			});
		}

		// ========================================
		// AUTO-CONNECT BELT: Deferred manifold wiring
		// ========================================
		// Auto-Connect belts are built as children of distributor holograms.
		// Manifold belts (Output1→Input1) are built BEFORE target distributors,
		// so we need deferred wiring to connect them after all distributors are built.
		if (ActiveHologram.IsValid() && AutoConnectService &&
		    AutoConnectService->IsDistributorHologram(ActiveHologram.Get()))
		{
			static TArray<TWeakObjectPtr<AFGBuildableConveyorBelt>> PendingAutoConnectBelts;
			static TWeakObjectPtr<AFGHologram> LastAutoConnectHologram;
			static FTimerHandle AutoConnectWiringTimerHandle;

			// Reset tracking when hologram changes
			if (ActiveHologram.Get() != LastAutoConnectHologram.Get())
			{
				LastAutoConnectHologram = ActiveHologram;
				PendingAutoConnectBelts.Empty();
			}

			PendingAutoConnectBelts.Add(StackableBelt);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BELT SPAWN: Tracking %s for deferred wiring (%d pending)"),
				*StackableBelt->GetName(), PendingAutoConnectBelts.Num());

			// Set/reset timer to wire belts on next tick (after all distributors are spawned)
			GetWorld()->GetTimerManager().ClearTimer(AutoConnectWiringTimerHandle);
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BELT WIRING: Processing %d belt(s)..."), PendingAutoConnectBelts.Num());

				int32 ConnectionsMade = 0;
				const float ConnectionRadius = 100.0f;

				// Clean up invalid weak pointers and collect valid belts
				TArray<AFGBuildableConveyorBelt*> ValidBelts;
				for (const auto& WeakBelt : PendingAutoConnectBelts)
				{
					if (WeakBelt.IsValid())
					{
						ValidBelts.Add(WeakBelt.Get());
					}
				}

				// Wire each belt's unconnected endpoints to nearby built connectors
				for (AFGBuildableConveyorBelt* Belt : ValidBelts)
				{
					UFGFactoryConnectionComponent* Conn0 = Belt->GetConnection0();  // Input
					UFGFactoryConnectionComponent* Conn1 = Belt->GetConnection1();  // Output

					// Wire Conn0 (belt input) to nearby OUTPUT connector on built actors
					if (Conn0 && !Conn0->IsConnected())
					{
						FVector SearchLoc = Conn0->GetComponentLocation();
						UFGFactoryConnectionComponent* BestMatch = nullptr;
						float BestDist = ConnectionRadius;

						for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
						{
							AFGBuildable* Buildable = *It;
							if (Buildable == Belt) continue;

							TArray<UFGFactoryConnectionComponent*> Connectors;
							Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);

							for (UFGFactoryConnectionComponent* Conn : Connectors)
							{
								if (!Conn || Conn->IsConnected()) continue;
								if (Conn->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT) continue;

								float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
								if (Dist < BestDist)
								{
									BestDist = Dist;
									BestMatch = Conn;
								}
							}
						}

						if (BestMatch)
						{
							Conn0->SetConnection(BestMatch);
							ConnectionsMade++;
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BELT: ✅ Wired %s.Conn0 → %s.%s (dist=%.1f)"),
								*Belt->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
						}
					}

					// Wire Conn1 (belt output) to nearby INPUT connector on built actors
					if (Conn1 && !Conn1->IsConnected())
					{
						FVector SearchLoc = Conn1->GetComponentLocation();
						UFGFactoryConnectionComponent* BestMatch = nullptr;
						float BestDist = ConnectionRadius;

						for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
						{
							AFGBuildable* Buildable = *It;
							if (Buildable == Belt) continue;

							TArray<UFGFactoryConnectionComponent*> Connectors;
							Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);

							for (UFGFactoryConnectionComponent* Conn : Connectors)
							{
								if (!Conn || Conn->IsConnected()) continue;
								if (Conn->GetDirection() != EFactoryConnectionDirection::FCD_INPUT) continue;

								float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
								if (Dist < BestDist)
								{
									BestDist = Dist;
									BestMatch = Conn;
								}
							}
						}

						if (BestMatch)
						{
							Conn1->SetConnection(BestMatch);
							ConnectionsMade++;
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BELT: ✅ Wired %s.Conn1 → %s.%s (dist=%.1f)"),
								*Belt->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
						}
					}
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 AUTO-CONNECT BELT WIRING: Made %d connection(s)"), ConnectionsMade);
				PendingAutoConnectBelts.Empty();
			});
		}
	}

	// Handle power pole construction for auto-connect
	// CRITICAL: Only process power poles if we have active Smart! power line previews
	// This mirrors the belt/pipe pattern and prevents processing save-loaded poles
	AFGBuildablePowerPole* PowerPole = Cast<AFGBuildablePowerPole>(SpawnedActor);
	if (PowerPole)
	{
		// CRITICAL TIMING: Poles spawn BEFORE hologram destruction calls CommitBuildingConnections!
		// So we must check PlannedBuildingConnections AND PlannedPoleConnections as fallbacks.
		bool bHasBuildingConnections = CommittedBuildingConnections.Num() > 0;
		bool bHasDeferredConnections = HasDeferredPoleConnections();
		bool bHasPlannedPoleConnections = PlannedPoleConnections.Num() > 0;
		bool bHasPlannedBuildingConnections = PlannedBuildingConnections.Num() > 0;

		// If we have ANY planned connections but nothing committed yet, commit NOW
		// This handles the race condition where pole spawns before hologram destruction
		if ((bHasPlannedPoleConnections || bHasPlannedBuildingConnections) &&
		    !bHasBuildingConnections && !bHasDeferredConnections)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnActorSpawned: Early commit - pole spawned before hologram destruction! PlannedBuildings=%d, PlannedPoles=%d"),
				PlannedBuildingConnections.Num(), PlannedPoleConnections.Num());
			CommitBuildingConnections();
			// Refresh the flags after commit
			bHasBuildingConnections = CommittedBuildingConnections.Num() > 0;
			bHasDeferredConnections = HasDeferredPoleConnections();
		}

		if (!bHasBuildingConnections && !bHasDeferredConnections)
		{
			// No active Smart! power auto-connect session
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnActorSpawned: Power pole %s skipped - no building connections (%d) and no deferred (%d)"),
				*PowerPole->GetName(), CommittedBuildingConnections.Num(), DeferredPoleConnections.Num());
			return;
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnActorSpawned: Power pole detected - Buildings=%d, DeferredPoleConnections=%d"),
			CommittedBuildingConnections.Num(),
			DeferredPoleConnections.Num());

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnActorSpawned: Power pole built - checking for deferred auto-connections"));

		// Register as grid-built pole (guard above ensures we only get here for Smart! placed poles)
		RegisterGridBuiltPowerPole(PowerPole);

		// Check if power connections are ready (like arrow asset system)
		if (ArePowerConnectionsReady(PowerPole))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnActorSpawned: Power connections ready - creating auto-connections immediately"));
			OnPowerPoleBuilt(PowerPole);
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnActorSpawned: Power connections not ready - queuing for deferred processing"));
			QueuePowerPoleForDeferredConnection(PowerPole);
		}
	}
}

TSubclassOf<UFGRecipe> USFSubsystem::FindRecipeForSpawnedBuilding(AFGBuildableManufacturer* SpawnedBuilding)
{
	// CRITICAL: Enhanced SpawnedBuilding validation (deterministic safety)
	if (!IsValid(SpawnedBuilding))
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindRecipeForSpawnedBuilding: SpawnedBuilding is invalid"));
		return nullptr;
	}

	// Get the hologram registry to search for matching holograms (now uses weak pointers)
	const TMap<TWeakObjectPtr<AFGHologram>, FSFHologramData>& HologramRegistry = USFHologramDataRegistry::GetRegistry();

	// Copy registry to local array to prevent iterator invalidation during iteration
	TArray<TPair<TWeakObjectPtr<AFGHologram>, FSFHologramData>> RegistryEntries;
	RegistryEntries.Reserve(HologramRegistry.Num());
	for (const auto& HologramPair : HologramRegistry)
	{
		RegistryEntries.Add(HologramPair);
	}

	for (const auto& HologramPair : RegistryEntries)
	{
		// Use weak pointer for deterministic safety
		TWeakObjectPtr<AFGHologram> WeakHologram = HologramPair.Key;
		const FSFHologramData& Data = HologramPair.Value;

		// Check if hologram is still valid
		if (!WeakHologram.IsValid())
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("FindRecipeForSpawnedBuilding: Found invalid hologram in registry"));
			continue;
		}

		// CRITICAL: Deterministic weak pointer validation (works for ANY address state)
		if (!WeakHologram.IsValid())
		{
			continue;
		}

		// Enhanced recipe validation
		if (!Data.StoredRecipe || !Data.StoredRecipe.Get())
		{
			continue;
		}

		// Check if this recipe can be produced in the spawned building
		TArray< TSubclassOf< UObject > > Producers = UFGRecipe::GetProducedIn(Data.StoredRecipe);
		bool bCanProduceInBuilding = false;
		for (TSubclassOf<UObject> Producer : Producers)
		{
			if (Producer == SpawnedBuilding->GetClass())
			{
				bCanProduceInBuilding = true;
				break;
			}
		}

		if (bCanProduceInBuilding)
		{
			// CRITICAL: Final deterministic weak pointer validation (atomic safety)
			if (!WeakHologram.IsValid())
			{
				continue;
			}

			// Get fresh hologram pointer (guaranteed valid by weak pointer check)
			AFGHologram* ValidHologram = WeakHologram.Get();

			// Check spatial proximity (within 500cm) - now safe to access
			float Distance = FVector::Dist(ValidHologram->GetActorLocation(), SpawnedBuilding->GetActorLocation());
			if (Distance < 500.0f)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FindRecipeForSpawnedBuilding: Found matching hologram %s at distance %f with stored recipe %s"),
					*GetNameSafe(ValidHologram), Distance, *GetNameSafe(Data.StoredRecipe));

				// Clear the hologram data after successful match
				USFHologramDataRegistry::ClearData(ValidHologram);

				return Data.StoredRecipe;
			}
		}
	}

	// No matching hologram found
	return nullptr;
}

void USFSubsystem::ApplyRecipeDelayed(AFGBuildableManufacturer* ManufacturerBuilding, TSubclassOf<UFGRecipe> Recipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipeDelayed(ManufacturerBuilding, Recipe);
	}
}

// ========================================
// Auto-Connect Production Implementation
// ========================================

TArray<AFGBuildable*> USFSubsystem::FindNearbyBuildings(FVector Center, float Radius)
{
	TArray<AFGBuildable*> NearbyBuildings;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindNearbyBuildings: No world context"));
		return NearbyBuildings;
	}

	// Simple sphere trace to find nearby buildables
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams QueryParams;

	TArray<FOverlapResult> OverlapResults;
	bool bFoundOverlaps = World->OverlapMultiByChannel(
		OverlapResults,
		Center,
		FQuat::Identity,
		ECC_WorldStatic,
		SphereShape,
		QueryParams
	);

	if (bFoundOverlaps)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			if (AFGBuildable* Buildable = Cast<AFGBuildable>(Result.GetActor()))
			{
				NearbyBuildings.Add(Buildable);
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found nearby building: %s"), *Buildable->GetName());
			}
		}
	}

	// Issue #269: Diagnostic logging at Log level to trace building search results
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FindNearbyBuildings: Center=%s Radius=%.0f Overlaps=%d Buildings=%d"),
		*Center.ToString(), Radius, OverlapResults.Num(), NearbyBuildings.Num());

	return NearbyBuildings;
}

// ========================================
// Smart Auto-Connect: Distributor Lifecycle
// ========================================

USFAutoConnectOrchestrator* USFSubsystem::GetOrCreateOrchestrator(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !AutoConnectService)
	{
		return nullptr;
	}

	// Check if orchestrator already exists
	if (USFAutoConnectOrchestrator** ExistingOrchestrator = AutoConnectOrchestrators.Find(ParentHologram))
	{
		return *ExistingOrchestrator;
	}

	// Create new orchestrator
	USFAutoConnectOrchestrator* NewOrchestrator = NewObject<USFAutoConnectOrchestrator>(this);
	NewOrchestrator->Initialize(ParentHologram, AutoConnectService);

	// Store in map
	AutoConnectOrchestrators.Add(ParentHologram, NewOrchestrator);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Created new Auto-Connect Orchestrator for %s"),
		*ParentHologram->GetName());

	return NewOrchestrator;
}

void USFSubsystem::OnDistributorHologramUpdated(AFGHologram* DistributorHologram)
{
	if (!DistributorHologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("OnDistributorHologramUpdated: DistributorHologram is null"));
		return;
	}

	// Issue #198: Skip auto-connect if disabled via double-tap
	if (bDisableSmartForNextAction)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏸️ Auto-Connect skipped - Smart disabled for current action"));
		return;
	}

	// Use orchestrator for coordinated evaluation (Refactor: Auto-Connect Orchestrator)
	USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(DistributorHologram);
	if (!Orchestrator)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Auto-Connect Orchestrator not available - skipping distributor update"));
		return;
	}

	// Issue #269 FIX: For single distributors (no Smart! grid children), use OnGridChanged()
	// which does force-recreate evaluation. Without children, OnGridChanged never fires naturally
	// (no grid updates), so the orchestrator only gets non-forced OnDistributorsMoved() calls
	// which may fail to establish initial connections. Force recreate ensures a clean evaluation.
	// For distributors WITH children, use the lighter OnDistributorsMoved() since OnGridChanged
	// already fires when children are added/removed.
	{
		const TArray<AFGHologram*>& Children = DistributorHologram->GetHologramChildren();
		bool bHasSmartChildren = false;
		for (AFGHologram* Child : Children)
		{
			if (Child && Child->Tags.Contains(FName(TEXT("SF_GridChild"))))
			{
				bHasSmartChildren = true;
				break;
			}
		}

		if (!bHasSmartChildren)
		{
			// Single distributor (no grid children) - force recreate for reliable tracking
			Orchestrator->OnGridChanged();
		}
		else
		{
			// Has children - use lighter non-forced update (grid changes handle force recreate)
			Orchestrator->OnDistributorsMoved();
		}
	}

	// Also update pipe previews if this is a pipeline junction (evaluation guard prevents recursion)
	Orchestrator->OnPipeJunctionsMoved();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Distributors moved (parent update)"));
}

// DEPRECATED: OnPipeJunctionHologramUpdated - Replaced by Auto-Connect Orchestrator
// Pipe preview updates are now handled by USFAutoConnectOrchestrator::OnPipeJunctionsMoved()
// to prevent duplicate manager creation and ensure proper preview lifecycle management

void USFSubsystem::ClearBlueprintProxyFlag()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearBlueprintProxyFlag();
	}
}

void USFSubsystem::ResetCounters()
{
    // Reset via centralized grid state service when available
    if (GridStateService)
    {
        GridStateService->Reset();
        CounterState = GridStateService->GetCounterState();
    }
    else
    {
        CounterState.Reset();
    }

    // Trigger HUD refresh
    UpdateCounterDisplay();
}

// ========================================
// Debug Tools
// ========================================

void USFSubsystem::AnalyzeNearbyPipeSplines(float Radius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 SPLINE ANALYZER: No world context"));
		return;
	}

	// Get player location as search center
	AFGPlayerController* PlayerController = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 SPLINE ANALYZER: No player controller"));
		return;
	}

	AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(PlayerController->GetPawn());
	if (!Player)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 SPLINE ANALYZER: No player character"));
		return;
	}

	FVector PlayerLocation = Player->GetActorLocation();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 SMART! PIPE SPLINE ANALYZER"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Player Location: %s"), *PlayerLocation.ToString());
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Search Radius: %.1fm"), Radius / 100.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));

	// Use the analyzer utility (analyzes both pipes and belts)
	FSFSplineAnalyzer::AnalyzeNearLocation(World, PlayerLocation, Radius);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 Analysis complete. Check logs for detailed spline data."));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
}

void USFSubsystem::OnPowerPoleBuilt(AFGBuildablePowerPole* BuiltPole)
{
	if (!BuiltPole)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Invalid power pole"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnPowerPoleBuilt: Power pole built - creating connections"));

	// NOTE: Wire costs are now represented by real child wire holograms (preview children).
	// Vanilla deducts the combined cost (pole + child wires) when the hologram is built.
	// We must NOT manually deduct wire costs here to avoid double-charging.
	if (!bPowerCostsDeductedThisCycle && AutoConnectService)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnPowerPoleBuilt: First pole of cycle - wire costs already deducted via child hologram aggregation"));
		bPowerCostsDeductedThisCycle = true;  // Mark as handled (vanilla already deducted)
	}
	else if (bPowerCostsDeductedThisCycle)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnPowerPoleBuilt: Child pole - wire costs already deducted by first pole"));
	}

	// Forward to power auto-connect manager (with costs already deducted)
	if (PowerAutoConnectManager.IsValid())
	{
		PowerAutoConnectManager->OnPowerPoleBuilt(BuiltPole, true);  // true = costs pre-deducted
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Power auto-connect manager not available"));
		return; // Removed return false here
	}
}

bool USFSubsystem::ArePowerConnectionsReady(AFGBuildablePowerPole* PowerPole)
{
	if (!PowerPole)
	{
		return false;
	}

	// Check if power connection components are initialized
	const TArray<UFGPowerConnectionComponent*>& PowerConnections = PowerPole->GetPowerConnections();
	bool bReady = PowerConnections.Num() > 0;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ArePowerConnectionsReady: %s - Connections=%d, Ready=%s"),
		*PowerPole->GetName(), PowerConnections.Num(), bReady ? TEXT("true") : TEXT("false"));

	return bReady;
}

void USFSubsystem::QueuePowerPoleForDeferredConnection(AFGBuildablePowerPole* PowerPole)
{
	if (!PowerPole)
	{
		return;
	}

	// Add to pending queue if not already there
	if (!PendingPowerPoleConnections.Contains(PowerPole))
	{
		// CRITICAL: Cache the building connections when FIRST pole is queued
		// This must happen BEFORE the new parent hologram spawns and overwrites PlannedBuildingConnections
		if (PendingPowerPoleConnections.Num() == 0 && PlannedBuildingConnections.Num() > 0)
		{
			CommitBuildingConnections();
		}

		PendingPowerPoleConnections.Add(PowerPole);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ QueuePowerPoleForDeferredConnection: Added %s to queue (total: %d)"),
			*PowerPole->GetName(), PendingPowerPoleConnections.Num());

		// Set timer to process deferred connections (like arrow system)
		UWorld* World = GetWorld();
		if (World && !PowerPoleDeferredTimer.IsValid())
		{
			World->GetTimerManager().SetTimer(
				PowerPoleDeferredTimer,
				this,
				&USFSubsystem::ProcessDeferredPowerPoleConnections,
				0.1f,  // Check every 100ms
				true   // Loop until all processed
			);

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ QueuePowerPoleForDeferredConnection: Set deferred timer (100ms interval)"));
		}
	}
}

void USFSubsystem::ProcessDeferredPowerPoleConnections()
{
	if (PendingPowerPoleConnections.Num() == 0)
	{
		// Clear timer when no pending poles
		UWorld* World = GetWorld();
		if (World && PowerPoleDeferredTimer.IsValid())
		{
			World->GetTimerManager().ClearTimer(PowerPoleDeferredTimer);
			PowerPoleDeferredTimer.Invalidate();
		}

		// Reset the cost deduction flag for next build cycle
		bPowerCostsDeductedThisCycle = false;

		return;
	}

	// Process each pending pole (limit to prevent spam)
	int32 MaxPolesPerTick = 3;
	int32 ProcessedCount = 0;

	for (int32 i = PendingPowerPoleConnections.Num() - 1; i >= 0 && ProcessedCount < MaxPolesPerTick; i--)
	{
		TWeakObjectPtr<AFGBuildablePowerPole> WeakPole = PendingPowerPoleConnections[i];
		if (!WeakPole.IsValid())
		{
			// Remove invalid poles
			PendingPowerPoleConnections.RemoveAt(i);
			continue;
		}

		AFGBuildablePowerPole* PowerPole = WeakPole.Get();

		// Check if connections are ready now
		if (ArePowerConnectionsReady(PowerPole))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessDeferredPowerPoleConnections: %s is now ready - creating connections"),
				*PowerPole->GetName());

			// Process the pole
			OnPowerPoleBuilt(PowerPole);

			// Remove from pending queue
			PendingPowerPoleConnections.RemoveAt(i);
			ProcessedCount++;
		}
		else
		{
			// Still not ready, check timeout (5 seconds like arrow system)
			// TODO: Add timeout tracking if needed
		}
	}

	if (PendingPowerPoleConnections.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessDeferredPowerPoleConnections: %d poles remain pending"), PendingPowerPoleConnections.Num());
	}
}

void USFSubsystem::RegisterGridBuiltPowerPole(AFGBuildablePowerPole* PowerPole)
{
	if (!PowerPole) return;

	// Cleanup invalid entries first
	CleanupGridBuiltPowerPoles();

	// Add to registry if not already present
	if (!GridBuiltPowerPoles.Contains(PowerPole))
	{
		GridBuiltPowerPoles.Add(PowerPole);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ RegisterGridBuiltPowerPole: Registered %s as grid-built"), *PowerPole->GetName());
	}
}

bool USFSubsystem::IsGridBuiltPowerPole(AFGBuildablePowerPole* PowerPole)
{
	if (!PowerPole) return false;

	// Cleanup invalid entries first
	CleanupGridBuiltPowerPoles();

	return GridBuiltPowerPoles.Contains(PowerPole);
}

void USFSubsystem::CleanupGridBuiltPowerPoles()
{
	// Remove invalid entries (destroyed poles)
	for (int32 i = GridBuiltPowerPoles.Num() - 1; i >= 0; i--)
	{
		if (!GridBuiltPowerPoles[i].IsValid())
		{
			GridBuiltPowerPoles.RemoveAt(i);
		}
	}
}

const TArray<TWeakObjectPtr<AFGBuildablePowerPole>>& USFSubsystem::GetGridBuiltPowerPoles()
{
	CleanupGridBuiltPowerPoles();
	return GridBuiltPowerPoles;
}

void USFSubsystem::CacheStackablePipeHologramPositions(const TArray<AFGHologram*>& AllSupports, AFGHologram* ParentHologram)
{
	// Cache ALL hologram positions for matching to spawned actors
	PendingStackablePipeHologramPositions.Empty();
	for (AFGHologram* Support : AllSupports)
	{
		if (Support)
		{
			PendingStackablePipeHologramPositions.Add(Support->GetActorLocation());
		}
	}

	// Store parent coordinate system for sorting spawned actors
	if (ParentHologram)
	{
		PendingStackablePipeParentPosition = ParentHologram->GetActorLocation();
		PendingStackablePipeScaleAxis = ParentHologram->GetActorForwardVector();
	}

	// Set expected count and enable deferred build
	StackablePipeSupportExpectedCount = PendingStackablePipeHologramPositions.Num();
	StackablePipeSupportSpawnedCount = 0;
	bStackablePipeBuildPending = (StackablePipeSupportExpectedCount >= 2);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE: Cached %d hologram positions for deferred build (bPending=%d)"),
		StackablePipeSupportExpectedCount, bStackablePipeBuildPending);
}

void USFSubsystem::ClearStackablePipeBuildCache()
{
	PendingStackablePipeHologramPositions.Empty();
	PendingStackablePipeBuildData.Empty();
	bStackablePipeBuildPending = false;
	StackablePipeSupportExpectedCount = 0;
	StackablePipeSupportSpawnedCount = 0;
}

// ========================================
// PIPE AUTO-CONNECT: Deferred Wiring Registration
// ========================================
// Called from SFPipelineHologram::Construct() to register pipes for deferred wiring.
// This bypasses OnActorSpawned timing issues where tags are added AFTER the spawn delegate fires.

void USFSubsystem::RegisterPipeForDeferredWiring(AFGBuildablePipeline* Pipe)
{
	if (!Pipe)
	{
		return;
	}

	// Static tracking arrays - persist across calls within a build operation
	static TArray<TWeakObjectPtr<AFGBuildablePipeline>> PendingPipes;
	static FTimerHandle PipeWiringTimerHandle;

	// Track this pipe
	PendingPipes.Add(Pipe);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT REGISTER: %s queued for deferred wiring (%d pending)"),
		*Pipe->GetName(), PendingPipes.Num());

	// Set/reset timer to wire pipes on next tick (after all junctions are spawned)
	GetWorld()->GetTimerManager().ClearTimer(PipeWiringTimerHandle);
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT DEFERRED WIRING: Processing %d pipe(s)..."), PendingPipes.Num());

		int32 ConnectionsMade = 0;
		const float ConnectionRadius = 100.0f;  // 1m tolerance for connector matching

		// Collect valid pipes
		TArray<AFGBuildablePipeline*> ValidPipes;
		for (const auto& WeakPipe : PendingPipes)
		{
			if (WeakPipe.IsValid())
			{
				ValidPipes.Add(WeakPipe.Get());
			}
		}

		// Wire each pipe's unconnected endpoints to nearby built connectors
		for (AFGBuildablePipeline* Pipe : ValidPipes)
		{
			UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
			UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();

			// Wire Conn0 to nearby unconnected pipe connector
			if (Conn0 && !Conn0->IsConnected())
			{
				FVector SearchLoc = Conn0->GetComponentLocation();
				UFGPipeConnectionComponent* BestMatch = nullptr;
				float BestDist = ConnectionRadius;

				for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
				{
					AFGBuildable* Buildable = *It;
					if (Buildable == Pipe) continue;

					TArray<UFGPipeConnectionComponent*> Connectors;
					Buildable->GetComponents<UFGPipeConnectionComponent>(Connectors);

					for (UFGPipeConnectionComponent* Conn : Connectors)
					{
						if (!Conn || Conn->IsConnected()) continue;

						float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());

						// Special case for floor holes (passthroughs): their connector is at the center,
						// but the pipe ends at the foundation surface. Allow vertical gap for exact XY matches.
						if (Buildable->IsA(AFGBuildablePassthrough::StaticClass()))
						{
							FVector ConnLoc = Conn->GetComponentLocation();
							float DistXY = FVector::Dist2D(SearchLoc, ConnLoc);
							float DistZ = FMath::Abs(SearchLoc.Z - ConnLoc.Z);

							// If perfectly aligned vertically and within typical foundation thickness
							if (DistXY < 10.0f && DistZ <= 450.0f)
							{
								// Treat it as a perfect match (0 distance) so it beats anything else
								Dist = DistXY;
							}
						}

						if (Dist < BestDist)
						{
							BestDist = Dist;
							BestMatch = Conn;
						}
					}
				}

				if (BestMatch)
				{
					Conn0->SetConnection(BestMatch);
					ConnectionsMade++;
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("PIPE AUTO-CONNECT: Wired %s.Conn0 → %s.%s (dist=%.1f)"),
						*Pipe->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("PIPE AUTO-CONNECT: No connector found for %s.Conn0 @ %s"),
						*Pipe->GetName(), *SearchLoc.ToString());
				}
			}

			// Wire Conn1 to nearby unconnected pipe connector
			if (Conn1 && !Conn1->IsConnected())
			{
				FVector SearchLoc = Conn1->GetComponentLocation();
				UFGPipeConnectionComponent* BestMatch = nullptr;
				float BestDist = ConnectionRadius;

				for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
				{
					AFGBuildable* Buildable = *It;
					if (Buildable == Pipe) continue;

					TArray<UFGPipeConnectionComponent*> Connectors;
					Buildable->GetComponents<UFGPipeConnectionComponent>(Connectors);

					for (UFGPipeConnectionComponent* Conn : Connectors)
					{
						if (!Conn || Conn->IsConnected()) continue;

						float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());

						// Special case for floor holes (passthroughs): their connector is at the center,
						// but the pipe ends at the foundation surface. Allow vertical gap for exact XY matches.
						if (Buildable->IsA(AFGBuildablePassthrough::StaticClass()))
						{
							FVector ConnLoc = Conn->GetComponentLocation();
							float DistXY = FVector::Dist2D(SearchLoc, ConnLoc);
							float DistZ = FMath::Abs(SearchLoc.Z - ConnLoc.Z);

							// If perfectly aligned vertically and within typical foundation thickness
							if (DistXY < 10.0f && DistZ <= 450.0f)
							{
								// Treat it as a perfect match (0 distance) so it beats anything else
								Dist = DistXY;
							}
						}

						if (Dist < BestDist)
						{
							BestDist = Dist;
							BestMatch = Conn;
						}
					}
				}

				if (BestMatch)
				{
					Conn1->SetConnection(BestMatch);
					ConnectionsMade++;
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT: ✅ Wired %s.Conn1 → %s.%s (dist=%.1f)"),
						*Pipe->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
				}
			}
		}

		// Merge pipe networks for connected pipes
		if (ConnectionsMade > 0)
		{
			AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(GetWorld());
			if (PipeSubsystem)
			{
				for (AFGBuildablePipeline* Pipe : ValidPipes)
				{
					if (UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0())
					{
						if (Conn0->IsConnected())
						{
							int32 NetworkID = Conn0->GetPipeNetworkID();
							if (AFGPipeNetwork* Network = PipeSubsystem->FindPipeNetwork(NetworkID))
							{
								Network->MarkForFullRebuild();
							}
						}
					}
				}
			}
		}

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE AUTO-CONNECT DEFERRED WIRING: Made %d connection(s) for %d pipe(s)"),
			ConnectionsMade, ValidPipes.Num());
		PendingPipes.Empty();
	});
}

// ========================================
// Chain Actor Rebuild System (Issue #220 - Stackable Belt Fix)
// ========================================
// Uses a deferred timer to safely rebuild chains after all belts are placed.

void USFSubsystem::QueueChainRebuild(AFGBuildableConveyorBelt* Belt)
{
	if (!Belt)
	{
		return;
	}

	PendingChainRebuilds.Add(Belt);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Queued %s for deferred chain rebuild (pending: %d)"),
		*Belt->GetName(), PendingChainRebuilds.Num());

	// Reset the timer - this allows batching multiple belts placed in quick succession
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ChainRebuildTimerHandle);
		World->GetTimerManager().SetTimer(
			ChainRebuildTimerHandle,
			this,
			&USFSubsystem::ExecuteDeferredChainRebuild,
			0.5f,  // 500ms delay to allow all belts to be placed
			false  // Don't loop
		);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Timer set for 0.5s"));
	}
}

TSet<AFGBuildableConveyorBelt*> USFSubsystem::CollectChainBelts(AFGBuildableConveyorBelt* StartBelt)
{
	TSet<AFGBuildableConveyorBelt*> Result;
	if (!StartBelt)
	{
		return Result;
	}

	TQueue<AFGBuildableConveyorBelt*> ToVisit;
	ToVisit.Enqueue(StartBelt);

	while (!ToVisit.IsEmpty())
	{
		AFGBuildableConveyorBelt* Current;
		ToVisit.Dequeue(Current);

		if (!Current || Result.Contains(Current))
		{
			continue;
		}

		Result.Add(Current);

		// Follow Conn0
		UFGFactoryConnectionComponent* Conn0 = Current->GetConnection0();
		if (Conn0 && Conn0->IsConnected())
		{
			UFGFactoryConnectionComponent* OtherConn = Conn0->GetConnection();
			if (OtherConn)
			{
				AActor* Owner = OtherConn->GetOwner();
				if (AFGBuildableConveyorBelt* Neighbor = Cast<AFGBuildableConveyorBelt>(Owner))
				{
					if (!Result.Contains(Neighbor))
					{
						ToVisit.Enqueue(Neighbor);
					}
				}
			}
		}

		// Follow Conn1
		UFGFactoryConnectionComponent* Conn1 = Current->GetConnection1();
		if (Conn1 && Conn1->IsConnected())
		{
			UFGFactoryConnectionComponent* OtherConn = Conn1->GetConnection();
			if (OtherConn)
			{
				AActor* Owner = OtherConn->GetOwner();
				if (AFGBuildableConveyorBelt* Neighbor = Cast<AFGBuildableConveyorBelt>(Owner))
				{
					if (!Result.Contains(Neighbor))
					{
						ToVisit.Enqueue(Neighbor);
					}
				}
			}
		}
	}

	return Result;
}

void USFSubsystem::ExecuteDeferredChainRebuild()
{
	if (PendingChainRebuilds.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Executing deferred rebuild for %d pending belt(s)"),
		PendingChainRebuilds.Num());

	AFGBuildableSubsystem* Subsystem = AFGBuildableSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ CHAIN REBUILD: No BuildableSubsystem - aborting"));
		PendingChainRebuilds.Empty();
		return;
	}

	// Collect ALL belts from ALL pending chains
	TSet<AFGBuildableConveyorBelt*> AllChainBelts;
	for (const TWeakObjectPtr<AFGBuildableConveyorBelt>& WeakBelt : PendingChainRebuilds)
	{
		if (WeakBelt.IsValid())
		{
			TSet<AFGBuildableConveyorBelt*> ChainBelts = CollectChainBelts(WeakBelt.Get());
			AllChainBelts.Append(ChainBelts);
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Collected %d total belt(s) in chain(s)"),
		AllChainBelts.Num());

	// Remove all from subsystem
	int32 RemovedCount = 0;
	for (AFGBuildableConveyorBelt* Belt : AllChainBelts)
	{
		if (Belt && Belt->GetConveyorChainActor())
		{
			Subsystem->RemoveConveyor(Belt);
			RemovedCount++;
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Removed %d belt(s) from chain system"),
		RemovedCount);

	// Re-add all (rebuilds with correct topology)
	int32 AddedCount = 0;
	for (AFGBuildableConveyorBelt* Belt : AllChainBelts)
	{
		if (Belt)
		{
			Subsystem->AddConveyor(Belt);
			AddedCount++;
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Re-added %d belt(s) to chain system"),
		AddedCount);

	// Clear pending
	PendingChainRebuilds.Empty();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: Complete"));
}

void USFSubsystem::CacheStackableBeltPreviewsForBuild()
{
	// Uses file-scope global cache: GCachedStackableBeltData, bGStackableBeltDataCached
	// OnActorSpawned consumes this cache when pole is built

	if (!AutoConnectService || !ActiveHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 STACKABLE BELT CACHE: Missing service or hologram"));
		return;
	}

	// Get belt previews from the AutoConnectService
	TArray<TSharedPtr<FBeltPreviewHelper>>* BeltPreviews = AutoConnectService->GetBeltPreviews(ActiveHologram.Get());
	if (!BeltPreviews || BeltPreviews->Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CACHE: No belt previews to cache"));
		return;
	}

	GCachedStackableBeltData.Empty();

	for (const TSharedPtr<FBeltPreviewHelper>& PreviewHelper : *BeltPreviews)
	{
		if (!PreviewHelper.IsValid() || !PreviewHelper->IsPreviewValid()) continue;

		AFGSplineHologram* PreviewHolo = PreviewHelper->GetHologram();
		if (!PreviewHolo) continue;

		FStackableBeltBuildData Data;
		Data.BeltTier = PreviewHelper->GetBeltTier();
		Data.OutputConnector = PreviewHelper->GetOutputConnector();
		Data.InputConnector = PreviewHelper->GetInputConnector();

		// Extract spline data from the preview hologram
		USplineComponent* SplineComp = PreviewHolo->FindComponentByClass<USplineComponent>();
		if (SplineComp)
		{
			int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
			for (int32 i = 0; i < NumPoints; i++)
			{
				FSplinePointData Point;
				Point.Location = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
				Point.ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
				Point.LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
				Data.SplineData.Add(Point);
			}
		}

		if (Data.SplineData.Num() >= 2)
		{
			GCachedStackableBeltData.Add(Data);
		}
	}

	bGStackableBeltDataCached = GCachedStackableBeltData.Num() > 0;

	if (bGStackableBeltDataCached)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT CACHE: Cached %d belt(s) from hologram previews"), GCachedStackableBeltData.Num());
	}
}
