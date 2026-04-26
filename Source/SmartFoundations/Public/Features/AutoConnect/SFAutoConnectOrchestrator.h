// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryConnectionComponent.h"
#include "Hologram/FGHologram.h"
#include "Buildables/FGBuildable.h"
#include "SFAutoConnectOrchestrator.generated.h"

class USFAutoConnectService;
class FBeltPreviewHelper;

/**
 * Represents a potential connection between a distributor output and a building input.
 * Used for global scoring and optimal assignment to minimize crossovers.
 */
struct FPotentialConnection
{
	/** The distributor hologram that would make this connection */
	AFGHologram* Distributor = nullptr;
	
	/** The distributor's output connector (splitter) or input connector (merger) */
	UFGFactoryConnectionComponent* DistributorConnector = nullptr;
	
	/** The building's input connector (splitter) or output connector (merger) */
	UFGFactoryConnectionComponent* BuildingConnector = nullptr;
	
	/** The building this connection targets */
	AFGBuildable* Building = nullptr;
	
	/** 
	 * Connection score (lower = better).
	 * Combines distance, alignment, and lane-crossing penalty.
	 */
	float Score = FLT_MAX;
	
	/** Lane index of the distributor (based on lateral position in grid) */
	int32 DistributorLaneIndex = -1;
	
	/** Lane index of the building input (based on connector name, e.g., Input0 = 0) */
	int32 BuildingInputIndex = -1;
	
	/** Whether this is a valid connection candidate */
	bool bIsValid = false;
	
	/** For sorting - lower score = better connection */
	bool operator<(const FPotentialConnection& Other) const
	{
		return Score < Other.Score;
	}
};

/**
 * Orchestrator for managing auto-connect belt previews across a grid of distributors.
 * 
 * Responsibilities:
 * - Track all distributors in a grid (parent + children)
 * - Evaluate optimal connections for all distributors
 * - Manage shared input reservation to prevent conflicts
 * - Coordinate belt preview creation/updates
 * - Handle grid changes (children added/removed)
 * - Handle movement (parent/children repositioned)
 * 
 * Future: Will handle manifold connections between distributors
 */
UCLASS()
class SMARTFOUNDATIONS_API USFAutoConnectOrchestrator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the orchestrator for a parent distributor hologram.
	 * 
	 * @param InParentHologram - The parent distributor hologram
	 * @param InAutoConnectService - The auto-connect service to use for belt preview creation
	 */
	void Initialize(AFGHologram* InParentHologram, USFAutoConnectService* InAutoConnectService);

	/**
	 * Evaluate all distributors in the grid and create/update belt previews.
	 * This is the main entry point - call this whenever the grid changes or moves.
	 * 
	 * @param bForceRecreate - If true, destroy and recreate all belt previews (default: false, update existing)
	 */
	void EvaluateGrid(bool bForceRecreate = false);

	/**
	 * Notify the orchestrator that the grid has changed (children added/removed).
	 * This will trigger a full re-evaluation.
	 */
	void OnGridChanged();

	/**
	 * Notify the orchestrator that distributors have moved.
	 * This will update existing belt previews to new positions.
	 */
	void OnDistributorsMoved();

	/**
	 * Notify the orchestrator that pipe junctions have moved.
	 * This will update existing pipe previews to new positions.
	 */
	void OnPipeJunctionsMoved();

	/**
	 * Notify the orchestrator that the pipe grid has changed (children added/removed).
	 * This will trigger a full re-evaluation of pipe connections.
	 */
	void OnPipeGridChanged();

	/**
	 * Notify the orchestrator that power poles have moved.
	 * This will update existing power line previews to new positions.
	 */
	void OnPowerPolesMoved();

	/**
	 * Notify the orchestrator that the power pole grid has changed (children added/removed).
	 * This will trigger a full re-evaluation of power connections.
	 */
	void OnPowerGridChanged();

	// ========================================
	// Support Structure Auto-Connect (Issue #220)
	// ========================================

	/**
	 * Notify the orchestrator that stackable conveyor poles have changed.
	 * This will trigger belt preview creation between consecutive poles.
	 */
	void OnStackableConveyorPolesChanged();

	/**
	 * Notify the orchestrator that stackable pipeline supports have changed.
	 * This will trigger pipe preview creation between consecutive supports.
	 */
	void OnStackablePipelineSupportsChanged();

	// ========================================
	// Floor Hole Pipe Auto-Connect (Issue #187)
	// ========================================

	/**
	 * Notify the orchestrator that floor hole passthroughs have changed.
	 * Triggers pipe preview creation between floor holes and nearby buildings.
	 */
	void OnFloorHolePipesChanged();

	/**
	 * Force a refresh of all auto-connect previews.
	 * Called when settings change with Apply Immediately enabled.
	 */
	void ForceRefresh();

	/**
	 * Clean up all belt previews managed by this orchestrator.
	 */
	void Cleanup();

	/**
	 * Get the parent hologram this orchestrator manages.
	 */
	AFGHologram* GetParentHologram() const { return ParentHologram.Get(); }

	/**
	 * Check if orchestrator is currently evaluating belts (prevents recursive calls).
	 */
	bool IsEvaluating() const { return bIsEvaluatingBelts; }

	/**
	 * Get all distributor → building connection pairings.
	 * Returns a map of distributor hologram to array of building input connectors.
	 */
	const TMap<AFGHologram*, TArray<UFGFactoryConnectionComponent*>>& GetConnectionPairings() const { return ConnectionPairings; }

	/**
	 * Log all connection pairings for debugging.
	 */
	void LogConnectionPairings() const;

	/**
	 * Log manifold distributor→distributor pairings for debugging.
	 */
	void LogManifoldPairings() const;

