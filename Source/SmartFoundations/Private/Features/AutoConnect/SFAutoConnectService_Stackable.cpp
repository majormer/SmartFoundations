// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFAutoConnectService - stackable conveyor/pipe supports + power-pole auto-connect. Split out of SFAutoConnectService.cpp (slice AC,
 * pure impl-split, one class across .cpp) to keep each file <2k. No behavior change.
 */

#include "Features/AutoConnect/SFAutoConnectServiceImpl.h"

void USFAutoConnectService::ProcessFloorHolePipes(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		return;
	}
	
	// Get or create the pipe auto-connect manager for this hologram
	TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(ParentHologram);
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager for this hologram
		TSharedPtr<FSFPipeAutoConnectManager> NewManager = MakeShared<FSFPipeAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PipeAutoConnectManagers.Add(ParentHologram, NewManager);
		ManagerPtr = PipeAutoConnectManagers.Find(ParentHologram);
	}
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		return;
	}
	
	FSFPipeAutoConnectManager* Manager = ManagerPtr->Get();
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		Manager->ClearFloorHolePipePreviews();
		return;
	}
	
	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bPipeAutoConnectEnabled)
	{
		Manager->ClearFloorHolePipePreviews();
		return;
	}
	
	// Delegate to PipeAutoConnectManager which has the pipe spawning infrastructure
	Manager->ProcessFloorHolePipes(ParentHologram);
}

