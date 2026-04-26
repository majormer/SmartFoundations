#pragma once

#include "CoreMinimal.h"
#include "FGSplineHologram.h"  // For FSplinePointData
#include "SFHologramData.generated.h"

// Forward declarations
class AFGHologram;
class AFGBuildable;
class UFGRecipe;
class UFGPipeConnectionComponentBase;
class UFGFactoryConnectionComponent;

// Child hologram type enum
UENUM(BlueprintType)
enum class ESFChildHologramType : uint8 {
    Normal,
    ScalingGrid,
    AutoConnect,
    ExtendClone,
    CustomData
};

// Core data structure for hologram control
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFHologramData {
    GENERATED_BODY()
    
    // Validation control flags
    UPROPERTY()
    bool bNeedToCheckPlacement = true;
    
    UPROPERTY()
    bool bIgnoreLocationUpdates = false;
    
    // Child hologram metadata
    UPROPERTY()
    ESFChildHologramType ChildType = ESFChildHologramType::Normal;
    
    UPROPERTY()
    AFGHologram* ParentHologram = nullptr;
    
    UPROPERTY()
    bool bIsChildHologram = false;
    
    // Construction tracking
    UPROPERTY()
    bool bWasBuilt = false;
    
    UPROPERTY()
    AFGBuildable* CreatedActor = nullptr;
    
    // Recipe copying support
    UPROPERTY()
    TSubclassOf<UFGRecipe> StoredRecipe = nullptr;
    
    // Backup spline data for EXTEND belt holograms
    // Stored because mSplineData on AFGSplineHologram can be cleared by replication
    UPROPERTY()
    TArray<FSplinePointData> BackupSplineData;
    
    // Flag to indicate backup spline data is valid
    UPROPERTY()
    bool bHasBackupSplineData = false;
    
    // EXTEND chain tracking
    UPROPERTY()
    int32 ExtendChainId = -1;
    
    // Index within the chain (for ordering belts/lifts)
    UPROPERTY()
    int32 ExtendChainIndex = -1;
    
    // Total length of the chain (belts + lifts combined)
    UPROPERTY()
    int32 ExtendChainLength = 0;
    
    // Whether this is an input chain (vs output chain)
    UPROPERTY()
    bool bIsInputChain = false;
    
    // Manifold pipe tracking (for wiring after build)
    UPROPERTY()
    bool bIsManifoldPipe = false;
    
    // Source junction connector for manifold wiring (raw pointer - only valid during build session)
    UPROPERTY()
    UFGPipeConnectionComponentBase* ManifoldSourceConnector = nullptr;
    
    // Chain ID for finding the clone junction after build
    UPROPERTY()
    int32 ManifoldCloneChainId = -1;
    
    // Manifold belt tracking (for wiring after build)
    UPROPERTY()
    bool bIsManifoldBelt = false;
    
    // Source distributor connector for manifold wiring (raw pointer - only valid during build session)
    UPROPERTY()
    UFGFactoryConnectionComponent* ManifoldSourceBeltConnector = nullptr;
    
    // Chain ID for finding the clone distributor after build
    UPROPERTY()
    int32 ManifoldBeltCloneChainId = -1;
    
    // EXTEND: Distributor connector name for this chain (e.g., "Output1", "Output2")
    // Used to find the correct output on splitters with multiple outputs
    UPROPERTY()
    FName ExtendDistributorConnectorName = NAME_None;
    
    // EXTEND: Lift-specific data for JSON-driven spawning
    UPROPERTY()
    bool bHasLiftData = false;
    
    UPROPERTY()
    float LiftHeight = 0.0f;
    
    UPROPERTY()
    bool bLiftIsReversed = false;
    
    // Top transform is RELATIVE to actor (local space)
    UPROPERTY()
    FTransform LiftTopTransform = FTransform::Identity;
    
    // Bottom transform is WORLD space
    UPROPERTY()
    FTransform LiftBottomTransform = FTransform::Identity;
    
    // JSON clone ID for post-build wiring (e.g., "belt_segment_0", "distributor_1")
    // Set during JSON hologram spawning, used to register built actor with ExtendService
    UPROPERTY()
    FString JsonCloneId;
    
    // Issue #288: Pipe-attachment (valve/pump) clone metadata
    // Set by the spawn handler for role="pipe_attachment" so Construct() can
    // apply the captured UserFlowLimit to the built AFGBuildablePipelinePump.
    UPROPERTY()
    bool bIsPipeAttachmentClone = false;
    
    UPROPERTY()
    float PipeAttachmentUserFlowLimit = -1.0f;
    
    // ========================================================================
    // Stackable Pipe Support Auto-Connect (Issue #220)
    // ========================================================================
    // Connectors to wire the pipe to after construction
    UPROPERTY()
    bool bIsStackablePipe = false;
    
    UPROPERTY()
    UFGPipeConnectionComponentBase* StackablePipeConn0 = nullptr;  // Start connector (on source support)
    
    UPROPERTY()
    UFGPipeConnectionComponentBase* StackablePipeConn1 = nullptr;  // End connector (on target support)
    
    UPROPERTY()
    int32 StackablePipeIndex = -1;  // Position in pipe chain (0, 1, 2...) for pipe-to-pipe wiring
    
    // ========================================================================
    // Stackable Conveyor Pole Auto-Connect (Belt connections)
    // ========================================================================
    UPROPERTY()
    bool bIsStackableBelt = false;
    
    UPROPERTY()
    int32 StackableBeltIndex = -1;  // Position in belt chain (0, 1, 2...)
    
    UPROPERTY()
    TWeakObjectPtr<UFGFactoryConnectionComponent> StackableBeltConn0;  // Source pole connector (belt output connects here)
    
    UPROPERTY()
    TWeakObjectPtr<UFGFactoryConnectionComponent> StackableBeltConn1;  // Target pole connector (belt input connects here)
    
    // ========================================================================
    // Pipe Junction Auto-Connect (Issue #235)
    // ========================================================================
    // Connectors to wire the pipe to after construction (junction→building or junction→junction)
    UPROPERTY()
    bool bIsPipeAutoConnectChild = false;
    
    UPROPERTY()
    UFGPipeConnectionComponentBase* PipeAutoConnectConn0 = nullptr;  // Junction-side connector
    
    UPROPERTY()
    UFGPipeConnectionComponentBase* PipeAutoConnectConn1 = nullptr;  // Target connector (building or other junction)
    
    UPROPERTY()
    bool bIsPipeManifold = false;  // True if junction→junction, false if junction→building
    
    // ========================================================================
    // Pre-tick connection targets (for ConfigureComponents wiring)
    // ========================================================================
    // These are populated during hologram creation from the wiring manifest.
    // In ConfigureComponents, we look up the built actor by clone_id and connect.
    // This allows connections to be made DURING construction (like AutoLink).
    
    // Connection target for Conn0 (belt input / "ConveyorAny0")
    UPROPERTY()
    FString Conn0TargetCloneId;  // e.g., "belt_segment_1", "distributor_0"
    
    UPROPERTY()
    FName Conn0TargetConnectorName = NAME_None;  // e.g., "ConveyorAny1", "Output0"
    
    // Connection target for Conn1 (belt output / "ConveyorAny1")
    UPROPERTY()
    FString Conn1TargetCloneId;
    
    UPROPERTY()
    FName Conn1TargetConnectorName = NAME_None;
};
