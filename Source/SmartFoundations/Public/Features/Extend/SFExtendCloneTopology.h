// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "SFExtendCloneTopology.generated.h"

// Forward declarations
class AFGBuildable;
class AFGBuildableFactory;
class AFGBuildableConveyorBelt;
class AFGBuildableConveyorLift;
class AFGBuildablePipeline;
class AFGBuildableConveyorAttachment;
class UFGFactoryConnectionComponent;
class UFGPipeConnectionComponent;

/**
 * Lightweight vector value used by clone topology structs.
 */
USTRUCT(BlueprintType)
struct FSFVec3
{
    GENERATED_BODY()
    
    UPROPERTY() float X = 0.0f;
    UPROPERTY() float Y = 0.0f;
    UPROPERTY() float Z = 0.0f;
    
    FSFVec3() = default;
    FSFVec3(const FVector& V) : X(V.X), Y(V.Y), Z(V.Z) {}
    FSFVec3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
    
    FVector ToFVector() const { return FVector(X, Y, Z); }
    
    FSFVec3 operator+(const FSFVec3& Other) const { return FSFVec3(X + Other.X, Y + Other.Y, Z + Other.Z); }
};

/**
 * Lightweight rotator value used by clone topology structs.
 */
USTRUCT(BlueprintType)
struct FSFRot3
{
    GENERATED_BODY()
    
    UPROPERTY() float Pitch = 0.0f;
    UPROPERTY() float Yaw = 0.0f;
    UPROPERTY() float Roll = 0.0f;
    
    FSFRot3() = default;
    FSFRot3(const FRotator& R) : Pitch(R.Pitch), Yaw(R.Yaw), Roll(R.Roll) {}
    
    FRotator ToFRotator() const { return FRotator(Pitch, Yaw, Roll); }
};

/**
 * Transform value used by captured source and emitted clone topology.
 */
USTRUCT(BlueprintType)
struct FSFTransform
{
    GENERATED_BODY()
    
    UPROPERTY() FSFVec3 Location;
    UPROPERTY() FSFRot3 Rotation;
    
    FSFTransform() = default;
    FSFTransform(const FVector& Loc, const FRotator& Rot) : Location(Loc), Rotation(Rot) {}
    FSFTransform(const FTransform& T) : Location(T.GetLocation()), Rotation(T.Rotator()) {}
    
    FTransform ToFTransform() const { return FTransform(Rotation.ToFRotator(), Location.ToFVector()); }
    
    /** Apply world offset to location */
    FSFTransform WithOffset(const FSFVec3& Offset) const
    {
        FSFTransform Result = *this;
        Result.Location = Location + Offset;
        return Result;
    }
};

/**
 * Spline point matching schema with tangents
 */
USTRUCT(BlueprintType)
struct FSFSplinePoint
{
    GENERATED_BODY()
    
    UPROPERTY() FSFVec3 Local;
    UPROPERTY() FSFVec3 World;
    UPROPERTY() FSFVec3 ArriveTangent;
    UPROPERTY() FSFVec3 LeaveTangent;
    
    /** Apply world offset to world coordinates only */
    FSFSplinePoint WithOffset(const FSFVec3& Offset) const
    {
        FSFSplinePoint Result = *this;
        Result.World = World + Offset;
        return Result;
    }
};

/**
 * Spline data for belts/pipes
 */
USTRUCT(BlueprintType)
struct FSFSplineData
{
    GENERATED_BODY()
    
    UPROPERTY() float Length = 0.0f;
    UPROPERTY() TArray<FSFSplinePoint> Points;
    
    /** Apply world offset to all points */
    FSFSplineData WithOffset(const FSFVec3& Offset) const
    {
        FSFSplineData Result;
        Result.Length = Length;
        for (const FSFSplinePoint& Point : Points)
        {
            Result.Points.Add(Point.WithOffset(Offset));
        }
        return Result;
    }
};

/**
 * Lift-specific data
 */
USTRUCT(BlueprintType)
struct FSFLiftData
{
    GENERATED_BODY()
    
    UPROPERTY() float Height = 0.0f;
    UPROPERTY() bool bIsReversed = false;
    UPROPERTY() FSFTransform TopTransform;      // Local/relative to actor
    UPROPERTY() FSFTransform BottomTransform;   // World space
    
    // Issue #260: Passthrough references for half-height lift rendering
    UPROPERTY() TArray<FString> PassthroughCloneIds;  // CloneIds of passthroughs at [0]=bottom, [1]=top
    