void USFAutoConnectService::ProcessStackableConveyorPoles(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("🚧 ProcessStackableConveyorPoles: null parent hologram or subsystem"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		CleanupAllStackableBelts(ParentHologram);
		UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 ProcessStackableConveyorPoles: Skipped - Smart disabled for current action"));
		return;
	}

	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bStackableBeltEnabled)
	{
		UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 ProcessStackableConveyorPoles: Auto-connect disabled (global=%d, stackable belt=%d) - clearing belt children"),
			RuntimeSettings.bEnabled, RuntimeSettings.bStackableBeltEnabled);
		
		// Clear all existing stackable belt children when disabled
		CleanupAllStackableBelts(ParentHologram);
		return;
	}
	
	// Track active pole pairs for orphan removal at end
	TSet<uint64> ActivePolePairs;

	// ========================================================================
	// GRID-BASED BELT CONNECTIONS (same approach as pipes)
	// ========================================================================
	// Connect poles that are X-neighbors in the grid (same Y, same Z, adjacent X).
	// This is the same logic used by ProcessStackablePipeSupports().
	//
	// Grid iteration order (how children are spawned): Z → X → Y
	// For each Z layer:
	//   For each X column:
	//     For each Y row:
	//       Spawn child (skipping [0,0,0] which is parent)
	//
	// To find X-neighbors, we need poles at [X, Y, Z] and [X+1, Y, Z].
	// ========================================================================

	// Get hologram helper to find children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🚧 No HologramHelper available"));
		return;
	}

	// Get grid dimensions from CounterState to understand the grid structure
	const FSFCounterState& CounterState = Subsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);

	// Collect ALL poles for belt connections (parent + children)
	TArray<AFGHologram*> AllPoles;
	AllPoles.Add(ParentHologram);

	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (ChildPtr.IsValid() && IsBeltSupportHologram(ChildPtr.Get()))
		{
			AllPoles.Add(ChildPtr.Get());
		}
	}

	if (AllPoles.Num() < 2)
	{
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🚧 Found %d belt support poles (parent + %d children), Grid[%d,%d,%d]"), 
		AllPoles.Num(), AllPoles.Num() - 1, XCount, YCount, ZCount);

	// Get belt tier from settings
	int32 BeltTier = RuntimeSettings.BeltTierMain;
	if (BeltTier == 0)
	{
		AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
			ParentHologram->GetConstructionInstigator()->GetController());
		BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
	}

	// Get belt direction from settings (0=Forward, 1=Backward)
	bool bReverseDirection = RuntimeSettings.StackableBeltDirection == 1;

	// Issue #268: Wall conveyor poles scale along Y axis instead of X.
	// Detect this so we connect Y-neighbors instead of X-neighbors.
	bool bConnectAlongY = IsWallConveyorPoleHologram(ParentHologram);

	// Build a map from grid position [X,Y,Z] to hologram for fast neighbor lookup
	TMap<int64, AFGHologram*> GridToHologram;
	
	auto PackGridPos = [](int32 X, int32 Y, int32 Z) -> int64 {
		return ((int64)(X + 128) << 16) | ((int64)(Y + 128) << 8) | (int64)(Z + 128);
	};
	
	// Parent is at [0,0,0]
	GridToHologram.Add(PackGridPos(0, 0, 0), ParentHologram);
	
	// Map children to their grid positions based on spawn order
	// Spawn order: Z → X → Y, skipping [0,0,0]
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount && ChildIndex < SpawnedChildren.Num(); ++Z)
	{
		for (int32 X = 0; X < XCount && ChildIndex < SpawnedChildren.Num(); ++X)
		{
			for (int32 Y = 0; Y < YCount && ChildIndex < SpawnedChildren.Num(); ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				if (SpawnedChildren[ChildIndex].IsValid() && IsBeltSupportHologram(SpawnedChildren[ChildIndex].Get()))
				{
					GridToHologram.Add(PackGridPos(X, Y, Z), SpawnedChildren[ChildIndex].Get());
				}
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🚧 Mapped %d conveyor poles to grid positions"), GridToHologram.Num());

	// Connect neighbors along the primary axis:
	// - Most belt supports: X-neighbors ([X,Y,Z] -> [X+1,Y,Z])
	// - Wall conveyor poles: Y-neighbors ([X,Y,Z] -> [X,Y+1,Z]) because they scale along Y
	int32 BeltIndex = 0;
	const int32 PrimaryCount = bConnectAlongY ? YCount : XCount;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 Y = 0; Y < YCount; ++Y)
		{
			for (int32 X = 0; X < XCount; ++X)
			{
				// Skip if we're at the last position along the primary axis
				if (bConnectAlongY && Y >= YCount - 1) continue;
				if (!bConnectAlongY && X >= XCount - 1) continue;

				AFGHologram** SourcePtr = GridToHologram.Find(PackGridPos(X, Y, Z));
				AFGHologram** TargetPtr = bConnectAlongY
					? GridToHologram.Find(PackGridPos(X, Y + 1, Z))
					: GridToHologram.Find(PackGridPos(X + 1, Y, Z));
				
				if (!SourcePtr || !TargetPtr || !*SourcePtr || !*TargetPtr)
				{
					continue; // One or both neighbors missing
				}
				
				// ========================================================================
				// DISTANCE AND ANGLE RESTRICTIONS
				// ========================================================================
				// Skip if poles are too far apart (>50m) or angle too steep (>30°)
				FVector Pos1 = (*SourcePtr)->GetActorLocation();
				FVector Pos2 = (*TargetPtr)->GetActorLocation();
				float Distance = FVector::Dist(Pos1, Pos2);
				
				// Distance restriction: >56m is too far (game engine limit)
				if (Distance > MAX_PIPE_LENGTH)
				{
					UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 Belt skipped: distance %.1f cm > 56m between [%d,%d,%d] and [%d,%d,%d]"),
						Distance, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Angle restriction: >30° from horizontal
				FVector DirectionVec = (Pos2 - Pos1).GetSafeNormal();
				float VerticalComponent = FMath::Abs(DirectionVec.Z);
				float AngleDegrees = FMath::RadiansToDegrees(FMath::Asin(VerticalComponent));
				if (AngleDegrees > 30.0f)
				{
					UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 Belt skipped: angle %.1f° > 30° between [%d,%d,%d] and [%d,%d,%d]"),
						AngleDegrees, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Direction: Forward connects X -> X+1, Backward connects X+1 -> X
				AFGHologram* SourcePole = bReverseDirection ? *TargetPtr : *SourcePtr;
				AFGHologram* TargetPole = bReverseDirection ? *SourcePtr : *TargetPtr;

				// Get the SnapOnly0 connector from each pole for positioning
				UFGFactoryConnectionComponent* SourceConnector = nullptr;
				UFGFactoryConnectionComponent* TargetConnector = nullptr;

				TArray<UFGFactoryConnectionComponent*> SourceConnectors;
				SourcePole->GetComponents<UFGFactoryConnectionComponent>(SourceConnectors);
				if (SourceConnectors.Num() > 0)
				{
					SourceConnector = SourceConnectors[0];
				}
				
				TArray<UFGFactoryConnectionComponent*> TargetConnectors;
				TargetPole->GetComponents<UFGFactoryConnectionComponent>(TargetConnectors);
				if (TargetConnectors.Num() > 0)
				{
					TargetConnector = TargetConnectors[0];
				}

				// Track this pole pair as active
				uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
				ActivePolePairs.Add(PairKey);

				// Create or update belt hologram child
				AFGHologram* BeltChild = UpdateOrCreateBeltForPolePair(
					ParentHologram, SourcePole, TargetPole,
					SourceConnector, TargetConnector, BeltTier, BeltIndex++);
				
				if (!BeltChild)
				{
					UE_LOG(LogSmartAutoConnect, Warning, TEXT("🚧 ⚠️ Failed to create belt for grid [%d,%d,%d] -> [%d,%d,%d]"),
						X, Y, Z, X + 1, Y, Z);
				}
			}
		}
	}
	
	// Remove orphaned belts (poles that were removed from grid)
	RemoveOrphanedBelts(ParentHologram, ActivePolePairs);
	
	// Ensure visibility is correct for all tracked belts
	FStackableBeltState* FinalState = StackableBeltStates.Find(ParentHologram);
	if (FinalState)
	{
		bool bParentLocked = ParentHologram->IsHologramLocked();
		for (auto& Pair : FinalState->BeltsByPolePair)
		{
			if (Pair.Value.IsValid())
			{
				AFGHologram* Belt = Pair.Value.Get();
				Belt->SetActorHiddenInGame(false);
				Belt->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
				
				if (bParentLocked && !Belt->IsHologramLocked())
				{
					Belt->LockHologramPosition(true);
				}
			}
		}
	}
}

void USFAutoConnectService::ProcessStackablePipelineSupports(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("🔧 ProcessStackablePipelineSupports: null parent hologram or subsystem"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		CleanupAllStackablePipes(ParentHologram);
		UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 ProcessStackablePipelineSupports: Skipped - Smart disabled for current action"));
		return;
	}

	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bPipeAutoConnectEnabled)
	{
		UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 ProcessStackablePipelineSupports: Auto-connect disabled (global=%d, pipe=%d) - clearing pipe children"),
			RuntimeSettings.bEnabled, RuntimeSettings.bPipeAutoConnectEnabled);
		
		// Clear all existing stackable pipe children when disabled
		CleanupAllStackablePipes(ParentHologram);
		return;
	}
	
	// Track active pole pairs for orphan removal at end
	TSet<uint64> ActivePolePairs;

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 ProcessStackablePipelineSupports: Starting pipeline support analysis for %s"), 
		*ParentHologram->GetName());

	// ========================================================================
	// STACKABLE PIPE SUPPORT AUTO-CONNECT
	// ========================================================================
	// This system automatically spawns pipe holograms between scaled stackable
	// pipe supports (poles). Key concepts:
	//
	// COORDINATE SYSTEMS:
	// - World Space: Absolute game coordinates (e.g., X=292000, Y=-127300)
	// - Local Space: Relative to an actor's origin (e.g., X=0, Y=0 at actor center)
	// - Spline data for pipe holograms must be in LOCAL space relative to the
	//   pipe hologram's spawn position. If you use world coordinates, the pipe
	//   will render at the wrong location (offset by the world position).
	//
	// CONNECTOR POSITIONS:
	// - Pole holograms have PCT_SNAP_ONLY connectors (SnapOnly0) that are just
	//   snap points, not fluid connections. Pipes snap TO these but don't
	//   actually connect to them for fluid flow.
	// - When hologram is LOCKED, GetComponentLocation() may return stale data.
	//   Use GetActorLocation() + RotateVector(GetRelativeLocation()) instead.
	//
	// PIPE-TO-PIPE WIRING:
	// - Actual fluid connections are pipe-to-pipe at pole locations
	// - After build, FindCompatibleOverlappingConnection() finds adjacent pipes
	// - SetConnection() wires them together, then MergeNetworks() for fluid flow
	//
	// HOLOGRAM SPAWNING PATTERN (same as Extend):
	// 1. SpawnActor<ASFPipelineHologram> with bDeferConstruction=true
	// 2. SetBuildClass() BEFORE FinishSpawning (or GetDefaultBuildable crashes)
	// 3. Tag with SF_StackableChild for PostHologramPlacement skip
	// 4. FinishSpawning() - creates mSplineComponent
	// 5. SetSplineDataAndUpdate() - sets mSplineData in LOCAL coordinates
	// 6. AddChild() - registers with parent for vanilla build system
	// 7. TriggerMeshGeneration() - creates visible spline mesh components
	// 8. ForceApplyHologramMaterial() - applies hologram material to meshes
	// ========================================================================
	
	// Get hologram helper to find children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 No HologramHelper available"));
		return;
	}

	// Get grid dimensions from CounterState to understand the grid structure
	// Children are spawned in order: Z → X → Y (innermost), skipping [0,0,0] (parent)
	const FSFCounterState& CounterState = Subsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);
	
	// Collect ALL supports for pipe connections (parent + children)
	// Parent is at grid position [0,0,0], children follow in spawn order
	TArray<AFGHologram*> AllSupports;
	
	// Include parent hologram first (grid position [0,0,0])
	AllSupports.Add(ParentHologram);
	
	// Then add all children in their spawn order (which matches grid iteration order)
	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (ChildPtr.IsValid() && IsStackablePipelineSupportHologram(ChildPtr.Get()))
		{
			AllSupports.Add(ChildPtr.Get());
		}
	}

	if (AllSupports.Num() < 2)
	{
		// Need at least 2 supports for auto-connect - skip silently
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Found %d stackable pipeline supports (parent + %d children), Grid[%d,%d,%d]"), 
		AllSupports.Num(), AllSupports.Num() - 1, XCount, YCount, ZCount);
	
	// ========================================================================
	// GRID-BASED PIPE CONNECTIONS
	// ========================================================================
	// Connect poles that are X-neighbors in the grid (same Y, same Z, adjacent X).
	// This works correctly with stepping because we use grid indices, not world positions.
	//
	// Grid iteration order (how children are spawned): Z → X → Y
	// For each Z layer:
	//   For each X column:
	//     For each Y row:
	//       Spawn child (skipping [0,0,0] which is parent)
	//
	// To find X-neighbors, we need poles at [X, Y, Z] and [X+1, Y, Z].
	// ========================================================================

	// Get pipe tier and style from global settings
	int32 PipeTier = RuntimeSettings.PipeTierMain;
	bool bWithIndicator = RuntimeSettings.bPipeIndicator;
	
	// Handle tier 0 (Auto mode) - resolve to highest unlocked tier
	if (PipeTier == 0)
	{
		AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
			ParentHologram->GetConstructionInstigator()->GetController());
		PipeTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
		if (PipeTier == 0) PipeTier = 2; // Default to Mk2 if detection fails
		UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE: Auto tier resolved to Mk%d"), PipeTier);
	}

	// Build a map from grid position [X,Y,Z] to hologram for fast neighbor lookup
	// Key = packed grid position, Value = hologram pointer
	TMap<int64, AFGHologram*> GridToHologram;
	
	auto PackGridPos = [](int32 X, int32 Y, int32 Z) -> int64 {
		// Pack X, Y, Z into a single int64 key (each can be -128 to 127)
		return ((int64)(X + 128) << 16) | ((int64)(Y + 128) << 8) | (int64)(Z + 128);
	};
	
	// Parent is at [0,0,0]
	GridToHologram.Add(PackGridPos(0, 0, 0), ParentHologram);
	
	// Map children to their grid positions based on spawn order
	// Spawn order: Z → X → Y, skipping [0,0,0]
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount && ChildIndex < SpawnedChildren.Num(); ++Z)
	{
		for (int32 X = 0; X < XCount && ChildIndex < SpawnedChildren.Num(); ++X)
		{
			for (int32 Y = 0; Y < YCount && ChildIndex < SpawnedChildren.Num(); ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				if (SpawnedChildren[ChildIndex].IsValid() && IsStackablePipelineSupportHologram(SpawnedChildren[ChildIndex].Get()))
				{
					GridToHologram.Add(PackGridPos(X, Y, Z), SpawnedChildren[ChildIndex].Get());
				}
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Mapped %d supports to grid positions"), GridToHologram.Num());

	// Connect X-neighbors: for each grid position, connect to X+1 if it exists
	int32 PipeIndex = 0;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 Y = 0; Y < YCount; ++Y)
		{
			for (int32 X = 0; X < XCount - 1; ++X)  // Stop at XCount-1 since we connect to X+1
			{
				AFGHologram** SourcePtr = GridToHologram.Find(PackGridPos(X, Y, Z));
				AFGHologram** TargetPtr = GridToHologram.Find(PackGridPos(X + 1, Y, Z));
				
				if (!SourcePtr || !TargetPtr || !*SourcePtr || !*TargetPtr)
				{
					continue; // One or both neighbors missing
				}
				
				// ========================================================================
				// DISTANCE AND ANGLE RESTRICTIONS
				// ========================================================================
				// Skip if poles are too far apart (>50m) or angle too steep (>30°)
				FVector Pos1 = (*SourcePtr)->GetActorLocation();
				FVector Pos2 = (*TargetPtr)->GetActorLocation();
				float Distance = FVector::Dist(Pos1, Pos2);
				
				// Distance restriction: >56m is too far (game engine limit)
				if (Distance > MAX_PIPE_LENGTH)
				{
					UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 Pipe skipped: distance %.1f cm > 56m between [%d,%d,%d] and [%d,%d,%d]"),
						Distance, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Angle restriction: >30° from horizontal
				FVector DirectionVec = (Pos2 - Pos1).GetSafeNormal();
				float VerticalComponent = FMath::Abs(DirectionVec.Z);
				float AngleDegrees = FMath::RadiansToDegrees(FMath::Asin(VerticalComponent));
				if (AngleDegrees > 30.0f)
				{
					UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 Pipe skipped: angle %.1f° > 30° between [%d,%d,%d] and [%d,%d,%d]"),
						AngleDegrees, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				AFGHologram* SourceSupport = *SourcePtr;
				AFGHologram* TargetSupport = *TargetPtr;
				
				// Get the SnapOnly0 connector from each pole hologram for accurate positioning
				UFGPipeConnectionComponent* SourceConnector = nullptr;
				UFGPipeConnectionComponent* TargetConnector = nullptr;
				
				TArray<UFGPipeConnectionComponent*> SourceConnectors;
				SourceSupport->GetComponents<UFGPipeConnectionComponent>(SourceConnectors);
				if (SourceConnectors.Num() > 0)
				{
					SourceConnector = SourceConnectors[0];
				}
				
				TArray<UFGPipeConnectionComponent*> TargetConnectors;
				TargetSupport->GetComponents<UFGPipeConnectionComponent>(TargetConnectors);
				if (TargetConnectors.Num() > 0)
				{
					TargetConnector = TargetConnectors[0];
				}
				
				// Track this pole pair as active
				uint64 PairKey = MakePolePairKey(SourceSupport, TargetSupport);
				ActivePolePairs.Add(PairKey);
				
				// Update or create pipe for this pole pair
				UpdateOrCreatePipeForPolePair(
					ParentHologram,
					SourceSupport,
					TargetSupport,
					SourceConnector,
					TargetConnector,
					PipeTier,
					bWithIndicator,
					PipeIndex++);
			}
		}
	}
	
	// Remove pipes for pole pairs that no longer exist (e.g., grid scaled down)
	RemoveOrphanedPipes(ParentHologram, ActivePolePairs);
	
	// ========================================================================
	// VISIBILITY REFRESH FOR ALL TRACKED PIPES
	// ========================================================================
	// When parent is locked (during scaling), vanilla may hide children.
	// Ensure ALL tracked pipes have correct visibility regardless of pole pair changes.
	// This runs AFTER all updates to catch any pipes that weren't individually updated.
	FStackablePipeState* FinalState = StackablePipeStates.Find(ParentHologram);
	if (FinalState)
	{
		bool bParentLocked = ParentHologram->IsHologramLocked();
		EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
		
		for (auto& Pair : FinalState->PipesByPolePair)
		{
			if (Pair.Value.IsValid())
			{
				ASFPipelineHologram* Pipe = Cast<ASFPipelineHologram>(Pair.Value.Get());
				if (Pipe)
				{
					// GRID SCALING PATTERN: Children must match parent's lock state for visibility
					// (from SFHologramHelperService::RestoreChildLock documentation)
					bool bPipeLocked = Pipe->IsHologramLocked();
					if (bParentLocked && !bPipeLocked)
					{
						Pipe->LockHologramPosition(true);  // Match parent's locked state
					}
					else if (!bParentLocked && bPipeLocked)
					{
						Pipe->LockHologramPosition(false);  // Match parent's unlocked state
					}
					
					// Ensure visibility and material state
					Pipe->SetActorHiddenInGame(false);
					Pipe->SetPlacementMaterialState(ParentMaterialState);
				}
			}
		}
	}
}

// ========================================================================
// POLE-PAIR BASED PIPE TRACKING
// ========================================================================

uint64 USFAutoConnectService::MakePolePairKey(const AFGHologram* PoleA, const AFGHologram* PoleB)
{
	if (!PoleA || !PoleB) return 0;
	
	uint32 IdA = PoleA->GetUniqueID();
	uint32 IdB = PoleB->GetUniqueID();
	
	// Sort IDs so (A,B) and (B,A) produce the same key
	uint32 LowId = FMath::Min(IdA, IdB);
	uint32 HighId = FMath::Max(IdA, IdB);
	
	return ((uint64)LowId << 32) | (uint64)HighId;
}

AFGHologram* USFAutoConnectService::UpdateOrCreatePipeForPolePair(
	AFGHologram* ParentHologram,
	AFGHologram* SourcePole,
	AFGHologram* TargetPole,
	UFGPipeConnectionComponent* SourceConnector,
	UFGPipeConnectionComponent* TargetConnector,
	int32 PipeTier,
	bool bWithIndicator,
	int32 PipeIndex)
{
	if (!ParentHologram || !SourcePole || !TargetPole || !Subsystem)
	{
		return nullptr;
	}
	
	uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
	FStackablePipeState& State = StackablePipeStates.FindOrAdd(ParentHologram);
	
	// Get pole positions - these should be WORLD positions
	FVector SourcePolePos = SourcePole->GetActorLocation();
	FVector TargetPolePos = TargetPole->GetActorLocation();
	
	// Calculate positions using pole actor location + connector relative offset
	FVector StartPos, EndPos;
	if (SourceConnector)
	{
		FVector ConnectorRelative = SourceConnector->GetRelativeLocation();
		StartPos = SourcePolePos + SourcePole->GetActorRotation().RotateVector(ConnectorRelative);
	}
	else
	{
		// No connector component on hologram - use pole location + Z offset for snap point
		StartPos = SourcePolePos + FVector(0, 0, 200.0f);
	}
	
	if (TargetConnector)
	{
		FVector ConnectorRelative = TargetConnector->GetRelativeLocation();
		EndPos = TargetPolePos + TargetPole->GetActorRotation().RotateVector(ConnectorRelative);
	}
	else
	{
		// No connector component on hologram - use pole location + Z offset for snap point
		EndPos = TargetPolePos + FVector(0, 0, 200.0f);
	}
	
	// DEBUG: Log positions to diagnose pipe placement issue
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 PIPE POSITIONS [%d]: SourcePole=%s @ %s, TargetPole=%s @ %s"),
		PipeIndex, *SourcePole->GetName(), *SourcePolePos.ToString(), *TargetPole->GetName(), *TargetPolePos.ToString());
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 PIPE ENDPOINTS [%d]: Start=%s, End=%s, Dist=%.1f"),
		PipeIndex, *StartPos.ToString(), *EndPos.ToString(), FVector::Dist(StartPos, EndPos));
	
	// Issue #291 (pipe variant): route straight toward the partner pole in 3D. Pole forward vector
	// was causing the same bulge/ramp seen on belts when pole forward didn't exactly match the
	// direction between pole endpoints (slight Z tilt or perpendicular pole rotation).
	const FVector Direction = (EndPos - StartPos);
	const FVector ToTarget = Direction.GetSafeNormal();
	const FVector ToSource = (-Direction).GetSafeNormal();

	FVector StartNormal = ToTarget;
	FVector EndNormal = ToSource;

	if (StartNormal.IsNearlyZero())
	{
		StartNormal = SourcePole ? SourcePole->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	}
	if (EndNormal.IsNearlyZero())
	{
		EndNormal = TargetPole ? (-TargetPole->GetActorForwardVector()).GetSafeNormal() : -FVector::ForwardVector;
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE: Routing endpoints StartN=%s EndN=%s"), *StartNormal.ToString(), *EndNormal.ToString());
	
	// ========================================================================
	// BUILD 6-POINT SPLINE WITH 50CM STRAIGHT SECTIONS AT EACH END
	// ========================================================================
	// Stackable poles have connectors facing each other horizontally.
	// The pipe should stick out 50cm straight (parallel to ground) from each
	// pole before curving to meet in the middle.
	
	float Distance = FVector::Dist(StartPos, EndPos);
	
	// Skip if too long (game engine limit)
	if (Distance > MAX_PIPE_LENGTH)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 STACKABLE PIPE: Skipping pipe %d - distance %.1f exceeds max %.1f"),
			PipeIndex, Distance, MAX_PIPE_LENGTH);
		return nullptr;
	}
	
	// Horizontal direction between poles (parallel to ground)
	FVector HorizontalDir = EndPos - StartPos;
	HorizontalDir.Z = 0;  // Remove vertical component
	HorizontalDir = HorizontalDir.GetSafeNormal();
	
	// End position in local space
	FVector EndLocal = EndPos - StartPos;
	
	// 50cm straight section at each end
	const float StraightLength = 50.0f;
	
	// Calculate the key positions first
	FVector Point1Pos = HorizontalDir * StraightLength;                    // 50cm out from start
	FVector Point4Pos = EndLocal - HorizontalDir * StraightLength;         // 50cm in from end
	
	// The curve direction is the direct path from Point 1 to Point 4
	FVector CurveVector = Point4Pos - Point1Pos;
	FVector CurveDir = CurveVector.GetSafeNormal();
	float CurveLength = CurveVector.Size();
	
	// Tangent strengths - scale with curve length for smooth transitions
	const float HorizontalTangent = 50.0f;
	float CurveTangent = FMath::Max(50.0f, CurveLength * 0.25f);
	
	// Build 6-point spline in LOCAL space (relative to StartPos)
	TArray<FSplinePointData> SplinePoints;
	
	// Point 0: Start connector - pipe exits horizontally
	FSplinePointData Point0;
	Point0.Location = FVector::ZeroVector;
	Point0.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point0.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point0);
	
	// Point 1: End of 50cm straight section from start - transition to curve
	FSplinePointData Point1;
	Point1.Location = Point1Pos;
	Point1.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point1.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point1);
	
	// Point 2: 1/3 along the curve section
	FSplinePointData Point2;
	Point2.Location = Point1Pos + CurveDir * (CurveLength * 0.33f);
	Point2.ArriveTangent = CurveDir * CurveTangent;
	Point2.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point2);
	
	// Point 3: 2/3 along the curve section
	FSplinePointData Point3;
	Point3.Location = Point1Pos + CurveDir * (CurveLength * 0.67f);
	Point3.ArriveTangent = CurveDir * CurveTangent;
	Point3.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point3);
	
	// Point 4: Start of 50cm straight section at end - transition from curve
	FSplinePointData Point4;
	Point4.Location = Point4Pos;
	Point4.ArriveTangent = CurveDir * CurveTangent;
	Point4.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point4);
	
	// Point 5: End connector - pipe enters horizontally
	FSplinePointData Point5;
	Point5.Location = EndLocal;
	Point5.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point5.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point5);
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 STACKABLE PIPE: 6-point spline (dist=%.1f, curve=%.1f)"), Distance, CurveLength);
	
	// Check if we already have a pipe for this pole pair
	TWeakObjectPtr<AFGHologram>* ExistingPipePtr = State.PipesByPolePair.Find(PairKey);
	if (ExistingPipePtr && ExistingPipePtr->IsValid())
	{
		ASFPipelineHologram* ExistingPipe = Cast<ASFPipelineHologram>(ExistingPipePtr->Get());
		if (ExistingPipe)
		{
			// Check if tier/indicator has changed - if so, we need to recreate the pipe
			UClass* DesiredBuildClass = Subsystem->GetPipeClassFromConfig(PipeTier, bWithIndicator, nullptr);
			UClass* CurrentBuildClass = ExistingPipe->GetBuildClass();
			
			if (DesiredBuildClass && CurrentBuildClass != DesiredBuildClass)
			{
				// Tier or indicator changed - destroy existing pipe and create new one below
				UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE: Tier/indicator changed for pair 0x%016llX - recreating pipe"), PairKey);
				
				// Remove from parent's mChildren array
				FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
				if (ChildrenProp)
				{
					TArray<AFGHologram*>* ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
					if (ParentChildrenArray)
					{
						ParentChildrenArray->Remove(ExistingPipe);
					}
				}
				
				ExistingPipe->Destroy();
				State.PipesByPolePair.Remove(PairKey);
				ExistingPipePtr = nullptr;  // Fall through to create new pipe
			}
			else
			{
			// ========================================================================
			// GRID SCALING PATTERN: Temporary unlock -> Update -> Restore lock
			// Same pattern as SFGridSpawnerService uses for grid children visibility
			// ========================================================================
			bool bParentLocked = ParentHologram->IsHologramLocked();
			bool bPipeWasLocked = ExistingPipe->IsHologramLocked();
			
			// Step 1: Temporarily unlock for positioning (same as TemporarilyUnlockChild)
			if (bParentLocked && bPipeWasLocked)
			{
				ExistingPipe->LockHologramPosition(false);
			}
			
			// Step 2: Update position and spline data
			ExistingPipe->SetActorLocation(StartPos);
			if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
			{
				const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
				ExistingPipe->SetRoutingMode(Settings.PipeRoutingMode);
			}
			if (!ExistingPipe->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
			{
				ExistingPipe->SetSplineDataAndUpdate(SplinePoints);
			}
			
			// Step 3: Ensure visibility state is correct
			ExistingPipe->SetActorHiddenInGame(false);
			ExistingPipe->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
			
			// Step 4: Regenerate mesh and apply hologram material
			ExistingPipe->TriggerMeshGeneration();
			ExistingPipe->ForceApplyHologramMaterial();
			
			// Step 5: Restore lock state (same as RestoreChildLock)
			// Children must match parent's lock state for visibility
			if (bParentLocked)
			{
				ExistingPipe->LockHologramPosition(true);
			}
			
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 STACKABLE PIPE: Updated existing pipe %s for pair 0x%016llX (locked=%d)"), 
				*ExistingPipe->GetName(), PairKey, bParentLocked ? 1 : 0);
			
			return ExistingPipe;
			}  // end else (tier unchanged)
		}  // end if (ExistingPipe)
	}  // end if (ExistingPipePtr)
	
	// CREATE new pipe hologram
	UClass* PipeBuildClass = Subsystem->GetPipeClassFromConfig(PipeTier, bWithIndicator, nullptr);
	if (!PipeBuildClass)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("🔧 STACKABLE PIPE: No pipe build class for tier %d"), PipeTier);
		return nullptr;
	}
	
	static int32 StackablePipeCounter = 0;
	FName ChildName(*FString::Printf(TEXT("StackablePipe_%d"), StackablePipeCounter++));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	UWorld* World = ParentHologram->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
		ASFPipelineHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!PipeChild)
	{
		UE_LOG(LogSmartAutoConnect, Error, TEXT("🔧 STACKABLE PIPE: SpawnActor returned null"));
		return nullptr;
	}
	
	PipeChild->SetReplicates(false);
	PipeChild->SetReplicateMovement(false);
	PipeChild->SetBuildClass(PipeBuildClass);
	PipeChild->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
	
	// Set the pipe recipe so vanilla can aggregate costs for children
	TSubclassOf<UFGRecipe> PipeRecipe = Subsystem->GetPipeRecipeForTier(PipeTier, bWithIndicator);
	if (PipeRecipe)
	{
		PipeChild->SetRecipe(PipeRecipe);
	}
	
	USFHologramDataService::DisableValidation(PipeChild);
	USFHologramDataService::MarkAsChild(PipeChild, ParentHologram, ESFChildHologramType::AutoConnect);
	
	// Store data for post-build wiring
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(PipeChild);
	if (HoloData)
	{
		HoloData->bIsStackablePipe = true;
		HoloData->StackablePipeConn0 = SourceConnector;
		HoloData->StackablePipeConn1 = TargetConnector;
		HoloData->StackablePipeIndex = PipeIndex;
	}
	
	PipeChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections for validation
	if (SourceConnector || TargetConnector)
	{
		FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
		if (SnappedProp)
		{
			void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(PipeChild);
			UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
			if (SnappedArray)
			{
				SnappedArray[0] = SourceConnector;
				SnappedArray[1] = TargetConnector;
			}
		}
	}
	
	if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
	{
		const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
		PipeChild->SetRoutingMode(Settings.PipeRoutingMode);
	}
	if (!PipeChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		PipeChild->SetSplineDataAndUpdate(SplinePoints);
	}
	PipeChild->SetActorHiddenInGame(false);
	PipeChild->SetActorEnableCollision(false);
	PipeChild->SetActorTickEnabled(false);
	PipeChild->RegisterAllComponents();
	PipeChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	
	ParentHologram->AddChild(PipeChild, ChildName);
	
	PipeChild->TriggerMeshGeneration();
	PipeChild->ForceApplyHologramMaterial();
	
	// Track by pole-pair key
	State.PipesByPolePair.Add(PairKey, PipeChild);
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE: Created new pipe %s for pair 0x%016llX (dist=%.1f)"), 
		*ChildName.ToString(), PairKey, Distance);
	
	return PipeChild;
}