private:
	/** Parent distributor hologram */
	UPROPERTY()
	TWeakObjectPtr<AFGHologram> ParentHologram;

	/** Auto-connect service for creating belt previews */
	UPROPERTY()
	USFAutoConnectService* AutoConnectService;

	/** Shared input reservation map - tracks which distributor claimed which building input */
	TMap<UFGFactoryConnectionComponent*, AFGHologram*> ReservedInputs;

	/** Manifold reservation map - tracks distributor→distributor chaining inputs separately */
	TMap<UFGFactoryConnectionComponent*, AFGHologram*> ManifoldReservedInputs;
	
	/** Manifold output reservation map - tracks which outputs are already used for manifold connections */
	TMap<UFGFactoryConnectionComponent*, AFGHologram*> ManifoldReservedOutputs;

	/** Connection pairings - tracks which building inputs each distributor connected to */
	TMap<AFGHologram*, TArray<UFGFactoryConnectionComponent*>> ConnectionPairings;

	/** Flag to prevent recursive evaluation during belt preview updates */
	bool bIsEvaluatingBelts = false;
	
	/** Issue #269: Flag set when evaluation is requested during cooldown, triggers re-eval when cooldown clears */
	bool bPendingBeltReevaluation = false;
	
	/** Flag to prevent recursive evaluation during pipe preview updates */
	bool bIsEvaluatingPipes = false;

	/** Flag to prevent recursive evaluation during power preview updates */
	bool bIsEvaluatingPower = false;

	/** Timer handle for clearing belt evaluation flag after a delay */
	FTimerHandle BeltCooldownTimer;
	
	/** Timer handle for clearing pipe evaluation flag after a delay */
	FTimerHandle PipeCooldownTimer;

	/** Timer handle for clearing power evaluation flag after a delay */
	FTimerHandle PowerCooldownTimer;
	
	/** Context-aware spacing: Track if spacing has been auto-adjusted to prevent repeated adjustments */
	bool bContextSpacingApplied = false;
	
	/** Context-aware spacing: Track which building class spacing was adjusted for (reset if target changes) */
	TWeakObjectPtr<UClass> LastTargetBuildingClass;

	/**
	 * Clear the belt evaluation flag after cooldown period.
	 */
	void ClearBeltEvaluationFlag();
	
	/**
	 * Clear the pipe evaluation flag after cooldown period.
	 */
	void ClearPipeEvaluationFlag();

	/**
	 * Clear the power evaluation flag after cooldown period.
	 */
	void ClearPowerEvaluationFlag();

	/**
	 * Collect all distributors in the grid (parent + children).
	 * 
	 * @param OutDistributors - Array to populate with distributor holograms
	 */
	void CollectDistributors(TArray<AFGHologram*>& OutDistributors);

	/**
	 * Evaluate connections for all distributors with shared input reservation.
	 * This is the core logic that determines which distributor connects to which building.
	 * 
	 * Uses global scoring approach:
	 * 1. Collect ALL potential (distributor, building input) pairs
	 * 2. Score each by lane alignment (penalize crossovers)
	 * 3. Sort by score and assign optimally
	 */
	void EvaluateConnections();
	
	/**
	 * Collect all potential connections between distributors and buildings.
	 * Does NOT make reservations - just gathers candidates with scores.
	 * 
	 * @param Distributors - All distributors in the grid
	 * @param ParentTransform - Transform for local-space lane calculations
	 * @param OutConnections - Array to populate with potential connections
	 */
	void CollectPotentialConnections(
		const TArray<AFGHologram*>& Distributors,
		const FTransform& ParentTransform,
		TArray<FPotentialConnection>& OutConnections);
	
	/**
	 * Calculate lane index for a distributor based on its lateral position in the grid.
	 * Uses parent-local coordinates so lanes are consistent regardless of grid rotation.
	 * 
	 * @param Distributor - The distributor hologram
	 * @param ParentTransform - Transform of the parent hologram
	 * @param AllDistributors - All distributors for calculating relative positions
	 * @return Lane index (0-based, from one side of grid to the other)
	 */
	int32 CalculateDistributorLaneIndex(
		AFGHologram* Distributor,
		const FTransform& ParentTransform,
		const TArray<AFGHologram*>& AllDistributors);
	
	/**
	 * Extract input index from a building connector name (e.g., "Input0" -> 0, "Input1" -> 1).
	 * 
	 * @param Connector - The building connector
	 * @return Input index, or -1 if not parseable
	 */
	int32 ExtractBuildingInputIndex(UFGFactoryConnectionComponent* Connector);

	// Debounce scheduling API (Belts)
	void ScheduleEvaluation(bool bForceRecreate);
	void RunScheduledEvaluation();

	// Debounce scheduling API (Pipes)
	void SchedulePipeEvaluation(bool bForceRecreate);
	void RunScheduledPipeEvaluation();

	// Debounce scheduling API (Power)
	void SchedulePowerEvaluation(bool bForceRecreate);
	void RunScheduledPowerEvaluation();

	/**
	 * Clear all belt previews for all distributors in the grid.
	 */
	void ClearAllPreviews();

	// Debounce state (Belts)
	FTimerHandle EvalTimerHandle;
	bool bEvalScheduled = false;
	bool bForceRecreatePending = false;
	
	// Debounce state (Pipes)
	FTimerHandle PipeEvalTimerHandle;
	bool bPipeEvalScheduled = false;
	bool bPipeForceRecreatePending = false;

	// Debounce state (Power)
	FTimerHandle PowerEvalTimerHandle;
	bool bPowerEvalScheduled = false;
	bool bPowerForceRecreatePending = false;

	// Debounce state (Stackable Pipeline Supports - Issue #220)
	FTimerHandle StackablePipeEvalTimerHandle;
	bool bStackablePipeEvalScheduled = false;

	/** Schedule stackable pipeline support evaluation with debouncing */
	void ScheduleStackablePipeEvaluation();
	
	/** Run the scheduled stackable pipeline support evaluation */
	void RunScheduledStackablePipeEvaluation();

	// Debounce state (Stackable Conveyor Poles - Issue #220)
	FTimerHandle StackableBeltEvalTimerHandle;
	bool bStackableBeltEvalScheduled = false;

	/** Schedule stackable conveyor pole evaluation with debouncing */
	void ScheduleStackableBeltEvaluation();
	
	/** Run the scheduled stackable conveyor pole evaluation */
	void RunScheduledStackableBeltEvaluation();

	// Debounce state (Floor Hole Pipes - Issue #187)
	FTimerHandle FloorHolePipeEvalTimerHandle;
	bool bFloorHolePipeEvalScheduled = false;

	/** Schedule floor hole pipe evaluation with debouncing */
	void ScheduleFloorHolePipeEvaluation();
	
	/** Run the scheduled floor hole pipe evaluation */
	void RunScheduledFloorHolePipeEvaluation();
};
