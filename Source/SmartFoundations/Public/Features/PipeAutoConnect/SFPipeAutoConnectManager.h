#pragma once

#include "CoreMinimal.h"
#include "Features/PipeAutoConnect/PipePreviewHelper.h"
#include "FGPipeConnectionComponent.h"

class USFSubsystem;
class USFAutoConnectService;
class AFGHologram;
class ASFPipelineHologram;

/**
 * FSFPipeAutoConnectManager
 *
 * Feature-level coordinator for Pipe Auto-Connect.
 * - Phase 1: junction → building pipe previews
 * - Phase 2: junction ↔ junction manifolds
 *
 * This is a thin, non-UObject helper owned by USFSubsystem / auto-connect layer.
 * It keeps most pipe-specific logic out of the subsystem.
 */
class SMARTFOUNDATIONS_API FSFPipeAutoConnectManager
{
public:
	FSFPipeAutoConnectManager();

	/** Initialize with owning subsystem and auto-connect service context */
	void Initialize(USFSubsystem* InSubsystem, USFAutoConnectService* InAutoConnectService);

	/**
	 * Coordinated processing: collect ALL junctions (parent + children) and process
	 * them sequentially with shared connector reservation (mirrors belt orchestrator).
	 * This prevents multiple junctions from claiming the same building connector.
	 */
	void ProcessAllJunctions(AFGHologram* ParentJunctionHologram);

	/**
	 * Phase 2 entry point: evaluate junction ↔ junction manifolds based on
	 * previously computed junction → building pairings.
	 */
	void EvaluatePipeConnections(AFGHologram* ParentJunctionHologram);

	/** Clear all pipe preview helpers */
	void ClearPipePreviews();
	
	/** Clean up pipe previews for junctions that no longer exist (removed children) */
	void CleanupOrphanedPreviews(const TArray<AFGHologram*>& CurrentJunctions);

	// ========================================
	// Floor Hole Pipe Auto-Connect (Issue #187)
	// ========================================

	/**
	 * Process pipe auto-connect for floor hole passthroughs.
	 * Finds nearby buildings with unconnected pipe connectors and creates
	 * pipe child holograms. Single connector per floor hole (no manifold).
	 * Supports scaling: each child floor hole also gets its own pipe.
	 * @param ParentHologram The parent passthrough hologram
	 */
	void ProcessFloorHolePipes(AFGHologram* ParentHologram);

	/** Clear all floor hole pipe previews */
	void ClearFloorHolePipePreviews();

	/** Get floor hole pipe child holograms */
	const TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>>& GetFloorHolePipeChildren() const { return FloorHolePipeChildren; }

	/**
	 * Create a pipe preview between two pipe connectors (Issue #220: Support structure auto-connect)
	 * Used for stackable pipeline support chaining
	 * @param SourceConnector - Source pipe connector
	 * @param TargetConnector - Target pipe connector
	 * @param StorageHologram - Hologram to associate the preview with (for cleanup)
	 * @param PipeTier - Pipe tier to use (0 = Auto, 1 = Mk1, 2 = Mk2)
	 * @param bWithIndicator - Pipe style: true = Normal (with indicators), false = Clean
	 * @return True if preview was created successfully
	 */
	bool CreatePipePreviewBetweenConnectors(
		UFGPipeConnectionComponent* SourceConnector,
		UFGPipeConnectionComponent* TargetConnector,
		AFGHologram* StorageHologram,
		int32 PipeTier = 0,
		bool bWithIndicator = true);

	/** Get Phase 1 child holograms (junction -> building, side A) */
	const TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>>& GetBuildingPipeChildren() const { return BuildingPipeChildren; }

	/** Get Phase 1 child holograms (junction -> building, side B - opposite side) */
	const TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>>& GetBuildingPipeChildrenB() const { return BuildingPipeChildrenB; }

	/** Get Phase 2 child holograms (junction <-> junction) */
	const TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>>& GetManifoldPipeChildren() const { return ManifoldPipeChildren; }
	
	/** DEPRECATED: Get Phase 1 previews (junction -> building) - kept for transition */
	const TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>>& GetBuildingPipePreviews() const { return JunctionPipePreviews; }

	/** DEPRECATED: Get Phase 2 previews (junction <-> junction) - kept for transition */
	const TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>>& GetManifoldPipePreviews() const { return ManifoldPipePreviews; }

private:
	/**
	 * Spawn a pipe child hologram between two connectors.
	 * Uses real AddChild() for vanilla cost aggregation.
	 * @param ParentJunction - The junction hologram to add the pipe as a child of
	 * @param JunctionConnector - Connector on the junction side
	 * @param TargetConnector - Connector on the target (building or other junction)
	 * @param bIsManifold - True if this is a junction-to-junction manifold pipe
	 * @return The spawned pipe hologram, or nullptr on failure
	 */
	ASFPipelineHologram* SpawnPipeChild(
		AFGHologram* ParentJunction,
		UFGPipeConnectionComponent* JunctionConnector,
		UFGPipeConnectionComponent* TargetConnector,
		bool bIsManifold);