void USFAutoConnectService::RemoveOrphanedPipes(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackablePipeState* State = StackablePipeStates.Find(ParentHologram);
	if (!State)
	{
		return;
	}
	
	// Get parent's mChildren array for removal
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	// Find and remove pipes for pole pairs that are no longer active
	TArray<uint64> KeysToRemove;
	for (auto& Pair : State->PipesByPolePair)
	{
		if (!ActivePolePairs.Contains(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
			
			if (Pair.Value.IsValid())
			{
				AFGHologram* Pipe = Pair.Value.Get();
				if (ParentChildrenArray)
				{
					ParentChildrenArray->Remove(Pipe);
				}
				UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE: Removing orphaned pipe %s (pair 0x%016llX)"), 
					*Pipe->GetName(), Pair.Key);
				Pipe->Destroy();
			}
		}
	}
	
	for (uint64 Key : KeysToRemove)
	{
		State->PipesByPolePair.Remove(Key);
	}
}

void USFAutoConnectService::CleanupAllStackablePipes(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackablePipeState* State = StackablePipeStates.Find(ParentHologram);
	if (!State || State->PipesByPolePair.Num() == 0)
	{
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🔧 STACKABLE PIPE CLEANUP: Removing all %d pipes for %s"), 
		State->PipesByPolePair.Num(), *ParentHologram->GetName());
	
	// Get parent's mChildren array for removal
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	for (auto& Pair : State->PipesByPolePair)
	{
		if (Pair.Value.IsValid())
		{
			AFGHologram* Pipe = Pair.Value.Get();
			if (ParentChildrenArray)
			{
				ParentChildrenArray->Remove(Pipe);
			}
			Pipe->Destroy();
		}
	}
	
	State->PipesByPolePair.Empty();
	StackablePipeStates.Remove(ParentHologram);
}

