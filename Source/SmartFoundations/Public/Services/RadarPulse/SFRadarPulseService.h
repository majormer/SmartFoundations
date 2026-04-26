// Copyright Coffee Stain Studios. All Rights Reserved.
// Smart! Foundations Mod - Object Radar Pulse Diagnostic Service

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/RadarPulse/SFRadarPulseSnapshot.h"
#include "SFRadarPulseService.generated.h"

// Forward declarations
class USFSubsystem;
class AActor;
class AFGBuildable;
class AFGBuildableFactory;
class AFGBuildableConveyorBase;
class AFGBuildableConveyorBelt;
class AFGBuildableConveyorLift;
class AFGBuildableConveyorAttachment;
class AFGBuildablePipeline;
class AFGBuildablePipelineJunction;
class AFGBuildablePowerPole;
class AFGBuildableFoundation;
class AFGBuildableWall;
class USplineComponent;
class AFGHologram;
class AFGSplineHologram;
class AFGConveyorBeltHologram;
struct FSFExtendTopology;

/**
 * Object Radar Pulse Diagnostic Service
 * 
 * A comprehensive diagnostic system that captures complete snapshots of all objects
 * within a configurable radius around the player. Designed as a general-purpose tool
 * for development, debugging, and feature validation.
 * 
 * Key Features:
 * - Complete Object Capture: Every actor within radius
 * - Full Property Extraction: All accessible properties per object class
 * - Before/After Comparison: Diff capability for tracking changes
 * - Source Flagging: Mark objects that are part of specific systems (EXTEND, etc.)
 * - Reusable Service: Available to any Smart! feature
 */
UCLASS()
class SMARTFOUNDATIONS_API USFRadarPulseService : public UObject
{
    GENERATED_BODY()

public:
    USFRadarPulseService();

    /** Initialize with owning subsystem reference */
    void Initialize(USFSubsystem* InSubsystem);

    /** Shutdown service */
    void Shutdown();

    // ==================== CAPTURE ====================

    /**
     * Capture everything in radius around player (no filtering)
     * @param Radius - Capture radius in cm (default 20000 = 200m)
     * @param Label - Label for the snapshot (e.g., "PRE_BUILD", "POST_BUILD")
     * @return Complete snapshot of all objects in radius
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    FSFRadarPulseSnapshot CaptureSnapshot(float Radius = 20000.0f, const FString& Label = TEXT("Snapshot"));

    /**
     * Capture everything in radius around player with category filter
     * @param Radius - Capture radius in cm (default 20000 = 200m)
     * @param Label - Label for the snapshot (e.g., "PRE_BUILD", "POST_BUILD")
     * @param CategoryFilter - Category filter (only capture matching). Valid categories:
     *                         "Belt", "Lift", "Pipe", "Factory", "Distributor", "Junction",
     *                         "Power", "Foundation", "Wall", "Wire", "Other"
     * @return Complete snapshot of filtered objects in radius
     */
    FSFRadarPulseSnapshot CaptureSnapshotFiltered(
        float Radius,
        const FString& Label,
        const TArray<FString>& CategoryFilter
    );

    /**
     * Capture around specific location (not player, no filtering)
     * @param Origin - World location to capture around
     * @param Radius - Capture radius in cm
     * @param Label - Label for the snapshot
     * @return Complete snapshot of all objects in radius
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    FSFRadarPulseSnapshot CaptureSnapshotAtLocation(const FVector& Origin, float Radius, const FString& Label);

    /**
     * Capture around specific location with category filter
     * @param Origin - World location to capture around
     * @param Radius - Capture radius in cm
     * @param Label - Label for the snapshot
     * @param CategoryFilter - Category filter (only capture matching)
     * @return Complete snapshot of filtered objects in radius
     */
    FSFRadarPulseSnapshot CaptureSnapshotAtLocationFiltered(
        const FVector& Origin,
        float Radius,
        const FString& Label,
        const TArray<FString>& CategoryFilter
    );

    // ==================== SOURCE FLAGGING ====================

    /**
     * Flag objects in snapshot that match EXTEND topology
     * @param Snapshot - Snapshot to flag (modified in place)
     * @param Topology - EXTEND topology data to match against
     * @param SourceFactory - The source factory building
     */
    void FlagExtendSourceObjects(
        FSFRadarPulseSnapshot& Snapshot,
        const FSFExtendTopology& Topology,
        AActor* SourceFactory
    );

    /**
     * Generic flag function for marking objects
     * @param Snapshot - Snapshot to flag (modified in place)
     * @param ActorsToFlag - Array of actors to mark
     * @param FlagName - Name of the flag to add
     * @param Role - Role description for flagged objects
     */
    void FlagObjectsMatching(
        FSFRadarPulseSnapshot& Snapshot,
        const TArray<AActor*>& ActorsToFlag,
        const FString& FlagName,
        const FString& Role
    );

    // ==================== COMPARISON ====================

    /**
     * Compare two snapshots and generate diff
     * @param Before - Snapshot taken before operation
     * @param After - Snapshot taken after operation
     * @return Diff containing new, removed, and modified objects
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    FSFSnapshotDiff CompareSnapshots(const FSFRadarPulseSnapshot& Before, const FSFRadarPulseSnapshot& After);

    // ==================== LOGGING ====================

    /**
     * Log full snapshot to Output Log
     * @param Snapshot - Snapshot to log
     * @param bVerbose - If true, log full details for each object
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void LogSnapshot(const FSFRadarPulseSnapshot& Snapshot, bool bVerbose = false);

    /**
     * Log full snapshot with category filter for enumeration
     * @param Snapshot - Snapshot to log
     * @param bVerbose - If true, log full details for each object
     * @param EnumerateCategories - Filter for which categories to enumerate in detail.
     *                              Summary always shows all categories.
     */
    void LogSnapshotFiltered(
        const FSFRadarPulseSnapshot& Snapshot,
        bool bVerbose,
        const TArray<FString>& EnumerateCategories
    );

