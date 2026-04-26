// Copyright Coffee Stain Studios. All Rights Reserved.
// Smart! Foundations Mod - Object Radar Pulse Diagnostic Service

#include "Services/RadarPulse/SFRadarPulseService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"

// Satisfactory includes
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "FGConveyorChainActor.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableWall.h"
#include "Buildables/FGBuildableWire.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"

// Hologram includes (for InspectHologram)
#include "Hologram/FGHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGPipelineHologram.h"

// Engine includes
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

USFRadarPulseService::USFRadarPulseService()
    : Subsystem(nullptr)
{
}

void USFRadarPulseService::Initialize(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Service initialized"));
}

void USFRadarPulseService::Shutdown()
{
    SnapshotCache.Empty();
    Subsystem = nullptr;
    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Service shutdown"));
}

// ==================== CAPTURE ====================

FSFRadarPulseSnapshot USFRadarPulseService::CaptureSnapshot(float Radius, const FString& Label)
{
    return CaptureSnapshotFiltered(Radius, Label, TArray<FString>());
}

FSFRadarPulseSnapshot USFRadarPulseService::CaptureSnapshotFiltered(float Radius, const FString& Label, const TArray<FString>& CategoryFilter)
{
    if (!Subsystem || !Subsystem->GetWorld())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 RadarPulse: Cannot capture - no valid world"));
        return FSFRadarPulseSnapshot();
    }

    // Get player location
    APlayerController* PC = Subsystem->GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 RadarPulse: Cannot capture - no valid player"));
        return FSFRadarPulseSnapshot();
    }

    FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
    return CaptureSnapshotAtLocationFiltered(PlayerLocation, Radius, Label, CategoryFilter);
}

FSFRadarPulseSnapshot USFRadarPulseService::CaptureSnapshotAtLocation(const FVector& Origin, float Radius, const FString& Label)
{
    return CaptureSnapshotAtLocationFiltered(Origin, Radius, Label, TArray<FString>());
}

FSFRadarPulseSnapshot USFRadarPulseService::CaptureSnapshotAtLocationFiltered(const FVector& Origin, float Radius, const FString& Label, const TArray<FString>& CategoryFilter)
{
    FSFRadarPulseSnapshot Snapshot;
    Snapshot.CaptureTime = FDateTime::Now();
    Snapshot.CaptureOrigin = Origin;
    Snapshot.CaptureRadius = Radius;
    Snapshot.SnapshotLabel = Label;

    if (!Subsystem || !Subsystem->GetWorld())
    {
        return Snapshot;
    }

    UWorld* World = Subsystem->GetWorld();

    // Get all actors in radius
    TArray<AActor*> FoundActors = GetActorsInRadius(Origin, Radius, World);

    // Build filter set for efficient lookup (empty = no filtering)
    TSet<FString> FilterSet;
    for (const FString& Cat : CategoryFilter)
    {
        FilterSet.Add(Cat);
    }
    const bool bHasFilter = FilterSet.Num() > 0;

    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Capturing %d actors within %.0fcm of (%.0f, %.0f, %.0f) [%s]%s"),
        FoundActors.Num(), Radius, Origin.X, Origin.Y, Origin.Z, *Label,
        bHasFilter ? *FString::Printf(TEXT(" (filtering to %d categories)"), FilterSet.Num()) : TEXT(""));

    // Capture each actor
    for (AActor* Actor : FoundActors)
    {
        if (!IsValid(Actor)) continue;

        // Pre-categorize to check filter before full capture
        FString Category = CategorizeActor(Actor);
        
        // Skip if not in filter (when filter is active)
        if (bHasFilter && !FilterSet.Contains(Category))
        {
            continue;
        }

        FSFPulseCapturedObject CapturedObj = CaptureObject(Actor);

        // Add to snapshot
        int32 Index = Snapshot.Objects.Num();
        Snapshot.Objects.Add(CapturedObj);
        Snapshot.ActorNameToIndex.Add(CapturedObj.ActorName, Index);

        // Update statistics
        int32& CategoryCount = Snapshot.CountByCategory.FindOrAdd(CapturedObj.Category);
        CategoryCount++;

        int32& ClassCount = Snapshot.CountByClass.FindOrAdd(CapturedObj.ClassName);
        ClassCount++;

        if (CapturedObj.bIsExtendSource)
        {
            Snapshot.ExtendSourceCount++;
        }
    }

    Snapshot.TotalObjects = Snapshot.Objects.Num();

    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Captured %d objects in snapshot '%s'"),
        Snapshot.TotalObjects, *Label);

    return Snapshot;
}

TArray<AActor*> USFRadarPulseService::GetActorsInRadius(const FVector& Origin, float Radius, UWorld* World)
{
    TArray<AActor*> Result;

    if (!World) return Result;

    // Get all buildables (main objects we care about)
    TArray<AActor*> AllBuildables;
    UGameplayStatics::GetAllActorsOfClass(World, AFGBuildable::StaticClass(), AllBuildables);

    for (AActor* Actor : AllBuildables)
    {
        if (!IsValid(Actor)) continue;

        float Distance = FVector::Dist(Origin, Actor->GetActorLocation());
        if (Distance <= Radius)
        {
            Result.Add(Actor);
        }
    }

    return Result;
}

FSFPulseCapturedObject USFRadarPulseService::CaptureObject(AActor* Actor)
{
    FSFPulseCapturedObject Captured;

    if (!IsValid(Actor)) return Captured;

    // === Identity ===
    Captured.ActorName = Actor->GetName();
    Captured.ClassName = Actor->GetClass()->GetName();
    Captured.UniqueId = Actor->GetUniqueID();
    Captured.Category = CategorizeActor(Actor);

    // === Transform ===
    Captured.Location = Actor->GetActorLocation();
    Captured.Rotation = Actor->GetActorRotation();
    Captured.Scale = Actor->GetActorScale3D();

    // Get bounds
    FVector Origin, BoxExtent;
    Actor->GetActorBounds(false, Origin, BoxExtent);
    Captured.BoundsMin = Origin - BoxExtent;
    Captured.BoundsMax = Origin + BoxExtent;

    // === State ===
    Captured.bIsHidden = Actor->IsHidden();
    Captured.bIsPendingKill = !IsValid(Actor);
    Captured.bHasBegunPlay = Actor->HasActorBegunPlay();

    // === Hierarchy ===
    if (Actor->GetOwner())
    {
        Captured.OwnerName = Actor->GetOwner()->GetName();
    }
    if (Actor->GetAttachParentActor())
    {
        Captured.ParentName = Actor->GetAttachParentActor()->GetName();
    }
    TArray<AActor*> AttachedActors;
    Actor->GetAttachedActors(AttachedActors);
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child))
        {
            Captured.ChildNames.Add(Child->GetName());
        }
    }

    // === Extract type-specific properties ===
    if (AFGBuildable* Buildable = Cast<AFGBuildable>(Actor))
    {
        ExtractBuildableProperties(Buildable, Captured);
        ExtractFactoryConnections(Buildable, Captured);
        ExtractPipeConnections(Buildable, Captured);
        ExtractPowerConnections(Buildable, Captured);

        // Type-specific extraction
        if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Buildable))
        {
            ExtractFactoryProperties(Factory, Captured);
        }
        if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Buildable))
        {
            ExtractConveyorProperties(Conveyor, Captured);
        }
        if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(Buildable))
        {
            ExtractPipelineProperties(Pipeline, Captured);
        }
        if (AFGBuildableConveyorAttachment* Attachment = Cast<AFGBuildableConveyorAttachment>(Buildable))
        {
            ExtractDistributorProperties(Attachment, Captured);
        }
        if (AFGBuildablePipelineJunction* Junction = Cast<AFGBuildablePipelineJunction>(Buildable))
        {
            ExtractJunctionProperties(Junction, Captured);
        }
    }

    return Captured;
}