void USFAutoConnectService::FinalizeBeltChildrenVisibility(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	// Get belt previews for this distributor
	TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = DistributorBeltPreviews.Find(ParentHologram);
	if (!Previews || Previews->Num() == 0)
	{
		return;
	}
	
	// Finalize visibility and locking for all belt children (matches stackable pattern)
	bool bParentLocked = ParentHologram->IsHologramLocked();
	EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
	
	for (const TSharedPtr<FBeltPreviewHelper>& Preview : *Previews)
	{
		if (Preview.IsValid() && Preview->IsPreviewValid())
		{
			AFGSplineHologram* BeltChild = Preview->GetHologram();
			if (BeltChild)
			{
				// Ensure visibility (critical for locked parent updates)
				BeltChild->SetActorHiddenInGame(false);
				
				// Match parent's material state (valid/invalid)
				BeltChild->SetPlacementMaterialState(ParentMaterialState);
				
				// Match parent's lock state
				if (bParentLocked && !BeltChild->IsHologramLocked())
				{
					BeltChild->LockHologramPosition(true);
				}
			}
		}
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("✅ Finalized visibility for %d belt children of %s (locked=%d)"), 
		Previews->Num(), *ParentHologram->GetName(), bParentLocked ? 1 : 0);
}

void USFAutoConnectService::ProcessPowerPoles(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("⚡ ProcessPowerPoles: null parent hologram"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem && Subsystem->IsSmartDisabledForCurrentAction())
	{
		// Clear any existing previews since we're disabled
		ClearAllPowerPreviews();
		UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ ProcessPowerPoles: Skipped - Smart disabled for current action"));
		return;
	}

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("⚡ ProcessPowerPoles: Starting power pole analysis for %s"), 
		*ParentHologram->GetName());

	// Get or create power manager for this parent hologram
	TSharedPtr<FSFPowerAutoConnectManager>* ManagerPtr = PowerAutoConnectManagers.Find(ParentHologram);
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager
		TSharedPtr<FSFPowerAutoConnectManager> NewManager = MakeShared<FSFPowerAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PowerAutoConnectManagers.Add(ParentHologram, NewManager);
		ManagerPtr = &PowerAutoConnectManagers[ParentHologram];
		
		UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ ProcessPowerPoles: Created new power manager"));
	}

	// Process all power poles through the manager
	if (ManagerPtr->IsValid())
	{
		(*ManagerPtr)->ProcessAllPowerPoles(ParentHologram);
		
		// Power pole affordability is now handled by GetCost hook - vanilla ValidatePlacementAndCost
	// will automatically check the total cost (poles + wires) and add disqualifiers
	}
}

