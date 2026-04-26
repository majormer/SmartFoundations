// Copyright Coffee Stain Studios. All Rights Reserved.
// Smart! Foundations Mod - Object Radar Pulse Snapshot Data Structures

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFRadarPulseSnapshot.generated.h"

/**
 * Captured connection data for belts/pipes/power
 */
USTRUCT(BlueprintType)
struct FSFPulseCapturedConnection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString ConnectorName;

    UPROPERTY(BlueprintReadOnly)
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector Direction = FVector::ZeroVector;

    /** 0=Input, 1=Output, 2=Any/Bidirectional */
    UPROPERTY(BlueprintReadOnly)
    int32 ConnectionDirection = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bIsConnected = false;

    UPROPERTY(BlueprintReadOnly)
    FString ConnectedToActor;

    UPROPERTY(BlueprintReadOnly)
    FString ConnectedToConnector;

    /** For pipe connections - 0=Any, 1=Consumer, 2=Producer */
    UPROPERTY(BlueprintReadOnly)
    int32 PipeConnectionType = 0;

    FSFPulseCapturedConnection() = default;
};

/**
 * Captured spline point data
 */
USTRUCT(BlueprintType)
struct FSFPulseCapturedSplinePoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PointIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector ArriveTangent = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector LeaveTangent = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation = FRotator::ZeroRotator;

    FSFPulseCapturedSplinePoint() = default;
};

/**
 * Complete property capture for a single object
 * Captures ALL available data about an actor
 */
USTRUCT(BlueprintType)
struct FSFPulseCapturedObject
{
    GENERATED_BODY()

    // === Identity ===
    UPROPERTY(BlueprintReadOnly)
    FString ActorName;

    UPROPERTY(BlueprintReadOnly)
    FString ClassName;

    UPROPERTY(BlueprintReadOnly)
    FString Category;  // Factory, Belt, Pipe, Distributor, Junction, Power, Foundation, Wall, Other

    UPROPERTY(BlueprintReadOnly)
    int32 UniqueId = 0;  // For tracking across snapshots

    // === Transform ===
    UPROPERTY(BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly)
    FVector Scale = FVector::OneVector;

    UPROPERTY(BlueprintReadOnly)
    FVector BoundsMin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector BoundsMax = FVector::ZeroVector;

    // === State ===
    UPROPERTY(BlueprintReadOnly)
    bool bIsHidden = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsPendingKill = false;

    UPROPERTY(BlueprintReadOnly)
    bool bHasBegunPlay = false;

    // === Hierarchy ===
    UPROPERTY(BlueprintReadOnly)
    FString OwnerName;

    UPROPERTY(BlueprintReadOnly)
    FString ParentName;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> ChildNames;

    // === Connections ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedConnection> FactoryConnections;

    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedConnection> PipeConnections;

    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedConnection> PowerConnections;

    // === Spline Data (Belts/Pipes) ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedSplinePoint> SplinePoints;

    UPROPERTY(BlueprintReadOnly)
    float SplineLength = 0.0f;

    // === Belt-specific ===
    UPROPERTY(BlueprintReadOnly)
    float BeltSpeed = 0.0f;

    // === Lift-specific ===
    UPROPERTY(BlueprintReadOnly)
    float LiftHeight = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bLiftIsReversed = false;

    UPROPERTY(BlueprintReadOnly)
    FVector LiftTopLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector LiftBottomLocation = FVector::ZeroVector;

    // === Factory-specific ===
    UPROPERTY(BlueprintReadOnly)
    FString CurrentRecipe;

    UPROPERTY(BlueprintReadOnly)
    float ProductionProgress = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsProducing = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsPaused = false;

    UPROPERTY(BlueprintReadOnly)
    float PowerConsumption = 0.0f;

    // === Custom Properties (class-specific, key-value pairs) ===
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, FString> Properties;

    // === Source System Flags ===
    /** Is this object flagged as part of a source system? */
    UPROPERTY(BlueprintReadOnly)
    bool bIsExtendSource = false;

    /** Role in the EXTEND topology */
    UPROPERTY(BlueprintReadOnly)
    FString ExtendRole;

    /** Chain ID for EXTEND (-1 if not part of chain) */
    UPROPERTY(BlueprintReadOnly)
    int32 ExtendChainId = -1;

    /** Index within EXTEND chain (-1 if not applicable) */
    UPROPERTY(BlueprintReadOnly)
    int32 ExtendChainIndex = -1;

    /** Generic flag names this object is marked with */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> SystemFlags;

    FSFPulseCapturedObject() = default;
};

/**
 * Complete snapshot of all objects at a point in time
 */
USTRUCT(BlueprintType)
struct FSFRadarPulseSnapshot
{
    GENERATED_BODY()

    // === Metadata ===
    UPROPERTY(BlueprintReadOnly)
    FDateTime CaptureTime;

    UPROPERTY(BlueprintReadOnly)
    FVector CaptureOrigin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float CaptureRadius = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FString SnapshotLabel;

    // === All captured objects ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedObject> Objects;

    // === Statistics ===
    UPROPERTY(BlueprintReadOnly)
    int32 TotalObjects = 0;

    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> CountByCategory;

    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> CountByClass;

    // === Source System Counts ===
    UPROPERTY(BlueprintReadOnly)
    int32 ExtendSourceCount = 0;

    /** Quick lookup by actor name (not serialized) */
    TMap<FString, int32> ActorNameToIndex;

    FSFRadarPulseSnapshot() = default;

    /** Find object by actor name, returns nullptr if not found */
    const FSFPulseCapturedObject* FindByActorName(const FString& Name) const
    {
        const int32* IndexPtr = ActorNameToIndex.Find(Name);
        if (IndexPtr && Objects.IsValidIndex(*IndexPtr))
        {
            return &Objects[*IndexPtr];
        }
        return nullptr;
    }
};

/**
 * Modification tracking for a single object between snapshots
 */
USTRUCT(BlueprintType)
struct FSFObjectModification
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString ActorName;

    UPROPERTY(BlueprintReadOnly)
    FSFPulseCapturedObject Before;

    UPROPERTY(BlueprintReadOnly)
    FSFPulseCapturedObject After;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> ChangedProperties;

    FSFObjectModification() = default;
};

/**
 * Comparison result between two snapshots
 */
USTRUCT(BlueprintType)
struct FSFSnapshotDiff
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString BeforeLabel;

    UPROPERTY(BlueprintReadOnly)
    FString AfterLabel;

    UPROPERTY(BlueprintReadOnly)
    int32 BeforeTotal = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 AfterTotal = 0;

    // === New objects (in After but not Before) ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedObject> NewObjects;

    // === Removed objects (in Before but not After) ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFPulseCapturedObject> RemovedObjects;

    // === Modified objects (exist in both but changed) ===
    UPROPERTY(BlueprintReadOnly)
    TArray<FSFObjectModification> ModifiedObjects;

    // === Summary counts by category ===
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> NewByCategory;

    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> RemovedByCategory;

    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> ModifiedByCategory;

    FSFSnapshotDiff() = default;
};