FString USFRadarPulseService::CategorizeActor(AActor* Actor)
{
    if (!Actor) return TEXT("Unknown");

    // Check for stackable poles by class name (before generic checks)
    FString ClassName = Actor->GetClass()->GetName();
    if (ClassName.Contains(TEXT("PipeSupportStackable")) || ClassName.Contains(TEXT("PipelineStackable")))
    {
        return TEXT("StackablePipePole");
    }
    if (ClassName.Contains(TEXT("ConveyorPoleStackable")))
    {
        return TEXT("StackableBeltPole");
    }

    // Check specific types in order of specificity
    if (Cast<AFGBuildableConveyorBelt>(Actor)) return TEXT("Belt");
    if (Cast<AFGBuildableConveyorLift>(Actor)) return TEXT("Lift");
    if (Cast<AFGBuildableConveyorAttachment>(Actor)) return TEXT("Distributor");
    if (Cast<AFGBuildablePipeline>(Actor)) return TEXT("Pipe");
    if (Cast<AFGBuildablePipelineJunction>(Actor)) return TEXT("Junction");
    if (Cast<AFGBuildablePipelinePump>(Actor)) return TEXT("Pump");
    if (Cast<AFGBuildablePowerPole>(Actor)) return TEXT("Power");
    if (Cast<AFGBuildableWire>(Actor)) return TEXT("Wire");
    if (Cast<AFGBuildableFoundation>(Actor)) return TEXT("Foundation");
    if (Cast<AFGBuildableWall>(Actor)) return TEXT("Wall");
    if (Cast<AFGBuildableFactory>(Actor)) return TEXT("Factory");
    if (Cast<AFGBuildable>(Actor)) return TEXT("Other");

    return TEXT("Unknown");
}

// ==================== PROPERTY EXTRACTION ====================

void USFRadarPulseService::ExtractBuildableProperties(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject)
{
    if (!Buildable) return;

    // Add generic buildable properties
    OutObject.Properties.Add(TEXT("BuildableClass"), Buildable->GetClass()->GetPathName());
}

void USFRadarPulseService::ExtractFactoryProperties(AFGBuildableFactory* Factory, FSFPulseCapturedObject& OutObject)
{
    if (!Factory) return;

    // Note: GetCurrentRecipe and GetPowerConsumption are on AFGBuildableManufacturer, not AFGBuildableFactory
    // For now, we extract the basic factory properties that are available
    OutObject.bIsProducing = Factory->IsProducing();
    OutObject.bIsPaused = Factory->IsProductionPaused();
    OutObject.ProductionProgress = Factory->GetProductionProgress();

    OutObject.Properties.Add(TEXT("IsProducing"), OutObject.bIsProducing ? TEXT("true") : TEXT("false"));
    OutObject.Properties.Add(TEXT("IsPaused"), OutObject.bIsPaused ? TEXT("true") : TEXT("false"));
    OutObject.Properties.Add(TEXT("ProductionProgress"), FString::Printf(TEXT("%.2f"), OutObject.ProductionProgress));
}

void USFRadarPulseService::ExtractConveyorProperties(AFGBuildableConveyorBase* Conveyor, FSFPulseCapturedObject& OutObject)
{
    if (!Conveyor) return;

    // === Chain Actor Information ===
    AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
    if (ChainActor)
    {
        OutObject.Properties.Add(TEXT("ChainActorName"), ChainActor->GetName());
        OutObject.Properties.Add(TEXT("ChainActorPtr"), FString::Printf(TEXT("0x%p"), ChainActor));
        OutObject.Properties.Add(TEXT("ChainNumSegments"), FString::FromInt(ChainActor->GetNumChainSegments()));
    }
    else
    {
        OutObject.Properties.Add(TEXT("ChainActorName"), TEXT("NULL"));
        OutObject.Properties.Add(TEXT("ChainActorPtr"), TEXT("0x0"));
    }

    // Belt-specific
    if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Conveyor))
    {
        OutObject.BeltSpeed = Belt->GetSpeed();
        OutObject.Properties.Add(TEXT("BeltSpeed"), FString::Printf(TEXT("%.1f"), OutObject.BeltSpeed));

        // Extract spline data
        if (USplineComponent* Spline = Belt->GetSplineComponent())
        {
            ExtractSplineData(Spline, OutObject);
        }
    }

    // Lift-specific
    if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Conveyor))
    {
        OutObject.LiftHeight = Lift->GetHeight();
        OutObject.bLiftIsReversed = Lift->GetIsReversed();
        OutObject.Properties.Add(TEXT("LiftHeight"), FString::Printf(TEXT("%.1f"), OutObject.LiftHeight));
        OutObject.Properties.Add(TEXT("IsReversed"), OutObject.bLiftIsReversed ? TEXT("true") : TEXT("false"));

        // Get connection locations
        TArray<UFGFactoryConnectionComponent*> LiftConns;
        Lift->GetComponents<UFGFactoryConnectionComponent>(LiftConns);
        for (UFGFactoryConnectionComponent* Conn : LiftConns)
        {
            if (Conn && Conn->GetName().Contains(TEXT("0")))
            {
                OutObject.LiftBottomLocation = Conn->GetComponentLocation();
            }
            else if (Conn)
            {
                OutObject.LiftTopLocation = Conn->GetComponentLocation();
            }
        }
    }
}

void USFRadarPulseService::ExtractPipelineProperties(AFGBuildablePipeline* Pipeline, FSFPulseCapturedObject& OutObject)
{
    if (!Pipeline) return;

    // Extract spline data
    if (USplineComponent* Spline = Pipeline->GetSplineComponent())
    {
        ExtractSplineData(Spline, OutObject);
    }

    OutObject.Properties.Add(TEXT("SplineLength"), FString::Printf(TEXT("%.1f"), OutObject.SplineLength));
}

void USFRadarPulseService::ExtractDistributorProperties(AFGBuildableConveyorAttachment* Attachment, FSFPulseCapturedObject& OutObject)
{
    if (!Attachment) return;

    // Determine if splitter or merger based on connection count
    int32 InputCount = 0;
    int32 OutputCount = 0;
    for (const FSFPulseCapturedConnection& Conn : OutObject.FactoryConnections)
    {
        if (Conn.ConnectionDirection == 0) InputCount++;
        else OutputCount++;
    }

    OutObject.Properties.Add(TEXT("InputCount"), FString::FromInt(InputCount));
    OutObject.Properties.Add(TEXT("OutputCount"), FString::FromInt(OutputCount));
    OutObject.Properties.Add(TEXT("DistributorType"), InputCount > OutputCount ? TEXT("Merger") : TEXT("Splitter"));
}

void USFRadarPulseService::ExtractJunctionProperties(AFGBuildablePipelineJunction* Junction, FSFPulseCapturedObject& OutObject)
{
    if (!Junction) return;

    int32 ConnectedCount = 0;
    for (const FSFPulseCapturedConnection& Conn : OutObject.PipeConnections)
    {
        if (Conn.bIsConnected) ConnectedCount++;
    }

    OutObject.Properties.Add(TEXT("TotalConnections"), FString::FromInt(OutObject.PipeConnections.Num()));
    OutObject.Properties.Add(TEXT("ConnectedCount"), FString::FromInt(ConnectedCount));
}

// ==================== CONNECTION EXTRACTION ====================

void USFRadarPulseService::ExtractFactoryConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject)
{
    if (!Buildable) return;

    TArray<UFGFactoryConnectionComponent*> FactoryConns;
    Buildable->GetComponents<UFGFactoryConnectionComponent>(FactoryConns);

    for (UFGFactoryConnectionComponent* Conn : FactoryConns)
    {
        if (!Conn) continue;

        FSFPulseCapturedConnection CapturedConn;
        CapturedConn.ConnectorName = Conn->GetName();
        CapturedConn.WorldLocation = Conn->GetComponentLocation();
        CapturedConn.Direction = Conn->GetForwardVector();  // Use component forward vector for direction
        CapturedConn.ConnectionDirection = (int32)Conn->GetDirection();
        CapturedConn.bIsConnected = Conn->IsConnected();

        if (Conn->IsConnected())
        {
            UFGFactoryConnectionComponent* OtherConn = Conn->GetConnection();
            if (OtherConn && OtherConn->GetOwner())
            {
                CapturedConn.ConnectedToActor = OtherConn->GetOwner()->GetName();
                CapturedConn.ConnectedToConnector = OtherConn->GetName();
            }
        }

        OutObject.FactoryConnections.Add(CapturedConn);
    }
}

void USFRadarPulseService::ExtractPipeConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject)
{
    if (!Buildable) return;

    TArray<UFGPipeConnectionComponent*> PipeConns;
    Buildable->GetComponents<UFGPipeConnectionComponent>(PipeConns);

    for (UFGPipeConnectionComponent* Conn : PipeConns)
    {
        if (!Conn) continue;

        FSFPulseCapturedConnection CapturedConn;
        CapturedConn.ConnectorName = Conn->GetName();
        CapturedConn.WorldLocation = Conn->GetComponentLocation();
        CapturedConn.PipeConnectionType = (int32)Conn->GetPipeConnectionType();
        CapturedConn.bIsConnected = Conn->IsConnected();

        if (Conn->IsConnected())
        {
            UFGPipeConnectionComponentBase* OtherConnBase = Conn->GetConnection();
            if (OtherConnBase && OtherConnBase->GetOwner())
            {
                CapturedConn.ConnectedToActor = OtherConnBase->GetOwner()->GetName();
                CapturedConn.ConnectedToConnector = OtherConnBase->GetName();
            }
        }

        OutObject.PipeConnections.Add(CapturedConn);
    }
}

