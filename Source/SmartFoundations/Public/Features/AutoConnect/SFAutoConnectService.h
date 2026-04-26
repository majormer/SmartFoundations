// Copyright 2024 Maxx. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FGFactoryConnectionComponent.h"
#include "Buildables/FGBuildable.h"
#include "Hologram/FGHologram.h"
#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
// NOTE: SFBeltCostProxyHologram.h removed - child holograms automatically aggregate costs via GetCost()
#include "SFAutoConnectService.generated.h"

// Forward declarations
class USFSubsystem;
class FSFPipeAutoConnectManager;
class FSFPowerAutoConnectManager;

/**
 * Power pole grid node for topology analysis
 * Must be declared before the class to be used in public methods
 */
struct FPowerPoleGridNode
{
	AFGHologram* Pole;
	FIntVector2 GridPosition;
	TArray<AFGHologram*> XAxisNeighbors;
	TArray<AFGHologram*> YAxisNeighbors;
};

/**
 * SFAutoConnectService
 * 
 * Manages automatic belt preview connections for distributor holograms (splitters/mergers).
 * Handles manifold chaining (distributor-to-distributor) and building connections.
 * 
 * Responsibilities:
 * - Detect nearby compatible buildings for distributors
 * - Create and manage belt preview holograms
 * - Implement manifold chaining logic (priority 1: distributor chains, priority 2: buildings)
 * - Handle both parent and child distributor holograms
 */
UCLASS()
class SMARTFOUNDATIONS_API USFAutoConnectService : public UObject
{
	GENERATED_BODY()

public:
	// ========================================
	// Constants
	// ========================================

	/** Maximum pipe length in cm (56.01m - game engine limit) */
	static constexpr float MAX_PIPE_LENGTH = 5601.0f;

	/** Minimum angle alignment for auto-connect (cos(45°) = 0.707) */
	static constexpr float MIN_ANGLE_ALIGNMENT = 0.707f;

	/** Penalty multiplier for angle misalignment in scoring */
	static constexpr float ANGLE_PENALTY_MULTIPLIER = 10.0f;

	USFAutoConnectService();

	/** Initialize the service with subsystem reference */
	void Init(USFSubsystem* InSubsystem);

	/** Cleanup and destroy all belt previews */
	void Shutdown();
	
	/** Get the owning subsystem (for context-aware spacing and other features) */
	USFSubsystem* GetSubsystem() const { return Subsystem; }

	// ========================================
	// Main Entry Points
	// ========================================

	/**
	 * Update belt previews for a distributor hologram
	 * Handles both parent and child distributors
	 * @param DistributorHologram The distributor to update
	 */
	void OnDistributorHologramUpdated(AFGHologram* DistributorHologram);

	/**
	 * Clear all belt preview helpers
	 */
	void ClearBeltPreviewHelpers();

	/**
	 * Process auto-connect for child distributor holograms
	 * @param ParentHologram The parent hologram to get children from
	 */
	void ProcessChildDistributors(AFGHologram* ParentHologram);

	/**
	 * Finalize visibility and locking for all belt children of a distributor
	 * @param ParentHologram The parent distributor hologram
	 */
	void FinalizeBeltChildrenVisibility(AFGHologram* ParentHologram);

	/**
	 * Clean up belt previews for a specific distributor hologram
	 * Called when a child distributor is destroyed to prevent memory leaks
	 * @param DistributorHologram The distributor hologram to clean up previews for
	 */
	void CleanupDistributorPreviews(AFGHologram* DistributorHologram);

	/**
	 * Store belt previews for a distributor hologram
	 * Used by the orchestrator to manage belt previews
	 * @param DistributorHologram The distributor hologram
	 * @param Previews The belt preview helpers to store
	 */
	void StoreBeltPreviews(AFGHologram* DistributorHologram, const TArray<TSharedPtr<FBeltPreviewHelper>>& Previews);

	/**
	 * Get belt previews for a distributor hologram
	 * Used by the orchestrator to check/update belt previews
	 * @param DistributorHologram The distributor hologram
	 * @return Pointer to array of belt preview helpers, or nullptr if none exist
	 */
	TArray<TSharedPtr<FBeltPreviewHelper>>* GetBeltPreviews(AFGHologram* DistributorHologram);
	const TArray<TSharedPtr<FBeltPreviewHelper>>* GetBeltPreviews(const AFGHologram* DistributorHologram) const;

