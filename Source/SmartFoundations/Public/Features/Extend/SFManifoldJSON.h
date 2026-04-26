#pragma once

#include "CoreMinimal.h"
#include "SFManifoldJSON.generated.h"

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
 * JSON-serializable 3D vector matching schema: { "x": 0, "y": 0, "z": 0 }
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
 * JSON-serializable rotator matching schema: { "pitch": 0, "yaw": 0, "roll": 0 }
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
 * Transform matching schema: { "location": {...}, "rotation": {...} }
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
// SOURCE JSON STRUCTURES (Capture from world)
// ============================================================================

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
};

/**
 * Complete source topology JSON (captured from world)
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
    
    /** Capture topology from built factory (for verification against source) */
    static FSFSourceTopology CaptureFromBuiltFactory(class AFGBuildableFactory* Factory);
    
    /** Optional: Save to file for debugging */
    bool SaveToFile(const FString& FilePath) const;
    
    /** Check if topology has any chains */
    bool IsValid() const 
    { 
        return BeltInputChains.Num() > 0 || BeltOutputChains.Num() > 0 || 
               PipeInputChains.Num() > 0 || PipeOutputChains.Num() > 0; 
    }
};

// ============================================================================
// CLONE JSON STRUCTURES (For hologram spawning)
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
    
    /** Generate from source topology with offset */
    static FSFCloneTopology FromSource(const FSFSourceTopology& Source, const FVector& Offset);
    
    /** Optional: Save to file for debugging */
    bool SaveToFile(const FString& FilePath) const;
    
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
