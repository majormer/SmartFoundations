// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendDiagnosticsService Implementation
 *
 * See SFExtendDiagnosticsService.h for overview. Extracted verbatim from
 * SFExtendService (T1 decomposition); the only changes from the original bodies are
 * member-access rebindings through the parent USFExtendService (current target +
 * topology), which are behavior-identical.
 */

#include "Features/Extend/SFExtendDiagnosticsService.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendTypes.h"
#include "Subsystem/SFSubsystem.h"
#include "SmartFoundations.h"  // For LogSmartExtend
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

USFExtendDiagnosticsService::USFExtendDiagnosticsService()
{
}

void USFExtendDiagnosticsService::Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService)
{
    Subsystem = InSubsystem;
    ExtendService = InExtendService;
}

void USFExtendDiagnosticsService::Shutdown()
{
    ExtendService = nullptr;
    Subsystem.Reset();
    PreviewSnapshot = FSFBuildableSnapshot();
    bHasPreviewSnapshot = false;
}

// ==================== Diagnostic Capture ====================

FSFBuildableSnapshot USFExtendDiagnosticsService::CaptureNearbyBuildables(float Radius)
{
    FSFBuildableSnapshot Snapshot;
    Snapshot.CaptureRadius = Radius;
    Snapshot.CaptureTime = FDateTime::Now();

    if (!Subsystem.IsValid())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("📊 DIAG: Cannot capture - no subsystem"));
        return Snapshot;
    }

    if (!ExtendService)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("📊 DIAG: Cannot capture - no extend service"));
        return Snapshot;
    }

    // Get player location
    APlayerController* PC = Subsystem->GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("📊 DIAG: Cannot capture - no player"));
        return Snapshot;
    }

    FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

    // ==================== Build EXTEND Topology Lookup ====================
    // Maps buildable actor -> (Role, ChainId, ChainIndex)
    struct FExtendTopologyInfo {
        FString Role;
        int32 ChainId = -1;
        int32 ChainIndex = -1;
    };
    TMap<AActor*, FExtendTopologyInfo> TopologyLookup;

    const FSFExtendTopology& Topology = ExtendService->GetCurrentTopology();

    // Add source factory
    if (AFGBuildable* SourceTarget = ExtendService->GetCurrentTarget())
    {
        FExtendTopologyInfo Info;
        Info.Role = TEXT("SourceFactory");
        TopologyLookup.Add(SourceTarget, Info);
    }

    // Add input belt chain members (conveyors = belts + lifts, poles, and distributors)
    for (int32 ChainIdx = 0; ChainIdx < Topology.InputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = Topology.InputChains[ChainIdx];

        // Add distributor (splitter) at chain end
        if (Chain.Distributor.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Splitter");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Distributor.Get(), Info);
        }

        // Add all conveyors (belts and lifts) in chain
        for (int32 ConvIdx = 0; ConvIdx < Chain.Conveyors.Num(); ++ConvIdx)
        {
            if (Chain.Conveyors[ConvIdx].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[ConvIdx].Get();
                FExtendTopologyInfo Info;
                // Distinguish belt from lift
                Info.Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("InputLift") : TEXT("InputBelt");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = ConvIdx;
                TopologyLookup.Add(Conv, Info);
            }
        }

        // Add all support poles in chain (future cloning candidate)
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("Pole");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }

    // Add output belt chain members
    for (int32 ChainIdx = 0; ChainIdx < Topology.OutputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = Topology.OutputChains[ChainIdx];

        // Add distributor (merger) at chain end
        if (Chain.Distributor.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Merger");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Distributor.Get(), Info);
        }

        // Add all conveyors (belts and lifts) in chain
        for (int32 ConvIdx = 0; ConvIdx < Chain.Conveyors.Num(); ++ConvIdx)
        {
            if (Chain.Conveyors[ConvIdx].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[ConvIdx].Get();
                FExtendTopologyInfo Info;
                Info.Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("OutputLift") : TEXT("OutputBelt");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = ConvIdx;
                TopologyLookup.Add(Conv, Info);
            }
        }

        // Add all support poles in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("Pole");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }

    // Add input pipe chain members
    for (int32 ChainIdx = 0; ChainIdx < Topology.PipeInputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = Topology.PipeInputChains[ChainIdx];

        // Add junction at chain end
        if (Chain.Junction.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Junction");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Junction.Get(), Info);
        }

        // Add all pipes in chain
        for (int32 PipeIdx = 0; PipeIdx < Chain.Pipelines.Num(); ++PipeIdx)
        {
            if (Chain.Pipelines[PipeIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("InputPipe");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PipeIdx;
                TopologyLookup.Add(Chain.Pipelines[PipeIdx].Get(), Info);
            }
        }

        // Add all support poles (pipe supports) in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("PipeSupport");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }

    // Add output pipe chain members
    for (int32 ChainIdx = 0; ChainIdx < Topology.PipeOutputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = Topology.PipeOutputChains[ChainIdx];

        // Add junction at chain end
        if (Chain.Junction.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Junction");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Junction.Get(), Info);
        }

        // Add all pipes in chain
        for (int32 PipeIdx = 0; PipeIdx < Chain.Pipelines.Num(); ++PipeIdx)
        {
            if (Chain.Pipelines[PipeIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("OutputPipe");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PipeIdx;
                TopologyLookup.Add(Chain.Pipelines[PipeIdx].Get(), Info);
            }
        }

        // Add all support poles (pipe supports) in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("PipeSupport");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("📊 DIAG: Built topology lookup with %d members"), TopologyLookup.Num());
    Snapshot.CaptureLocation = PlayerLocation;

    // Find all buildables in radius
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(Subsystem->GetWorld(), AFGBuildable::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AFGBuildable* Buildable = Cast<AFGBuildable>(Actor);
        if (!Buildable) continue;

        float Distance = FVector::Dist(PlayerLocation, Buildable->GetActorLocation());
        if (Distance > Radius) continue;

        FSFCapturedBuildable Captured;

        // === Identity ===
        Captured.Name = Buildable->GetName();
        Captured.ClassName = Buildable->GetClass()->GetName();

        // === Transform ===
        Captured.Location = Buildable->GetActorLocation();
        Captured.Rotation = Buildable->GetActorRotation();
        Captured.Scale = Buildable->GetActorScale3D();

        // Get bounds
        FVector Origin, BoxExtent;
        Buildable->GetActorBounds(false, Origin, BoxExtent);
        Captured.BoundsMin = Origin - BoxExtent;
        Captured.BoundsMax = Origin + BoxExtent;

        // === State ===
        Captured.bIsHidden = Buildable->IsHidden();
        Captured.bIsPendingKill = !IsValid(Buildable);
        Captured.bHasBegunPlay = Buildable->HasActorBegunPlay();

        // === Capture Factory Connections ===
        TArray<UFGFactoryConnectionComponent*> FactoryConns;
        Buildable->GetComponents<UFGFactoryConnectionComponent>(FactoryConns);
        for (UFGFactoryConnectionComponent* Conn : FactoryConns)
        {
            if (!Conn) continue;

            FSFCapturedConnection CapturedConn;
            CapturedConn.ConnectorName = Conn->GetName();
            CapturedConn.ConnectorClass = Conn->GetClass()->GetName();
            CapturedConn.WorldLocation = Conn->GetComponentLocation();
            CapturedConn.WorldRotation = Conn->GetComponentRotation();
            CapturedConn.Direction = (int32)Conn->GetDirection();
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

            Captured.FactoryConnections.Add(CapturedConn);
        }

        // === Capture Pipe Connections ===
        TArray<UFGPipeConnectionComponent*> PipeConns;
        Buildable->GetComponents<UFGPipeConnectionComponent>(PipeConns);
        for (UFGPipeConnectionComponent* Conn : PipeConns)
        {
            if (!Conn) continue;

            FSFCapturedConnection CapturedConn;
            CapturedConn.ConnectorName = Conn->GetName();
            CapturedConn.ConnectorClass = Conn->GetClass()->GetName();
            CapturedConn.WorldLocation = Conn->GetComponentLocation();
            CapturedConn.WorldRotation = Conn->GetComponentRotation();
            CapturedConn.Direction = (int32)Conn->GetPipeConnectionType();
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

            Captured.PipeConnections.Add(CapturedConn);
        }

        // === Categorize and gather type-specific data ===
        if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Buildable))
        {
            Captured.Category = TEXT("Belt");
            Captured.BeltSpeed = Belt->GetSpeed();

            // Get spline data
            if (USplineComponent* Spline = Belt->GetSplineComponent())
            {
                Captured.SplineLength = Spline->GetSplineLength();
                Captured.SplinePointCount = Spline->GetNumberOfSplinePoints();

                for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); ++i)
                {
                    FSFCapturedSplinePoint Point;
                    Point.Index = i;
                    Point.Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
                    Point.WorldLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.Rotation = Spline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Captured.SplinePoints.Add(Point);
                }
            }
        }
        else if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Buildable))
        {
            Captured.Category = TEXT("Lift");
            Captured.LiftHeight = Lift->GetHeight();
            Captured.bLiftIsReversed = Lift->GetIsReversed();

            // Get connection locations
            TArray<UFGFactoryConnectionComponent*> LiftConns;
            Lift->GetComponents<UFGFactoryConnectionComponent>(LiftConns);
            for (UFGFactoryConnectionComponent* Conn : LiftConns)
            {
                if (Conn->GetName().Contains(TEXT("0")))
                {
                    Captured.LiftBottomLocation = Conn->GetComponentLocation();
                }
                else
                {
                    Captured.LiftTopLocation = Conn->GetComponentLocation();
                }
            }
        }
        else if (Cast<AFGBuildableConveyorAttachment>(Buildable))
        {
            Captured.Category = TEXT("Distributor");
        }
        else if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable))
        {
            Captured.Category = TEXT("Pipe");

            // Get spline data
            if (USplineComponent* Spline = Pipe->GetSplineComponent())
            {
                Captured.SplineLength = Spline->GetSplineLength();
                Captured.SplinePointCount = Spline->GetNumberOfSplinePoints();

                for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); ++i)
                {
                    FSFCapturedSplinePoint Point;
                    Point.Index = i;
                    Point.Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
                    Point.WorldLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.Rotation = Spline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Captured.SplinePoints.Add(Point);
                }
            }
        }
        else if (Cast<AFGBuildablePipelineJunction>(Buildable))
        {
            Captured.Category = TEXT("Junction");
        }
        else if (Cast<AFGBuildableFactory>(Buildable))
        {
            Captured.Category = TEXT("Factory");
        }
        else
        {
            Captured.Category = TEXT("Other");
        }

        // === Check if this buildable is part of EXTEND topology ===
        if (FExtendTopologyInfo* TopoInfo = TopologyLookup.Find(Buildable))
        {
            Captured.bIsExtendSource = true;
            Captured.ExtendRole = TopoInfo->Role;
            Captured.ExtendChainId = TopoInfo->ChainId;
            Captured.ExtendChainIndex = TopoInfo->ChainIndex;
        }

        Snapshot.Buildables.Add(Captured);

        // Update category count
        int32& Count = Snapshot.CountByCategory.FindOrAdd(Captured.Category);
        Count++;
    }

    return Snapshot;
}