void USFRadarPulseService::ExtractPowerConnections(AFGBuildable* Buildable, FSFPulseCapturedObject& OutObject)
{
    if (!Buildable) return;

    TArray<UFGPowerConnectionComponent*> PowerConns;
    Buildable->GetComponents<UFGPowerConnectionComponent>(PowerConns);

    for (UFGPowerConnectionComponent* Conn : PowerConns)
    {
        if (!Conn) continue;

        FSFPulseCapturedConnection CapturedConn;
        CapturedConn.ConnectorName = Conn->GetName();
        CapturedConn.WorldLocation = Conn->GetComponentLocation();
        CapturedConn.bIsConnected = Conn->GetNumConnections() > 0;

        OutObject.PowerConnections.Add(CapturedConn);
    }
}

// ==================== SPLINE EXTRACTION ====================

void USFRadarPulseService::ExtractSplineData(USplineComponent* Spline, FSFPulseCapturedObject& OutObject)
{
    if (!Spline) return;

    OutObject.SplineLength = Spline->GetSplineLength();

    int32 NumPoints = Spline->GetNumberOfSplinePoints();
    for (int32 i = 0; i < NumPoints; ++i)
    {
        FSFPulseCapturedSplinePoint Point;
        Point.PointIndex = i;
        Point.LocalPosition = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        Point.WorldPosition = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
        Point.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
        Point.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
        Point.Rotation = Spline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);

        OutObject.SplinePoints.Add(Point);
    }
}

// ==================== SOURCE FLAGGING ====================