	/**
	 * Calculate the total cost of all belt previews for a distributor
	 * @param DistributorHologram The distributor hologram
	 * @return Array of item costs for all belt previews
	 */
	TArray<FItemAmount> GetBeltPreviewsCost(const AFGHologram* DistributorHologram) const;

	/**
	 * Clear cached belt costs for a distributor (used when children are removed)
	 * @param DistributorHologram The distributor hologram to clear costs for
	 */
	void ClearBeltCostsForDistributor(AFGHologram* DistributorHologram);

	// NOTE: UpdateBeltCostProxy removed - child holograms automatically aggregate costs via GetCost()

	// ========================================
	// Pipe Auto-Connect Methods
	// ========================================

	/**
	 * Update pipe previews for a pipeline junction hologram
	 * Handles both parent and child junction holograms
	 * @param JunctionHologram The pipeline junction to update
	 */
	void UpdatePipePreviews(AFGHologram* JunctionHologram);

	/**
	 * Clear all pipe previews for a pipeline junction hologram
	 * @param JunctionHologram The pipeline junction to clear previews for
	 */
	void ClearPipePreviews(AFGHologram* JunctionHologram);

	/**
	 * Get aggregated cost for all pipe previews associated with a junction hologram
	 * @param JunctionHologram The pipeline junction hologram
	 * @return Array of item costs for all pipe previews
	 */
	TArray<FItemAmount> GetPipePreviewsCost(const AFGHologram* JunctionHologram) const;

	/**
	 * Check if a hologram is a pipeline junction hologram
	 * @param Hologram The hologram to check
	 * @return True if this is a pipeline junction hologram
	 */
	static bool IsPipelineJunctionHologram(const AFGHologram* Hologram);

	/**
	 * Clear all pipe preview managers (called on hologram destruction)
	 */
	void ClearAllPipeManagers();

	/**
	 * Get the pipe manager for a specific junction hologram
	 * @param JunctionHologram The junction hologram
	 * @return Pointer to the manager, or nullptr if none exists
	 */
	FSFPipeAutoConnectManager* GetPipeManager(AFGHologram* JunctionHologram);
	const FSFPipeAutoConnectManager* GetPipeManager(const AFGHologram* JunctionHologram) const;

	// ========================================
	// Power Auto-Connect Methods
	// ========================================

	/**
	 * Update power line previews for power pole holograms in a scaled grid
	 * Handles grid topology analysis and X/Y axis connections
	 * @param ParentHologram The parent hologram containing power poles
	 */
	void ProcessPowerPoles(AFGHologram* ParentHologram);

	/**
	 * Clear all power line previews
	 */
	void ClearAllPowerPreviews();

	/**
	 * Check if a hologram is a power pole
	 * @param Hologram The hologram to check
	 * @return True if this is a power pole hologram (excludes wall-mounted poles)
	 */
	static bool IsPowerPoleHologram(const AFGHologram* Hologram);

	// ========================================
	// Support Structure Auto-Connect Methods (Issue #220)
	// ========================================

