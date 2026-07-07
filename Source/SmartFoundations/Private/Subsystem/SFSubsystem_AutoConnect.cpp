// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - auto-connect production + distributor lifecycle + debug tools + deferred pipe wiring + chain-actor rebuild.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"
#include "Engine/OverlapResult.h"


// ========================================
// Auto-Connect Production Implementation
// ========================================

TArray<AFGBuildable*> USFSubsystem::FindNearbyBuildings(FVector Center, float Radius)
{
	TArray<AFGBuildable*> NearbyBuildings;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("FindNearbyBuildings: No world context"));
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
	if (TObjectPtr<USFAutoConnectOrchestrator>* ExistingOrchestrator = AutoConnectOrchestrators.Find(ParentHologram))
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("OnDistributorHologramUpdated: DistributorHologram is null"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Auto-Connect Orchestrator not available - skipping distributor update"));
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


void USFSubsystem::OnPowerPoleBuilt(AFGBuildablePowerPole* BuiltPole)
{
	if (!BuiltPole)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚡ OnPowerPoleBuilt: Invalid power pole"));
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
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚡ OnPowerPoleBuilt: Power auto-connect manager not available"));
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

int32 USFSubsystem::WireBlueprintSeamPipe(AFGBuildablePipeline* Pipe)
{
	if (!Pipe || !GetWorld())
	{
		return 0;
	}

	// [#168] Synchronous seam-pipe wiring. Mirrors the belt seam path (immediate proximity scan
	// against BUILT actors), but adds explicit network MERGE — SetConnection alone leaves the two
	// copies' pipe networks separate, so fluid wouldn't cross without this (the deferred junction
	// path only MarkForFullRebuild's, which is why seam pipes built but never flowed).
	const float WiringRadius = 100.0f;   // 1m — matches the deferred junction tolerance
	int32 Wired = 0;

	auto WireEndpoint = [this, Pipe, WiringRadius, &Wired](UFGPipeConnectionComponent* OwnConn)
	{
		if (!OwnConn)
		{
			return;
		}
		// Vanilla's Construct wires the built pipe to its SNAPPED connections — which for a seam
		// pipe are the clone HOLOGRAM's dup connector components (set for pole suppression). That
		// "connection" reports IsConnected()==true right now and dangles the moment the hologram
		// dies, which silently skipped all real wiring (live 2026-07-07: every seam pipe logged
		// wired 0/2 with no search — final state unconnected). A peer owned by a hologram (or by
		// anything that is not a BUILT buildable) is bogus: clear it and wire to the real world.
		if (OwnConn->IsConnected())
		{
			UFGPipeConnectionComponentBase* Peer = OwnConn->GetConnection();
			AActor* PeerOwner = Peer ? Peer->GetOwner() : nullptr;
			if (!PeerOwner || PeerOwner->IsA<AFGHologram>() || !PeerOwner->IsA<AFGBuildable>())
			{
				UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Seam pipe %s endpoint %s: clearing bogus hologram-peer connection (%s) before real wiring"),
					*Pipe->GetName(), *OwnConn->GetName(), *GetNameSafe(PeerOwner));
				OwnConn->ClearConnection();
			}
			else
			{
				return; // genuinely wired to a built actor — leave it
			}
		}
		const FVector SearchLoc = OwnConn->GetComponentLocation();
		UFGPipeConnectionComponent* BestMatch = nullptr;
		float BestDist = WiringRadius;
		for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
		{
			AFGBuildable* Buildable = *It;
			if (Buildable == Pipe)
			{
				continue;
			}
			TArray<UFGPipeConnectionComponent*> Connectors;
			Buildable->GetComponents<UFGPipeConnectionComponent>(Connectors);
			for (UFGPipeConnectionComponent* Conn : Connectors)
			{
				if (!Conn || Conn->IsConnected())
				{
					continue;
				}
				if (Conn->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY)
				{
					continue;
				}
				if (!OwnConn->CanConnectTo(Conn))
				{
					continue;   // producer/consumer/any compatibility — vanilla's own rule
				}
				const float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
				if (Dist < BestDist)
				{
					BestDist = Dist;
					BestMatch = Conn;
				}
			}
		}
		if (BestMatch)
		{
			OwnConn->SetConnection(BestMatch);
			Wired++;
			UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Seam pipe %s wired -> %s.%s (dist=%.1f)"),
				*Pipe->GetName(), *GetNameSafe(BestMatch->GetOwner()), *BestMatch->GetName(), BestDist);
		}
		else
		{
			UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Seam pipe %s: NO connector within %.0fcm of endpoint %s @ %s"),
				*Pipe->GetName(), WiringRadius, *OwnConn->GetName(), *SearchLoc.ToString());
		}
	};

	WireEndpoint(Pipe->GetPipeConnection0());
	WireEndpoint(Pipe->GetPipeConnection1());

	// Merge the now-joined networks so fluid actually crosses the seam.
	if (Wired > 0)
	{
		if (AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(GetWorld()))
		{
			auto MergeAt = [PipeSubsystem, Pipe](UFGPipeConnectionComponent* OwnConn)
			{
				if (!OwnConn || !OwnConn->IsConnected())
				{
					return;
				}
				UFGPipeConnectionComponent* Other = Cast<UFGPipeConnectionComponent>(OwnConn->GetConnection());
				if (!Other)
				{
					return;
				}
				const int32 NetA = OwnConn->GetPipeNetworkID();
				const int32 NetB = Other->GetPipeNetworkID();
				AFGPipeNetwork* NetworkA = (NetA != INDEX_NONE) ? PipeSubsystem->FindPipeNetwork(NetA) : nullptr;
				if (NetA != NetB && NetA != INDEX_NONE && NetB != INDEX_NONE)
				{
					if (AFGPipeNetwork* NetworkB = PipeSubsystem->FindPipeNetwork(NetB))
					{
						if (NetworkA)
						{
							NetworkA->MergeNetworks(NetworkB);
						}
					}
				}
				if (NetworkA)
				{
					NetworkA->MarkForFullRebuild();
				}
			};
			MergeAt(Pipe->GetPipeConnection0());
			MergeAt(Pipe->GetPipeConnection1());
		}
	}

	return Wired;
}

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
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("PIPE AUTO-CONNECT: No connector found for %s.Conn0 @ %s"),
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

// Chain Actor Rebuild System (Issue #220) removed (dead): QueueChainRebuild / CollectChainBelts /
// ExecuteDeferredChainRebuild were never called. They did RemoveConveyor/AddConveyor on live belts
// off a timer (crash-class — ParallelFor tick race) and are fully superseded by the STACK-CHAIN
// construct handler in ASFConveyorBeltHologram (THESIS §6.9–§6.13).

// CacheStackableBeltPreviewsForBuild() removed (dead): its only consumer was the deferred
// OnActorSpawned SpawnActor belt builder, deleted when stacked belts moved to the STACK-CHAIN
// construct handler (THESIS §6.9–§6.13).