    /** Apply world offset to bottom transform */
    FSFLiftData WithOffset(const FSFVec3& Offset) const
    {
        FSFLiftData Result = *this;
        Result.BottomTransform = BottomTransform.WithOffset(Offset);
        return Result;
    }
};

/**
 * Connection reference: { "target": "Splitter", "connector": "Output2" }
 */
USTRUCT(BlueprintType)
struct FSFConnectionRef
{
    GENERATED_BODY()
    
    UPROPERTY() FString Target;      // "Splitter", "Segment[0]", "Factory", "parent", "distributor_0"
    UPROPERTY() FString Connector;   // "Output2", "ConveyorAny0", "Input0"
    
    FSFConnectionRef() = default;
    FSFConnectionRef(const FString& InTarget, const FString& InConnector) 
        : Target(InTarget), Connector(InConnector) {}
};

/**
 * Connections map for a segment: { "ConveyorAny0": {...}, "ConveyorAny1": {...} }
 */
USTRUCT(BlueprintType)
struct FSFConnections
{
    GENERATED_BODY()
    
    UPROPERTY() FSFConnectionRef ConveyorAny0;
    UPROPERTY() FSFConnectionRef ConveyorAny1;
};

// ============================================================================
// SOURCE TOPOLOGY STRUCTURES (Capture from world)
// ============================================================================

/**
 * [#477] Appearance captured from a source actor at topology-BUILD time (descriptor class paths +
 * override colors), so saved presets replay paint without any live source actor. Additive:
 * bCaptured=false (old presets / uncaptured records) makes ApplyTo a no-op and the spawner falls
 * back to the legacy live-actor harvest. Class paths (not raw pointers) keep the record
 * JSON-serializable and MP-safe; they resolve via TryLoadClass at apply time.
 */
USTRUCT(BlueprintType)
struct FSFCapturedCustomization
{
    GENERATED_BODY()

    UPROPERTY() bool bCaptured = false;
    UPROPERTY() FString SwatchClass;
    UPROPERTY() FString PatternClass;
    UPROPERTY() FString MaterialClass;
    UPROPERTY() FString SkinClass;
    UPROPERTY() FString PaintFinishClass;
    UPROPERTY() FLinearColor OverridePrimary = FLinearColor(0.f, 0.f, 0.f, 1.f);
    UPROPERTY() FLinearColor OverrideSecondary = FLinearColor(0.f, 0.f, 0.f, 1.f);
    UPROPERTY() uint8 PatternRotation = 0;

    /** Snapshot a live actor's customization (defined in SFExtendCloneTopology.cpp). */
    void CaptureFrom(const struct FFactoryCustomizationData& Data);

    /** Rebuild customization data; returns false when nothing was captured. */
    bool ApplyTo(struct FFactoryCustomizationData& Out) const;
};

/**
 * Source segment (belt, lift, or pipe in a chain)
 */
USTRUCT(BlueprintType)
struct FSFSourceSegment
{
    GENERATED_BODY()
    
    UPROPERTY() int32 Index = 0;
    UPROPERTY() FString Type;           // "belt", "lift", "pipe", "passthrough", "pipe_attachment"
    UPROPERTY() FString Id;             // Actor unique ID
    UPROPERTY() FString Class;          // Build class name
    UPROPERTY() FString RecipeClass;    // Recipe class name
    UPROPERTY() FSFTransform Transform;
    UPROPERTY() FSFConnections Connections;

    // Spatial attachments (floor holes) are outside the logical conduit chain. Capture the
    // snapped conduit actor IDs so clone planning can drop the attachment with an excluded branch.
    UPROPERTY() TArray<FString> RelatedSourceIds;
    
    // Chain actor info (for belts/lifts - helps debug chain groupings)
    UPROPERTY() FString ChainActorName;     // Name of AFGConveyorChainActor
    UPROPERTY() int32 ChainSegmentIndex = -1; // Index within the chain actor
    
    // Type-specific data (only one is valid based on Type)
    UPROPERTY() FSFSplineData SplineData;   // For belts/pipes
    UPROPERTY() FSFLiftData LiftData;       // For lifts
    UPROPERTY() bool bHasSplineData = false;
    UPROPERTY() bool bHasLiftData = false;
    
    // Issue #260: Foundation thickness for passthrough floor holes
    UPROPERTY() float Thickness = 0.0f;
    
    // Issue #288: User-configured flow limit for pipe attachments (valves, pumps).
    // -1.0 = unlimited (vanilla default, i.e. fully-open valve). Ignored for other types.
    UPROPERTY() float UserFlowLimit = -1.0f;
    