void USFRadarPulseService::FlagExtendSourceObjects(
    FSFRadarPulseSnapshot& Snapshot,
    const FSFExtendTopology& Topology,
    AActor* SourceFactory)
{
    if (!SourceFactory) return;

    // Build lookup map of topology members
    TMap<FString, TPair<FString, int32>> TopologyLookup; // ActorName -> (Role, ChainId)

    // Add source factory
    TopologyLookup.Add(SourceFactory->GetName(), TPair<FString, int32>(TEXT("SourceFactory"), -1));

    // Add input belt chains
    for (int32 ChainIdx = 0; ChainIdx < Topology.InputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = Topology.InputChains[ChainIdx];

        if (Chain.Distributor.IsValid())
        {
            TopologyLookup.Add(Chain.Distributor->GetName(), TPair<FString, int32>(TEXT("Splitter"), ChainIdx));
        }

        for (int32 i = 0; i < Chain.Conveyors.Num(); ++i)
        {
            if (Chain.Conveyors[i].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[i].Get();
                FString Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("InputLift") : TEXT("InputBelt");
                TopologyLookup.Add(Conv->GetName(), TPair<FString, int32>(Role, ChainIdx));
            }
        }

        for (int32 i = 0; i < Chain.SupportPoles.Num(); ++i)
        {
            if (Chain.SupportPoles[i].IsValid())
            {
                TopologyLookup.Add(Chain.SupportPoles[i]->GetName(), TPair<FString, int32>(TEXT("Pole"), ChainIdx));
            }
        }
    }

    // Add output belt chains
    for (int32 ChainIdx = 0; ChainIdx < Topology.OutputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = Topology.OutputChains[ChainIdx];

        if (Chain.Distributor.IsValid())
        {
            TopologyLookup.Add(Chain.Distributor->GetName(), TPair<FString, int32>(TEXT("Merger"), ChainIdx));
        }

        for (int32 i = 0; i < Chain.Conveyors.Num(); ++i)
        {
            if (Chain.Conveyors[i].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[i].Get();
                FString Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("OutputLift") : TEXT("OutputBelt");
                TopologyLookup.Add(Conv->GetName(), TPair<FString, int32>(Role, ChainIdx));
            }
        }

        for (int32 i = 0; i < Chain.SupportPoles.Num(); ++i)
        {
            if (Chain.SupportPoles[i].IsValid())
            {
                TopologyLookup.Add(Chain.SupportPoles[i]->GetName(), TPair<FString, int32>(TEXT("Pole"), ChainIdx));
            }
        }
    }

    // Add input pipe chains
    for (int32 ChainIdx = 0; ChainIdx < Topology.PipeInputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = Topology.PipeInputChains[ChainIdx];

        if (Chain.Junction.IsValid())
        {
            TopologyLookup.Add(Chain.Junction->GetName(), TPair<FString, int32>(TEXT("Junction"), ChainIdx));
        }

        for (int32 i = 0; i < Chain.Pipelines.Num(); ++i)
        {
            if (Chain.Pipelines[i].IsValid())
            {
                TopologyLookup.Add(Chain.Pipelines[i]->GetName(), TPair<FString, int32>(TEXT("InputPipe"), ChainIdx));
            }
        }

        for (int32 i = 0; i < Chain.SupportPoles.Num(); ++i)
        {
            if (Chain.SupportPoles[i].IsValid())
            {
                TopologyLookup.Add(Chain.SupportPoles[i]->GetName(), TPair<FString, int32>(TEXT("PipeSupport"), ChainIdx));
            }
        }
    }

    // Add output pipe chains
    for (int32 ChainIdx = 0; ChainIdx < Topology.PipeOutputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = Topology.PipeOutputChains[ChainIdx];

        if (Chain.Junction.IsValid())
        {
            TopologyLookup.Add(Chain.Junction->GetName(), TPair<FString, int32>(TEXT("Junction"), ChainIdx));
        }

        for (int32 i = 0; i < Chain.Pipelines.Num(); ++i)
        {
            if (Chain.Pipelines[i].IsValid())
            {
                TopologyLookup.Add(Chain.Pipelines[i]->GetName(), TPair<FString, int32>(TEXT("OutputPipe"), ChainIdx));
            }
        }

        for (int32 i = 0; i < Chain.SupportPoles.Num(); ++i)
        {
            if (Chain.SupportPoles[i].IsValid())
            {
                TopologyLookup.Add(Chain.SupportPoles[i]->GetName(), TPair<FString, int32>(TEXT("PipeSupport"), ChainIdx));
            }
        }
    }

    // Flag objects in snapshot
    Snapshot.ExtendSourceCount = 0;
    for (FSFPulseCapturedObject& Obj : Snapshot.Objects)
    {
        if (TPair<FString, int32>* Found = TopologyLookup.Find(Obj.ActorName))
        {
            Obj.bIsExtendSource = true;
            Obj.ExtendRole = Found->Key;
            Obj.ExtendChainId = Found->Value;
            Obj.SystemFlags.AddUnique(TEXT("ExtendSource"));
            Snapshot.ExtendSourceCount++;
        }
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Flagged %d EXTEND source objects"), Snapshot.ExtendSourceCount);
}

void USFRadarPulseService::FlagObjectsMatching(
    FSFRadarPulseSnapshot& Snapshot,
    const TArray<AActor*>& ActorsToFlag,
    const FString& FlagName,
    const FString& Role)
{
    // Build name lookup
    TSet<FString> ActorNames;
    for (AActor* Actor : ActorsToFlag)
    {
        if (IsValid(Actor))
        {
            ActorNames.Add(Actor->GetName());
        }
    }

    // Flag objects
    int32 FlaggedCount = 0;
    for (FSFPulseCapturedObject& Obj : Snapshot.Objects)
    {
        if (ActorNames.Contains(Obj.ActorName))
        {
            Obj.SystemFlags.AddUnique(FlagName);
            if (!Role.IsEmpty())
            {
                Obj.Properties.Add(FlagName + TEXT("_Role"), Role);
            }
            FlaggedCount++;
        }
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Flagged %d objects with '%s'"), FlaggedCount, *FlagName);
}

// ==================== COMPARISON ====================

FSFSnapshotDiff USFRadarPulseService::CompareSnapshots(const FSFRadarPulseSnapshot& Before, const FSFRadarPulseSnapshot& After)
{
    FSFSnapshotDiff Diff;
    Diff.BeforeLabel = Before.SnapshotLabel;
    Diff.AfterLabel = After.SnapshotLabel;
    Diff.BeforeTotal = Before.TotalObjects;
    Diff.AfterTotal = After.TotalObjects;

    // Build set of "Before" actor names
    TSet<FString> BeforeNames;
    for (const FSFPulseCapturedObject& Obj : Before.Objects)
    {
        BeforeNames.Add(Obj.ActorName);
    }

    // Build set of "After" actor names
    TSet<FString> AfterNames;
    for (const FSFPulseCapturedObject& Obj : After.Objects)
    {
        AfterNames.Add(Obj.ActorName);
    }

    // Find NEW objects (in After but not Before)
    for (const FSFPulseCapturedObject& Obj : After.Objects)
    {
        if (!BeforeNames.Contains(Obj.ActorName))
        {
            Diff.NewObjects.Add(Obj);
            int32& Count = Diff.NewByCategory.FindOrAdd(Obj.Category);
            Count++;
        }
    }

    // Find REMOVED objects (in Before but not After)
    for (const FSFPulseCapturedObject& Obj : Before.Objects)
    {
        if (!AfterNames.Contains(Obj.ActorName))
        {
            Diff.RemovedObjects.Add(Obj);
            int32& Count = Diff.RemovedByCategory.FindOrAdd(Obj.Category);
            Count++;
        }
    }

    // Find MODIFIED objects (exist in both but changed)
    for (const FSFPulseCapturedObject& AfterObj : After.Objects)
    {
        if (BeforeNames.Contains(AfterObj.ActorName))
        {
            const FSFPulseCapturedObject* BeforeObj = Before.FindByActorName(AfterObj.ActorName);
            if (BeforeObj)
            {
                // Check for modifications (simplified - just check transform and connections)
                TArray<FString> Changes;

                if (!BeforeObj->Location.Equals(AfterObj.Location, 0.1f))
                    Changes.Add(TEXT("Location"));
                if (!BeforeObj->Rotation.Equals(AfterObj.Rotation, 0.1f))
                    Changes.Add(TEXT("Rotation"));
                if (BeforeObj->bIsHidden != AfterObj.bIsHidden)
                    Changes.Add(TEXT("Hidden"));
                if (BeforeObj->FactoryConnections.Num() != AfterObj.FactoryConnections.Num())
                    Changes.Add(TEXT("FactoryConnections"));
                if (BeforeObj->PipeConnections.Num() != AfterObj.PipeConnections.Num())
                    Changes.Add(TEXT("PipeConnections"));

                // Check connection status changes
                for (int32 i = 0; i < FMath::Min(BeforeObj->FactoryConnections.Num(), AfterObj.FactoryConnections.Num()); ++i)
                {
                    if (BeforeObj->FactoryConnections[i].bIsConnected != AfterObj.FactoryConnections[i].bIsConnected)
                    {
                        Changes.AddUnique(TEXT("ConnectionStatus"));
                        break;
                    }
                }

                if (Changes.Num() > 0)
                {
                    FSFObjectModification Mod;
                    Mod.ActorName = AfterObj.ActorName;
                    Mod.Before = *BeforeObj;
                    Mod.After = AfterObj;
                    Mod.ChangedProperties = Changes;
                    Diff.ModifiedObjects.Add(Mod);

                    int32& Count = Diff.ModifiedByCategory.FindOrAdd(AfterObj.Category);
                    Count++;
                }
            }
        }
    }

    return Diff;
}

// ==================== CACHING ====================

void USFRadarPulseService::CacheSnapshot(const FString& Key, const FSFRadarPulseSnapshot& Snapshot)
{
    SnapshotCache.Add(Key, Snapshot);
    UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Cached snapshot '%s' (%d objects)"), *Key, Snapshot.TotalObjects);
}

bool USFRadarPulseService::GetCachedSnapshot(const FString& Key, FSFRadarPulseSnapshot& OutSnapshot)
{
    if (FSFRadarPulseSnapshot* Found = SnapshotCache.Find(Key))
    {
        OutSnapshot = *Found;
        return true;
    }
    return false;
}

void USFRadarPulseService::ClearCache(const FString& Key)
{
    if (Key.IsEmpty())
    {
        int32 Count = SnapshotCache.Num();
        SnapshotCache.Empty();
        UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Cleared all %d cached snapshots"), Count);
    }
    else
    {
        if (SnapshotCache.Remove(Key) > 0)
        {
            UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: Cleared cached snapshot '%s'"), *Key);
        }
    }
}

// ==================== HOLOGRAM INSPECTION ====================

void USFRadarPulseService::InspectHologram(AFGHologram* Hologram, const FString& DebugLabel)
{
    if (!IsValid(Hologram))
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 RadarPulse: InspectHologram called with invalid hologram"));
        return;
    }

    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════════════════"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ 📡 HOLOGRAM INSPECTION: %s"), *DebugLabel);
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Name: %s"), *Hologram->GetName());
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Class: %s"), *Hologram->GetClass()->GetName());
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Location: %s"), *Hologram->GetActorLocation().ToString());
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Rotation: %s"), *Hologram->GetActorRotation().ToString());
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Hidden: %d | TickEnabled: %d | BegunPlay: %d"),
        Hologram->IsHidden(), Hologram->PrimaryActorTick.bCanEverTick, Hologram->HasActorBegunPlay());
    
    // Get material state
    EHologramMaterialState MatState = Hologram->GetHologramMaterialState();
    UE_LOG(LogSmartFoundations, Display, TEXT("║ MaterialState: %d (0=OK, 1=Warning, 2=Error)"), (int32)MatState);
    
    // Note: mValidPlacementMaterial and mInvalidPlacementMaterial are protected members
    // We can inspect them indirectly through the materials applied to mesh components
    
    // Check if it's a spline hologram (belt/pipe)
    if (AFGSplineHologram* SplineHologram = Cast<AFGSplineHologram>(Hologram))
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ SPLINE HOLOGRAM DATA ═══"));
        
        // Find spline component via FindComponentByClass (mSplineComponent is protected)
        if (USplineComponent* SplineComp = Hologram->FindComponentByClass<USplineComponent>())
        {
            int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
            float SplineLength = SplineComp->GetSplineLength();
            UE_LOG(LogSmartFoundations, Display, TEXT("║ SplineComponent: %d points, Length=%.1f cm"), NumPoints, SplineLength);
            UE_LOG(LogSmartFoundations, Display, TEXT("║ SplineComponent World Location: %s"), *SplineComp->GetComponentLocation().ToString());
            
            // Log each spline point
            for (int32 i = 0; i < NumPoints; i++)
            {
                FVector PointLocation = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
                FVector WorldLocation = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                FVector ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
                FVector LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
                
                UE_LOG(LogSmartFoundations, Display, TEXT("║   Point[%d] Local=%s World=%s"),
                    i, *PointLocation.ToString(), *WorldLocation.ToString());
                UE_LOG(LogSmartFoundations, Display, TEXT("║            ArriveTangent=%s LeaveTangent=%s"),
                    *ArriveTangent.ToString(), *LeaveTangent.ToString());
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║ SplineComponent: NULL (not found via FindComponentByClass)"));
        }
        
        // Check for belt hologram specific data
        if (AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(SplineHologram))
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ BELT HOLOGRAM SPECIFIC ═══"));
            
            UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClass: %s"),
                BeltHologram->GetBuildClass() ? *BeltHologram->GetBuildClass()->GetName() : TEXT("NULL"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClassPath: %s"),
                BeltHologram->GetBuildClass() ? *BeltHologram->GetBuildClass()->GetPathName() : TEXT("NULL"));
            
            // Check for snapped connections
            UE_LOG(LogSmartFoundations, Display, TEXT("║ IsHologramLocked: %d"), BeltHologram->IsHologramLocked() ? 1 : 0);
        }
        
        // Check for pipeline hologram specific data
        if (AFGPipelineHologram* PipeHologram = Cast<AFGPipelineHologram>(SplineHologram))
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ PIPE HOLOGRAM SPECIFIC ═══"));
            
            UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClass: %s"),
                PipeHologram->GetBuildClass() ? *PipeHologram->GetBuildClass()->GetName() : TEXT("NULL"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClassPath: %s"),
                PipeHologram->GetBuildClass() ? *PipeHologram->GetBuildClass()->GetPathName() : TEXT("NULL"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ IsHologramLocked: %d"), PipeHologram->IsHologramLocked() ? 1 : 0);
        }
        
        // CRITICAL: Log SplineMeshComponents and their meshes - this is what we need for EXTEND
        TArray<USplineMeshComponent*> SplineMeshes;
        Hologram->GetComponents<USplineMeshComponent>(SplineMeshes);
        if (SplineMeshes.Num() > 0)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
            UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ SPLINE MESH COMPONENTS (%d) ═══"), SplineMeshes.Num());
            for (int32 i = 0; i < SplineMeshes.Num(); i++)
            {
                USplineMeshComponent* MeshComp = SplineMeshes[i];
                if (MeshComp)
                {
                    UStaticMesh* Mesh = MeshComp->GetStaticMesh();
                    FString MeshPath = Mesh ? Mesh->GetPathName() : TEXT("NULL");
                    FString MeshName = Mesh ? Mesh->GetName() : TEXT("NULL");
                    
                    UE_LOG(LogSmartFoundations, Display, TEXT("║   [%d] Visible=%d Mesh=%s"), 
                        i, MeshComp->IsVisible() ? 1 : 0, *MeshName);
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       MeshPath: %s"), *MeshPath);
                    
                    // Log material info with full paths and material instance details
                    int32 NumMaterials = MeshComp->GetNumMaterials();
                    for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
                    {
                        UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
                        UE_LOG(LogSmartFoundations, Display, TEXT("║       Material[%d]: %s"), 
                            MatIdx, Mat ? *Mat->GetName() : TEXT("NULL"));
                        if (Mat)
                        {
                            UE_LOG(LogSmartFoundations, Display, TEXT("║           Path: %s"), *Mat->GetPathName());
                            UE_LOG(LogSmartFoundations, Display, TEXT("║           Class: %s"), *Mat->GetClass()->GetName());
                            
                            // Check if it's a material instance (dynamic or constant)
                            if (UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Mat))
                            {
                                UE_LOG(LogSmartFoundations, Display, TEXT("║           IsDynamic: YES"));
                                if (DynMat->Parent)
                                {
                                    UE_LOG(LogSmartFoundations, Display, TEXT("║           Parent: %s"), *DynMat->Parent->GetPathName());
                                }
                            }
                            else if (UMaterialInstanceConstant* ConstMat = Cast<UMaterialInstanceConstant>(Mat))
                            {
                                UE_LOG(LogSmartFoundations, Display, TEXT("║           IsConstant: YES"));
                                if (ConstMat->Parent)
                                {
                                    UE_LOG(LogSmartFoundations, Display, TEXT("║           Parent: %s"), *ConstMat->Parent->GetPathName());
                                }
                            }
                            
                            // Get base material for animation info
                            UMaterial* BaseMat = Mat->GetMaterial();
                            if (BaseMat)
                            {
                                UE_LOG(LogSmartFoundations, Display, TEXT("║           BaseMaterial: %s"), *BaseMat->GetPathName());
                            }
                        }
                    }
                    
                    // Log forward axis (important for belt direction)
                    ESplineMeshAxis::Type ForwardAxis = MeshComp->GetForwardAxis();
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       ForwardAxis: %d (0=X, 1=Y, 2=Z)"), (int32)ForwardAxis);
                    
                    // Log spline mesh geometry - use individual getters
                    FVector StartPos = MeshComp->GetStartPosition();
                    FVector EndPos = MeshComp->GetEndPosition();
                    FVector StartTangent = MeshComp->GetStartTangent();
                    FVector EndTangent = MeshComp->GetEndTangent();
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       Start=%s End=%s"), 
                        *StartPos.ToString(), *EndPos.ToString());
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       StartTangent=%s EndTangent=%s"), 
                        *StartTangent.ToString(), *EndTangent.ToString());
                    
                    // Log render settings (custom depth stencil for hologram effect)
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       RenderCustomDepth=%d StencilValue=%d StencilWriteMask=%d"),
                        MeshComp->bRenderCustomDepth ? 1 : 0,
                        MeshComp->CustomDepthStencilValue,
                        (int32)MeshComp->CustomDepthStencilWriteMask);
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       HiddenInGame=%d CastShadow=%d"),
                        MeshComp->bHiddenInGame ? 1 : 0,
                        MeshComp->CastShadow ? 1 : 0);
                }
            }
        }
    }
    
    // Check for lift hologram specific data
    if (AFGConveyorLiftHologram* LiftHologram = Cast<AFGConveyorLiftHologram>(Hologram))
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ LIFT HOLOGRAM SPECIFIC ═══"));
        
        UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClass: %s"),
            LiftHologram->GetBuildClass() ? *LiftHologram->GetBuildClass()->GetName() : TEXT("NULL"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ BuildClassPath: %s"),
            LiftHologram->GetBuildClass() ? *LiftHologram->GetBuildClass()->GetPathName() : TEXT("NULL"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ IsHologramLocked: %d"), LiftHologram->IsHologramLocked() ? 1 : 0);
        
        // Note: Lift height and other properties are protected, but we can inspect components
    }
    
    // ALL STATIC MESH COMPONENTS - captures lift meshes and any other static meshes
    TArray<UStaticMeshComponent*> StaticMeshes;
    Hologram->GetComponents<UStaticMeshComponent>(StaticMeshes);
    if (StaticMeshes.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ ALL STATIC MESH COMPONENTS (%d) ═══"), StaticMeshes.Num());
        for (int32 i = 0; i < StaticMeshes.Num(); i++)
        {
            UStaticMeshComponent* MeshComp = StaticMeshes[i];
            if (MeshComp)
            {
                UStaticMesh* Mesh = MeshComp->GetStaticMesh();
                FString MeshPath = Mesh ? Mesh->GetPathName() : TEXT("NULL");
                FString MeshName = Mesh ? Mesh->GetName() : TEXT("NULL");
                FString CompName = MeshComp->GetName();
                FString CompClass = MeshComp->GetClass()->GetName();
                
                UE_LOG(LogSmartFoundations, Display, TEXT("║   [%d] %s (%s)"), i, *CompName, *CompClass);
                UE_LOG(LogSmartFoundations, Display, TEXT("║       Visible=%d Mesh=%s"), 
                    MeshComp->IsVisible() ? 1 : 0, *MeshName);
                UE_LOG(LogSmartFoundations, Display, TEXT("║       MeshPath: %s"), *MeshPath);
                UE_LOG(LogSmartFoundations, Display, TEXT("║       RelativeLocation: %s"), *MeshComp->GetRelativeLocation().ToString());
                UE_LOG(LogSmartFoundations, Display, TEXT("║       WorldLocation: %s"), *MeshComp->GetComponentLocation().ToString());
                
                // Log materials with paths
                int32 NumMaterials = MeshComp->GetNumMaterials();
                for (int32 MatIdx = 0; MatIdx < NumMaterials && MatIdx < 3; MatIdx++)
                {
                    UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       Material[%d]: %s"), 
                        MatIdx, Mat ? *Mat->GetName() : TEXT("NULL"));
                    if (Mat)
                    {
                        UE_LOG(LogSmartFoundations, Display, TEXT("║           Path: %s"), *Mat->GetPathName());
                    }
                }
                if (NumMaterials > 3)
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("║       ... and %d more materials"), NumMaterials - 3);
                }
                
                // Log render settings (custom depth stencil for hologram effect)
                UE_LOG(LogSmartFoundations, Display, TEXT("║       RenderCustomDepth=%d StencilValue=%d StencilWriteMask=%d"),
                    MeshComp->bRenderCustomDepth ? 1 : 0,
                    MeshComp->CustomDepthStencilValue,
                    (int32)MeshComp->CustomDepthStencilWriteMask);
                UE_LOG(LogSmartFoundations, Display, TEXT("║       HiddenInGame=%d CastShadow=%d"),
                    MeshComp->bHiddenInGame ? 1 : 0,
                    MeshComp->CastShadow ? 1 : 0);
            }
        }
    }
    
    // ALL SCENE COMPONENTS - for understanding hierarchy
    TArray<USceneComponent*> AllSceneComps;
    Hologram->GetComponents<USceneComponent>(AllSceneComps);
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ COMPONENT HIERARCHY (%d scene components) ═══"), AllSceneComps.Num());
    for (USceneComponent* Comp : AllSceneComps)
    {
        if (Comp)
        {
            FString ParentName = Comp->GetAttachParent() ? Comp->GetAttachParent()->GetName() : TEXT("ROOT");
            UE_LOG(LogSmartFoundations, Display, TEXT("║   %s (%s) -> Parent: %s"),
                *Comp->GetName(), *Comp->GetClass()->GetName(), *ParentName);
        }
    }
    
    // Hologram children (mChildren array)
    const TArray<AFGHologram*>& HologramChildren = Hologram->GetHologramChildren();
    if (HologramChildren.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ HOLOGRAM CHILDREN (mChildren: %d) ═══"), HologramChildren.Num());
        for (int32 i = 0; i < HologramChildren.Num(); i++)
        {
            AFGHologram* Child = HologramChildren[i];
            if (IsValid(Child))
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║   [%d] %s (%s) at %s"),
                    i, *Child->GetName(), *Child->GetClass()->GetName(), *Child->GetActorLocation().ToString());
                UE_LOG(LogSmartFoundations, Display, TEXT("║       MaterialState: %d | Hidden: %d"),
                    (int32)Child->GetHologramMaterialState(), Child->IsHidden() ? 1 : 0);
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║   [%d] INVALID/NULL"), i);
            }
        }
    }
    
    // Attached actors (different from mChildren)
    TArray<AActor*> AttachedActors;
    Hologram->GetAttachedActors(AttachedActors);
    if (AttachedActors.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ ATTACHED ACTORS (%d) ═══"), AttachedActors.Num());
        for (AActor* Child : AttachedActors)
        {
            if (IsValid(Child))
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║   %s (%s) at %s"),
                    *Child->GetName(), *Child->GetClass()->GetName(), *Child->GetActorLocation().ToString());
            }
        }
    }
    
    // Actor tags
    if (Hologram->Tags.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════════════════"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ ACTOR TAGS (%d) ═══"), Hologram->Tags.Num());
        for (const FName& Tag : Hologram->Tags)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║   %s"), *Tag.ToString());
        }
    }
    
    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════════════════"));
    UE_LOG(LogSmartFoundations, Display, TEXT(""));
}

