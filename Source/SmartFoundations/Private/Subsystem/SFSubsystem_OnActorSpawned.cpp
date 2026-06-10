// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - OnActorSpawned handler + recipe application for spawned buildings.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"
#include "Core/Helpers/SFNetworkHelper.h"  // [#334] authority gate


void USFSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->OnActorSpawned(SpawnedActor);
	}

	// ========================================
	// [MP / #334] AUTHORITY GATE for everything below.
	// Every remaining section of this handler MUTATES world state post-build (blueprint-proxy
	// grouping, belt/pipe auto-connect builds, stackable pipe/belt wiring). On a NETWORK CLIENT,
	// OnActorSpawned fires for REPLICATED server-built actors - running these sections there
	// spawns client-only ghost actors and connections the server never sees (the root cause of
	// #334: client-only power wires, and the same class of bug for belts/pipes). Authority sides
	// (single player standalone, listen-server host, dedicated server) are unchanged: the server
	// runs its OWN preview/decision pipeline against its mirrored hologram and performs the
	// wiring here with authority.
	// ========================================
	if (FSFNetworkHelper::IsClient(GetWorld()))
	{
		return;
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

						// [EXTEND-MP] Display level while MP Extend validates: this is the wiring
						// pass trigger and the dedi log is the only visibility (Verbose stripped).
						UE_LOG(LogSmartFoundations, Verbose, TEXT("[EXTEND-MP] Deferred connection wiring executing for %s"), *WeakFactory->GetName());
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
						SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: Deferred connection wiring cancelled - factory or service no longer valid"));
					}
				});
			}
		}
	}

	// [MP-334] TEMP diagnostic: report every gate of the belt auto-connect build path at once
	// (the path is silent at Display level, and on the dedi we cannot see which gate fails).
	if (AFGBuildableConveyorAttachment* DiagAttachment = Cast<AFGBuildableConveyorAttachment>(SpawnedActor))
	{
		const bool bDiagHoloValid = ActiveHologram.IsValid();
		const bool bDiagIsDistributor = bDiagHoloValid && AutoConnectService
			&& AutoConnectService->IsDistributorHologram(ActiveHologram.Get());
		int32 DiagPreviewCount = -1;
		if (bDiagIsDistributor)
		{
			if (TArray<TSharedPtr<FBeltPreviewHelper>>* DiagPreviews = AutoConnectService->GetBeltPreviews(ActiveHologram.Get()))
			{
				DiagPreviewCount = DiagPreviews->Num();
			}
		}
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[MP-334] OnActorSpawned(attachment=%s): svc=%d activeHolo=%s isDistributorHolo=%d busyLatch=%d previews=%d"),
			*DiagAttachment->GetName(),
			AutoConnectService ? 1 : 0,
			*GetNameSafe(ActiveHologram.Get()),
			bDiagIsDistributor ? 1 : 0,
			bProcessingGridPlacement ? 1 : 0,
			DiagPreviewCount);
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
	// DISTRIBUTOR-CHILD BELT: Deferred manifold wiring (block below)
	// ========================================
	// Stacked-pole belts are wired at Construct (STACK-CHAIN handler in
	// ASFConveyorBeltHologram::Construct) and do NOT use any deferred timer here.
	AFGBuildableConveyorBelt* StackableBelt = Cast<AFGBuildableConveyorBelt>(SpawnedActor);
	if (StackableBelt && SpawnedClassName.Contains(TEXT("ConveyorBelt")))
	{
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
