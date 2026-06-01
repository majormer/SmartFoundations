// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendService - EXTEND Feature Service
 *
 * Enables cloning of factory infrastructure (belts, lifts, pipes, distributors, junctions)
 * when placing a matching factory hologram over an existing factory building.
 *
 * Architecture (as of Dec 2025):
 * - Topology capture via WalkTopology() builds FSFExtendTopology
 * - JSON-based spawning via FSFCloneTopology::SpawnChildHolograms()
 * - Child holograms added to parent via AddChild() for vanilla cost aggregation
 * - Post-build wiring via FSFWiringManifest after factory construction
 *
 * Key Files:
 * - SFExtendService.h/.cpp - Main service (topology, previews, diagnostics)
 * - SFExtendCloneTopology.h/.cpp - clone topology capture/transform/spawn
 * - SFWiringManifest.h/.cpp - Post-build connection wiring
 *
 * Legacy systems removed (Dec 2025 cleanup):
 * - Deferred build system (PendingBuilds arrays, BuildPending* functions)
 * - Manual spawning functions (SpawnExtend*Child, SpawnManifold*Child)
 * - Old JSON system (SFExtendTopologyJSON)
 * - Cost injection (vanilla handles via AddChild)
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Hologram/FGHologram.h"  // For EHologramMaterialState
#include "ItemAmount.h"  // For FItemAmount
#include "HUD/SFHUDTypes.h"  // For FSFCounterState
#include "Features/Extend/SFExtendTypes.h"  // For FSFExtendTopology, FSFConnectionChainNode, FSFPipeConnectionChainNode
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendScaledService.h"  // For FSFScaledExtendClone (ScaledExtendClones member)
#include "SFExtendService.generated.h"

class AFGBuildable;
class AFGBuildableConveyorBase;
class AFGBuildableConveyorBelt;
class AFGBuildableConveyorLift;
class AFGBuildableConveyorAttachment;
class AFGBuildablePipeline;
class AFGBuildablePipelineJunction;
class AFGBuildableFactory;
class AFGBuildablePole;
class AFGBuildablePowerPole;
class AFGHologram;
class AFGBuildGun;
class UFGBuildGunStateBuild;
class ASFFactoryHologram;
class ASFConveyorBeltHologram;
class ASFConveyorLiftHologram;
class ASFPipelineHologram;
class ASFPipelineJunctionChildHologram;
class USFExtendDiagnosticsService;
class USFExtendRestoreReplayService;
class USFExtendScaledService;
class USFSubsystem;
struct FSplinePointData;
struct FSFCloneTopology;
struct FSFWiringManifest;

// ESFExtendDirection enum moved to SFExtendDetectionService.h
// Topology structs (FSFConnectionChainNode, FSFPipeConnectionChainNode, FSFExtendTopology) moved to SFExtendTypes.h

// Diagnostic snapshot structs (FSFCapturedConnection/SplinePoint/Buildable, FSFBuildableSnapshot)
// and the before/after capture logic now live in SFExtendDiagnosticsService.h/.cpp (T1 split).