void USFRadarPulseService::InspectAllHologramsInRadius(float Radius)
{
    if (!Subsystem || !Subsystem->GetWorld())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 RadarPulse: Cannot scan - no valid world"));
        return;
    }

    UWorld* World = Subsystem->GetWorld();
    
    // Get player location
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 RadarPulse: Cannot scan - no valid player"));
        return;
    }

    FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
    
    // Get all holograms in range
    TArray<AActor*> AllHolograms;
    UGameplayStatics::GetAllActorsOfClass(World, AFGHologram::StaticClass(), AllHolograms);
    
    // Filter by distance
    TArray<AFGHologram*> NearbyHolograms;
    for (AActor* Actor : AllHolograms)
    {
        if (AFGHologram* Hologram = Cast<AFGHologram>(Actor))
        {
            if (IsValid(Hologram))
            {
                float Distance = FVector::Dist(PlayerLocation, Hologram->GetActorLocation());
                if (Distance <= Radius)
                {
                    NearbyHolograms.Add(Hologram);
                }
            }
        }
    }

    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════════════════"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ 📡 HOLOGRAM RADIUS SCAN - Found %d holograms within %.0fm"), NearbyHolograms.Num(), Radius / 100.0f);
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Player Location: %s"), *PlayerLocation.ToString());
    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════════════════"));

    if (NearbyHolograms.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("  No holograms found in range."));
        return;
    }

    // Sort by distance for cleaner output
    NearbyHolograms.Sort([&PlayerLocation](const AFGHologram& A, const AFGHologram& B)
    {
        return FVector::Dist(PlayerLocation, A.GetActorLocation()) < FVector::Dist(PlayerLocation, B.GetActorLocation());
    });

    // Inspect each hologram
    int32 Index = 0;
    for (AFGHologram* Hologram : NearbyHolograms)
    {
        float Distance = FVector::Dist(PlayerLocation, Hologram->GetActorLocation());
        FString Label = FString::Printf(TEXT("Hologram %d/%d (%.1fm away)"), ++Index, NearbyHolograms.Num(), Distance / 100.0f);
        InspectHologram(Hologram, Label);
    }

    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("📡 Hologram scan complete - inspected %d holograms"), NearbyHolograms.Num());
}