    // Issue #288: Source-id of the power pole this pipe attachment's PowerInput is
    // directly connected to in the source manifold. Empty string means the attachment
    // has no PowerInput (valves) or was unpowered (pump with no cable). Used at
    // preview time to validate clone-pole connection capacity and at post-build time
    // to wire the clone pump to the matching clone pole. Only populated when the
    // source pump's directly-connected pole is itself inside the manifold (i.e. also
    // present in FSFSourceTopology::PowerPoles); out-of-manifold poles are ignored.
    UPROPERTY() FString ConnectedPowerPoleSourceId;

    // [#477] Appearance captured while the source actor is alive.
    UPROPERTY() FSFCapturedCustomization Customization;
};

/**
 * Source distributor (splitter, merger, or pipe junction)
 */
USTRUCT(BlueprintType)
struct FSFSourceDistributor
{
    GENERATED_BODY()

    UPROPERTY() FString Id;
    UPROPERTY() FString Class;
    UPROPERTY() FString RecipeClass;
    UPROPERTY() FSFTransform Transform;
    UPROPERTY() FString ConnectorUsed;  // Which connector connects to this chain
    UPROPERTY() TArray<FString> ConnectedConnectors;  // Connectors already connected to something (for lane selection)
    UPROPERTY() TMap<FString, FVector> ConnectorWorldPositions;  // Actual world positions of each connector

    // [#477] Appearance captured while the source actor is alive.
    UPROPERTY() FSFCapturedCustomization Customization;
    // [#477] Resolved LANE appearance, sampled at capture time from the first belt/pipe of this
    // distributor's chain (the same source the live LaneColorMap uses). Lanes are GENERATED
    // segments with no source actor, and the far-side infrastructure can't be resampled after
    // the session ends - so their look must be decided here.
    UPROPERTY() FSFCapturedCustomization LaneCustomization;
};

/**
 * Source chain (factory connector → segments → distributor)
 */
USTRUCT(BlueprintType)
struct FSFSourceChain
{
    GENERATED_BODY()
    
    UPROPERTY() FString ChainId;            // "belt_input_0", "pipe_output_1"
    UPROPERTY() FString FactoryConnector;   // "Input0", "Output0"
    UPROPERTY() FSFSourceDistributor Distributor;
    UPROPERTY() TArray<FSFSourceSegment> Segments;
};

/**
 * Source factory building
 */
USTRUCT(BlueprintType)
struct FSFSourceFactory
{
    GENERATED_BODY()
    
    UPROPERTY() FString Id;
    UPROPERTY() FString Class;
    UPROPERTY() FSFTransform Transform;
};

/**
 * Source power pole connected to the factory within manifold bounds (Issue #229)
 */
USTRUCT(BlueprintType)
struct FSFSourcePowerPole
{
    GENERATED_BODY()
    
    UPROPERTY() FString Id;              // Actor unique ID
    UPROPERTY() FString Class;           // Build class name (e.g., Build_PowerPoleMk1_C)
    UPROPERTY() FString RecipeClass;     // Recipe class name
    UPROPERTY() FSFTransform Transform;  // World transform of the pole
    UPROPERTY() FSFVec3 RelativeOffset;  // Offset from factory to pole (world space)
    UPROPERTY() bool bSourceHasFreeConnections = false;  // Whether source pole has free slots
    UPROPERTY() int32 SourceFreeConnections = 0;         // Number of free connections
    UPROPERTY() int32 MaxConnections = 4;                // Tier limit (Mk1=4, Mk2=7, Mk3=10)
    // Issue #345: real source power-connector world positions, so the cable preview sits exactly on
    // the connectors (clone connector = source connector + extend Offset) rather than a guessed height.
    UPROPERTY() FSFVec3 PoleConnectorWorld;              // Source pole's power connector world location
    UPROPERTY() FSFVec3 FactoryConnectorWorld;          // Source factory's power connector world location
    UPROPERTY() bool bHasConnectorWorld = false;        // True when both connector positions were captured

    // [#477] Appearance captured while the source actor is alive.
    UPROPERTY() FSFCapturedCustomization Customization;
};

/**
 * Complete source topology captured from world state.
 */
USTRUCT(BlueprintType)
struct FSFSourceTopology
{
    GENERATED_BODY()
    
    UPROPERTY() FString SchemaVersion = TEXT("1.0");
    UPROPERTY() FString CaptureTimestamp;
    