    /**
     * Log diff summary
     * @param Diff - Diff to log
     * @param bVerbose - If true, log full details for each object
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void LogDiff(const FSFSnapshotDiff& Diff, bool bVerbose = false);

    /**
     * Log diff summary with category filter for enumeration
     * @param Diff - Diff to log
     * @param bVerbose - If true, log full details for each object
     * @param EnumerateCategories - Filter for which categories to enumerate in detail.
     *                              Summary always shows all categories.
     */
    void LogDiffFiltered(
        const FSFSnapshotDiff& Diff,
        bool bVerbose,
        const TArray<FString>& EnumerateCategories
    );

    /**
     * Log only objects with specific flag
     * @param Snapshot - Snapshot to log from
     * @param FlagName - Flag to filter by (e.g., "ExtendSource")
     * @param bVerbose - If true, log full details
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void LogFlaggedObjects(const FSFRadarPulseSnapshot& Snapshot, const FString& FlagName, bool bVerbose = false);

    // ==================== CACHING ====================

    /**
     * Store snapshot for later comparison
     * @param Key - Cache key (e.g., "EXTEND_PRE")
     * @param Snapshot - Snapshot to cache
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void CacheSnapshot(const FString& Key, const FSFRadarPulseSnapshot& Snapshot);

    /**
     * Retrieve cached snapshot
     * @param Key - Cache key to retrieve
     * @param OutSnapshot - Retrieved snapshot (if found)
     * @return true if snapshot was found
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    bool GetCachedSnapshot(const FString& Key, FSFRadarPulseSnapshot& OutSnapshot);

    /**
     * Clear specific or all cached snapshots
     * @param Key - Cache key to clear (empty = clear all)
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void ClearCache(const FString& Key = TEXT(""));

    // ==================== CHAIN ACTOR SCANNING ====================

    /**
     * Scan for all conveyor chain actors from belts/lifts in radius and log detailed chain information.
     * Useful for researching how vanilla builds chain structures.
     * @param Radius - Scan radius in cm (default 30000 = 300m)
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void ScanChainActors(float Radius = 30000.0f);

    // ==================== HOLOGRAM INSPECTION ====================

    /**
     * Inspect a hologram and log all its details (for debugging vanilla hologram behavior)
     * Designed to be called while actively placing a belt to understand vanilla state.
     * @param Hologram - The hologram to inspect
     * @param DebugLabel - Label for the log output
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void InspectHologram(AFGHologram* Hologram, const FString& DebugLabel = TEXT("Hologram Inspection"));

    /**
     * Scan for all holograms in a radius around the player and inspect each one.
     * Useful for debugging hologram combinations like power lines with attached poles.
     * @param Radius - Scan radius in cm (default 20000 = 200m)
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|RadarPulse")
    void InspectAllHologramsInRadius(float Radius = 20000.0f);

private:
    UPROPERTY()
    USFSubsystem* Subsystem;

    /** Snapshot cache */
    TMap<FString, FSFRadarPulseSnapshot> SnapshotCache;

    // ==================== CAPTURE HELPERS ====================

    /** Get all actors in radius */
    TArray<AActor*> GetActorsInRadius(const FVector& Origin, float Radius, UWorld* World);

    /** Capture single object with all properties */
    FSFPulseCapturedObject CaptureObject(AActor* Actor);

    /** Categorize actor into standard categories */
    FString CategorizeActor(AActor* Actor);

    // ==================== PROPERTY EXTRACTION ====================

    /** Extract properties common to all buildables */
    void ExtractBuildableProperties(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject);

    /** Extract factory-specific properties */
    void ExtractFactoryProperties(AFGBuildableFactory* Factory, FSFPulseCapturedObject& OutObject);

    /** Extract conveyor belt/lift properties */
    void ExtractConveyorProperties(AFGBuildableConveyorBase* Conveyor, FSFPulseCapturedObject& OutObject);

    /** Extract pipeline properties */
    void ExtractPipelineProperties(AFGBuildablePipeline* Pipeline, FSFPulseCapturedObject& OutObject);

    /** Extract distributor (splitter/merger) properties */
    void ExtractDistributorProperties(AFGBuildableConveyorAttachment* Attachment, FSFPulseCapturedObject& OutObject);

    /** Extract pipe junction properties */
    void ExtractJunctionProperties(AFGBuildablePipelineJunction* Junction, FSFPulseCapturedObject& OutObject);

    // ==================== CONNECTION EXTRACTION ====================

    /** Extract factory belt connections */
    void ExtractFactoryConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject);

    /** Extract pipe connections */
    void ExtractPipeConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject);

    /** Extract power connections */
    void ExtractPowerConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject);

    // ==================== SPLINE EXTRACTION ====================

    /** Extract spline data from component */
    void ExtractSplineData(USplineComponent* Spline, FSFPulseCapturedObject& OutObject);

    // ==================== LOGGING HELPERS ====================

    /** Log single object with full details */
    void LogObjectDetails(const FSFPulseCapturedObject& Object, int32 Index);

    /** Log summary table header */
    void LogSummaryHeader(const FString& Title);

    /** Log category summary table */
    void LogCategorySummary(const TMap<FString, int32>& CountByCategory, int32 ExtendSourceCount = 0);
};