// ==================== LOGGING ====================

void USFRadarPulseService::LogSnapshot(const FSFRadarPulseSnapshot& Snapshot, bool bVerbose)
{
    LogSnapshotFiltered(Snapshot, bVerbose, TArray<FString>());
}

void USFRadarPulseService::LogSnapshotFiltered(const FSFRadarPulseSnapshot& Snapshot, bool bVerbose, const TArray<FString>& EnumerateCategories)
{
    LogSummaryHeader(FString::Printf(TEXT("RADAR PULSE SNAPSHOT: %s"), *Snapshot.SnapshotLabel));

    UE_LOG(LogSmartFoundations, Display, TEXT("║ Capture Time: %s"), *Snapshot.CaptureTime.ToString());
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Origin: X=%.0f Y=%.0f Z=%.0f"),
        Snapshot.CaptureOrigin.X, Snapshot.CaptureOrigin.Y, Snapshot.CaptureOrigin.Z);
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Radius: %.0fm (%.0f cm)"),
        Snapshot.CaptureRadius / 100.0f, Snapshot.CaptureRadius);
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Total Objects: %d"), Snapshot.TotalObjects);
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));

    LogCategorySummary(Snapshot.CountByCategory, Snapshot.ExtendSourceCount);

    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

    if (bVerbose)
    {
        // Build filter set for efficient lookup (empty = no filtering)
        TSet<FString> FilterSet;
        for (const FString& Cat : EnumerateCategories)
        {
            FilterSet.Add(Cat);
        }
        const bool bHasFilter = FilterSet.Num() > 0;

        // Count objects to enumerate
        int32 EnumerateCount = 0;
        if (bHasFilter)
        {
            for (const FSFPulseCapturedObject& Obj : Snapshot.Objects)
            {
                if (FilterSet.Contains(Obj.Category)) EnumerateCount++;
            }
        }
        else
        {
            EnumerateCount = Snapshot.Objects.Num();
        }

        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        if (bHasFilter)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║       ENUMERATION: FILTERED OBJECTS (%d of %d total)          ║"), 
                EnumerateCount, Snapshot.TotalObjects);
        }
        else
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║            FULL ENUMERATION: ALL OBJECTS (%d total)            ║"), Snapshot.TotalObjects);
        }
        UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

        int32 DisplayIndex = 0;
        for (int32 i = 0; i < Snapshot.Objects.Num(); ++i)
        {
            const FSFPulseCapturedObject& Obj = Snapshot.Objects[i];
            
            // Skip if not in filter (when filter is active)
            if (bHasFilter && !FilterSet.Contains(Obj.Category))
            {
                continue;
            }

            LogObjectDetails(Obj, DisplayIndex++);
        }
    }
}

void USFRadarPulseService::LogDiff(const FSFSnapshotDiff& Diff, bool bVerbose)
{
    LogDiffFiltered(Diff, bVerbose, TArray<FString>());
}

