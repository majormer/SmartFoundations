#pragma once

#include "CoreMinimal.h"
#include "SFWiringManifest.generated.h"

// Forward declarations
class AFGBuildableFactory;
class UFGFactoryConnectionComponent;
class UFGPipeConnectionComponentBase;
struct FSFCloneTopology;

/**
 * Connection endpoint - identifies a specific connector on a buildable
 */
USTRUCT()
struct FSFWiringEndpoint
{
    GENERATED_BODY()
    
    /** Clone ID from FSFCloneTopology (e.g., "belt_segment_0", "parent") */
    UPROPERTY()
    FString CloneId;
    
    /** Actual actor name after build (e.g., "Build_ConveyorBeltMk5_C_2147465600") */
    UPROPERTY()
    FString ActorName;
    
    /** Connector component name (e.g., "ConveyorAny0", "Input0", "PipelineConnection1") */
    UPROPERTY()
    FString Connector;
    
    /** Resolved pointer (populated during wiring, not serialized) */
    UPROPERTY(Transient)
    AActor* ResolvedActor = nullptr;
    
    /** True if this endpoint refers to a source buildable (not cloned), needs resolution via ExtendService */
    UPROPERTY(Transient)
    bool bIsSourceBuildable = false;
    
    /** Default constructor */
    FSFWiringEndpoint() = default;
    
    /** Convenience constructor */
    FSFWiringEndpoint(const FString& InCloneId, const FString& InActorName, const FString& InConnector)
        : CloneId(InCloneId), ActorName(InActorName), Connector(InConnector), ResolvedActor(nullptr), bIsSourceBuildable(false) {}
};

/**
 * Single connection to wire between two buildables
 */
USTRUCT()
struct FSFWiringConnection
{
    GENERATED_BODY()
    
    /** Human-readable description (e.g., "belt_segment_0.ConveyorAny0 -> belt_segment_1.ConveyorAny1") */
    UPROPERTY()
    FString Description;
    
    /** Source endpoint */
    UPROPERTY()
    FSFWiringEndpoint Source;
    
    /** Target endpoint */
    UPROPERTY()
    FSFWiringEndpoint Target;
    
    /** Default constructor */
    FSFWiringConnection() = default;
    
    /** Convenience constructor */
    FSFWiringConnection(const FSFWiringEndpoint& InSource, const FSFWiringEndpoint& InTarget)
        : Source(InSource), Target(InTarget)
    {
        Description = FString::Printf(TEXT("%s.%s -> %s.%s"),
            *Source.CloneId, *Source.Connector,
            *Target.CloneId, *Target.Connector);
    }
};

/**
 * Built buildable entry - maps clone_id to actual built actor
 */
USTRUCT()
struct FSFBuiltBuildable
{
    GENERATED_BODY()
    
    /** Clone ID from FSFCloneTopology */
    UPROPERTY()
    FString CloneId;
    
    /** Actual actor name after build */
    UPROPERTY()
    FString ActorName;
    
    /** Build class name */
    UPROPERTY()
    FString BuildClass;
    
    /** Role (belt_segment, lift_segment, distributor, pipe_segment, junction) */
    UPROPERTY()
    FString Role;
    
    /** World location */
    UPROPERTY()
    FVector Location = FVector::ZeroVector;
    
    /** Resolved pointer (populated during wiring, not serialized) */
    UPROPERTY(Transient)
    AActor* ResolvedActor = nullptr;
    
    /** Default constructor */
    FSFBuiltBuildable() = default;
};

/**
 * Ordered chain of belt connections for sequential wiring
 * Connections are ordered from distributor -> segments -> factory (for input)
 * or factory -> segments -> distributor (for output)
 */
USTRUCT()
struct FSFOrderedBeltChain
{
    GENERATED_BODY()
    
    /** Chain identifier (e.g., "input_0", "output_0") */
    UPROPERTY()
    FString ChainId;
    
    /** Is this an input chain (distributor -> factory) or output (factory -> distributor) */
    UPROPERTY()
    bool bIsInputChain = true;
    
    /** Ordered connections - process in this order for proper chain unification */
    UPROPERTY()
    TArray<FSFWiringConnection> OrderedConnections;
    
    /** Distributor clone_id at the end of this chain */
    UPROPERTY()
    FString DistributorCloneId;
};

/**
 * Complete wiring manifest - contains all information needed to wire connections post-build
 * 
 * This is generated after all child holograms have been built into buildables (Phase 5),
 * and executed in a single deferred tick to wire all connections (Phase 6).
 */
USTRUCT()
struct SMARTFOUNDATIONS_API FSFWiringManifest
{
    GENERATED_BODY()
    
    /** Schema version */
    UPROPERTY()
    FString SchemaVersion = TEXT("1.0");
    
    /** Timestamp when manifest was generated */
    UPROPERTY()
    FString Timestamp;
    
    /** Parent factory actor name */
    UPROPERTY()
    FString ParentActorName;
    
    /** Parent factory class name */
    UPROPERTY()
    FString ParentClass;
    
    /** Parent factory location */
    UPROPERTY()
    FVector ParentLocation = FVector::ZeroVector;
    
