// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendDiagnosticsService - EXTEND Before/After Snapshot Diagnostics
 *
 * Captures world-state snapshots of nearby buildables and logs a before/after diff
 * around an EXTEND build, for debugging the clone/wiring pipeline. This is a pure
 * diagnostic concern: it is on no gameplay path and changes no build behavior.
 *
 * Extracted verbatim from SFExtendService (T1 decomposition, 2026-05-30). The owning
 * USFExtendService keeps thin forwarders (CapturePreviewSnapshot /
 * CapturePostBuildSnapshotAndLogDiff) so existing call sites are unchanged.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFExtendDiagnosticsService.generated.h"

class AFGBuildable;
class USFSubsystem;
class USFExtendService;

// ==================== Diagnostic Capture ====================

/**
 * Captured connection data
 */
USTRUCT()
struct FSFCapturedConnection
{
    GENERATED_BODY()

    FString ConnectorName;
    FString ConnectorClass;
    FVector WorldLocation = FVector::ZeroVector;
    FRotator WorldRotation = FRotator::ZeroRotator;
    int32 Direction = 0;  // EFactoryConnectionDirection
    bool bIsConnected = false;
    FString ConnectedToActor;
    FString ConnectedToConnector;

    FSFCapturedConnection() = default;
};

/**
 * Captured spline point data
 */
USTRUCT()
struct FSFCapturedSplinePoint
{
    GENERATED_BODY()

    int32 Index = 0;
    FVector Location = FVector::ZeroVector;      // Local space
    FVector WorldLocation = FVector::ZeroVector;  // World space
    FVector ArriveTangent = FVector::ZeroVector;
    FVector LeaveTangent = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;

    FSFCapturedSplinePoint() = default;
};

/**
 * Captured buildable data for diagnostic comparison
 * Used to compare world state before/after EXTEND build
 * COMPREHENSIVE - captures ALL available data
 */
USTRUCT()
struct FSFCapturedBuildable
{
    GENERATED_BODY()

    // === Identity ===
    FString Name;
    FString ClassName;
    FString Category;

    // === Transform ===
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    FVector BoundsMin = FVector::ZeroVector;
    FVector BoundsMax = FVector::ZeroVector;

    // === State ===
    bool bIsHidden = false;
    bool bIsPendingKill = false;
    bool bHasBegunPlay = false;

    // === Connections (Belts/Distributors/Factories) ===
    TArray<FSFCapturedConnection> FactoryConnections;

    // === Pipe Connections ===
    TArray<FSFCapturedConnection> PipeConnections;

    // === Spline Data (Belts/Pipes) ===
    TArray<FSFCapturedSplinePoint> SplinePoints;
    float SplineLength = 0.0f;
    int32 SplinePointCount = 0;

    // === Belt-specific ===
    float BeltSpeed = 0.0f;

    // === Lift-specific ===
    float LiftHeight = 0.0f;
    bool bLiftIsReversed = false;
    FVector LiftTopLocation = FVector::ZeroVector;
    FVector LiftBottomLocation = FVector::ZeroVector;

    // === All Properties (raw dump) ===
    TArray<FString> AllProperties;

    // === EXTEND Topology Flags ===
    /** Is this buildable part of the source EXTEND topology? */
    bool bIsExtendSource = false;

    /** Specific role in the EXTEND topology (if applicable) */
    FString ExtendRole;  // "SourceFactory", "InputBelt", "OutputBelt", "InputLift", "OutputLift",
                         // "InputPipe", "OutputPipe", "Splitter", "Merger", "Junction",
                         // "Pole" (future), "LiftFloorHole" (future), "PipelinePump" (future)

    /** Chain ID this buildable belongs to (-1 if not part of a chain) */
    int32 ExtendChainId = -1;

    /** Index within the chain (-1 if not applicable) */
    int32 ExtendChainIndex = -1;

    FSFCapturedBuildable() = default;
};

/**
 * Snapshot of all buildables within capture radius
 */
USTRUCT()
struct FSFBuildableSnapshot
{
    GENERATED_BODY()

    /** All captured buildables */
    UPROPERTY()
    TArray<FSFCapturedBuildable> Buildables;

    /** Capture location (player position) */
    UPROPERTY()
    FVector CaptureLocation = FVector::ZeroVector;

    /** Capture radius used */
    UPROPERTY()
    float CaptureRadius = 0.0f;

    /** Timestamp of capture */
    UPROPERTY()
    FDateTime CaptureTime;

    /** Count by category */
    TMap<FString, int32> CountByCategory;

    FSFBuildableSnapshot() = default;
};

/**
 * Service that owns the EXTEND before/after diagnostic snapshots. Reads the current
 * extend target and topology from the parent USFExtendService; owns no gameplay state.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendDiagnosticsService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendDiagnosticsService();

    /** Initialize with owning subsystem and extend service references */
    void Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService);

    /** Shutdown service */
    void Shutdown();

    /** Capture all buildables within radius of player for diagnostic comparison */
    FSFBuildableSnapshot CaptureNearbyBuildables(float Radius = 15000.0f);  // 150m default

    /** Capture the "before" snapshot when preview phase starts */
    void CapturePreviewSnapshot();

    /** Capture the "after" snapshot and log diff when all builds complete */
    void CapturePostBuildSnapshotAndLogDiff();

    /** Log comparison between two snapshots */
    void LogSnapshotDiff(const FSFBuildableSnapshot& Before, const FSFBuildableSnapshot& After);

    /** Whether a preview snapshot has been captured (guards re-capture in CreateBeltPreviews) */
    bool HasPreviewSnapshot() const { return bHasPreviewSnapshot; }

private:
    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Parent extend service (source of current target + topology) */
    UPROPERTY()
    TObjectPtr<USFExtendService> ExtendService = nullptr;

    /** Snapshot captured during preview phase */
    FSFBuildableSnapshot PreviewSnapshot;

    /** Whether preview snapshot has been captured */
    bool bHasPreviewSnapshot = false;
};