void USFAutoConnectService::ClearAllPowerPreviews()
{
	UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ ClearAllPowerPreviews: Clearing all power line previews (%d managers)"), 
		PowerAutoConnectManagers.Num());
	
	// CRITICAL: Commit planned connections BEFORE clearing anything!
	// This is called when build happens (hologram destroyed), so we need to preserve
	// the planned connections for the deferred OnPowerPoleBuilt processing.
	// The new hologram will trigger ProcessAllPowerPoles which would clear PlannedPoleConnections
	// BEFORE OnActorSpawned can call CommitBuildingConnections, so we do it here first.
	if (Subsystem)
	{
		Subsystem->CommitBuildingConnections();
	}
	
	// Clear all power lines from all managers
	for (auto& Pair : PowerAutoConnectManagers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ClearPowerLinePreviews();
			Pair.Value->ClearAllReservations();
			Pair.Value->ResetSpacingState();
		}
	}
	
	// Now remove all managers
	PowerAutoConnectManagers.Empty();
	
	// Note: power wire costs now aggregate through real child holograms.
}

FSFPowerAutoConnectManager* USFAutoConnectService::GetPowerManager(AFGHologram* PoleHologram)
{
	if (!PoleHologram)
	{
		return nullptr;
	}
	
	if (TSharedPtr<FSFPowerAutoConnectManager>* ManagerPtr = PowerAutoConnectManagers.Find(PoleHologram))
	{
		if (ManagerPtr->IsValid())
		{
			return ManagerPtr->Get();
		}
	}
	
	return nullptr;
}