void USFRadarPulseService::LogDiffFiltered(const FSFSnapshotDiff& Diff, bool bVerbose, const TArray<FString>& EnumerateCategories)
{
    LogSummaryHeader(TEXT("RADAR PULSE DIFF"));

    UE_LOG(LogSmartFoundations, Display, TEXT("║ Before: %s (%d objects)"), *Diff.BeforeLabel, Diff.BeforeTotal);
    UE_LOG(LogSmartFoundations, Display, TEXT("║ After:  %s (%d objects)"), *Diff.AfterLabel, Diff.AfterTotal);
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Category        │ Before │ After  │ New    │ Removed │ Modified  ║"));
    UE_LOG(LogSmartFoundations, Display, TEXT("╟─────────────────┼────────┼────────┼────────┼─────────┼───────────╢"));

    // Collect all categories
    TSet<FString> AllCategories;
    for (const auto& Pair : Diff.NewByCategory) AllCategories.Add(Pair.Key);
    for (const auto& Pair : Diff.RemovedByCategory) AllCategories.Add(Pair.Key);
    for (const auto& Pair : Diff.ModifiedByCategory) AllCategories.Add(Pair.Key);

    for (const FString& Category : AllCategories)
    {
        int32 NewCount = Diff.NewByCategory.Contains(Category) ? Diff.NewByCategory[Category] : 0;
        int32 RemovedCount = Diff.RemovedByCategory.Contains(Category) ? Diff.RemovedByCategory[Category] : 0;
        int32 ModifiedCount = Diff.ModifiedByCategory.Contains(Category) ? Diff.ModifiedByCategory[Category] : 0;

        UE_LOG(LogSmartFoundations, Display, TEXT("║ %-15s │ %6s │ %6s │ %6d │ %7d │ %9d ║"),
            *Category, TEXT("-"), TEXT("-"), NewCount, RemovedCount, ModifiedCount);
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ TOTAL           │ %6d │ %6d │ %6d │ %7d │ %9d ║"),
        Diff.BeforeTotal, Diff.AfterTotal, Diff.NewObjects.Num(), Diff.RemovedObjects.Num(), Diff.ModifiedObjects.Num());
    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

    // Build filter set for enumeration (empty = no filtering)
    TSet<FString> FilterSet;
    for (const FString& Cat : EnumerateCategories)
    {
        FilterSet.Add(Cat);
    }
    const bool bHasFilter = FilterSet.Num() > 0;

    // Lambda to check if object passes filter
    auto PassesFilter = [&](const FSFPulseCapturedObject& Obj) -> bool
    {
        return !bHasFilter || FilterSet.Contains(Obj.Category);
    };

    if (bVerbose && Diff.NewObjects.Num() > 0)
    {
        // Count filtered objects
        int32 FilteredCount = 0;
        for (const auto& Obj : Diff.NewObjects) { if (PassesFilter(Obj)) FilteredCount++; }

        if (FilteredCount > 0)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT(""));
            UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
            if (bHasFilter)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║       ENUMERATION: NEW OBJECTS (%d of %d filtered)           ║"), FilteredCount, Diff.NewObjects.Num());
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║            FULL ENUMERATION: NEW OBJECTS (%d total)            ║"), Diff.NewObjects.Num());
            }
            UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

            int32 DisplayIndex = 0;
            for (int32 i = 0; i < Diff.NewObjects.Num(); ++i)
            {
                if (PassesFilter(Diff.NewObjects[i]))
                {
                    LogObjectDetails(Diff.NewObjects[i], DisplayIndex++);
                }
            }
        }
    }

    if (bVerbose && Diff.RemovedObjects.Num() > 0)
    {
        // Count filtered objects
        int32 FilteredCount = 0;
        for (const auto& Obj : Diff.RemovedObjects) { if (PassesFilter(Obj)) FilteredCount++; }

        if (FilteredCount > 0)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT(""));
            UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
            if (bHasFilter)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║     ENUMERATION: REMOVED OBJECTS (%d of %d filtered)          ║"), FilteredCount, Diff.RemovedObjects.Num());
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("║          FULL ENUMERATION: REMOVED OBJECTS (%d total)          ║"), Diff.RemovedObjects.Num());
            }
            UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

            int32 DisplayIndex = 0;
            for (int32 i = 0; i < Diff.RemovedObjects.Num(); ++i)
            {
                if (PassesFilter(Diff.RemovedObjects[i]))
                {
                    LogObjectDetails(Diff.RemovedObjects[i], DisplayIndex++);
                }
            }
        }
    }

    if (bVerbose && Diff.ModifiedObjects.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║         FULL ENUMERATION: MODIFIED OBJECTS (%d total)          ║"), Diff.ModifiedObjects.Num());
        UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

        for (int32 i = 0; i < Diff.ModifiedObjects.Num(); ++i)
        {
            const FSFObjectModification& Mod = Diff.ModifiedObjects[i];
            // Note: Modified objects don't store Category, so we can't filter them
            UE_LOG(LogSmartFoundations, Display, TEXT(""));
            UE_LOG(LogSmartFoundations, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));
            UE_LOG(LogSmartFoundations, Display, TEXT("│ [%d] %s (MODIFIED)"), i, *Mod.ActorName);
            UE_LOG(LogSmartFoundations, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Changed: %s"), *FString::Join(Mod.ChangedProperties, TEXT(", ")));
            UE_LOG(LogSmartFoundations, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
        }
    }
}

void USFRadarPulseService::LogFlaggedObjects(const FSFRadarPulseSnapshot& Snapshot, const FString& FlagName, bool bVerbose)
{
    TArray<const FSFPulseCapturedObject*> FlaggedObjects;

    for (const FSFPulseCapturedObject& Obj : Snapshot.Objects)
    {
        bool bIsFlagged = false;

        if (FlagName == TEXT("ExtendSource"))
        {
            bIsFlagged = Obj.bIsExtendSource;
        }
        else
        {
            bIsFlagged = Obj.SystemFlags.Contains(FlagName);
        }

        if (bIsFlagged)
        {
            FlaggedObjects.Add(&Obj);
        }
    }

    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    LogSummaryHeader(FString::Printf(TEXT("FLAGGED OBJECTS: %s (%d)"), *FlagName, FlaggedObjects.Num()));

    // Group by role if EXTEND
    if (FlagName == TEXT("ExtendSource"))
    {
        TMap<FString, int32> ByRole;
        for (const FSFPulseCapturedObject* Obj : FlaggedObjects)
        {
            int32& Count = ByRole.FindOrAdd(Obj->ExtendRole);
            Count++;
        }

        for (const auto& Pair : ByRole)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║   %-20s: %3d"), *Pair.Key, Pair.Value);
        }
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

    if (bVerbose)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("📊 %s DETAILS:"), *FlagName.ToUpper());

        for (int32 i = 0; i < FlaggedObjects.Num(); ++i)
        {
            LogObjectDetails(*FlaggedObjects[i], i);
        }
    }
}

void USFRadarPulseService::LogObjectDetails(const FSFPulseCapturedObject& Object, int32 Index)
{
    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));

    // Header with EXTEND badge if applicable
    if (Object.bIsExtendSource)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ [%d] %s ★ [%s]"), Index, *Object.ActorName, *Object.ExtendRole);
    }
    else
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ [%d] %s"), Index, *Object.ActorName);
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
    UE_LOG(LogSmartFoundations, Display, TEXT("│ Category: %s | Class: %s"), *Object.Category, *Object.ClassName);
    UE_LOG(LogSmartFoundations, Display, TEXT("│ Location: X=%.3f Y=%.3f Z=%.3f"), Object.Location.X, Object.Location.Y, Object.Location.Z);
    UE_LOG(LogSmartFoundations, Display, TEXT("│ Rotation: P=%.3f Y=%.3f R=%.3f"), Object.Rotation.Pitch, Object.Rotation.Yaw, Object.Rotation.Roll);
    UE_LOG(LogSmartFoundations, Display, TEXT("│ Scale: X=%.3f Y=%.3f Z=%.3f"), Object.Scale.X, Object.Scale.Y, Object.Scale.Z);
    UE_LOG(LogSmartFoundations, Display, TEXT("│ Bounds: Min=(%.1f,%.1f,%.1f) Max=(%.1f,%.1f,%.1f)"),
        Object.BoundsMin.X, Object.BoundsMin.Y, Object.BoundsMin.Z,
        Object.BoundsMax.X, Object.BoundsMax.Y, Object.BoundsMax.Z);
    UE_LOG(LogSmartFoundations, Display, TEXT("│ State: Hidden=%d PendingKill=%d BegunPlay=%d"),
        Object.bIsHidden, Object.bIsPendingKill, Object.bHasBegunPlay);

    // Factory connections
    if (Object.FactoryConnections.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ FACTORY CONNECTIONS (%d) ═══"), Object.FactoryConnections.Num());
        for (const FSFPulseCapturedConnection& Conn : Object.FactoryConnections)
        {
            if (Conn.bIsConnected)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) -> %s.%s"),
                    *Conn.ConnectorName, Conn.ConnectionDirection,
                    Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                    *Conn.ConnectedToActor, *Conn.ConnectedToConnector);
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) (not connected)"),
                    *Conn.ConnectorName, Conn.ConnectionDirection,
                    Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z);
            }
        }
    }

    // Pipe connections
    if (Object.PipeConnections.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE CONNECTIONS (%d) ═══"), Object.PipeConnections.Num());
        for (const FSFPulseCapturedConnection& Conn : Object.PipeConnections)
        {
            if (Conn.bIsConnected)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) -> %s.%s"),
                    *Conn.ConnectorName, Conn.PipeConnectionType,
                    Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                    *Conn.ConnectedToActor, *Conn.ConnectedToConnector);
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) (not connected)"),
                    *Conn.ConnectorName, Conn.PipeConnectionType,
                    Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z);
            }
        }
    }

    // Belt data
    if (Object.Category == TEXT("Belt") && Object.SplinePoints.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ BELT DATA ═══"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ Speed: %.1f | SplineLength: %.1fcm | SplinePoints: %d"),
            Object.BeltSpeed, Object.SplineLength, Object.SplinePoints.Num());
        for (const FSFPulseCapturedSplinePoint& SP : Object.SplinePoints)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                SP.PointIndex, SP.LocalPosition.X, SP.LocalPosition.Y, SP.LocalPosition.Z,
                SP.WorldPosition.X, SP.WorldPosition.Y, SP.WorldPosition.Z);
        }
    }

    // Lift data
    if (Object.Category == TEXT("Lift"))
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ LIFT DATA ═══"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ Height: %.1fcm | Reversed: %d"), Object.LiftHeight, Object.bLiftIsReversed);
        UE_LOG(LogSmartFoundations, Display, TEXT("│ Bottom: (%.1f,%.1f,%.1f) Top: (%.1f,%.1f,%.1f)"),
            Object.LiftBottomLocation.X, Object.LiftBottomLocation.Y, Object.LiftBottomLocation.Z,
            Object.LiftTopLocation.X, Object.LiftTopLocation.Y, Object.LiftTopLocation.Z);
    }

    // Pipe data
    if (Object.Category == TEXT("Pipe") && Object.SplinePoints.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE DATA ═══"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ SplineLength: %.1fcm | SplinePoints: %d"), Object.SplineLength, Object.SplinePoints.Num());
        for (const FSFPulseCapturedSplinePoint& SP : Object.SplinePoints)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                SP.PointIndex, SP.LocalPosition.X, SP.LocalPosition.Y, SP.LocalPosition.Z,
                SP.WorldPosition.X, SP.WorldPosition.Y, SP.WorldPosition.Z);
        }
    }

    // Factory data
    if (Object.Category == TEXT("Factory") && !Object.CurrentRecipe.IsEmpty())
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ FACTORY DATA ═══"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ Recipe: %s | Progress: %.1f%% | Producing: %d | Paused: %d"),
            *Object.CurrentRecipe, Object.ProductionProgress * 100.0f, Object.bIsProducing, Object.bIsPaused);
    }

    // Stackable pole data
    if (Object.Category == TEXT("StackablePipePole") || Object.Category == TEXT("StackableBeltPole"))
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ STACKABLE POLE DATA ═══"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ Type: %s"), 
            Object.Category == TEXT("StackablePipePole") ? TEXT("Pipeline Support") : TEXT("Conveyor Pole"));
        UE_LOG(LogSmartFoundations, Display, TEXT("│ PipeConnections: %d | FactoryConnections: %d"),
            Object.PipeConnections.Num(), Object.FactoryConnections.Num());
    }

    // Custom properties
    if (Object.Properties.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PROPERTIES (%d) ═══"), Object.Properties.Num());
        for (const auto& Pair : Object.Properties)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("│   %s: %s"), *Pair.Key, *Pair.Value);
        }
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
}

