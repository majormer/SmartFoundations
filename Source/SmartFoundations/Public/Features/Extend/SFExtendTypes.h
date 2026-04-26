// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendTypes - Shared type definitions for EXTEND feature
 * 
 * Contains struct definitions used across EXTEND services:
 * - FSFConnectionChainNode - Belt chain data
 * - FSFPipeConnectionChainNode - Pipe chain data
 * - FSFPowerChainNode - Power pole data (Issue #229)
 * - FSFExtendTopology - Complete topology data
 * 
 * Separated to avoid circular dependencies between services.
 */

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "SFExtendTypes.generated.h"

class AFGBuildable;
class AFGBuildableConveyorBase;
class AFGBuildablePipeline;
class AFGBuildablePowerPole;
class AFGBuildableWire;
class UFGRecipe;

/**
 * Data about a single belt connection in the topology chain
 */
USTRUCT(BlueprintType)
struct FSFConnectionChainNode
{
    GENERATED_BODY()

    /** All conveyors in this chain (from factory to distributor) */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Conveyors;

    /** All support poles in this chain (conveyor poles, stackable poles) */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> SupportPoles;

    /** Issue #260: All passthroughs (floor holes) traversed in this chain */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> Passthroughs;

    /** The distributor/junction at the end of this connection */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildable> Distributor;

    /** The connector on the source building this chain starts from */
    UPROPERTY()
    TWeakObjectPtr<UFGFactoryConnectionComponent> SourceConnector;

    /** The connector on the distributor this chain ends at */
    UPROPERTY()
    TWeakObjectPtr<UFGFactoryConnectionComponent> DistributorConnector;

    /** Recipe used for the distributor */
    UPROPERTY()
    TSubclassOf<class UFGRecipe> DistributorRecipe;

    FSFConnectionChainNode()
        : Distributor(nullptr)
        , SourceConnector(nullptr)
        , DistributorConnector(nullptr)
        , DistributorRecipe(nullptr)
    {}
    
    /** Get the first conveyor in the chain (for backwards compatibility) */
    AFGBuildableConveyorBase* GetFirstConveyor() const
    {
        return Conveyors.Num() > 0 ? Conveyors[0].Get() : nullptr;
    }
};

/**
 * Data about a single pipe connection in the topology chain
 */
USTRUCT(BlueprintType)
struct FSFPipeConnectionChainNode
{
    GENERATED_BODY()

    /** All pipelines in this chain (from factory to junction) */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildablePipeline>> Pipelines;

    /** All support poles in this chain (pipe supports, stackable pipe supports) */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> SupportPoles;

    /** Issue #260: All passthroughs (pipe floor holes) traversed in this chain */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> Passthroughs;

    /** Issue #288: All inline pipe attachments (valves, pumps — both are
     *  AFGBuildablePipelinePump subclasses) traversed in this chain. The walker
     *  treats them like passthroughs: record them and continue through to the
     *  other connector. Cloning preserves user-configured mUserFlowLimit. */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> PipeAttachments;

    /** The pipe junction at the end of this connection */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildable> Junction;

    /** The connector on the source building this chain starts from */
    UPROPERTY()
    TWeakObjectPtr<UFGPipeConnectionComponent> SourceConnector;

    /** The connector on the junction this chain ends at */
    UPROPERTY()
    TWeakObjectPtr<UFGPipeConnectionComponent> JunctionConnector;

    /** Recipe used for the junction */
    UPROPERTY()
    TSubclassOf<class UFGRecipe> JunctionRecipe;

    FSFPipeConnectionChainNode()
        : Junction(nullptr)
        , SourceConnector(nullptr)
        , JunctionConnector(nullptr)
        , JunctionRecipe(nullptr)
    {}
    
    /** Get the first pipeline in the chain (for backwards compatibility) */
    AFGBuildablePipeline* GetFirstPipeline() const
    {
        return Pipelines.Num() > 0 ? Pipelines[0].Get() : nullptr;
    }
};

/**
 * Data about a power pole connected to the source factory within the manifold bounds.
 * Issue #229: Power Extend - clone power infrastructure with factory.
 */
USTRUCT(BlueprintType)
struct FSFPowerChainNode
{
    GENERATED_BODY()

    /** The power pole connected to the source factory */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildablePowerPole> PowerPole;

    /** The wire connecting the factory to this pole */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildableWire> Wire;

    /** Which power connector on the factory this pole is wired to */
    UPROPERTY()
    TWeakObjectPtr<UFGPowerConnectionComponent> FactoryConnector;

    /** Which circuit connector on the pole the wire attaches to */
    UPROPERTY()
    TWeakObjectPtr<UFGCircuitConnectionComponent> PoleConnector;

    /** Relative offset from factory to pole (world space) */
    FVector RelativeOffset = FVector::ZeroVector;

    /** Whether the source pole has free connections for source↔clone wiring */
    bool bSourceHasFreeConnections = false;

    /** Number of free connections on source pole at time of topology capture */
    int32 SourceFreeConnections = 0;

    /** Max connections for this pole tier (Mk1=4, Mk2=7, Mk3=10) */
    int32 MaxConnections = 4;

    FSFPowerChainNode()
        : PowerPole(nullptr)
        , Wire(nullptr)
        , FactoryConnector(nullptr)
        , PoleConnector(nullptr)
    {}
};

/**
 * Complete topology data for a factory building's connections
 */
USTRUCT(BlueprintType)
struct FSFExtendTopology
{
    GENERATED_BODY()

    /** The source factory building being extended */
    UPROPERTY()
    TWeakObjectPtr<AFGBuildable> SourceBuilding;

    /** All belt input connection chains (distributor → factory) */
    UPROPERTY()
    TArray<FSFConnectionChainNode> InputChains;

    /** All belt output connection chains (factory → distributor) */
    UPROPERTY()
    TArray<FSFConnectionChainNode> OutputChains;

    /** All pipe input connection chains (junction → factory) */
    UPROPERTY()
    TArray<FSFPipeConnectionChainNode> PipeInputChains;

    /** All pipe output connection chains (factory → junction) */
    UPROPERTY()
    TArray<FSFPipeConnectionChainNode> PipeOutputChains;

    /** Power poles connected to the source factory within manifold bounds (Issue #229) */
    UPROPERTY()
    TArray<FSFPowerChainNode> PowerPoles;

    /** Issue #260: Pipe passthroughs (floor holes) discovered spatially near pipe chains.
     *  Pipe floor holes are NOT in the pipe connection chain — pipes pass through physically
     *  but not logically. These are discovered by spatial proximity after chain walking. */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> PipePassthroughs;

    /** Wall holes (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C, and any
     *  "*WallHole_C" variants) discovered spatially within the manifold bounds. These are
     *  Blueprint-only decorator buildables — they have no factory/pipe connection components
     *  and do NOT interrupt belt/pipe splines — so they are not captured by the chain walker.
     *  Instead, DiscoverWallPassthroughs() filters by class-name suffix and bounding-box
     *  overlap against captured chain belts/pipes/passthroughs (ownership proxy). */
    UPROPERTY()
    TArray<TWeakObjectPtr<AFGBuildable>> WallPassthroughs;

    /** Whether topology was successfully walked */
    UPROPERTY()
    bool bIsValid = false;

    void Reset()
    {
        SourceBuilding.Reset();
        InputChains.Empty();
        OutputChains.Empty();
        PipeInputChains.Empty();
        PipeOutputChains.Empty();
        PowerPoles.Empty();
        PipePassthroughs.Empty();
        WallPassthroughs.Empty();
        bIsValid = false;
    }
};