TArray<FPowerPoleGridNode> USFAutoConnectService::AnalyzeGridTopology(const TArray<AFGHologram*>& AllPoles)
{
	TArray<FPowerPoleGridNode> GridNodes;
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ AnalyzeGridTopology: Analyzing %d poles"), AllPoles.Num());
	
	if (AllPoles.Num() == 0)
	{
		return GridNodes;
	}
	
	// Issue #244: Use GRID POSITIONS instead of spatial distance for neighbor detection
	// This correctly handles transforms like stepping, rotation, stagger that change positions
	// Grid positions are computed from array index - children spawn in Z→X→Y order
	// Parent is at (0,0,0), first child at (0,1,0), etc.
	
	// Get grid dimensions from subsystem
	USFSubsystem* SmartSubsystem = USFSubsystem::Get(AllPoles[0]->GetWorld());
	if (!SmartSubsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("⚡ AnalyzeGridTopology: No subsystem - falling back to empty"));
		return GridNodes;
	}
	
	const FSFCounterState& CounterState = SmartSubsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);
	int32 XDir = CounterState.GridCounters.X >= 0 ? 1 : -1;
	int32 YDir = CounterState.GridCounters.Y >= 0 ? 1 : -1;
	int32 ZDir = CounterState.GridCounters.Z >= 0 ? 1 : -1;
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ AnalyzeGridTopology: Grid dimensions %dx%dx%d, dirs [%d,%d,%d]"),
		XCount, YCount, ZCount, XDir, YDir, ZDir);
	
	// Build a map from grid position to pole hologram
	// AllPoles[0] = parent at (0,0,0)
	// AllPoles[1+] = children in Z→X→Y spawn order (skipping 0,0,0)
	TMap<FIntVector, AFGHologram*> GridPositionToPole;
	TMap<AFGHologram*, FIntVector> PoleToGridPosition;
	
	// Parent is always at (0,0,0)
	if (AllPoles.Num() > 0 && AllPoles[0])
	{
		FIntVector ParentPos(0, 0, 0);
		GridPositionToPole.Add(ParentPos, AllPoles[0]);
		PoleToGridPosition.Add(AllPoles[0], ParentPos);
	}
	
	// Children are spawned in Z→X→Y order, skipping (0,0,0)
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 X = 0; X < XCount; ++X)
		{
			for (int32 Y = 0; Y < YCount; ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				int32 ArrayIndex = ChildIndex + 1; // +1 because parent is at index 0
				if (ArrayIndex >= AllPoles.Num())
				{
					break;
				}
				
				AFGHologram* Pole = AllPoles[ArrayIndex];
				if (Pole)
				{
					FIntVector GridPos(X * XDir, Y * YDir, Z * ZDir);
					GridPositionToPole.Add(GridPos, Pole);
					PoleToGridPosition.Add(Pole, GridPos);
				}
				
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ AnalyzeGridTopology: Mapped %d poles to grid positions"), PoleToGridPosition.Num());
	
	// Now find neighbors by grid position (adjacent = differ by 1 in exactly one axis)
	for (AFGHologram* Pole : AllPoles)
	{
		if (!Pole)
		{
			continue;
		}
		
		FIntVector* GridPosPtr = PoleToGridPosition.Find(Pole);
		if (!GridPosPtr)
		{
			continue;
		}
		
		FIntVector GridPos = *GridPosPtr;
		
		FPowerPoleGridNode Node;
		Node.Pole = Pole;
		Node.GridPosition = FIntVector2(GridPos.X, GridPos.Y);
		
		// Find X-axis neighbors (differ by 1 in X, same Y and Z)
		FIntVector XPosNeighbor(GridPos.X + XDir, GridPos.Y, GridPos.Z);
		FIntVector XNegNeighbor(GridPos.X - XDir, GridPos.Y, GridPos.Z);
		
		if (AFGHologram** XPosPtr = GridPositionToPole.Find(XPosNeighbor))
		{
			Node.XAxisNeighbors.Add(*XPosPtr);
		}
		if (AFGHologram** XNegPtr = GridPositionToPole.Find(XNegNeighbor))
		{
			Node.XAxisNeighbors.Add(*XNegPtr);
		}
		
		// Find Y-axis neighbors (differ by 1 in Y, same X and Z)
		FIntVector YPosNeighbor(GridPos.X, GridPos.Y + YDir, GridPos.Z);
		FIntVector YNegNeighbor(GridPos.X, GridPos.Y - YDir, GridPos.Z);
		
		if (AFGHologram** YPosPtr = GridPositionToPole.Find(YPosNeighbor))
		{
			Node.YAxisNeighbors.Add(*YPosPtr);
		}
		if (AFGHologram** YNegPtr = GridPositionToPole.Find(YNegNeighbor))
		{
			Node.YAxisNeighbors.Add(*YNegPtr);
		}
		
		UE_LOG(LogSmartAutoConnect, Log, TEXT("⚡ Pole %s at grid[%d,%d,%d]: %d X-neighbors, %d Y-neighbors"),
			*Pole->GetName(), GridPos.X, GridPos.Y, GridPos.Z,
			Node.XAxisNeighbors.Num(), Node.YAxisNeighbors.Num());
		
		GridNodes.Add(Node);
	}
	
	return GridNodes;
}

bool USFAutoConnectService::AreGridAxisNeighbors(AFGHologram* PoleA, AFGHologram* PoleB, 
	const FVector& GridXAxis, const FVector& GridYAxis, bool bXAxis)
{
	if (!PoleA || !PoleB)
	{
		return false;
	}
	
	FVector PosA = PoleA->GetActorLocation();
	FVector PosB = PoleB->GetActorLocation();
	
	// Maximum connection distance: 100m (10000cm)
	float Distance = FVector::Dist(PosA, PosB);
	if (Distance > 10000.0f)
	{
		return false;
	}
	
	FVector Delta = PosB - PosA;
	
	// Project delta onto grid axes
	// GridXAxis = Forward (columns), GridYAxis = Right (rows)
	float DeltaGridX = FVector::DotProduct(Delta, GridXAxis);  // Component along grid X (Forward)
	float DeltaGridY = FVector::DotProduct(Delta, GridYAxis);  // Component along grid Y (Right)
	float DeltaZ = Delta.Z; // Z is always world Z (height)
	
	// Tolerance for "same axis" detection (1m = 100cm)
	const float AxisTolerance = 100.0f;
	
	if (bXAxis)
	{
		// X-axis neighbors: Small Y component and Z difference, significant X component
		// This checks if they're on the same axis, but NOT if they're immediate neighbors
		// The immediate neighbor check is done in ConnectPoleToNeighbors by finding closest
		return FMath::Abs(DeltaGridY) < AxisTolerance && FMath::Abs(DeltaZ) < AxisTolerance;
	}
	else
	{
		// Y-axis neighbors: Small X component and Z difference, significant Y component
		return FMath::Abs(DeltaGridX) < AxisTolerance && FMath::Abs(DeltaZ) < AxisTolerance;
	}
}

int32 USFAutoConnectService::CalculateCableCost(float Distance)
{
	// Convert cm to meters
	float DistanceInMeters = Distance / 100.0f;
	
	// 1 Cable per 25 meters, rounded up
	int32 CablesNeeded = FMath::CeilToInt(DistanceInMeters / 25.0f);
	
	return FMath::Max(1, CablesNeeded); // Minimum 1 cable
}

// ========================================
// STACKABLE CONVEYOR POLE BELT FUNCTIONS
// ========================================

AFGHologram* USFAutoConnectService::UpdateOrCreateBeltForPolePair(
	AFGHologram* ParentHologram,
	AFGHologram* SourcePole,
	AFGHologram* TargetPole,
	UFGFactoryConnectionComponent* SourceConnector,
	UFGFactoryConnectionComponent* TargetConnector,
	int32 BeltTier,
	int32 BeltIndex)
{
	if (!ParentHologram || !SourcePole || !TargetPole || !Subsystem)
	{
		return nullptr;
	}
	
	uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
	FStackableBeltState& State = StackableBeltStates.FindOrAdd(ParentHologram);
	
	// Get connector positions for spline routing
	// NOTE: Stackable pole connectors are SnapOnly0 - they're snap points, not directional connectors
	// The connector normal points UP (vertical), not toward the next pole
	// For belt routing, we need HORIZONTAL direction between poles
	FVector StartPos = SourceConnector ? SourceConnector->GetComponentLocation() : SourcePole->GetActorLocation();
	FVector EndPos = TargetConnector ? TargetConnector->GetComponentLocation() : TargetPole->GetActorLocation();
	
	// Issue #291: For stackable/ceiling poles, the belt must exit the pole pointing DIRECTLY at
	// the partner pole (including pitch from any height delta). Using the pole's forward vector
	// produced a visible bulge/ramp at the pole whenever pole forward didn't exactly match
	// ToTarget — e.g., slight Z delta between poles (Dot > 0 so the old sign flip didn't fire),
	// or pole rotation perpendicular to the grid axis (Dot ≈ 0, also no flip → S-curve).
	//
	// Issue #268: Wall conveyor poles exit belts perpendicular to the wall (RightVector),
	// which is a deliberate orientation choice on the pole itself — preserve that behavior.
	const bool bUseRightVector = IsWallConveyorPoleHologram(ParentHologram);

	FVector Direction = (EndPos - StartPos);
	const FVector ToTarget = Direction.GetSafeNormal();
	const FVector ToSource = (-Direction).GetSafeNormal();

	FVector StartNormal;
	FVector EndNormal;

	if (bUseRightVector)
	{
		// Wall poles: keep existing "RightVector with sign correction" behavior (#268).
		StartNormal = SourcePole ? SourcePole->GetActorRightVector() : ToTarget;
		StartNormal = StartNormal.GetSafeNormal();
		if (!StartNormal.IsNearlyZero() && !ToTarget.IsNearlyZero() && FVector::DotProduct(StartNormal, ToTarget) < 0.0f)
		{
			StartNormal *= -1.0f;
		}

		EndNormal = TargetPole ? TargetPole->GetActorRightVector() : ToSource;
		EndNormal = EndNormal.GetSafeNormal();
		if (!EndNormal.IsNearlyZero() && !ToSource.IsNearlyZero() && FVector::DotProduct(EndNormal, ToSource) < 0.0f)
		{
			EndNormal *= -1.0f;
		}
	}
	else
	{
		// Stackable/ceiling poles: route straight toward the partner pole in 3D. Pole forward
		// is only used as a degenerate fallback for coincident endpoints.
		StartNormal = ToTarget;
		EndNormal = ToSource;
	}

	// Degenerate fallbacks (overlapping poles / zero direction)
	if (StartNormal.IsNearlyZero())
	{
		StartNormal = SourcePole ? SourcePole->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	}
	if (EndNormal.IsNearlyZero())
	{
		EndNormal = TargetPole ? (-TargetPole->GetActorForwardVector()).GetSafeNormal() : -FVector::ForwardVector;
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 STACKABLE BELT: Routing from %s to %s, StartN=%s EndN=%s, Dist=%.1f"),
		*StartPos.ToString(),
		*EndPos.ToString(),
		*StartNormal.ToString(),
		*EndNormal.ToString(),
		Direction.Size());
	
	// Check if we already have a belt for this pole pair
	TWeakObjectPtr<AFGHologram>* ExistingBeltPtr = State.BeltsByPolePair.Find(PairKey);
	if (ExistingBeltPtr && ExistingBeltPtr->IsValid())
	{
		ASFConveyorBeltHologram* ExistingBelt = Cast<ASFConveyorBeltHologram>(ExistingBeltPtr->Get());
		if (ExistingBelt)
		{
			// Check if tier has changed - if so, recreate the belt
			UClass* DesiredBuildClass = Subsystem->GetBeltClassForTier(BeltTier, nullptr);
			UClass* CurrentBuildClass = ExistingBelt->GetBuildClass();
			
			if (DesiredBuildClass && CurrentBuildClass != DesiredBuildClass)
			{
				// Tier changed - destroy existing belt and create new one below
				UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 STACKABLE BELT: Tier changed for pair 0x%016llX - recreating belt"), PairKey);
				
				FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
				if (ChildrenProp)
				{
					TArray<AFGHologram*>* ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
					if (ParentChildrenArray)
					{
						ParentChildrenArray->Remove(ExistingBelt);
					}
				}
				
				ExistingBelt->Destroy();
				State.BeltsByPolePair.Remove(PairKey);
				ExistingBeltPtr = nullptr;
			}
			else
			{
				// Update existing belt position and spline
				bool bParentLocked = ParentHologram->IsHologramLocked();
				bool bBeltWasLocked = ExistingBelt->IsHologramLocked();
				
				if (bParentLocked && bBeltWasLocked)
				{
					ExistingBelt->LockHologramPosition(false);
				}
				
				ExistingBelt->SetActorLocation(StartPos);
				if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
				{
					const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
					ExistingBelt->SetRoutingMode(Settings.BeltRoutingMode);
				}
				
				// Try using build mode for automatic spline routing, fallback to custom routing
				if (!ExistingBelt->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
				{
					// Build mode not available, use custom routing
					ExistingBelt->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
				}
				
				ExistingBelt->SetActorHiddenInGame(false);
				ExistingBelt->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
				ExistingBelt->TriggerMeshGeneration();
				ExistingBelt->ForceApplyHologramMaterial();
				
				if (bParentLocked)
				{
					ExistingBelt->LockHologramPosition(true);
				}
				
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🚧 STACKABLE BELT: Updated existing belt %s for pair 0x%016llX"), 
					*ExistingBelt->GetName(), PairKey);
				
				return ExistingBelt;
			}
		}
	}
	
	// CREATE new belt hologram
	UClass* BeltBuildClass = Subsystem->GetBeltClassForTier(BeltTier, nullptr);
	if (!BeltBuildClass)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("🚧 STACKABLE BELT: No belt build class for tier %d"), BeltTier);
		return nullptr;
	}
	
	static int32 StackableBeltCounter = 0;
	FName ChildName(*FString::Printf(TEXT("StackableBelt_%d"), StackableBeltCounter++));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	UWorld* World = ParentHologram->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	ASFConveyorBeltHologram* BeltChild = World->SpawnActor<ASFConveyorBeltHologram>(
		ASFConveyorBeltHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!BeltChild)
	{
		UE_LOG(LogSmartAutoConnect, Error, TEXT("🚧 STACKABLE BELT: SpawnActor returned null"));
		return nullptr;
	}
	
	BeltChild->SetReplicates(false);
	BeltChild->SetReplicateMovement(false);
	BeltChild->SetBuildClass(BeltBuildClass);
	BeltChild->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
	
	USFHologramDataService::DisableValidation(BeltChild);
	USFHologramDataService::MarkAsChild(BeltChild, ParentHologram, ESFChildHologramType::AutoConnect);
	
	// Store data for post-build wiring (ConfigureComponents will use this)
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(BeltChild);
	if (HoloData)
	{
		HoloData->bIsStackableBelt = true;
		HoloData->StackableBeltIndex = BeltIndex;
		// Store pole connector references for ConfigureComponents to wire after construction
		HoloData->StackableBeltConn0 = SourceConnector;  // Belt output connects to source pole
		HoloData->StackableBeltConn1 = TargetConnector;  // Belt input connects to target pole
	}
	
	BeltChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections to prevent child pole spawning
	BeltChild->SetSnappedConnections(SourceConnector, TargetConnector);
	
	if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
	{
		const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
		BeltChild->SetRoutingMode(Settings.BeltRoutingMode);
	}
	
	// Try using build mode for automatic spline routing, fallback to custom routing
	if (!BeltChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		// Build mode not available, use custom routing
		BeltChild->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
	}
	
	BeltChild->SetActorHiddenInGame(false);
	BeltChild->SetActorEnableCollision(false);
	BeltChild->SetActorTickEnabled(false);
	BeltChild->RegisterAllComponents();
	BeltChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	
	ParentHologram->AddChild(BeltChild, ChildName);
	
	BeltChild->TriggerMeshGeneration();
	BeltChild->ForceApplyHologramMaterial();
	
	// Track by pole-pair key
	State.BeltsByPolePair.Add(PairKey, BeltChild);
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 STACKABLE BELT: Created new belt %s for pair 0x%016llX"), 
		*ChildName.ToString(), PairKey);
	
	return BeltChild;
}