	/**
	 * Check if a hologram is a stackable conveyor pole
	 * Only stackable poles support auto-connect (not regular conveyor poles)
	 * @param Hologram The hologram to check
	 * @return True if this is a stackable conveyor pole hologram (Build_ConveyorPoleStackable_C)
	 */
	static bool IsStackableConveyorPoleHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is a stackable pipeline support
	 * Only stackable supports support auto-connect (not regular pipeline supports)
	 * @param Hologram The hologram to check
	 * @return True if this is a stackable pipeline support hologram (Build_PipeSupportStackable_C)
	 */
	static bool IsStackablePipelineSupportHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is any stackable support structure (conveyor pole or pipeline support)
	 * @param Hologram The hologram to check
	 * @return True if this is a stackable conveyor pole or stackable pipeline support
	 */
	static bool IsStackableSupportHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is a conveyor ceiling support (Issue #268)
	 * @param Hologram The hologram to check
	 * @return True if this is a conveyor ceiling attachment hologram (Build_ConveyorCeilingAttachment_C)
	 */
	static bool IsCeilingConveyorSupportHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is a wall-mounted conveyor pole (Issue #268)
	 * @param Hologram The hologram to check
	 * @return True if this is a wall conveyor pole hologram (Build_ConveyorPoleWall_C)
	 */
	static bool IsWallConveyorPoleHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is any belt-capable support structure (Issue #268)
	 * Includes: stackable conveyor poles, ceiling conveyor supports, wall conveyor poles
	 * @param Hologram The hologram to check
	 * @return True if this hologram supports belt auto-connect between grid neighbors
	 */
	static bool IsBeltSupportHologram(AFGHologram* Hologram);

	/**
	 * Process auto-connect for stackable conveyor poles
	 * Creates belt previews between consecutive poles in a scaled grid
	 * @param ParentHologram The parent hologram containing stackable conveyor poles
	 */
	void ProcessStackableConveyorPoles(AFGHologram* ParentHologram);

	/**
	 * Process auto-connect for stackable pipeline supports
	 * Creates pipe previews between consecutive supports in a scaled grid
	 * @param ParentHologram The parent hologram containing stackable pipeline supports
	 */
	void ProcessStackablePipelineSupports(AFGHologram* ParentHologram);

	// ========================================
	// Floor Hole Pipe Auto-Connect (Issue #187)
	// ========================================

	/**
	 * Check if a hologram is a passthrough (floor hole) with pipe connections.
	 * Only passthroughs that have pipe connectors are valid targets.
	 * @param Hologram The hologram to check
	 * @return True if this is a passthrough hologram with pipe connections
	 */
	static bool IsPassthroughPipeHologram(const AFGHologram* Hologram);

	/**
	 * Process auto-connect for floor hole passthroughs with pipe connections.
	 * Finds nearby buildings and creates pipe previews from floor hole to building.
	 * Single connector per floor hole (no manifold).
	 * @param ParentHologram The parent passthrough hologram
	 */
	void ProcessFloorHolePipes(AFGHologram* ParentHologram);

	/**
	 * Get the power manager for a specific power pole hologram
	 * @param PoleHologram The pole hologram
	 * @return Pointer to the manager, or nullptr if none exists
	 */
	FSFPowerAutoConnectManager* GetPowerManager(AFGHologram* PoleHologram);

	/**
	 * Analyze power pole grid topology and organize into structured grid
	 * Public for use by PowerAutoConnectManager
	 * @param AllPoles All power pole holograms in the grid
	 * @return Array of grid nodes with neighbor information
	 */
	TArray<FPowerPoleGridNode> AnalyzeGridTopology(const TArray<AFGHologram*>& AllPoles);

	// ========================================
	// Belt Preview Creation
	// ========================================

	/**
	 * Create or update a belt preview between an output and input connector
	 * Performs dual-angle validation to ensure belt geometry is valid
	 * 
	 * Validates angles at BOTH connectors:
	 * - Input angle: Belt arrival angle at the input connector
	 * - Output angle: Belt departure angle at the output connector
	 * 
	 * If either angle exceeds MaxAngleDegrees, the connection is rejected.
	 * This prevents belt geometry issues such as:
	 * - Belt twisting (invalid mesh deformation)
	 * - "Too steep" errors in vanilla Satisfactory
	 * - Visual clipping through buildings or terrain
	 * 
	 * The 30° default limit provides a good balance between connection flexibility
	 * and belt geometry validity, ensuring all generated belts are valid and placeable.
	 * 
	 * @param OutputConnector - Where the belt starts (building output, splitter output, etc.)
	 * @param InputConnector - Where the belt ends (building input, merger input, etc.)
	 * @param BeltHelper - Existing helper to update, or nullptr to create new one
	 * @param MaxAngleDegrees - Maximum allowed angle at BOTH connectors (default 30°)
	 * @param bSkipAngleValidation - Skip angle checks (default false)
	 * @param ParentDistributor - Parent distributor hologram for child registration (nullptr if none)
	 * @return True if belt preview was created/updated successfully, false if angle check failed
	 */
	bool CreateOrUpdateBeltPreview(
		UFGFactoryConnectionComponent* OutputConnector,
		UFGFactoryConnectionComponent* InputConnector,
		TSharedPtr<FBeltPreviewHelper>& BeltHelper,
		float MaxAngleDegrees = 30.0f,
		bool bSkipAngleValidation = false,
		AFGHologram* ParentDistributor = nullptr);

	/**
	 * Simple universal connection creator - connects any two connectors
	 * Works with holograms or buildables, just needs the connectors
	 * 
	 * @param OutputConnector - Source connector (output from building/distributor)
	 * @param InputConnector - Target connector (input to building/distributor)
	 * @param StorageHologram - Which hologram to store the preview on (usually the source distributor)
	 * @param bSkipAngleValidation - Skip angle checks (default false)
	 * @return True if connection was created and stored successfully
	 */
	bool ConnectAnyConnectors(
		UFGFactoryConnectionComponent* OutputConnector,
		UFGFactoryConnectionComponent* InputConnector,
		AFGHologram* StorageHologram,
		bool bSkipAngleValidation = false);

	bool BuildBeltFromPreview(const TSharedPtr<FBeltPreviewHelper>& Preview);
	bool BuildBeltsForDistributor(AFGHologram* DistributorHologram, AFGBuildable* BuiltDistributor);

	// ========================================
	// Connector Pair Storage for Build Handoff
	// ========================================

	/**
	 * Store connector pair for a distributor during preview updates
	 * @param DistributorHologram The distributor hologram
	 * @param HologramConnector The connector on the distributor hologram
	 * @param BuildingConnector The connector on the target building
	 */
	void StoreConnectorPair(AFGHologram* DistributorHologram, UFGFactoryConnectionComponent* HologramConnector, UFGFactoryConnectionComponent* BuildingConnector);

	/**
	 * Clear all stored connector pairs for a distributor
	 * @param DistributorHologram The distributor hologram
	 */
	void ClearConnectorPairs(AFGHologram* DistributorHologram);

	/**
	 * Get all stored connector pairs for a distributor
	 * @param DistributorHologram The distributor hologram
	 * @return Pointer to array of connector pairs, or nullptr if none exist
	 */
	TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>* GetConnectorPairs(AFGHologram* DistributorHologram);

	// ========================================
	// Distributor Detection
	// ========================================

	/**
	 * Check if a hologram is a distributor (splitter or merger)
	 * @param Hologram The hologram to check
	 * @return True if it's a distributor hologram
	 */
	static bool IsDistributorHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is a splitter
	 * @param Hologram The hologram to check
	 * @return True if it's a splitter hologram
	 */
	static bool IsSplitterHologram(AFGHologram* Hologram);

	/**
	 * Check if a hologram is a merger
	 * @param Hologram The hologram to check
	 * @return True if it's a merger hologram
	 */
	static bool IsMergerHologram(AFGHologram* Hologram);

	/**
	 * Find the middle connector on a distributor (for manifold chaining)
	 * Returns middle output for splitters, middle input for mergers
	 * Used by the orchestrator for manifold connections
	 * 
	 * Uses forward-vector alignment to identify the center connector:
	 * - Splitters: Middle output points WITH the actor's forward vector
	 * - Mergers: Middle input points AGAINST the actor's forward vector (reversed!)
	 * 
	 * This reversal is critical because merger inputs face backward while splitter
	 * outputs face forward. Without reversal, side inputs would incorrectly be
	 * identified as "middle" due to their more forward-facing orientation.
	 * 
	 * @param Distributor The distributor hologram (splitter or merger)
	 * @return The middle connector, or nullptr if not found
	 */
	UFGFactoryConnectionComponent* FindMiddleConnector(AFGHologram* Distributor);

	/**
	 * Find the connector on a source distributor that faces toward a target distributor
	 * Used for manifold connections where we want connectors facing along the chain
	 * 
	 * @param SourceDistributor The source distributor hologram
	 * @param TargetDistributor The target distributor hologram
	 * @param Direction The direction of connector to find (output or input)
	 * @return The connector whose normal points most toward the target, or nullptr if not found
	 */
	UFGFactoryConnectionComponent* FindConnectorFacingTarget(
		AFGHologram* SourceDistributor,
		AFGHologram* TargetDistributor,
		EFactoryConnectionDirection Direction);

	/**
	 * Process auto-connect logic for a single distributor hologram
	 * Creates belt previews for manifold chains and building connections
	 * Used by the orchestrator to evaluate connections for all distributors in a grid
	 * @param Distributor The distributor hologram to process (splitter or merger)
	 * @param ReservedInputs Optional map of building inputs already reserved by other distributors
	 * @return Array of belt preview helpers created for this distributor
	 */
	TArray<TSharedPtr<FBeltPreviewHelper>> ProcessSingleDistributor(
		AFGHologram* Distributor,
		TMap<UFGFactoryConnectionComponent*, AFGHologram*>* ReservedInputs = nullptr);

/** Reference to the owning subsystem */

	/** Stored connector pairs for build handoff, keyed by distributor hologram */
	TMap<AFGHologram*, TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>> StoredConnectorPairs;

	/** Search radius for finding compatible buildings (in cm) */
	static constexpr float BUILDING_SEARCH_RADIUS = 2500.0f; // 25 meters

	// ========================================
	// Connector Management (Public for Orchestrator access)
	// ========================================

	/**
	 * Get input and output connectors for a building
	 * @param Building The building to query
	 * @param OutInputs Array to populate with input connectors
	 * @param OutOutputs Array to populate with output connectors
	 */
	void GetBuildingConnectors(AFGBuildable* Building, 
	                           TArray<UFGFactoryConnectionComponent*>& OutInputs,
	                           TArray<UFGFactoryConnectionComponent*>& OutOutputs);

	/**
	 * Get all side connectors on a distributor (excludes middle connector)
	 * Returns side outputs for splitters, side inputs for mergers, sorted left-to-right
	 * @param Distributor The distributor hologram (splitter or merger)
	 * @param OutSideConnectors Array to populate with side connectors
	 * @param DirectionOverride Optional direction override (default: auto-detect based on type)
	 */
	void GetAllSideConnectors(AFGHologram* Distributor, TArray<UFGFactoryConnectionComponent*>& OutSideConnectors, EFactoryConnectionDirection DirectionOverride = EFactoryConnectionDirection::FCD_MAX);

private:
	// ========================================
	// Manifold Chaining
	// ========================================

	/**
	 * Find all distributor holograms in the scaled grid for manifold chaining
	 * @param ParentDistributor The parent distributor hologram
	 * @param OutDistributorChains Array to populate with chain targets
	 */
	void FindDistributorChains(AFGHologram* ParentDistributor, TArray<AFGHologram*>& OutDistributorChains);

	// ========================================
	// Power Grid Analysis
	// ========================================

	/**
	 * Check if two poles are axis neighbors within 100m
	 * @param PoleA First pole
	 * @param PoleB Second pole
	 * @param GridXAxis The grid's X-axis direction (parent's Forward vector - columns)
	 * @param GridYAxis The grid's Y-axis direction (parent's Right vector - rows)
	 * @param bXAxis True to check X-axis neighbors, false for Y-axis
	 * @return True if poles are on same grid axis within connection range
	 */
	bool AreGridAxisNeighbors(AFGHologram* PoleA, AFGHologram* PoleB, 
		const FVector& GridXAxis, const FVector& GridYAxis, bool bXAxis);

	/**
	 * Calculate cable cost for a power line connection
	 * @param Distance Distance in centimeters
	 * @return Number of cables required (1 per 25m, rounded up)
	 */
	int32 CalculateCableCost(float Distance);

	// ========================================
	// Building Search
	// ========================================

	/**
	 * Find all compatible buildings near a distributor for auto-connect
	 * @param DistributorHologram The distributor hologram
	 * @param OutBuildings Array to populate with compatible buildings
	 */
	void FindCompatibleBuildingsForDistributor(AFGHologram* DistributorHologram, TArray<AFGBuildable*>& OutBuildings);

	/** Owning subsystem */
	USFSubsystem* Subsystem = nullptr;

	/** Cached preview helpers per distributor */
	TMap<AFGHologram*, TArray<TSharedPtr<FBeltPreviewHelper>>> DistributorBeltPreviews;


	/** Pipe auto-connect managers per junction */
	TMap<AFGHologram*, TSharedPtr<FSFPipeAutoConnectManager>> PipeAutoConnectManagers;

	/** Power auto-connect managers per parent pole hologram */
	TMap<AFGHologram*, TSharedPtr<FSFPowerAutoConnectManager>> PowerAutoConnectManagers;

	// NOTE: EnsureCostProxy/DestroyCostProxy removed - child holograms automatically aggregate costs via GetCost()

	UFGFactoryConnectionComponent* FindMatchingBuildableConnector(AFGBuildable* BuiltDistributor, UFGFactoryConnectionComponent* HologramConnector, const TArray<UFGFactoryConnectionComponent*>& BuiltConnectors) const;

	/**
	 * Find a side connector on a distributor (for building connections)
	 * Returns side output for splitters, side input for mergers
	 * @param Distributor The distributor hologram (splitter or merger)
	 * @param Index Which side connector to get (0 = first side, 1 = second side, etc.)
	 * @return The side connector, or nullptr if not found
	 */
	UFGFactoryConnectionComponent* FindSideConnector(AFGHologram* Distributor, int32 Index = 0);

private:
	// ========================================
	// Stackable Pipe Child Tracking (Pole-Pair Based)
	// ========================================
	// Track spawned pipe children by their pole-pair key for:
	// 1. Update-in-place instead of destroy/recreate (fixes visibility during transforms)
	// 2. Deduplication - only one pipe per pole pair (fixes duplicate builds)
	//
	// Key format: Sorted pair of pole hologram unique IDs as uint64
	//   - LowID in high 32 bits, HighID in low 32 bits
	//   - Sorted ensures (A,B) and (B,A) map to same key
	
	/** Generate a unique key for a pole pair (order-independent) */
	static uint64 MakePolePairKey(const AFGHologram* PoleA, const AFGHologram* PoleB);
	
	/** Per-parent map of pole-pair keys to their pipe holograms */
	struct FStackablePipeState
	{
		TMap<uint64, TWeakObjectPtr<AFGHologram>> PipesByPolePair;
	};
	TMap<TWeakObjectPtr<AFGHologram>, FStackablePipeState> StackablePipeStates;
	
	/** Update or create pipe hologram for a pole pair, returns the pipe hologram */
	AFGHologram* UpdateOrCreatePipeForPolePair(
		AFGHologram* ParentHologram,
		AFGHologram* SourcePole,
		AFGHologram* TargetPole,
		UFGPipeConnectionComponent* SourceConnector,
		UFGPipeConnectionComponent* TargetConnector,
		int32 PipeTier,
		bool bWithIndicator,
		int32 PipeIndex);
	
	/** Remove pipes for pole pairs that are no longer needed */
	void RemoveOrphanedPipes(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs);
	
	/** Clean up all tracked stackable pipe children for a parent hologram */
	void CleanupAllStackablePipes(AFGHologram* ParentHologram);
	
	// ========================================
	// STACKABLE CONVEYOR POLE BELT TRACKING
	// ========================================
	// Similar to pipes, we track belt holograms by pole pair for:
	// 1. Update-in-place - reuse existing belt if pole pair unchanged
	// 2. Deduplication - only one belt per pole pair
	// 3. Direction support - belts can flow forward or backward
	
	/** Per-parent map of pole-pair keys to their belt holograms */
	struct FStackableBeltState
	{
		TMap<uint64, TWeakObjectPtr<AFGHologram>> BeltsByPolePair;
	};
	TMap<TWeakObjectPtr<AFGHologram>, FStackableBeltState> StackableBeltStates;
	
	/** Update or create belt hologram for a pole pair, returns the belt hologram */
	AFGHologram* UpdateOrCreateBeltForPolePair(
		AFGHologram* ParentHologram,
		AFGHologram* SourcePole,
		AFGHologram* TargetPole,
		UFGFactoryConnectionComponent* SourceConnector,
		UFGFactoryConnectionComponent* TargetConnector,
		int32 BeltTier,
		int32 BeltIndex);
	
	/** Remove belts for pole pairs that are no longer needed */
	void RemoveOrphanedBelts(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs);
	
	/** Clean up all tracked stackable belt children for a parent hologram */
	void CleanupAllStackableBelts(AFGHologram* ParentHologram);
};
