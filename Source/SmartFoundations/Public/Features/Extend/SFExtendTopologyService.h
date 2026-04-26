// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendTopologyService - Topology Walking and Capture
 * 
 * Handles:
 * - Walking connection chains from factory buildings
 * - Capturing belt/lift chains with distributors
 * - Capturing pipe chains with junctions
 * - Capturing power poles within manifold bounds (Issue #229)
 * - Building FSFExtendTopology data structure
 * 
 * Part of EXTEND feature refactor (Dec 2025).
 * Extracted from SFExtendService for separation of concerns.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Features/Extend/SFExtendTypes.h"  // For FSFExtendTopology, FSFConnectionChainNode, FSFPipeConnectionChainNode
#include "SFExtendTopologyService.generated.h"

class AFGBuildable;
class AFGBuildableConveyorBase;
class AFGBuildableConveyorAttachment;
class AFGBuildablePipeline;
class AFGBuildablePipelineJunction;
class USFSubsystem;
class USFExtendDetectionService;

/**
 * Service for walking and capturing factory connection topology.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendTopologyService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendTopologyService();

    /** Initialize with owning subsystem reference */
    void Initialize(USFSubsystem* InSubsystem, USFExtendDetectionService* InDetectionService);

    /** Shutdown service */
    void Shutdown();

    // ==================== Topology Walking ====================

    /** Walk the connection topology from a factory building */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    bool WalkTopology(AFGBuildable* SourceBuilding);

    /** Get the current cached topology */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    const FSFExtendTopology& GetCurrentTopology() const;

    /** Clear cached topology data */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    void ClearTopology();

    /** Check if we have valid cached topology */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    bool HasValidTopology() const;

    // ==================== Helper Methods ====================

    /** Check if a building is a distributor (merger/splitter) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    bool IsDistributor(AFGBuildable* Building) const;

    /** Check if a building is a pipe junction */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    bool IsPipeJunction(AFGBuildable* Building) const;

    // ==================== Power Extend (Issue #229) ====================

    /**
     * Calculate the axis-aligned bounding box encompassing all infrastructure
     * in the current topology (factory, belts, lifts, pipes, distributors, junctions).
     * Used to filter power poles by proximity to the manifold.
     * @param Padding Extra padding in cm to add to each side of the bounds (default: 200cm = 2m)
     * @return The bounding box, or an invalid box if topology is empty
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Topology")
    FBox CalculateManifoldBounds(float Padding = 200.0f) const;

private:
    /** Walk a single belt connection chain from a connector */
    bool WalkConnectionChain(UFGFactoryConnectionComponent* StartConnector, FSFConnectionChainNode& OutNode);

    /** Walk a single pipe connection chain from a connector */
    bool WalkPipeConnectionChain(UFGPipeConnectionComponent* StartConnector, FSFPipeConnectionChainNode& OutNode);

    /**
     * Walk power connections from the source factory to discover power poles
     * within the manifold bounding box. Called at the end of WalkTopology().
     * Issue #229: Power Extend
     */
    void WalkPowerConnections(AFGBuildable* SourceBuilding);

    /**
     * Issue #260: Discover pipe passthroughs (floor holes) spatially near the factory.
     * Pipe floor holes are NOT in the pipe connection chain — pipes pass through physically
     * but connections bypass them. Discovered by proximity to the source factory.
     */
    void DiscoverPipePassthroughs(AFGBuildable* SourceBuilding);

    /**
     * Discover wall holes (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C, and any
     * "*WallHole_C" variants) adjacent to the captured chains. In Satisfactory 1.1+ these are
     * Blueprint-only decorator buildables — no factory/pipe connection components, no passthrough
     * base class. They sit between two belts/pipes that each end at the wall hole's snap faces;
     * the two belt/pipe connectors are co-located at the wall hole's center plane.
     *
     * Ownership is determined by probing every connector on every captured chain belt/pipe for
     * a wall-hole candidate within ~150 cm (covers belt/pipe connector offset + Mk variants).
     * This matches the player's placement model: a wall hole sits exactly where two chain
     * segments meet at a wall crossing. No collision queries, no AABB heuristics.
     */
    void DiscoverWallPassthroughs(AFGBuildable* SourceBuilding);

    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Detection service for target validation */
    UPROPERTY()
    USFExtendDetectionService* DetectionService = nullptr;

    /** Cached topology from last walked building */
    UPROPERTY()
    FSFExtendTopology CachedTopology;
};
