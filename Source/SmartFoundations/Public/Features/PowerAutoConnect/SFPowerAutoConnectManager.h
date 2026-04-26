#pragma once

#include "CoreMinimal.h"
#include "PowerLinePreviewHelper.h"
#include "FGPowerConnectionComponent.h"

class USFSubsystem;
class USFAutoConnectService;
class AFGHologram;
class AFGBuildable;

/**
 * FSFPowerAutoConnectManager
 *
 * Feature-level coordinator for Power Pole Auto-Connect.
 * - Phase 1: Grid-only connections (X and Y axis neighbors)
 * - Phase 2: Building connections within user-defined range (future)
 *
 * This is a thin, non-UObject helper owned by USFSubsystem / auto-connect layer.
 * It keeps power-specific logic out of the subsystem.
 * 
 * Grid Connection Rules:
 * - Connect poles along X-axis only (same Y and Z, different X)
 * - Connect poles along Y-axis only (same X and Z, different Y)
 * - No diagonal connections (redundant paths avoided)
 * - Maximum distance: 100 meters (10,000cm)
 * - Cable cost: 1 per 25 meters, rounded up
 */
class SMARTFOUNDATIONS_API FSFPowerAutoConnectManager
{
public:
	FSFPowerAutoConnectManager();
	~FSFPowerAutoConnectManager();

	/** Initialize with owning subsystem and auto-connect service context */
	void Initialize(USFSubsystem* InSubsystem, USFAutoConnectService* InAutoConnectService);

	/**
	 * Process all power poles in a scaled grid
	 * @param ParentPoleHologram The parent hologram containing power poles
	 */
	void ProcessAllPowerPoles(AFGHologram* ParentPoleHologram);

	/** Clear all power line preview helpers */
	void ClearPowerLinePreviews();
	
	/** Clean up power line previews for poles that no longer exist (removed children) */
	void CleanupOrphanedPreviews(const TArray<AFGHologram*>& CurrentPoles);

	/** Get all power line previews (grid connections) */
	const TMap<AFGHologram*, TArray<TSharedPtr<FPowerLinePreviewHelper>>>& GetPowerLinePreviews() const { return PowerLinePreviews; }

	/** Phase 2: Connect poles to nearby buildings */
	void ProcessBuildingConnections(const TArray<AFGHologram*>& AllPoles);

	/**
	 * Called when a power pole is built to create actual power connections
	 * @param BuiltPole The power pole that was just built
	 * @param bCostsPreDeducted If true, wire costs were already deducted at grid level (skip per-wire deduction)
	 */
	void OnPowerPoleBuilt(class AFGBuildablePowerPole* BuiltPole, bool bCostsPreDeducted = false);

	/**
	 * Clear all connection reservations (called when grid changes)
	 */
	void ClearAllReservations();
	
	/**
	 * Reset spacing state (called when grid changes)
	 */
	void ResetSpacingState();

	/**
	 * Get the maximum number of connections for a power pole based on tier and user settings
	 * @param PowerPole The power pole to check
	 * @return Maximum connections allowed
	 */
	int32 GetMaxConnectionsForPole(class AFGBuildablePowerPole* PowerPole) const;

	/**
	 * Get the number of reserved slots for a power pole based on user configuration
	 * @param PowerPole The power pole to check
	 * @return Number of reserved slots (0 if no reservation)
	 */
	int32 GetReservedSlotsForPole(class AFGBuildablePowerPole* PowerPole) const;

	/**
	 * Get detailed connection information for a power pole
	 * @param PowerPole The power pole to check
	 * @param OutConnectedCount Number of currently connected circuit connections
	 * @param OutTotalCircuitConnections Total number of circuit connection components
	 * @param OutTotalPowerConnections Total number of power connection components
	 */
	void GetConnectionInfo(class AFGBuildablePowerPole* PowerPole, int32& OutConnectedCount, int32& OutTotalCircuitConnections, int32& OutTotalPowerConnections) const;

	/**
	 * Debug method: Log connection status for all power poles in the world
	 * Call this from console or debug commands to check connection status
	 */
	void DebugLogAllPowerPoleConnections() const;

private:
	/**
	 * Connect a pole to its X and Y axis neighbors
	 * @param Pole The power pole to connect
	 * @param XNeighbors X-axis neighbors within range
	 * @param YNeighbors Y-axis neighbors within range
	 * @param GridXAxis Grid X axis direction (parent's Forward vector)
	 * @param GridYAxis Grid Y axis direction (parent's Right vector)
	 */
	void ConnectPoleToNeighbors(
		AFGHologram* Pole,
		const TArray<AFGHologram*>& XNeighbors,
		const TArray<AFGHologram*>& YNeighbors,
		const FVector& GridXAxis,
		const FVector& GridYAxis);

	/**
	 * Create a power line preview between two poles
	 * @param SourcePole The source pole
	 * @param TargetPole The target pole
	 * @return True if preview was created successfully
	 */
	bool CreatePowerLinePreview(AFGHologram* SourcePole, AFGHologram* TargetPole);

	/**
	 * Create a power line preview between a pole and a building
	 * @param SourcePole The source pole
	 * @param TargetBuilding The target building
	 * @return True if preview was created successfully
	 */
	bool CreateBuildingPowerLinePreview(AFGHologram* SourcePole, class AFGBuildable* TargetBuilding);

	/**
	 * Get power connection component from a power pole hologram
	 * @param PoleHologram The power pole hologram
	 * @return The power connection component, or nullptr if not found
	 */
	UFGPowerConnectionComponent* GetPowerConnection(AFGHologram* PoleHologram);

	/** 
	 * Reserve a circuit connection for a specific pole
	 * @param Connection The circuit connection to reserve
	 * @param Pole The pole that will use this connection
	 * @return True if reservation was successful, false if already reserved
	 */
	bool ReserveConnection(class UFGCircuitConnectionComponent* Connection, AFGHologram* Pole);

	/**
	 * Release a circuit connection reservation
	 * @param Connection The circuit connection to release
	 */
	void ReleaseConnection(class UFGCircuitConnectionComponent* Connection);

	/**
	 * Check if a circuit connection is reserved
	 * @param Connection The circuit connection to check
	 * @return True if reserved, false if available
	 */
	bool IsConnectionReserved(class UFGCircuitConnectionComponent* Connection) const;

	/**
	 * Deduct cable cost from player inventory when creating a power line
	 * @param World The world context
	 * @param DistanceInCm The distance of the power line in centimeters
	 * @return True if cost was deducted successfully, false if not enough cables
	 */
	bool DeductCableCost(UWorld* World, float DistanceInCm);

	USFSubsystem* Subsystem = nullptr;
	USFAutoConnectService* AutoConnectService = nullptr;

	// Map of power pole holograms to their power line preview helpers
	// Key: Source pole hologram
	// Value: Array of power line previews (to neighbors and buildings)
	TMap<AFGHologram*, TArray<TSharedPtr<FPowerLinePreviewHelper>>> PowerLinePreviews;
	
	// Track pole transforms to detect movement
	TMap<AFGHologram*, FTransform> LastPoleTransforms;

	/** Reservation system to prevent multiple poles from connecting to the same circuit connection */
	TMap<class UFGCircuitConnectionComponent*, AFGHologram*> ReservedConnectors;
	
	// Context-aware spacing tracking (mirrors pipe auto-connect)
	bool bContextSpacingApplied = false;
	TWeakObjectPtr<UClass> LastTargetBuildingClass;
	
	// Note: PlannedBuildingConnections is stored on USFSubsystem so it's shared
	// between service managers (preview phase) and the build phase.
};