void USFAutoConnectService::RemoveOrphanedBelts(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackableBeltState* State = StackableBeltStates.Find(ParentHologram);
	if (!State)
	{
		return;
	}
	
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	TArray<uint64> KeysToRemove;
	for (auto& Pair : State->BeltsByPolePair)
	{
		if (!ActivePolePairs.Contains(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
			
			if (Pair.Value.IsValid())
			{
				AFGHologram* Belt = Pair.Value.Get();
				if (ParentChildrenArray)
				{
					ParentChildrenArray->Remove(Belt);
				}
				UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 STACKABLE BELT: Removing orphaned belt %s (pair 0x%016llX)"), 
					*Belt->GetName(), Pair.Key);
				Belt->Destroy();
			}
		}
	}
	
	for (uint64 Key : KeysToRemove)
	{
		State->BeltsByPolePair.Remove(Key);
	}
}

void USFAutoConnectService::CleanupAllStackableBelts(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackableBeltState* State = StackableBeltStates.Find(ParentHologram);
	if (!State || State->BeltsByPolePair.Num() == 0)
	{
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("🚧 CleanupAllStackableBelts: Cleaning up %d belt children for %s"),
		State->BeltsByPolePair.Num(), *ParentHologram->GetName());
	
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	for (auto& Pair : State->BeltsByPolePair)
	{
		if (Pair.Value.IsValid())
		{
			AFGHologram* Belt = Pair.Value.Get();
			if (ParentChildrenArray)
			{
				ParentChildrenArray->Remove(Belt);
			}
			Belt->Destroy();
		}
	}
	
	State->BeltsByPolePair.Empty();
	StackableBeltStates.Remove(ParentHologram);
}