    UPROPERTY() FSFSourceFactory Factory;
    UPROPERTY() TArray<FSFSourceChain> BeltInputChains;
    UPROPERTY() TArray<FSFSourceChain> BeltOutputChains;
    UPROPERTY() TArray<FSFSourceChain> PipeInputChains;
    UPROPERTY() TArray<FSFSourceChain> PipeOutputChains;
    UPROPERTY() TArray<FSFSourcePowerPole> PowerPoles;  // Issue #229: Power poles within manifold bounds
    UPROPERTY() TArray<FSFSourceSegment> PipePassthroughs;  // Issue #260: Spatially-discovered pipe floor holes
    UPROPERTY() TArray<FSFSourceSegment> WallHoles;          // Spatially-discovered wall holes (conveyor + pipeline)
    
    /** Capture topology from existing CachedTopology struct */
    static FSFSourceTopology CaptureFromTopology(const struct FSFExtendTopology& Topology);
    
    /** Check if topology has any chains */
    bool IsValid() const 
    { 
        return BeltInputChains.Num() > 0 || BeltOutputChains.Num() > 0 || 
               PipeInputChains.Num() > 0 || PipeOutputChains.Num() > 0; 
    }
};

// ============================================================================
// CLONE TOPOLOGY STRUCTURES (For hologram spawning)
// ============================================================================

/**
 * Clone hologram entry (child hologram to spawn)
 */
USTRUCT(BlueprintType)
struct FSFCloneHologram
{
    GENERATED_BODY()
    
    // Role and identification
    UPROPERTY() FString HologramId;         // Unique identifier: "distributor_0", "belt_segment_1", etc.
    UPROPERTY() FString Role;               // "distributor", "belt_segment", "lift_segment", "pipe_segment", "pipe_junction"
    UPROPERTY() FString SourceId;           // Original actor ID for traceability
    UPROPERTY() FString SourceClass;        // Original build class
    UPROPERTY() FString SourceChain;        // Which chain: "belt_input_0"
    UPROPERTY() int32 SourceSegmentIndex = -1;  // Position in source chain (-1 for distributors)

    // [#477] Appearance captured at topology-build time - replays without live source actors
    // (saved presets across restarts/worlds, MP). bCaptured=false = legacy live-harvest fallback.
    UPROPERTY() FSFCapturedCustomization Customization;
    
    // Hologram spawning data
    UPROPERTY() FString HologramClass;      // "ASFConveyorAttachmentChildHologram", "ASFConveyorBeltHologram"
    UPROPERTY() FString BuildClass;         // Build class for hologram
    UPROPERTY() FString RecipeClass;        // Recipe class (required for distributors)
    UPROPERTY() FSFTransform Transform;     // Clone transform (with offset applied)
    
    // Type-specific data
    UPROPERTY() FSFSplineData SplineData;
    UPROPERTY() FSFLiftData LiftData;
    UPROPERTY() bool bHasSplineData = false;
    UPROPERTY() bool bHasLiftData = false;
    
    // Connection wiring
    UPROPERTY() FSFConnections SourceConnections;   // Original connections (for reference)
    UPROPERTY() FSFConnections CloneConnections;    // Resolved to hologram identifiers
    
    // Hologram behavior
    UPROPERTY() bool bConstructible = false;    // true = builds with parent, false = preview only
    UPROPERTY() bool bPreviewOnly = false;      // true = visual only, Construct returns nullptr
    
    // Issue #260: Foundation thickness for passthrough floor holes
    UPROPERTY() float Thickness = 0.0f;
    
    // Issue #288: User-configured flow limit for pipe attachments (valves, pumps).
    // -1.0 = unlimited. Only applied when Role == "pipe_attachment".
    UPROPERTY() float UserFlowLimit = -1.0f;
    
    // Issue #288: Clone-side power pole HologramId that this pipe attachment
    // should wire its PowerInput to (e.g. "power_pole_2"). Empty string means
    // no wiring will be attempted (valve, unpowered pump, or pump whose source
    // pole was outside the manifold). Resolved from the source segment's
    // ConnectedPowerPoleSourceId via SourceIdToHologramId at emit time.
    UPROPERTY() FString ConnectedPowerPoleHologramId;
    
    // Issue #288: Tier cap of this clone pole (Mk1=4, Mk2=7, Mk3=10). Mirrored
    // from FSFSourcePowerPole.MaxConnections at emit time. Only populated when
    // Role == "power_pole"; zero otherwise. Used by the preview-time capacity
    // validator to decide if (factory + inter-pole + pumps) fits on this pole.
    UPROPERTY() int32 PowerPoleMaxConnections = 0;
    