void USFExtendDiagnosticsService::CapturePreviewSnapshot()
{
    // DISABLED: RadarPulse generates too much log output (~1400 lines per Extend)
    // Re-enable for debugging by uncommenting the block below
    /*
    // Use RadarPulse service for enhanced capture (200m radius)
    USFRadarPulseService* RadarPulse = Subsystem.IsValid() ? Subsystem->GetRadarPulseService() : nullptr;
    if (RadarPulse)
    {
        // Capture with RadarPulse (200m = 20000cm)
        FSFRadarPulseSnapshot PulseSnapshot = RadarPulse->CaptureSnapshot(20000.0f, TEXT("EXTEND_PRE"));

        // Flag EXTEND source objects
        RadarPulse->FlagExtendSourceObjects(PulseSnapshot, GetCurrentTopology(), CurrentExtendTarget.Get());

        // Cache for later comparison
        RadarPulse->CacheSnapshot(TEXT("EXTEND_PRE"), PulseSnapshot);

        // Log the snapshot with EXTEND source details
        // RadarPulse->LogFlaggedObjects(PulseSnapshot, TEXT("ExtendSource"), true);  // DISABLED: Too verbose (~2000 lines)

        bHasPreviewSnapshot = true;
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("📡 RadarPulse: EXTEND preview snapshot captured (%d objects, %d EXTEND sources)"),
            PulseSnapshot.TotalObjects, PulseSnapshot.ExtendSourceCount);
        return;
    }
    */

    // Fallback to legacy capture if RadarPulse unavailable
    PreviewSnapshot = CaptureNearbyBuildables(15000.0f);  // 150m
    bHasPreviewSnapshot = true;

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("📊 DIAG: Captured PREVIEW snapshot - %d buildables within 150m"),
        PreviewSnapshot.Buildables.Num());

    // Log summary by category
    for (const auto& Pair : PreviewSnapshot.CountByCategory)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("📊 DIAG:   %s: %d"), *Pair.Key, Pair.Value);
    }

    // === Log EXTEND Source Topology Summary ===
    int32 SourceCount = 0;
    TMap<FString, int32> SourceByRole;
    for (const FSFCapturedBuildable& B : PreviewSnapshot.Buildables)
    {
        if (B.bIsExtendSource)
        {
            SourceCount++;
            int32& Count = SourceByRole.FindOrAdd(B.ExtendRole);
            Count++;
        }
    }

    if (SourceCount > 0)
    {
        // DISABLED: Source topology box logging - generates ~30+ lines per Extend
        // Re-enable for debugging by uncommenting the block below
        /*
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║           EXTEND SOURCE TOPOLOGY (%d members)                      ║"), SourceCount);
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));

        for (const auto& Pair : SourceByRole)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║   %-20s: %3d                                       ║"), *Pair.Key, Pair.Value);
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
        */

        // DISABLED: Detailed source topology logging generates ~1500 lines per Extend
        // Re-enable for debugging by uncommenting the block below
        /*
        // Log full details of SOURCE topology members
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("📊 EXTEND SOURCE DETAILS:"));

        for (const FSFCapturedBuildable& B : PreviewSnapshot.Buildables)
        {
            if (B.bIsExtendSource)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ★ %s [%s] - Chain %d, Index %d"),
                    *B.ExtendRole, *B.Name, B.ExtendChainId, B.ExtendChainIndex);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Category: %s | Class: %s"), *B.Category, *B.ClassName);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Location: X=%.3f Y=%.3f Z=%.3f"), B.Location.X, B.Location.Y, B.Location.Z);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Rotation: P=%.3f Y=%.3f R=%.3f"), B.Rotation.Pitch, B.Rotation.Yaw, B.Rotation.Roll);

                // Belt-specific data
                if (B.Category == TEXT("Belt"))
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ BELT DATA ═══"));
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Speed: %.1f | SplineLength: %.1fcm | SplinePoints: %d"),
                        B.BeltSpeed, B.SplineLength, B.SplinePointCount);

                    for (const FSFCapturedSplinePoint& SP : B.SplinePoints)
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   [Point %d] World=(%.1f,%.1f,%.1f)"),
                            SP.Index, SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│             Arrive=(%.1f,%.1f,%.1f) Leave=(%.1f,%.1f,%.1f)"),
                            SP.ArriveTangent.X, SP.ArriveTangent.Y, SP.ArriveTangent.Z,
                            SP.LeaveTangent.X, SP.LeaveTangent.Y, SP.LeaveTangent.Z);
                    }
                }

                // Lift-specific data
                if (B.Category == TEXT("Lift"))
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ LIFT DATA ═══"));
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Height: %.1fcm | Reversed: %d"), B.LiftHeight, B.bLiftIsReversed);
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Bottom: (%.1f,%.1f,%.1f) Top: (%.1f,%.1f,%.1f)"),
                        B.LiftBottomLocation.X, B.LiftBottomLocation.Y, B.LiftBottomLocation.Z,
                        B.LiftTopLocation.X, B.LiftTopLocation.Y, B.LiftTopLocation.Z);
                }

                // Pipe-specific data
                if (B.Category == TEXT("Pipe"))
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ PIPE DATA ═══"));
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ SplineLength: %.1fcm | SplinePoints: %d"), B.SplineLength, B.SplinePointCount);

                    for (const FSFCapturedSplinePoint& SP : B.SplinePoints)
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   [Point %d] World=(%.1f,%.1f,%.1f)"),
                            SP.Index, SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                    }
                }

                // Factory connections
                if (B.FactoryConnections.Num() > 0)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ FACTORY CONNECTIONS (%d) ═══"), B.FactoryConnections.Num());
                    for (const FSFCapturedConnection& Conn : B.FactoryConnections)
                    {
                        FString ConnStatus = Conn.bIsConnected
                            ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                            : TEXT("(not connected)");
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) %s"),
                            *Conn.ConnectorName, Conn.Direction,
                            Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                            *ConnStatus);
                    }
                }

                // Pipe connections
                if (B.PipeConnections.Num() > 0)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ PIPE CONNECTIONS (%d) ═══"), B.PipeConnections.Num());
                    for (const FSFCapturedConnection& Conn : B.PipeConnections)
                    {
                        FString ConnStatus = Conn.bIsConnected
                            ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                            : TEXT("(not connected)");
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) %s"),
                            *Conn.ConnectorName, Conn.Direction,
                            Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                            *ConnStatus);
                    }
                }

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
            }
        }
        */
    }
}