/**
 * Service for managing EXTEND feature - clones factory buildings with their
 * connected infrastructure (belts, distributors, pipes, junctions).
 *
 * EXTEND performs 1:1 topology cloning, duplicating the exact infrastructure
 * layout rather than discovering new connections like auto-connect.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendService : public UObject
{
    GENERATED_BODY()

    // Restore-replay logic lives in USFExtendRestoreReplayService but operates on this
    // service's shared replay/wiring state (StoredCloneTopology, Json* maps, etc.) in place.
    friend class USFExtendRestoreReplayService;
    // Scaled Extend planning/preview lives in USFExtendScaledService but operates on this
    // service's shared scaled/wiring state (ScaledExtendClones, StoredCloneTopology, etc.) in place.
    friend class USFExtendScaledService;
    // Post-build wiring (E-chain / built-child / manifold / JSON) lives in USFExtendWiringService
    // but operates on this service's shared registry maps + StoredCloneTopology in place (slice E2).
    friend class USFExtendWiringService;

public:
    USFExtendService();

    /** Initialize with owning subsystem reference */
    void Initialize(USFSubsystem* InSubsystem);

    // ==================== Mode Management ====================
    // EXTEND mode is AUTOMATIC - activates when pointing at a compatible building
    // No manual toggle needed (matches original Smart! behavior)

    /** Check if EXTEND mode is currently active (has valid target) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool IsExtendModeActive() const { return bHasValidTarget; }

    /** Get the current extend target building (nullptr if not active) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    AFGBuildable* GetCurrentTarget() const { return CurrentExtendTarget.Get(); }

    /** Force clear the extend state (called when hologram changes) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void ClearExtendState();

    /** Shutdown service */
    void Shutdown();

    /** Check if final cleanup is needed and perform it (call when build gun leaves build mode) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void CheckAndPerformFinalCleanup();

    // ==================== Scaled Extend (Issue #265) ====================

    /**
     * Notify the extend service that grid state has changed (counter, spacing, steps, rotation).
     * Called from subsystem when inputs modify grid counters during extend mode.
     * Triggers recalculation of clone positions and respawning of preview holograms.
     */
    void OnScaledExtendStateChanged();

    /** Get the current extend clone count (X counter - 1, since X=1 means source only) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    int32 GetExtendCloneCount() const;

    /** Get the current extend row count (Y counter) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    int32 GetExtendRowCount() const;

    /** Check if Scaled Extend is active (extend mode + clone count > 1 or row count > 1) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool IsScaledExtendActive() const;

    /**
     * Get the invalidation reason if the current scaled extend configuration is invalid.
     * Returns empty string if valid.
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    FString GetScaledExtendInvalidReason() const { return ScaledExtendInvalidReason; }

    /**
     * Check if the current scaled extend configuration is valid for placement.
     * Returns false if belt/pipe constraints are violated.
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool IsScaledExtendValid() const { return bScaledExtendValid; }

    /** Material state Extend child previews should mirror (recomputed each RefreshExtension). */
    EHologramMaterialState GetExtendChildMaterialState() const { return ExtendChildMaterialState; }

    /**
     * Depot-aware affordability for the whole extend. Compares the aggregated extend cost
     * (GetCost(true), which includes belt/pipe/lift child costs via the GetCost hook) against the
     * player inventory PLUS the Dimensional Depot (central storage). Vanilla's own CheckCanAfford
     * ignores the depot, so ASFFactoryHologram::CheckCanAfford uses this to keep the preview's
     * FGCDUnaffordable disqualifier correct, and RefreshExtension uses it to drive ExtendMaterialState.
     * Returns true when the cost cannot be evaluated (null args) so it never blocks spuriously.
     */
    bool CanAffordExtendCost(class AFGHologram* Hologram, class UFGInventoryComponent* Inventory) const;

    /**
     * Resolve the material state an Extend child preview hologram should display.
     *
     * Child holograms (belts/pipes/lifts/distributors/etc.) re-run CheckValidPlacement
     * every frame via the build gun's validation cascade, which fires AFTER our
     * RefreshExtension propagation and would otherwise reset them to HMS_OK (or read a
     * stale HMS_OK from the parent before the build gun applies the parent's
     * insufficient-materials red). Reading this authoritative, frame-order-independent
     * value instead keeps child previews in sync with the extend's real state.
     *
     * Returns the last computed extend child state while Extend mode is active, otherwise
     * HMS_OK so non-Extend uses of these hologram classes are unaffected. Safe to call
     * from any child hologram's CheckValidPlacement.
     */
    static EHologramMaterialState ResolveChildPreviewMaterialState(const UObject* WorldContext);

    // ==================== Direction Cycling (delegates to DetectionService) ====================

    /** Get current extend direction */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    ESFExtendDirection GetExtendDirection() const;

    /** Cycle to next/previous direction (called from mouse wheel) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void CycleExtendDirection(int32 Delta);

    /** Get offset vector for current direction based on building size */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    FVector GetDirectionOffset(const FVector& BuildingSize, const FRotator& BuildingRotation) const;

    /** Check if a specific direction is valid (no building blocking the target position) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool IsDirectionValid(ESFExtendDirection Direction) const;

    /** Get the list of valid directions for the current target */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    TArray<ESFExtendDirection> GetValidDirections() const;

    /** Get the detection service (for direct access if needed) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    USFExtendDetectionService* GetDetectionService() const { return DetectionService; }

    // ==================== Topology Walking (delegates to TopologyService) ====================

    /** Walk the connection topology from a factory building */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool WalkTopology(AFGBuildable* SourceBuilding);

    /** Get the current cached topology */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    const FSFExtendTopology& GetCurrentTopology() const;

    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    const FSFExtendTopology& GetLastExtendTopology() const;

    TSharedPtr<FSFCloneTopology> GetLastCloneTopology() const;
    bool ReplayRestoreCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology);
    void TickRestoredCloneTopology(float DeltaTime);
    void OnRestoredCloneTopologyStateChanged();
    bool IsRestoredCloneTopologyActive() const { return bRestoredCloneTopologyActive; }
    bool IsHologramCompatibleWithRestoredCloneTopology(AFGHologram* ParentHologram) const;
    void ClearRestoredCloneTopologySession(const TCHAR* Reason);

    /** Clear cached topology data */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void ClearTopology();

    /** Get the topology service (for direct access if needed) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    USFExtendTopologyService* GetTopologyService() const { return TopologyService; }

    /** Get the hologram service (for direct access if needed) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    USFExtendHologramService* GetHologramService() const { return HologramService; }

    /** Get the wiring service (for direct access if needed) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    USFExtendWiringService* GetWiringService() const { return WiringService; }

    // ==================== Extension Execution ====================

    /**
     * Attempt to extend from a hit building
     * Called from TryUpgrade hook when EXTEND mode is active
     * @return true if extension preview was created
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    bool TryExtendFromBuilding(AFGBuildable* HitBuilding, AFGHologram* SourceHologram);

    /** Refresh extension previews (called during hologram update) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void RefreshExtension(AFGHologram* SourceHologram, bool bForceRefresh = false);

    /** Clean up all extension child holograms */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void CleanupExtension(AFGHologram* SourceHologram);

    // ==================== Phase 2: Belt Preview ====================

    /** Create belt preview holograms for all connection chains */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void CreateBeltPreviews(AFGHologram* ParentHologram);

    /** Clear all belt preview holograms */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void ClearBeltPreviews();

    // ==================== Post-Build Wiring ====================

    /**
     * Connect all chain elements (belts/lifts) after they've been built.
     * Called from deferred timer after factory construction.
     * @param NewFactory The newly constructed factory building
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void ConnectAllChainElements(AFGBuildableFactory* NewFactory);

    /**
     * Wire connections between built child holograms using hologram→buildable mapping from registry.
     * Must be called AFTER all child holograms are built via AddChild mechanism.
     * @param NewFactory The newly constructed factory building
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend")
    void WireBuiltChildConnections(AFGBuildableFactory* NewFactory);

    bool HasPendingPostBuildWiring() const;

    /**
     * Generate wiring manifest from stored clone topology and execute all connections.
     * This is Phase 5/6 of the EXTEND workflow - called in deferred tick after build.
     * @param NewFactory The newly constructed factory building
     * @return Number of connections successfully wired
     */
    int32 GenerateAndExecuteWiring(AFGBuildableFactory* NewFactory);

    /**
     * Register a built conveyor (belt or lift) with its chain ID and index for later wiring.
     * Called from belt/lift hologram's Construct() method.
     * @param ChainId The chain this conveyor belongs to
     * @param ChainIndex The index within the chain (for ordering)
     * @param BuiltConveyor The built conveyor buildable (belt or lift)
     * @param bIsInputChain True if this is an input chain (items flow to factory)
     */
    void RegisterBuiltConveyor(int32 ChainId, int32 ChainIndex, AFGBuildableConveyorBase* BuiltConveyor, bool bIsInputChain);

    /**
     * Get a previously built conveyor by chain ID and index.
     * Used during Construct() to get the previous belt for chain linking.
     * @param ChainId The chain to look up
     * @param ChainIndex The index within the chain
     * @return The built conveyor, or nullptr if not found
     */
    AFGBuildableConveyorBase* GetBuiltConveyor(int32 ChainId, int32 ChainIndex) const;

    /**
     * Register a built distributor with its chain ID for later wiring.
     * Called from distributor hologram's Construct() method.
     * @param ChainId The chain this distributor belongs to
     * @param BuiltDistributor The built distributor buildable
     */
    void RegisterBuiltDistributor(int32 ChainId, AFGBuildable* BuiltDistributor);

    /**
     * Get a previously built distributor by chain ID.
     * Used during Construct() to get the distributor for chain linking.
     * @param ChainId The chain to look up
     * @return The built distributor, or nullptr if not found
     */
    AFGBuildable* GetBuiltDistributor(int32 ChainId) const;

    /**
     * Copy user-configured settings from every source distributor to its cloned counterpart.
     * Covers Smart/Programmable Splitter sort rules (Issues #298, #299) and Priority Merger
     * input priorities. Vanilla 3-way splitters/mergers are skipped automatically since neither
     * cast matches. Called at the start of WireBuiltChildConnections() so filters are in place
     * before connections start flowing items.
     * @return Number of distributors whose configuration was successfully copied
     */
    int32 CopyDistributorConfigurations();

    /**
     * Get the distributor connector name for a chain.
     * Used during Construct() to find the correct output on the cloned distributor.
     * @param ChainId The chain to look up
     * @return The connector name (e.g., "Output1", "Output2"), or NAME_None if not found
     */
    FName GetDistributorConnectorName(int32 ChainId) const;

    /**
     * Set the distributor connector name for a chain.
     * Called when capturing source topology.
     * @param ChainId The chain ID
     * @param ConnectorName The connector name (e.g., "Output1", "Output2")
     */
    void SetDistributorConnectorName(int32 ChainId, FName ConnectorName);

    /**
     * Register a built pipe junction with its chain ID for later wiring.
     * Called from junction hologram's Construct() method.
     * @param ChainId The pipe chain this junction belongs to
     * @param BuiltJunction The built junction buildable
     */
    void RegisterBuiltJunction(int32 ChainId, AFGBuildable* BuiltJunction);

    /**
     * Register a built pipe with its chain ID and index for later wiring.
     * Called from pipe hologram's Construct() method.
     * @param ChainId The pipe chain this pipe belongs to
     * @param ChainIndex The index of this pipe within the chain
     * @param BuiltPipe The built pipe buildable
     * @param bIsInputChain Whether this is an input chain (true) or output chain (false)
     */
    void RegisterBuiltPipe(int32 ChainId, int32 ChainIndex, AFGBuildablePipeline* BuiltPipe, bool bIsInputChain);

    // ==================== Manifold Connections ====================

    /**
     * Wire manifold connections between source and clone distributors/junctions.
     * Creates belts/pipes between the source building's distributors and the clone's distributors.
     * @param SourceFactory The original factory building we extended from
     * @param CloneFactory The newly constructed clone factory
     */
    void WireManifoldConnections(AFGBuildableFactory* SourceFactory, AFGBuildableFactory* CloneFactory);

    /**
     * Create a belt between two distributor connectors for manifold connection.
     * @param FromConnector The output connector on the source distributor
     * @param ToConnector The input connector on the clone distributor
     * @return true if belt was created successfully
     */
    bool CreateManifoldBelt(UFGFactoryConnectionComponent* FromConnector, UFGFactoryConnectionComponent* ToConnector);

    /**
     * Create a pipe between two junction connectors for manifold connection.
     * @param FromConnector The connector on the source junction
     * @param ToConnector The connector on the clone junction
     * @return true if pipe was created successfully
     */
    bool CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector);

    /**
     * Get the source factory building from the cached topology.
     * @return The source factory that was extended from, or nullptr if not set
     */
    AFGBuildableFactory* GetSourceFactory() const;

    /**
     * Wire a manifold pipe to its source and clone junctions after building.
     * Called from SFPipelineHologram::Construct() for manifold pipes.
     * @param BuiltPipe The just-built manifold pipe
     * @param SourceConnector The connector on the source junction
     * @param CloneChainId The chain ID to find the clone junction
     */
    void WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId);

    /**
     * Wire a manifold belt to its source and clone distributors after building.
     * Called from SFConveyorBeltHologram::Construct() for manifold belts.
     * @param BuiltBelt The just-built manifold belt
     * @param SourceConnector The connector on the source distributor
     * @param CloneChainId The chain ID to find the clone distributor
     */
    void WireManifoldBelt(AFGBuildableConveyorBelt* BuiltBelt, UFGFactoryConnectionComponent* SourceConnector, int32 CloneChainId);

    // ==================== Utility Functions ====================