    // Lane segment metadata (only for role="lane_segment")
    UPROPERTY() bool bIsLaneSegment = false;
    UPROPERTY() FString LaneFromDistributorId;   // Source distributor HologramId (e.g., "distributor_0")
    UPROPERTY() FString LaneFromConnector;       // Source connector name (e.g., "Output1")
    UPROPERTY() FString LaneToDistributorId;     // Clone distributor HologramId (e.g., "distributor_0" at clone offset)
    UPROPERTY() FString LaneToConnector;         // Clone connector name (e.g., "Input1")
    UPROPERTY() FString LaneSegmentType;         // "belt", "lift", or "pipe"
    
    // Connector normals for proper spline routing (belt/pipe lanes only)
    UPROPERTY() FSFVec3 LaneStartNormal;         // World-space connector facing direction at start
    UPROPERTY() FSFVec3 LaneEndNormal;           // World-space connector facing direction at end

    // Issue #345: source-pole -> clone-pole power cable. Behaves like a lane segment for Scaled Extend:
    // its source-side endpoint must chain to the previous clone (not the source) and only its clone-side
    // endpoint rotates. Kept distinct from bIsLaneSegment so it does NOT pick up belt/pipe lane color
    // customization in the spawner.
    UPROPERTY() bool bIsSourceToCloneWire = false;
};

/**
 * Complete clone topology (for hologram spawning)
 */
USTRUCT(BlueprintType)
struct FSFCloneTopology
{
    GENERATED_BODY()
    
    UPROPERTY() FString SchemaVersion = TEXT("1.0");
    UPROPERTY() FSFVec3 WorldOffset;
    UPROPERTY() FString SourceFactoryId;
    
    // Parent hologram (factory)
    UPROPERTY() FString ParentBuildClass;
    UPROPERTY() FSFTransform ParentTransform;
    
    // Child holograms
    UPROPERTY() TArray<FSFCloneHologram> ChildHolograms;
    
    /** Generate from source topology with offset.
     *  PrincipalAxisWorld (#384): the extend's rotation-STABLE forward direction in world space. When
     *  supplied (non-zero), the pipe-lane SOURCE backbone port is chosen against this axis instead of
     *  the per-clone offset direction - which arcs ~5deg/clone on a rotated extend and otherwise drops
     *  the source connector below the facing threshold past ~clone 14, killing the remaining lanes.
     *  Zero (default) preserves the legacy per-clone-offset behavior for the parent/restore callers. */
    static FSFCloneTopology FromSource(const FSFSourceTopology& Source, const FVector& Offset, const FVector& PrincipalAxisWorld = FVector::ZeroVector);

    /** [#382] Apply a rigid-body yaw rotation to this clone set's infrastructure around Center.
     *  Internal segments rotate rigidly (position + rotation + spline + lift); lane segments are
     *  adaptive (only the clone-side endpoint rotates, determined by WorldOffsetToClone). This is the
     *  exact rotation the SP preview applies to clone 1 - shared so the server build matches the
     *  preview instead of leaving the parent's belts un-rotated (FromSource only positions them). */
    void ApplyRigidYawRotation(const FRotator& RotOffset, const FVector& Center, const FVector& WorldOffsetToClone);
    
    /**
     * Spawn child holograms from this clone topology
     * @param ParentHologram The parent factory hologram to attach children to
     * @param ExtendService The extend service for spawning helpers
     * @param OutSpawnedHolograms Map of hologram_id -> spawned hologram for connection wiring
     * @return Number of holograms successfully spawned
     */
    int32 SpawnChildHolograms(
        class AFGHologram* ParentHologram,
        class USFExtendService* ExtendService,
        TMap<FString, class AFGHologram*>& OutSpawnedHolograms) const;
    
    /**
     * Wire connections between spawned child holograms using CloneConnections data.
     * This sets mSnappedConnectionComponents on belt/pipe/lift holograms so that
     * when they are built, the connections are automatically established by vanilla code.
     * 
     * @param SpawnedHolograms Map of hologram_id -> spawned hologram (from SpawnChildHolograms)
     * @param ParentHologram The parent factory hologram (for "parent" connection targets)
     * @return Number of connections successfully wired
     */
    int32 WireChildHologramConnections(
        const TMap<FString, class AFGHologram*>& SpawnedHolograms,
        class AFGHologram* ParentHologram) const;
};