void USFExtendDiagnosticsService::CapturePostBuildSnapshotAndLogDiff()
{
    if (!bHasPreviewSnapshot)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("📊 DIAG: No preview snapshot to compare against"));
        return;
    }

    // DISABLED: RadarPulse generates too much log output (~1400 lines per Extend)
    // Re-enable for debugging by uncommenting the block below
    /*
    // Use RadarPulse service for enhanced capture and diff
    USFRadarPulseService* RadarPulse = Subsystem.IsValid() ? Subsystem->GetRadarPulseService() : nullptr;
    if (RadarPulse)
    {
        // Capture POST snapshot (200m = 20000cm)
        FSFRadarPulseSnapshot PostSnapshot = RadarPulse->CaptureSnapshot(20000.0f, TEXT("EXTEND_POST"));

        // Get the PRE snapshot from cache
        FSFRadarPulseSnapshot PreSnapshot;
        if (RadarPulse->GetCachedSnapshot(TEXT("EXTEND_PRE"), PreSnapshot))
        {
            // Compare and log the diff
            FSFSnapshotDiff Diff = RadarPulse->CompareSnapshots(PreSnapshot, PostSnapshot);
            RadarPulse->LogDiff(Diff, true);  // Verbose output

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("📡 RadarPulse: EXTEND build diff - %d new, %d removed, %d modified"),
                Diff.NewObjects.Num(), Diff.RemovedObjects.Num(), Diff.ModifiedObjects.Num());
        }
        else
        {
            // No PRE snapshot cached, just log the POST snapshot
            RadarPulse->LogSnapshot(PostSnapshot, true);
        }

        // Clear cache
        RadarPulse->ClearCache(TEXT("EXTEND_PRE"));
        bHasPreviewSnapshot = false;
        return;
    }
    */

    // Fallback to legacy capture if RadarPulse unavailable
    FSFBuildableSnapshot PostBuildSnapshot = CaptureNearbyBuildables(15000.0f);  // 150m

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("📊 DIAG: Captured POST-BUILD snapshot - %d buildables within 150m"),
        PostBuildSnapshot.Buildables.Num());

    LogSnapshotDiff(PreviewSnapshot, PostBuildSnapshot);

    // Clear the preview snapshot
    bHasPreviewSnapshot = false;
    PreviewSnapshot = FSFBuildableSnapshot();
}