public:

    /** Provide a valid floor hit result to a hologram to prevent "Surface is too uneven" errors */
    void ProvideFloorHitResult(AFGHologram* Hologram, const FVector& Location);

    // ==================== Build Gun Hologram Swapping ====================

    /**
     * Swap the build gun's active hologram to our custom ASFFactoryHologram.
     * This gives us control over SetHologramLocationAndRotation for EXTEND positioning.
     * @param VanillaHologram The current vanilla factory hologram
     * @return The swapped custom hologram, or nullptr if swap failed
     */
    ASFFactoryHologram* SwapToSmartFactoryHologram(AFGHologram* VanillaHologram);

    /**
     * Restore the original hologram when EXTEND mode ends.
     * Called from ClearExtendState.
     */
    void RestoreOriginalHologram();

protected:
    /** Check if a building is a valid factory for extension (delegates to DetectionService) */
    bool IsValidExtendTarget(AFGBuildable* Building) const;

    /** Get the player's build gun */
    AFGBuildGun* GetPlayerBuildGun() const;

    /** Get the build gun's build state */
    UFGBuildGunStateBuild* GetBuildGunBuildState(AFGBuildGun* BuildGun) const;

private:
    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Detection service for target validation and direction management */
    UPROPERTY()
    TObjectPtr<USFExtendDetectionService> DetectionService = nullptr;

    /** Topology service for walking and capturing connection chains */
    UPROPERTY()
    TObjectPtr<USFExtendTopologyService> TopologyService = nullptr;

    /** Hologram service for child hologram management and preview spawning */
    UPROPERTY()
    TObjectPtr<USFExtendHologramService> HologramService = nullptr;

    /** Wiring service for post-build connection wiring (interface only - implementation here for now) */
    UPROPERTY()
    TObjectPtr<USFExtendWiringService> WiringService = nullptr;

    /** Diagnostics service for EXTEND before/after world snapshots (pure diagnostics; no gameplay path) */
    UPROPERTY()
    TObjectPtr<USFExtendDiagnosticsService> DiagnosticsService = nullptr;

    /** Restore-replay service for Smart Restore Extend clone-topology replay (operates on this service's shared state) */
    UPROPERTY()
    TObjectPtr<USFExtendRestoreReplayService> RestoreReplayService = nullptr;

    /** Scaled Extend service (planning/preview/validate; operates on this service's shared state) */
    UPROPERTY()
    TObjectPtr<USFExtendScaledService> ScaledService = nullptr;

    /** Do we have a valid extend target this frame */
    UPROPERTY()
    bool bHasValidTarget = false;

    /** Building we're currently extending from */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildable> CurrentExtendTarget;

    /** Our custom hologram that replaced the vanilla one (for cleanup) */
    UPROPERTY()
    TWeakObjectPtr<ASFFactoryHologram> SwappedHologram;

    /** Whether we've swapped the vanilla hologram for our custom one */
    UPROPERTY()
    bool bHasSwappedHologram = false;

    /** Child hologram previews for belt connections (these BUILD but may be invisible) */
    UPROPERTY()
    TArray<TObjectPtr<AFGHologram>> BeltPreviewHolograms;

    /** Intended world positions for child holograms (engine resets them to origin, we force them back) */
    TMap<AFGHologram*, FVector> ChildIntendedPositions;

    /** Intended world rotations for child holograms */
    TMap<AFGHologram*, FRotator> ChildIntendedRotations;

    /** The hologram we've initialized for EXTEND (to detect when build gun creates a new one) */
    UPROPERTY()
    TWeakObjectPtr<AFGHologram> CurrentExtendHologram;

    /** Counters for unique child hologram naming (NEVER reset - engine uses name TMap that persists) */
    int32 ExtendDistributorCounter = 0;
    int32 ExtendMergerCounter = 0;
    int32 ExtendInputBeltCounter = 0;
    int32 ExtendOutputBeltCounter = 0;
    int32 ExtendJunctionCounter = 0;
    int32 ExtendInputPipeCounter = 0;
    int32 ExtendOutputPipeCounter = 0;

    /** Counter for unique lift hologram naming */
    int32 ExtendInputLiftCounter = 0;
    int32 ExtendOutputLiftCounter = 0;

    /** Temporary storage for built chain elements - used by ConnectAllChainElements */
    /** Key: ChainId, Value: Map of ChainIndex to built conveyor (belt or lift) */
    TMap<int32, TMap<int32, AFGBuildableConveyorBase*>> BuiltChainElements;

    /** Track which chains are input vs output for connection direction */
    TMap<int32, bool> ChainIsInputMap;

    // ==================== Hologram Connection Wiring (prevents child pole spawning) ====================

    /** Per-chain tracking of pipe holograms in spawn order (ChainId → array of pipe holograms) */
    TMap<int32, TArray<ASFPipelineHologram*>> PipeChainHologramMap;

    /** Junction hologram for each pipe chain (ChainId → junction hologram) */
    TMap<int32, class ASFPipelineJunctionChildHologram*> PipeChainJunctionMap;

    /** Unified chain map for all conveyors (belts + lifts) indexed by position (ChainId → ChainIndex → Hologram) */
    TMap<int32, TMap<int32, AFGHologram*>> UnifiedConveyorChainMap;

    /** Distributor hologram for each belt chain (ChainId → distributor hologram) */
    TMap<int32, AFGHologram*> BeltChainDistributorMap;

    /** Built conveyor buildables per chain, indexed by position (ChainId → (Index → Conveyor)) */
    TMap<int32, TMap<int32, AFGBuildableConveyorBase*>> BuiltConveyorsByChain;

    /** Built distributor buildables per chain (populated during Construct(), used by WireBuiltChildConnections) */
    TMap<int32, AFGBuildable*> BuiltDistributorsByChain;

    /** Built junction buildables per pipe chain (populated during Construct(), used by WireBuiltChildConnections) */
    TMap<int32, AFGBuildable*> BuiltJunctionsByChain;

    /** Built pipe buildables per chain, indexed by position (ChainId → (Index → Pipe)) */
    TMap<int32, TMap<int32, AFGBuildablePipeline*>> BuiltPipesByChain;

    /** Track chain direction for built chains (true = input chain, false = output chain) */
    TMap<int32, bool> BuiltChainIsInputMap;

    /** Track pipe chain direction separately (true = input chain, false = output chain) */
    TMap<int32, bool> BuiltPipeChainIsInputMap;

    /** Source (original) distributors per chain - for manifold connections (ChainId → source distributor) */
    TMap<int32, AFGBuildable*> SourceDistributorsByChain;

    /** Source (original) junctions per pipe chain - for manifold connections (ChainId → source junction) */
    TMap<int32, AFGBuildable*> SourceJunctionsByChain;

    /** Distributor connector name per chain (ChainId → ConnectorName like "Output1", "Output2") */
    /** Used during Construct() to find the correct output on the cloned distributor */
    TMap<int32, FName> DistributorConnectorNameByChain;

    // WirePipeChainConnections / WireBeltChainConnections / FindPipe/FactoryConnectionByIndex
    // moved to USFExtendWiringService (slice E2a).

    /** Clear all connection wiring tracking maps (forwards to WiringService) */
    void ClearConnectionWiringMaps();

    /** Building we just built from - prevents immediate re-activation on same building after build */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildable> LastBuiltFromBuilding;

    UPROPERTY()
    FSFExtendTopology LastExtendTopology;

    TSharedPtr<FSFCloneTopology> LastCloneTopology;

    // ==================== Scaled Extend State (Issue #265) ====================

    /** Whether the current scaled extend configuration is valid for placement */
    bool bScaledExtendValid = true;

    /** Authoritative material state for Extend child previews; set each RefreshExtension. */
    EHologramMaterialState ExtendChildMaterialState = EHologramMaterialState::HMS_OK;

    /** Reason why current configuration is invalid (empty if valid) */
    FString ScaledExtendInvalidReason;

    /**
     * All clone sets for Scaled Extend. FSFScaledExtendClone was relocated to
     * SFExtendScaledService.h (slice E1) so the scaled service + the post-build wiring path
     * share it; the scaled planning/preview/validate methods moved to USFExtendScaledService.
     */
    TArray<FSFScaledExtendClone> ScaledExtendClones;

    /** Clear all scaled extend clone data and holograms (forwards to ScaledService) */
    void ClearScaledExtendClones();

    /**
     * Issue #288: Validate cloned power pole capacity for pump wiring.
     * For each clone pole, sum the connections we're planning to make:
     *   1 (clone factory) + 1 (inter-pole wire back to source) + N (cloned pumps)
     * If the projected total exceeds the pole's tier cap, sets
     * ScaledExtendInvalidReason and returns false. Runs for both single-clone
     * and scaled Extend.
     */
    bool ValidatePowerCapacity();

    /** Flag indicating EXTEND was active and needs final cleanup when build gun leaves build mode */
    bool bNeedsFinalCleanup = false;

    /** Whether the user has committed to Extend by performing a scale action.
     *  Before committing, looking away deactivates Extend normally (allows middle-click sampling).
     *  After committing, sticky extend keeps Extend alive when looking away. */
    bool bExtendCommitted = false;

    /** Counter snapshot taken when Extend activates.
     *  Restored when Extend deactivates so normal scaling isn't polluted with Extend's counters. */
    FSFCounterState PreExtendCounterSnapshot;
    bool bHasCounterSnapshot = false;

    // ==================== JSON-Based Wiring (Phase 5/6) ====================

    /** Stored clone topology for post-build wiring (Phase 5) */
    TSharedPtr<FSFCloneTopology> StoredCloneTopology;

    /** Map of clone_id -> spawned hologram for post-build wiring (cleared after build).
     *  UPROPERTY so the GC tracks these pointers and NULLs them on collection instead of leaving
     *  dangling raws (the holograms are preview actors the engine destroys). Without this, a stale
     *  entry passes a bare null check and is dereferenced (use-after-free). */
    UPROPERTY()
    TMap<FString, TObjectPtr<AFGHologram>> JsonSpawnedHolograms;

    /** Map of clone_id -> built actor (populated during Construct(), used for wiring).
     *  UPROPERTY for GC tracking — collected/demolished actors become null rather than dangling. */
    UPROPERTY()
    TMap<FString, TObjectPtr<AActor>> JsonBuiltActors;

    /** Cached preview locations for restored scaled factories; hologram pointers are often invalid by post-build wiring time. */
    TMap<FString, FVector> RestoredScaledFactoryPreviewLocations;

    bool bRestoredCloneTopologyActive = false;
    TWeakObjectPtr<AFGHologram> RestoredCloneParentHologram;
    TSharedPtr<FSFCloneTopology> RestoredCloneTopologyTemplate;
    TSharedPtr<FSFCloneTopology> RestoredCloneBaseTopology;
    FVector RestoredCloneLastParentLocation = FVector::ZeroVector;
    FRotator RestoredCloneLastParentRotation = FRotator::ZeroRotator;
    bool bRestoredScaledWiringDeferred = false;
    bool bRestoredScaledWiringRetryScheduled = false;
    int32 RestoredScaledWiringRetryAttempts = 0;

    // Restore-replay helpers (BuildRestoredCloneTopologyForCurrentState, ClearRestoredCloneTopologyPreview,
    // SpawnRestoredScaledFactoryHolograms, SpawnRestoredCloneTopology) moved to USFExtendRestoreReplayService (T1 split).

    // ==================== Power Extend Tracking (Issue #229) ====================

    /** Source power pole data for post-build wiring, keyed by clone_id (e.g., "power_pole_0") */
    struct FSFSourcePoleWiringData
    {
        TWeakObjectPtr<class AFGBuildablePowerPole> SourcePole;
        bool bSourceHasFreeConnections = false;
        int32 SourceFreeConnections = 0;
        int32 MaxConnections = 4;
    };
    TMap<FString, FSFSourcePoleWiringData> PowerPoleWiringData;

public:
    /** Register a built actor with its clone_id (called from hologram Construct()) */
    void RegisterJsonBuiltActor(const FString& CloneId, AActor* BuiltActor);

    /** Get a built actor by its clone_id (for pre-tick wiring in ConfigureComponents) */
    AFGBuildable* GetBuiltActorByCloneId(const FString& CloneId) const;

    /** Get a source buildable by its actor name (for lane segments connecting to existing buildables) */
    AFGBuildable* GetSourceBuildableByName(const FString& ActorName) const;

public:
    // ==================== Diagnostic Capture (delegates to DiagnosticsService) ====================

    /** Capture the "before" snapshot when preview phase starts */
    void CapturePreviewSnapshot();

    /** Capture the "after" snapshot and log diff when all builds complete */
    void CapturePostBuildSnapshotAndLogDiff();

    /** Whether a preview snapshot has been captured (guards re-capture in CreateBeltPreviews) */
    bool HasPreviewSnapshot() const;
};