	/**
	 * Spawn a pipe child hologram from a position to a building connector.
	 * Used for floor hole passthroughs which don't have pipe connectors on the hologram.
	 * @param ParentHologram - The passthrough hologram to add the pipe as a child of
	 * @param StartPos - World position of the source (floor hole center)
	 * @param StartNormal - Normal direction at the source (typically vertical)
	 * @param TargetConnector - Connector on the target building
	 * @return The spawned pipe hologram, or nullptr on failure
	 */
	ASFPipelineHologram* SpawnPipeChildAtPosition(
		AFGHologram* ParentHologram,
		const FVector& StartPos,
		const FVector& StartNormal,
		UFGPipeConnectionComponent* TargetConnector);

	/** Remove a pipe child hologram from its parent */
	void RemovePipeChild(AFGHologram* ParentJunction, ASFPipelineHologram* PipeChild);

	/**
	 * Internal: Process a single junction hologram with optional connector reservation.
	 * Called by ProcessAllJunctions with shared reservation map.
	 * @param ParentJunctionHologram - The junction to process
	 * @param SharedReservedConnectors - Shared map to prevent connector conflicts
	 * @param AllowedConnectorIndices - Optional list of junction connector indices this hologram can use (for child restrictions)
	 * @param AllowedConnectionType - Optional constraint: only connect to this type of building connector (Input/Output)
	 */
	void ProcessPipeJunctions(
		AFGHologram* ParentJunctionHologram, 
		TMap<UFGPipeConnectionComponent*, AFGHologram*>* SharedReservedConnectors = nullptr,
		const TArray<int32>* AllowedConnectorIndices = nullptr,
		const EPipeConnectionType* AllowedConnectionType = nullptr);
	
	/** Helper: Get connector index in junction's connector array */
	int32 GetConnectorIndex(AFGHologram* JunctionHologram, UFGPipeConnectionComponent* Connector);
	
	/** Helper: Get opposite connector index (for 4-way junctions: 0↔2, 1↔3) */
	static int32 GetOppositeConnectorIndex(int32 Index, int32 TotalConnectors);
	
	/** Issue #206: Find the connector physically opposite the source by comparing world-space normals.
	 *  GetComponents() ordering is arbitrary — this finds the true physical opposite. */
	static int32 GetOppositeConnectorByNormal(int32 SourceIndex, const TArray<UFGPipeConnectionComponent*>& Connectors);
	
	/** Helper: Find an available connector on a junction for manifold chaining (not used for building connection) */
	UFGPipeConnectionComponent* FindAvailableManifoldConnector(AFGHologram* JunctionHologram);
	
	/** 
	 * Helper: Find best facing connector pair for manifold connection between two junctions.
	 * Ensures connectors face toward each other and aren't used for building connections.
	 */
	void FindBestManifoldConnectorPair(
		AFGHologram* SourceJunction,
		AFGHologram* TargetJunction,
		UFGPipeConnectionComponent*& OutSourceConnector,
		UFGPipeConnectionComponent*& OutTargetConnector);

	USFSubsystem* Subsystem = nullptr;
	USFAutoConnectService* AutoConnectService = nullptr;

	// Phase 1: Map of junction holograms to their building connection pipe CHILD holograms
	// Side A: first/closest building connection per junction
	TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>> BuildingPipeChildren;
	// Side B (Issue #206): opposite-side building connection per junction
	TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>> BuildingPipeChildrenB;
	
	// Issue #187: Map of floor hole holograms to their building pipe CHILD holograms
	TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>> FloorHolePipeChildren;
	
	// Phase 2: Map of junction-to-junction manifold pipe CHILD holograms (keyed by source junction)
	TMap<AFGHologram*, TWeakObjectPtr<ASFPipelineHologram>> ManifoldPipeChildren;
	
	// Track which target junction each manifold pipe connects to (for cleanup when target is removed)
	TMap<AFGHologram*, TWeakObjectPtr<AFGHologram>> ManifoldTargetJunctions;
	
	// DEPRECATED: Old preview helper maps - kept for transition
	TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>> JunctionPipePreviews;
	TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>> ManifoldPipePreviews;
	
	// Track junction transforms to detect movement
	TMap<AFGHologram*, FTransform> LastJunctionTransforms;
	
	// Track which connector index the parent junction uses (to restrict children)
	TMap<AFGHologram*, int32> ParentConnectorIndices;
	// Issue #206: Track second connector index for opposite-side connections
	TMap<AFGHologram*, int32> ParentConnectorIndicesB;
	
	// CRITICAL: Persistent connector reservation map to prevent flickering
	// Must persist between frames so multiple junctions don't fight over the same connectors
	TMap<UFGPipeConnectionComponent*, AFGHologram*> ReservedConnectors;
	
	// Context-aware spacing tracking (mirrors belt orchestrator)
	bool bContextSpacingApplied = false;
	TWeakObjectPtr<UClass> LastTargetBuildingClass;
	
	// Counter for unique child names
	static int32 PipeChildCounter;
};