    /** All built buildables (clone_id -> buildable mapping) */
    UPROPERTY()
    TArray<FSFBuiltBuildable> BuiltBuildables;
    
    /** Belt/lift connections to wire */
    UPROPERTY()
    TArray<FSFWiringConnection> BeltConnections;
    
    /** Pipe connections to wire */
    UPROPERTY()
    TArray<FSFWiringConnection> PipeConnections;
    
    // === Statistics ===
    
    UPROPERTY()
    int32 TotalBuildables = 0;
    
    UPROPERTY()
    int32 BeltSegments = 0;
    
    UPROPERTY()
    int32 LiftSegments = 0;
    
    UPROPERTY()
    int32 Distributors = 0;
    
    UPROPERTY()
    int32 PipeSegments = 0;
    
    UPROPERTY()
    int32 Junctions = 0;
    
    // === Methods ===
    
    /**
     * Generate wiring manifest from clone topology and hologram->buildable mapping
     * 
     * @param CloneTopology The clone topology containing connection data
     * @param CloneIdToBuildable Map of clone_id -> built AActor*
     * @param ParentFactory The newly built parent factory
     * @return Generated wiring manifest
     */
    static FSFWiringManifest Generate(
        const FSFCloneTopology& CloneTopology,
        const TMap<FString, AActor*>& CloneIdToBuildable,
        AFGBuildableFactory* ParentFactory);
    
    /**
     * Save manifest to JSON file for debugging
     * 
     * @param FilePath Full path to save JSON file
     * @return true if saved successfully
     */
    bool SaveToFile(const FString& FilePath) const;
    
    /**
     * Resolve all actor pointers from actor names
     * Must be called before ExecuteWiring if actors were looked up by name
     * 
     * @param World The world to search for actors
     * @return Number of actors successfully resolved
     */
    int32 ResolveActors(UWorld* World);
    
    /**
     * Execute all wiring in a single call
     * All belt and pipe connections are wired atomically
     * 
     * @param World The world context
     * @return Number of connections successfully wired
     */
    int32 ExecuteWiring(UWorld* World);
    
    /**
     * Create chain actors for all wired conveyor belts
     * Must be called AFTER ExecuteWiring to ensure connections are established
     * 
     * This walks chains from source distributors through lane segments to clone
     * distributors and then to the factory, processing conveyors in order to
     * ensure proper chain unification.
     * 
     * @param World The world context
     * @param AdditionalActors Map of CloneId -> AActor* for lane segments and other actors
     *                         built through ConfigureComponents (not in BuiltBuildables)
     * @return Number of conveyors with valid chain actors
     */
    int32 CreateChainActors(UWorld* World, const TMap<FString, AActor*>& AdditionalActors = TMap<FString, AActor*>());
    
    /**
     * Organize belt connections into ordered chains for sequential wiring
     * Groups connections by chain and orders them: distributor -> segments -> factory
     * 
     * @return Array of ordered chains ready for sequential execution
     */
    TArray<FSFOrderedBeltChain> OrganizeConnectionsByChain() const;
    
    /**
     * Execute a single belt connection and verify chain actor status
     * 
     * @param Connection The connection to wire
     * @param World The world context
     * @param OutChainActor Output: the chain actor after connection (if any)
     * @return true if connection succeeded
     */
    bool WireSingleBeltAndVerify(const FSFWiringConnection& Connection, UWorld* World, 
        class AFGConveyorChainActor*& OutChainActor) const;
    
    /**
     * Schedule deferred chain rebuild for conveyors
     * This runs on the next frame to avoid race conditions with factory tick
     * 
     * @param World The world context
     * @param Conveyors Array of conveyors to rebuild chains for
     */
    static void ScheduleDeferredChainRebuild(UWorld* World, const TArray<class AFGBuildableConveyorBase*>& Conveyors);
    
    /**
     * Rebuild pipe networks for all pipes in this manifest
     * This should be called AFTER all pipes are built and registered with the subsystem
     * to ensure proper fluid flow between source and clone manifolds.
     * 
     * @param World The world context
     * @param AdditionalActors Additional actors to check for pipe networks (e.g., lane segments)
     * @return Number of networks rebuilt
     */
    int32 RebuildPipeNetworks(UWorld* World, const TMap<FString, AActor*>& AdditionalActors = TMap<FString, AActor*>()) const;
    
private:
    /**
     * Find a connection component on an actor by name
     */
    static UFGFactoryConnectionComponent* FindBeltConnection(AActor* Actor, const FString& ConnectorName);
    
    /**
     * Find a pipe connection component on an actor by name
     */
    static UFGPipeConnectionComponentBase* FindPipeConnection(AActor* Actor, const FString& ConnectorName);
    
    /**
     * Wire a single belt/lift connection
     * @return true if wired successfully
     */
    bool WireBeltConnection(const FSFWiringConnection& Connection, UWorld* World) const;
    
    /**
     * Wire a single pipe connection (includes network merging)
     * @return true if wired successfully
     */
    bool WirePipeConnection(const FSFWiringConnection& Connection, UWorld* World) const;
};