void USFExtendDiagnosticsService::LogSnapshotDiff(const FSFBuildableSnapshot& Before, const FSFBuildableSnapshot& After)
{
    // DISABLED: BUILD DIFF SUMMARY box logging - generates ~20+ lines per Extend
    // Re-enable for debugging by uncommenting the block below
    /*
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║               EXTEND DIAGNOSTIC: BUILD DIFF SUMMARY               ║"));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║ Capture Radius: 150m | Before: %4d buildables | After: %4d        ║"),
        Before.Buildables.Num(), After.Buildables.Num());
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));

    // Build set of existing names for quick lookup
    TSet<FString> BeforeNames;
    for (const FSFCapturedBuildable& B : Before.Buildables)
    {
        BeforeNames.Add(B.Name);
    }

    TSet<FString> AfterNames;
    for (const FSFCapturedBuildable& A : After.Buildables)
    {
        AfterNames.Add(A.Name);
    }

    // Find new buildables (in After but not in Before)
    TArray<FSFCapturedBuildable> NewBuildables;
    for (const FSFCapturedBuildable& A : After.Buildables)
    {
        if (!BeforeNames.Contains(A.Name))
        {
            NewBuildables.Add(A);
        }
    }

    // Find removed buildables (in Before but not in After)
    TArray<FSFCapturedBuildable> RemovedBuildables;
    for (const FSFCapturedBuildable& B : Before.Buildables)
    {
        if (!AfterNames.Contains(B.Name))
        {
            RemovedBuildables.Add(B);
        }
    }

    // Count new by category
    TMap<FString, int32> NewByCategory;
    for (const FSFCapturedBuildable& N : NewBuildables)
    {
        int32& Count = NewByCategory.FindOrAdd(N.Category);
        Count++;
    }

    // Count removed by category
    TMap<FString, int32> RemovedByCategory;
    for (const FSFCapturedBuildable& R : RemovedBuildables)
    {
        int32& Count = RemovedByCategory.FindOrAdd(R.Category);
        Count++;
    }

    // Log category summary
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║ Category        │ Before │ After  │ New    │ Removed │ Delta   ║"));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╟─────────────────┼────────┼────────┼────────┼─────────┼─────────╢"));

    // Collect all categories
    TSet<FString> AllCategories;
    for (const auto& Pair : Before.CountByCategory) AllCategories.Add(Pair.Key);
    for (const auto& Pair : After.CountByCategory) AllCategories.Add(Pair.Key);

    for (const FString& Category : AllCategories)
    {
        int32 BeforeCount = Before.CountByCategory.Contains(Category) ? Before.CountByCategory[Category] : 0;
        int32 AfterCount = After.CountByCategory.Contains(Category) ? After.CountByCategory[Category] : 0;
        int32 NewCount = NewByCategory.Contains(Category) ? NewByCategory[Category] : 0;
        int32 RemovedCount = RemovedByCategory.Contains(Category) ? RemovedByCategory[Category] : 0;
        int32 Delta = AfterCount - BeforeCount;

        FString DeltaStr = Delta > 0 ? FString::Printf(TEXT("+%d"), Delta) : FString::Printf(TEXT("%d"), Delta);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║ %-15s │ %6d │ %6d │ %6d │ %7d │ %7s ║"),
            *Category, BeforeCount, AfterCount, NewCount, RemovedCount, *DeltaStr);
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║ TOTAL           │ %6d │ %6d │ %6d │ %7d │ %+7d ║"),
        Before.Buildables.Num(), After.Buildables.Num(),
        NewBuildables.Num(), RemovedBuildables.Num(),
        After.Buildables.Num() - Before.Buildables.Num());
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
    */

    // DISABLED: Detailed buildable enumeration generates ~1000 lines per Extend
    // Re-enable for debugging by uncommenting the block below
    /*
    // Log FULL details of ALL new buildables
    if (NewBuildables.Num() > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("║            FULL ENUMERATION: NEW BUILDABLES (%d total)            ║"), NewBuildables.Num());
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

        for (int32 i = 0; i < NewBuildables.Num(); ++i)
        {
            const FSFCapturedBuildable& N = NewBuildables[i];

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT(""));
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));

            // Show EXTEND source badge prominently
            if (N.bIsExtendSource)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ [%d] ★ EXTEND SOURCE ★ %s"), i, *N.Name);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│     Role: %s | Chain: %d | Index: %d"),
                    *N.ExtendRole, N.ExtendChainId, N.ExtendChainIndex);
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ [%d] %s"), i, *N.Name);
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Category: %s | Class: %s"), *N.Category, *N.ClassName);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Location: X=%.3f Y=%.3f Z=%.3f"), N.Location.X, N.Location.Y, N.Location.Z);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Rotation: P=%.3f Y=%.3f R=%.3f"), N.Rotation.Pitch, N.Rotation.Yaw, N.Rotation.Roll);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Scale: X=%.3f Y=%.3f Z=%.3f"), N.Scale.X, N.Scale.Y, N.Scale.Z);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Bounds: Min=(%.1f,%.1f,%.1f) Max=(%.1f,%.1f,%.1f)"),
                N.BoundsMin.X, N.BoundsMin.Y, N.BoundsMin.Z, N.BoundsMax.X, N.BoundsMax.Y, N.BoundsMax.Z);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ State: Hidden=%d PendingKill=%d BegunPlay=%d"),
                N.bIsHidden, N.bIsPendingKill, N.bHasBegunPlay);

            // Belt-specific data
            if (N.Category == TEXT("Belt"))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ BELT DATA ═══"));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Speed: %.1f | SplineLength: %.1fcm | SplinePoints: %d"),
                    N.BeltSpeed, N.SplineLength, N.SplinePointCount);

                for (const FSFCapturedSplinePoint& SP : N.SplinePoints)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                        SP.Index, SP.Location.X, SP.Location.Y, SP.Location.Z,
                        SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│             Arrive=(%.1f,%.1f,%.1f) Leave=(%.1f,%.1f,%.1f)"),
                        SP.ArriveTangent.X, SP.ArriveTangent.Y, SP.ArriveTangent.Z,
                        SP.LeaveTangent.X, SP.LeaveTangent.Y, SP.LeaveTangent.Z);
                }
            }

            // Lift-specific data
            if (N.Category == TEXT("Lift"))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ LIFT DATA ═══"));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Height: %.1fcm | Reversed: %d"), N.LiftHeight, N.bLiftIsReversed);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ Bottom: (%.1f,%.1f,%.1f) Top: (%.1f,%.1f,%.1f)"),
                    N.LiftBottomLocation.X, N.LiftBottomLocation.Y, N.LiftBottomLocation.Z,
                    N.LiftTopLocation.X, N.LiftTopLocation.Y, N.LiftTopLocation.Z);
            }

            // Pipe-specific data
            if (N.Category == TEXT("Pipe"))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ PIPE DATA ═══"));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ SplineLength: %.1fcm | SplinePoints: %d"), N.SplineLength, N.SplinePointCount);

                for (const FSFCapturedSplinePoint& SP : N.SplinePoints)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                        SP.Index, SP.Location.X, SP.Location.Y, SP.Location.Z,
                        SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                }
            }

            // Factory connections
            if (N.FactoryConnections.Num() > 0)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ FACTORY CONNECTIONS (%d) ═══"), N.FactoryConnections.Num());
                for (const FSFCapturedConnection& Conn : N.FactoryConnections)
                {
                    FString ConnStatus = Conn.bIsConnected
                        ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                        : TEXT("(not connected)");
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) %s"),
                        *Conn.ConnectorName, Conn.Direction,
                        Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                        *ConnStatus);
                }
            }

            // Pipe connections
            if (N.PipeConnections.Num() > 0)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│ ═══ PIPE CONNECTIONS (%d) ═══"), N.PipeConnections.Num());
                for (const FSFCapturedConnection& Conn : N.PipeConnections)
                {
                    FString ConnStatus = Conn.bIsConnected
                        ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                        : TEXT("(not connected)");
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) %s"),
                        *Conn.ConnectorName, Conn.Direction,
                        Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                        *ConnStatus);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
        }
    }

    // Log FULL details of removed buildables
    if (RemovedBuildables.Num() > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT(""));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("║          FULL ENUMERATION: REMOVED BUILDABLES (%d total)          ║"), RemovedBuildables.Num());
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));

        for (int32 i = 0; i < RemovedBuildables.Num(); ++i)
        {
            const FSFCapturedBuildable& R = RemovedBuildables[i];
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   [%d] [%s] %s @ (%.0f, %.0f, %.0f)"),
                i, *R.Category, *R.Name, R.Location.X, R.Location.Y, R.Location.Z);
        }
    }
    */
}
