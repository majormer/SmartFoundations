// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFSubsystem implementation (part 6). Split out of SFSubsystem.cpp (slice S, pure
 * impl-split: one class across multiple .cpp) to keep each file <2k. No behavior change.
 */

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
#include "Subsystem/SFInputHandler.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFValidationService.h"
#include "Subsystem/SFHologramHelperService.h"
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
#include "FGBlueprintProxy.h"
#include "FGBlueprintHologram.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Holograms/Core/SFBuildableHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Core/ASFLogisticsHologram.h"
#include "Holograms/Core/SFFoundationHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGStandaloneSignHologram.h"
#include "Holograms/Core/SFStandaloneSignChildHologram.h"
#include "Holograms/Adapters/SFSmartBuildableAdapter.h"
#include "Holograms/Adapters/SFSmartLogisticsAdapter.h"
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
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Input/SFInputRegistry.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/Spacing/SFSpacingModule.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGBuildablePolePipe.h"  // For stackable pipeline support auto-connect
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
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGRecipe.h"
#include "Subsystem/SFSubsystemStackableCache.h"

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