void USFRadarPulseService::LogSummaryHeader(const FString& Title)
{
    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ %s"), *Title.Left(65).RightPad(65));
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
}

void USFRadarPulseService::LogCategorySummary(const TMap<FString, int32>& CountByCategory, int32 ExtendSourceCount)
{
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Category        │ Count  │ ExtendSource │"));
    UE_LOG(LogSmartFoundations, Display, TEXT("╟─────────────────┼────────┼──────────────┼"));

    int32 Total = 0;
    for (const auto& Pair : CountByCategory)
    {
        Total += Pair.Value;
        UE_LOG(LogSmartFoundations, Display, TEXT("║ %-15s │ %6d │              │"), *Pair.Key, Pair.Value);
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("╟─────────────────┼────────┼──────────────┼"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ TOTAL           │ %6d │ %12d │"), Total, ExtendSourceCount);
}

// ==================== CHAIN ACTOR SCANNING ====================

void USFRadarPulseService::ScanChainActors(float Radius)
{
    if (!Subsystem || !Subsystem->GetWorld())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 ScanChainActors: Cannot scan - no valid world"));
        return;
    }

    UWorld* World = Subsystem->GetWorld();
    
    // Get player location
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📡 ScanChainActors: Cannot scan - no valid player"));
        return;
    }
    
    FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
    
    // Find all conveyors (belts and lifts) in radius
    TArray<AFGBuildableConveyorBase*> Conveyors;
    TArray<AActor*> AllConveyors;
    UGameplayStatics::GetAllActorsOfClass(World, AFGBuildableConveyorBase::StaticClass(), AllConveyors);
    
    for (AActor* Actor : AllConveyors)
    {
        if (!IsValid(Actor)) continue;
        float Distance = FVector::Dist(PlayerLocation, Actor->GetActorLocation());
        if (Distance <= Radius)
        {
            if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Actor))
            {
                Conveyors.Add(Conveyor);
            }
        }
    }
    
    // Collect unique chain actors and their members
    TMap<AFGConveyorChainActor*, TArray<AFGBuildableConveyorBase*>> ChainToConveyors;
    int32 NullChainCount = 0;
    
    for (AFGBuildableConveyorBase* Conveyor : Conveyors)
    {
        AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
        if (ChainActor)
        {
            ChainToConveyors.FindOrAdd(ChainActor).Add(Conveyor);
        }
        else
        {
            NullChainCount++;
        }
    }
    
    // Log results
    LogSummaryHeader(FString::Printf(TEXT("CHAIN ACTOR SCAN (%.0fm radius)"), Radius / 100.0f));
    
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Found %d conveyors, %d unique chain actors, %d with NULL chain"),
        Conveyors.Num(), ChainToConveyors.Num(), NullChainCount);
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    
    int32 ChainIndex = 0;
    for (auto& Pair : ChainToConveyors)
    {
        AFGConveyorChainActor* Chain = Pair.Key;
        TArray<AFGBuildableConveyorBase*>& Members = Pair.Value;
        
        UE_LOG(LogSmartFoundations, Display, TEXT("║"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ CHAIN[%d]: %s (ptr=0x%p) ═══"), 
            ChainIndex, *Chain->GetName(), Chain);
        UE_LOG(LogSmartFoundations, Display, TEXT("║   Members in radius: %d, Total segments: %d"), 
            Members.Num(), Chain->GetNumChainSegments());
        
        // Log each member with connection info
        for (AFGBuildableConveyorBase* Member : Members)
        {
            UFGFactoryConnectionComponent* Conn0 = Member->GetConnection0();
            UFGFactoryConnectionComponent* Conn1 = Member->GetConnection1();
            
            FString Conn0Info = TEXT("unconnected");
            FString Conn1Info = TEXT("unconnected");
            
            if (Conn0 && Conn0->IsConnected())
            {
                UFGFactoryConnectionComponent* Connected = Cast<UFGFactoryConnectionComponent>(Conn0->GetConnection());
                if (Connected && Connected->GetOwner())
                {
                    Conn0Info = FString::Printf(TEXT("→ %s.%s"), *Connected->GetOwner()->GetName(), *Connected->GetName());
                }
                else
                {
                    Conn0Info = TEXT("→ (connected but null ref)");
                }
            }
            
            if (Conn1 && Conn1->IsConnected())
            {
                UFGFactoryConnectionComponent* Connected = Cast<UFGFactoryConnectionComponent>(Conn1->GetConnection());
                if (Connected && Connected->GetOwner())
                {
                    Conn1Info = FString::Printf(TEXT("→ %s.%s"), *Connected->GetOwner()->GetName(), *Connected->GetName());
                }
                else
                {
                    Conn1Info = TEXT("→ (connected but null ref)");
                }
            }
            
            // Determine type
            FString TypeStr = TEXT("Belt");
            if (Cast<AFGBuildableConveyorLift>(Member))
            {
                TypeStr = TEXT("Lift");
            }
            
            UE_LOG(LogSmartFoundations, Display, TEXT("║   • %s %s"),
                *TypeStr, *Member->GetName());
            UE_LOG(LogSmartFoundations, Display, TEXT("║       Conn0: %s"), *Conn0Info);
            UE_LOG(LogSmartFoundations, Display, TEXT("║       Conn1: %s"), *Conn1Info);
        }
        
        ChainIndex++;
    }
    
    // Log conveyors with NULL chain actors
    if (NullChainCount > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("║"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║ ═══ CONVEYORS WITH NULL CHAIN ACTOR (%d) ═══"), NullChainCount);
        for (AFGBuildableConveyorBase* Conveyor : Conveyors)
        {
            if (!Conveyor->GetConveyorChainActor())
            {
                FString TypeStr = Cast<AFGBuildableConveyorLift>(Conveyor) ? TEXT("Lift") : TEXT("Belt");
                UE_LOG(LogSmartFoundations, Display, TEXT("║   • %s %s"), *TypeStr, *Conveyor->GetName());
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
}
