// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "SmartFoundations.h"  // For LogSmartFoundations
#include "Subsystem/SFSubsystem.h"
#include "FGBuildable.h"
#include "FGBuildableFactory.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"  // Issue #288: valves are AFGBuildablePipelinePump subclasses
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "Buildables/FGBuildablePassthrough.h"
// NOTE: No include for FGBuildableWallPassthrough.h — in-game testing confirmed that
// Build_ConveyorWallHole_C and Build_PipelineSupportWallHole_C are Blueprint-only classes
// derived directly from AFGBuildable (SnapOnly decorator components only, no connection
// components, not derived from AFGBuildableWallPassthrough). Wall-hole discovery happens
// spatially via class-name match in DiscoverWallPassthroughs.
#include "FGConnectionComponent.h"  // Issue #283: UFGConnectionComponent base for Cast<> in Passthrough snapped-connection checks
#include "EngineUtils.h"  // TActorIterator

USFExtendTopologyService::USFExtendTopologyService()
{
}

void USFExtendTopologyService::Initialize(USFSubsystem* InSubsystem, USFExtendDetectionService* InDetectionService)
{
    Subsystem = InSubsystem;
    DetectionService = InDetectionService;
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendTopologyService initialized"));
}

void USFExtendTopologyService::Shutdown()
{
    ClearTopology();
    DetectionService = nullptr;
    Subsystem.Reset();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendTopologyService shutdown"));
}

// ==================== Topology Walking ====================

bool USFExtendTopologyService::WalkTopology(AFGBuildable* SourceBuilding)
{
    ClearTopology();

    if (!IsValid(SourceBuilding))
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("Smart!: WalkTopology called with invalid building"));
        return false;
    }

    // Use detection service for target validation if available
    if (DetectionService && !DetectionService->IsValidExtendTarget(SourceBuilding))
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Smart!: Building %s is not a valid extend target"), 
            *SourceBuilding->GetName());
        return false;
    }

    CachedTopology.SourceBuilding = SourceBuilding;

    // Get all factory connection components on the building
    TArray<UFGFactoryConnectionComponent*> Connections;
    SourceBuilding->GetComponents<UFGFactoryConnectionComponent>(Connections);

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Walking topology for %s with %d connections"),
        *SourceBuilding->GetName(), Connections.Num());

    for (UFGFactoryConnectionComponent* Connector : Connections)
    {
        if (!IsValid(Connector))
        {
            continue;
        }

        // Check if this connector is connected to something
        UFGFactoryConnectionComponent* ConnectedTo = Connector->GetConnection();
        if (!IsValid(ConnectedTo))
        {
            continue;
        }

        FSFConnectionChainNode Node;
        if (WalkConnectionChain(Connector, Node))
        {
            // Categorize by direction
            if (Connector->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
            {
                CachedTopology.OutputChains.Add(Node);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Found output chain to %s (%d conveyors, %d poles)"),
                    Node.Distributor.IsValid() ? *Node.Distributor->GetName() : TEXT("null"),
                    Node.Conveyors.Num(),
                    Node.SupportPoles.Num());
            }
            else if (Connector->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
            {
                CachedTopology.InputChains.Add(Node);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Found input chain from %s (%d conveyors, %d poles)"),
                    Node.Distributor.IsValid() ? *Node.Distributor->GetName() : TEXT("null"),
                    Node.Conveyors.Num(),
                    Node.SupportPoles.Num());
            }
        }
    }

    // ==================== Walk Pipe Connections ====================
    // Get all pipe connection components on the building
    TArray<UFGPipeConnectionComponent*> PipeConnections;
    SourceBuilding->GetComponents<UFGPipeConnectionComponent>(PipeConnections);

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Walking pipe topology for %s with %d pipe connections"),
        *SourceBuilding->GetName(), PipeConnections.Num());

    for (UFGPipeConnectionComponent* PipeConnector : PipeConnections)
    {
        if (!IsValid(PipeConnector))
        {
            continue;
        }

        EPipeConnectionType PipeType = PipeConnector->GetPipeConnectionType();
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:   Pipe connector %s - Type=%d (%s)"),
            *PipeConnector->GetName(),
            (int32)PipeType,
            PipeType == EPipeConnectionType::PCT_PRODUCER ? TEXT("PRODUCER") : 
            (PipeType == EPipeConnectionType::PCT_CONSUMER ? TEXT("CONSUMER") : TEXT("ANY")));

        // Check if this connector is connected to something
        UFGPipeConnectionComponent* ConnectedTo = Cast<UFGPipeConnectionComponent>(PipeConnector->GetConnection());
        if (!IsValid(ConnectedTo))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:     -> Not connected"));
            continue;
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:     -> Connected to %s on %s"),
            *ConnectedTo->GetName(),
            ConnectedTo->GetOwner() ? *ConnectedTo->GetOwner()->GetName() : TEXT("null"));

        FSFPipeConnectionChainNode PipeNode;
        if (WalkPipeConnectionChain(PipeConnector, PipeNode))
        {
            // Categorize by pipe connection type
            // PCT_PRODUCER = output (factory produces fluid)
            // PCT_CONSUMER = input (factory consumes fluid)
            // PCT_ANY = can be either
            if (PipeType == EPipeConnectionType::PCT_PRODUCER)
            {
                CachedTopology.PipeOutputChains.Add(PipeNode);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:     -> Added as OUTPUT chain (Junction=%s, Pipelines=%d, Poles=%d)"),
                    PipeNode.Junction.IsValid() ? *PipeNode.Junction->GetName() : TEXT("null"),
                    PipeNode.Pipelines.Num(),
                    PipeNode.SupportPoles.Num());
            }
            else // PCT_CONSUMER or PCT_ANY
            {
                CachedTopology.PipeInputChains.Add(PipeNode);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:     -> Added as INPUT chain (Junction=%s, Pipelines=%d, Poles=%d)"),
                    PipeNode.Junction.IsValid() ? *PipeNode.Junction->GetName() : TEXT("null"),
                    PipeNode.Pipelines.Num(),
                    PipeNode.SupportPoles.Num());
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:     -> WalkPipeConnectionChain returned false"));
        }
    }

    CachedTopology.bIsValid = (CachedTopology.InputChains.Num() > 0 || CachedTopology.OutputChains.Num() > 0 ||
                               CachedTopology.PipeInputChains.Num() > 0 || CachedTopology.PipeOutputChains.Num() > 0);

    // ==================== Walk Power Connections (Issue #229) ====================
    // Must be called AFTER belt/pipe topology is captured so CalculateManifoldBounds() works
    // Gated by bExtendPower runtime setting
    bool bExtendPowerEnabled = Subsystem.IsValid() && Subsystem->GetAutoConnectRuntimeSettings().bExtendPower;
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Power walk gate: bIsValid=%d, SubsystemValid=%d, bExtendPower=%d"),
        CachedTopology.bIsValid, Subsystem.IsValid(), bExtendPowerEnabled);
    if (CachedTopology.bIsValid && bExtendPowerEnabled)
    {
        WalkPowerConnections(SourceBuilding);
    }

    // ==================== Discover Pipe Passthroughs (Issue #260) ====================
    // Pipe floor holes are NOT in the pipe connection chain (pipes pass through physically,
    // not logically). Must discover them spatially near the source factory.
    if (CachedTopology.bIsValid)
    {
        DiscoverPipePassthroughs(SourceBuilding);
        DiscoverWallPassthroughs(SourceBuilding);
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Topology walk complete. Belt Inputs: %d, Belt Outputs: %d, Pipe Inputs: %d, Pipe Outputs: %d, Power Poles: %d, Pipe Passthroughs: %d, Wall Passthroughs: %d, Valid: %s"),
        CachedTopology.InputChains.Num(),
        CachedTopology.OutputChains.Num(),
        CachedTopology.PipeInputChains.Num(),
        CachedTopology.PipeOutputChains.Num(),
        CachedTopology.PowerPoles.Num(),
        CachedTopology.PipePassthroughs.Num(),
        CachedTopology.WallPassthroughs.Num(),
        CachedTopology.bIsValid ? TEXT("true") : TEXT("false"));

    return CachedTopology.bIsValid;
}

bool USFExtendTopologyService::WalkConnectionChain(UFGFactoryConnectionComponent* StartConnector, FSFConnectionChainNode& OutNode)
{
    if (!IsValid(StartConnector))
    {
        return false;
    }

    // Get what we're connected to
    UFGFactoryConnectionComponent* ConnectedTo = StartConnector->GetConnection();
    if (!IsValid(ConnectedTo))
    {
        return false;
    }

    OutNode.SourceConnector = StartConnector;

    // The connected component should be on a conveyor belt
    AActor* ConnectedActor = ConnectedTo->GetOwner();
    AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(ConnectedActor);
    
    if (!IsValid(Conveyor))
    {
        // Might be directly connected to a distributor (no belt)
        AFGBuildableConveyorAttachment* DirectDistributor = Cast<AFGBuildableConveyorAttachment>(ConnectedActor);
        if (IsValid(DirectDistributor))
        {
            OutNode.Distributor = DirectDistributor;
            OutNode.DistributorConnector = ConnectedTo;
            // No conveyor in this chain
            return true;
        }
        return false;
    }

    // Walk through the chain of belts/lifts until we find a distributor or dead end
    TSet<AFGBuildableConveyorBase*> VisitedConveyors;
    UFGFactoryConnectionComponent* CurrentConnector = ConnectedTo;
    AFGBuildableConveyorBase* CurrentConveyor = Conveyor;
    
    constexpr int32 MaxIterations = 100; // Safety limit to prevent infinite loops
    int32 Iterations = 0;
    
    while (IsValid(CurrentConveyor) && Iterations < MaxIterations)
    {
        Iterations++;
        
        // Prevent infinite loops
        if (VisitedConveyors.Contains(CurrentConveyor))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: Belt chain loop detected, stopping walk"));
            break;
        }
        VisitedConveyors.Add(CurrentConveyor);
        
        // Add this conveyor to the chain
        OutNode.Conveyors.Add(CurrentConveyor);
        
        // Find the other end of the current conveyor
        UFGFactoryConnectionComponent* Connection0 = nullptr;
        UFGFactoryConnectionComponent* Connection1 = nullptr;
        
        // Handle both belts and lifts
        if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(CurrentConveyor))
        {
            Connection0 = Belt->GetConnection0();
            Connection1 = Belt->GetConnection1();
        }
        else if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(CurrentConveyor))
        {
            // Lifts have top and bottom connections
            Connection0 = Lift->GetConnection0();
            Connection1 = Lift->GetConnection1();
        }
        
        if (!IsValid(Connection0) || !IsValid(Connection1))
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Belt chain ended - conveyor missing connections"));
            break;
        }

        // Find which connection goes forward (not back where we came from)
        UFGFactoryConnectionComponent* OtherEnd = nullptr;
        if (Connection0 == CurrentConnector || Connection0->GetConnection() == StartConnector)
        {
            OtherEnd = Connection1;
        }
        else if (Connection1 == CurrentConnector)
        {
            OtherEnd = Connection0;
        }
        else
        {
            // Neither matches - try to find the one that's not connected back to our start
            OtherEnd = (Connection0->GetConnection() != StartConnector) ? Connection0 : Connection1;
        }

        if (!IsValid(OtherEnd) || !OtherEnd->IsConnected())
        {
            // Dead end - no distributor found
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Belt chain ended at dead end (no connection at other end)"));
            break;
        }

        UFGFactoryConnectionComponent* NextConnector = OtherEnd->GetConnection();
        if (!IsValid(NextConnector))
        {
            break;
        }

        AActor* NextActor = NextConnector->GetOwner();
        
        // Check if we've reached a distributor (terminal)
        if (IsDistributor(Cast<AFGBuildable>(NextActor)))
        {
            OutNode.Distributor = Cast<AFGBuildable>(NextActor);
            OutNode.DistributorConnector = NextConnector;
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Belt chain found distributor %s after %d conveyors"), 
                *NextActor->GetName(), VisitedConveyors.Num());
            return true;
        }
        
        // Check if it's another conveyor (belt or lift) - continue walking
        AFGBuildableConveyorBase* NextConveyor = Cast<AFGBuildableConveyorBase>(NextActor);
        if (IsValid(NextConveyor))
        {
            CurrentConveyor = NextConveyor;
            CurrentConnector = NextConnector;
            continue;
        }
        
        // Issue #260: Check if it's a passthrough (floor hole) - traverse through it
        AFGBuildablePassthrough* Passthrough = Cast<AFGBuildablePassthrough>(NextActor);
        if (IsValid(Passthrough))
        {
            OutNode.Passthroughs.Add(Passthrough);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Belt chain traversing through floor hole %s"),
                *Passthrough->GetName());
            
            // Find the other factory connection on the passthrough (not the one we came from)
            TArray<UFGFactoryConnectionComponent*> PassthroughConnections;
            Passthrough->GetComponents<UFGFactoryConnectionComponent>(PassthroughConnections);
            
            UFGFactoryConnectionComponent* OtherPassthroughConn = nullptr;
            for (UFGFactoryConnectionComponent* PassConn : PassthroughConnections)
            {
                if (PassConn && PassConn != NextConnector && PassConn->IsConnected())
                {
                    OtherPassthroughConn = PassConn;
                    break;
                }
            }
            
            if (IsValid(OtherPassthroughConn))
            {
                UFGFactoryConnectionComponent* AfterPassthrough = OtherPassthroughConn->GetConnection();
                if (IsValid(AfterPassthrough))
                {
                    AActor* AfterActor = AfterPassthrough->GetOwner();
                    
                    // Check if what's after the passthrough is a conveyor
                    AFGBuildableConveyorBase* AfterConveyor = Cast<AFGBuildableConveyorBase>(AfterActor);
                    if (IsValid(AfterConveyor))
                    {
                        CurrentConveyor = AfterConveyor;
                        CurrentConnector = AfterPassthrough;
                        continue;
                    }
                    
                    // Or a distributor
                    if (IsDistributor(Cast<AFGBuildable>(AfterActor)))
                    {
                        OutNode.Distributor = Cast<AFGBuildable>(AfterActor);
                        OutNode.DistributorConnector = AfterPassthrough;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Found distributor %s after floor hole"),
                            *AfterActor->GetName());
                        return true;
                    }
                }
            }
            
            // Passthrough has no other connected side - stop here
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Floor hole %s has no connection on other side"),
                *Passthrough->GetName());
            break;
        }

        // NOTE: Conveyor wall holes (Build_ConveyorWallHole_C) are cosmetic Blueprint decorators
        // that snap beside a belt passing through a wall; they do NOT interrupt the belt spline
        // and have no factory connection components. The belt itself is captured normally above.
        // Wall-hole cloning is handled purely by spatial discovery (see DiscoverWallPassthroughs).

        // It's something else (factory, pole, etc.) - stop here
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Belt chain ended at non-belt/non-distributor: %s"), 
            *NextActor->GetClass()->GetName());
        break;
    }

    // Conveyor(s) found but no valid distributor at the end
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Belt chain walked %d conveyors, no distributor found"), VisitedConveyors.Num());
    return OutNode.Conveyors.Num() > 0;
}

bool USFExtendTopologyService::WalkPipeConnectionChain(UFGPipeConnectionComponent* StartConnector, FSFPipeConnectionChainNode& OutNode)
{
    if (!IsValid(StartConnector))
    {
        return false;
    }

    // Get what we're connected to
    UFGPipeConnectionComponent* ConnectedTo = Cast<UFGPipeConnectionComponent>(StartConnector->GetConnection());
    if (!IsValid(ConnectedTo))
    {
        return false;
    }

    OutNode.SourceConnector = StartConnector;

    // The connected component should be on a pipeline
    AActor* ConnectedActor = ConnectedTo->GetOwner();
    AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(ConnectedActor);
    
    if (!IsValid(Pipeline))
    {
        // Might be directly connected to a junction (no pipe)
        AFGBuildablePipelineJunction* DirectJunction = Cast<AFGBuildablePipelineJunction>(ConnectedActor);
        if (IsValid(DirectJunction))
        {
            OutNode.Junction = DirectJunction;
            OutNode.JunctionConnector = ConnectedTo;
            // No pipeline in this chain
            return true;
        }
        
        // Issue #260: Might be directly connected to a passthrough (pipe floor hole)
        // Chain: factory → passthrough → pipe → ... → junction
        AFGBuildablePassthrough* DirectPassthrough = Cast<AFGBuildablePassthrough>(ConnectedActor);
        if (IsValid(DirectPassthrough))
        {
            OutNode.Passthroughs.Add(DirectPassthrough);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pipe chain starts with floor hole %s"),
                *DirectPassthrough->GetName());
            
            // Find the other pipe connection on the passthrough
            TArray<UFGPipeConnectionComponent*> PassPipeConns;
            DirectPassthrough->GetComponents<UFGPipeConnectionComponent>(PassPipeConns);
            
            for (UFGPipeConnectionComponent* PassConn : PassPipeConns)
            {
                if (PassConn && PassConn != ConnectedTo && PassConn->IsConnected())
                {
                    UFGPipeConnectionComponent* AfterPass = Cast<UFGPipeConnectionComponent>(PassConn->GetConnection());
                    if (IsValid(AfterPass))
                    {
                        AActor* AfterActor = AfterPass->GetOwner();
                        Pipeline = Cast<AFGBuildablePipeline>(AfterActor);
                        if (IsValid(Pipeline))
                        {
                            ConnectedTo = AfterPass;
                            break;
                        }
                        // Could be a junction right after the passthrough
                        AFGBuildablePipelineJunction* AfterJunction = Cast<AFGBuildablePipelineJunction>(AfterActor);
                        if (IsValid(AfterJunction))
                        {
                            OutNode.Junction = AfterJunction;
                            OutNode.JunctionConnector = AfterPass;
                            return true;
                        }
                    }
                }
            }
            
            if (!IsValid(Pipeline))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pipe floor hole %s has no pipe on other side"),
                    *DirectPassthrough->GetName());
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    // Walk through the chain of pipes until we find a junction or dead end
    TSet<AFGBuildablePipeline*> VisitedPipes;
    UFGPipeConnectionComponent* CurrentConnector = ConnectedTo;
    AFGBuildablePipeline* CurrentPipe = Pipeline;
    
    constexpr int32 MaxIterations = 100; // Safety limit to prevent infinite loops
    int32 Iterations = 0;
    
    while (IsValid(CurrentPipe) && Iterations < MaxIterations)
    {
        Iterations++;
        
        // Prevent infinite loops
        if (VisitedPipes.Contains(CurrentPipe))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: Pipe chain loop detected, stopping walk"));
            break;
        }
        VisitedPipes.Add(CurrentPipe);
        
        // Add this pipeline to the chain
        OutNode.Pipelines.Add(CurrentPipe);
        
        // Find the other end of the current pipeline
        UFGPipeConnectionComponent* Connection0 = CurrentPipe->GetPipeConnection0();
        UFGPipeConnectionComponent* Connection1 = CurrentPipe->GetPipeConnection1();

        // Find which connection goes forward (not back where we came from)
        UFGPipeConnectionComponent* OtherEnd = nullptr;
        if (Connection0 == CurrentConnector || (Connection0 && Connection0->GetConnection() == StartConnector))
        {
            OtherEnd = Connection1;
        }
        else if (Connection1 == CurrentConnector)
        {
            OtherEnd = Connection0;
        }
        else
        {
            // Neither matches - try to find the one that's not connected back to our start
            OtherEnd = (Connection0->GetConnection() != StartConnector) ? Connection0 : Connection1;
        }

        if (!IsValid(OtherEnd) || !OtherEnd->IsConnected())
        {
            // Dead end - no junction found
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe chain ended at dead end (no connection at other end)"));
            break;
        }

        UFGPipeConnectionComponent* NextConnector = Cast<UFGPipeConnectionComponent>(OtherEnd->GetConnection());
        if (!IsValid(NextConnector))
        {
            break;
        }

        AActor* NextActor = NextConnector->GetOwner();
        
        // Check if we've reached a junction (terminal)
        AFGBuildablePipelineJunction* Junction = Cast<AFGBuildablePipelineJunction>(NextActor);
        if (IsValid(Junction))
        {
            OutNode.Junction = Junction;
            OutNode.JunctionConnector = NextConnector;
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe chain found junction %s after %d pipes"), 
                *Junction->GetName(), VisitedPipes.Num());
            return true;
        }
        
        // Check if it's another pipeline - continue walking
        AFGBuildablePipeline* NextPipe = Cast<AFGBuildablePipeline>(NextActor);
        if (IsValid(NextPipe))
        {
            CurrentPipe = NextPipe;
            CurrentConnector = NextConnector;
            continue;
        }
        
        // Issue #260: Check if it's a passthrough (pipe floor hole) - traverse through it
        AFGBuildablePassthrough* Passthrough = Cast<AFGBuildablePassthrough>(NextActor);
        if (IsValid(Passthrough))
        {
            OutNode.Passthroughs.Add(Passthrough);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pipe chain traversing through floor hole %s"),
                *Passthrough->GetName());
            
            // Find the other pipe connection on the passthrough (not the one we came from)
            TArray<UFGPipeConnectionComponent*> PassthroughPipeConns;
            Passthrough->GetComponents<UFGPipeConnectionComponent>(PassthroughPipeConns);
            
            UFGPipeConnectionComponent* OtherPassthroughConn = nullptr;
            for (UFGPipeConnectionComponent* PassConn : PassthroughPipeConns)
            {
                if (PassConn && PassConn != NextConnector && PassConn->IsConnected())
                {
                    OtherPassthroughConn = PassConn;
                    break;
                }
            }
            
            if (IsValid(OtherPassthroughConn))
            {
                UFGPipeConnectionComponent* AfterPassthrough = Cast<UFGPipeConnectionComponent>(OtherPassthroughConn->GetConnection());
                if (IsValid(AfterPassthrough))
                {
                    AActor* AfterActor = AfterPassthrough->GetOwner();
                    
                    // Check if what's after the passthrough is a pipeline
                    AFGBuildablePipeline* AfterPipe = Cast<AFGBuildablePipeline>(AfterActor);
                    if (IsValid(AfterPipe))
                    {
                        CurrentPipe = AfterPipe;
                        CurrentConnector = AfterPassthrough;
                        continue;
                    }
                    
                    // Check if it's a junction (terminal)
                    AFGBuildablePipelineJunction* AfterJunction = Cast<AFGBuildablePipelineJunction>(AfterActor);
                    if (IsValid(AfterJunction))
                    {
                        OutNode.Junction = AfterJunction;
                        OutNode.JunctionConnector = AfterPassthrough;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Found junction %s after pipe floor hole"),
                            *AfterActor->GetName());
                        return true;
                    }
                }
            }
            
            // Passthrough has no other connected side - stop here
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pipe floor hole %s has no connection on other side"),
                *Passthrough->GetName());
            break;
        }

        // NOTE: Pipeline wall holes (Build_PipelineSupportWallHole_C) are cosmetic Blueprint
        // decorators with only a SnapOnly component; they do NOT interrupt pipeline splines
        // and have no pipe connection components. The pipe itself is captured normally above.
        // Wall-hole cloning is handled purely by spatial discovery (see DiscoverWallPassthroughs).

        // Issue #288: Pumps and valves are inline pipe attachments that DO interrupt the
        // spline — the vanilla pipe actually terminates at the attachment's first connector
        // and a second pipe begins at the second connector. Both Build_PipelinePump_C and
        // Build_Valve_C derive from AFGBuildablePipelinePump (valves are pumps with max
        // head-lift 0; the "valve" UI is just a UserFlowLimit slider on the same class).
        // Mirror the passthrough traversal: record the attachment, find the other connector,
        // continue walking.
        if (AFGBuildablePipelinePump* Attachment = Cast<AFGBuildablePipelinePump>(NextActor))
        {
            OutNode.PipeAttachments.Add(Attachment);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pipe chain traversing through pump/valve %s (class=%s)"),
                *Attachment->GetName(), *Attachment->GetClass()->GetName());

            TArray<UFGPipeConnectionComponent*> AttachmentPipeConns;
            Attachment->GetComponents<UFGPipeConnectionComponent>(AttachmentPipeConns);

            UFGPipeConnectionComponent* OtherAttachmentConn = nullptr;
            for (UFGPipeConnectionComponent* AttConn : AttachmentPipeConns)
            {
                if (AttConn && AttConn != NextConnector && AttConn->IsConnected())
                {
                    OtherAttachmentConn = AttConn;
                    break;
                }
            }

            if (IsValid(OtherAttachmentConn))
            {
                UFGPipeConnectionComponent* AfterAttachment = Cast<UFGPipeConnectionComponent>(OtherAttachmentConn->GetConnection());
                if (IsValid(AfterAttachment))
                {
                    AActor* AfterActor = AfterAttachment->GetOwner();

                    if (AFGBuildablePipeline* AfterPipe = Cast<AFGBuildablePipeline>(AfterActor))
                    {
                        CurrentPipe = AfterPipe;
                        CurrentConnector = AfterAttachment;
                        continue;
                    }

                    if (AFGBuildablePipelineJunction* AfterJunction = Cast<AFGBuildablePipelineJunction>(AfterActor))
                    {
                        OutNode.Junction = AfterJunction;
                        OutNode.JunctionConnector = AfterAttachment;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Found junction %s after pump/valve %s"),
                            *AfterActor->GetName(), *Attachment->GetName());
                        return true;
                    }
                    // If the next thing after the attachment is yet another attachment,
                    // fall through — the next while-iteration's Cast<AFGBuildablePipelinePump>
                    // can't run because CurrentPipe is required to be valid at loop head.
                    // Accepting break here is fine: multi-attachment chains (valve → valve)
                    // are rare and produce a captured chain up to the first attachment. If
                    // users hit this in the wild, extend to a recursive helper.
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Pump/valve %s has no usable connector on the other side — stopping walk"),
                *Attachment->GetName());
            break;
        }

        // It's something else (factory, other attachment, etc.) - stop here
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe chain ended at non-pipe/non-junction/non-attachment: %s"), 
            *NextActor->GetClass()->GetName());
        break;
    }

    // Pipeline(s) found but no valid junction at the end
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe chain walked %d pipes, no junction found"), VisitedPipes.Num());
    return OutNode.Pipelines.Num() > 0;
}

const FSFExtendTopology& USFExtendTopologyService::GetCurrentTopology() const
{
    return CachedTopology;
}

void USFExtendTopologyService::ClearTopology()
{
    CachedTopology.Reset();
}

bool USFExtendTopologyService::HasValidTopology() const
{
    return CachedTopology.bIsValid;
}

// ==================== Helper Methods ====================

bool USFExtendTopologyService::IsDistributor(AFGBuildable* Building) const
{
    if (!IsValid(Building))
    {
        return false;
    }

    // Check if it's a conveyor attachment (merger/splitter)
    if (Cast<AFGBuildableConveyorAttachment>(Building) != nullptr)
    {
        return true;
    }

    return false;
}

bool USFExtendTopologyService::IsPipeJunction(AFGBuildable* Building) const
{
    if (!IsValid(Building))
    {
        return false;
    }

    if (Cast<AFGBuildablePipelineJunction>(Building) != nullptr)
    {
        return true;
    }

    return false;
}

// ==================== Power Extend (Issue #229) ====================

FBox USFExtendTopologyService::CalculateManifoldBounds(float Padding) const
{
    FBox Bounds(ForceInit);
    
    // Include source factory
    if (CachedTopology.SourceBuilding.IsValid())
    {
        Bounds += CachedTopology.SourceBuilding->GetActorLocation();
    }
    
    // Include all belt chain infrastructure
    for (const FSFConnectionChainNode& Chain : CachedTopology.InputChains)
    {
        for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Conveyor : Chain.Conveyors)
        {
            if (Conveyor.IsValid())
            {
                Bounds += Conveyor->GetActorLocation();
            }
        }
        if (Chain.Distributor.IsValid())
        {
            Bounds += Chain.Distributor->GetActorLocation();
        }
    }
    for (const FSFConnectionChainNode& Chain : CachedTopology.OutputChains)
    {
        for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Conveyor : Chain.Conveyors)
        {
            if (Conveyor.IsValid())
            {
                Bounds += Conveyor->GetActorLocation();
            }
        }
        if (Chain.Distributor.IsValid())
        {
            Bounds += Chain.Distributor->GetActorLocation();
        }
    }
    
    // Include all pipe chain infrastructure
    for (const FSFPipeConnectionChainNode& Chain : CachedTopology.PipeInputChains)
    {
        for (const TWeakObjectPtr<AFGBuildablePipeline>& Pipe : Chain.Pipelines)
        {
            if (Pipe.IsValid())
            {
                Bounds += Pipe->GetActorLocation();
            }
        }
        if (Chain.Junction.IsValid())
        {
            Bounds += Chain.Junction->GetActorLocation();
        }
    }
    for (const FSFPipeConnectionChainNode& Chain : CachedTopology.PipeOutputChains)
    {
        for (const TWeakObjectPtr<AFGBuildablePipeline>& Pipe : Chain.Pipelines)
        {
            if (Pipe.IsValid())
            {
                Bounds += Pipe->GetActorLocation();
            }
        }
        if (Chain.Junction.IsValid())
        {
            Bounds += Chain.Junction->GetActorLocation();
        }
    }
    
    // Apply padding
    if (Bounds.IsValid)
    {
        Bounds = Bounds.ExpandBy(Padding);
    }
    
    return Bounds;
}

void USFExtendTopologyService::WalkPowerConnections(AFGBuildable* SourceBuilding)
{
    if (!IsValid(SourceBuilding))
    {
        return;
    }
    
    // Calculate the manifold bounding box from already-captured belt/pipe topology
    FBox ManifoldBounds = CalculateManifoldBounds(2000.0f); // 20m padding
    
    if (!ManifoldBounds.IsValid)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Power walk skipped - no valid manifold bounds"));
        return;
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Walking power connections for %s (bounds: min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f))"),
        *SourceBuilding->GetName(),
        ManifoldBounds.Min.X, ManifoldBounds.Min.Y, ManifoldBounds.Min.Z,
        ManifoldBounds.Max.X, ManifoldBounds.Max.Y, ManifoldBounds.Max.Z);
    
    // Get power connection components on the factory
    TArray<UFGPowerConnectionComponent*> PowerConnections;
    SourceBuilding->GetComponents<UFGPowerConnectionComponent>(PowerConnections);
    
    if (PowerConnections.Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: No power connections on factory - skipping power walk"));
        return;
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Factory has %d power connection(s)"), PowerConnections.Num());
    
    for (UFGPowerConnectionComponent* FactoryPowerConn : PowerConnections)
    {
        if (!IsValid(FactoryPowerConn))
        {
            continue;
        }
        
        // Get wires connected to this power connection
        TArray<AFGBuildableWire*> Wires;
        FactoryPowerConn->GetWires(Wires);
        
        for (AFGBuildableWire* Wire : Wires)
        {
            if (!IsValid(Wire))
            {
                continue;
            }
            
            // Find the other end of the wire
            UFGCircuitConnectionComponent* Conn0 = Wire->GetConnection(0);
            UFGCircuitConnectionComponent* Conn1 = Wire->GetConnection(1);
            
            UFGCircuitConnectionComponent* OtherConn = (Conn0 == FactoryPowerConn) ? Conn1 : Conn0;
            if (!IsValid(OtherConn))
            {
                continue;
            }
            
            AActor* OtherActor = OtherConn->GetOwner();
            AFGBuildablePowerPole* PowerPole = Cast<AFGBuildablePowerPole>(OtherActor);
            if (!IsValid(PowerPole))
            {
                continue;
            }
            
            // Check if power pole is within the manifold bounding box
            FVector PoleLocation = PowerPole->GetActorLocation();
            if (!ManifoldBounds.IsInsideOrOn(PoleLocation))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:   Power pole %s at (%.0f,%.0f,%.0f) is OUTSIDE manifold bounds - skipped"),
                    *PowerPole->GetName(), PoleLocation.X, PoleLocation.Y, PoleLocation.Z);
                continue;
            }
            
            // Power pole is within bounds - capture it
            FSFPowerChainNode PowerNode;
            PowerNode.PowerPole = PowerPole;
            PowerNode.Wire = Wire;
            PowerNode.FactoryConnector = FactoryPowerConn;
            PowerNode.PoleConnector = OtherConn;
            PowerNode.RelativeOffset = PoleLocation - SourceBuilding->GetActorLocation();
            
            // Determine pole tier (max connections) and count current connections
            // in one pass over the pole's circuit connectors. Reading the live
            // value via GetMaxNumConnections() is authoritative and automatically
            // correct for all pole variants — standard poles (Mk.1/Mk.2/Mk.3),
            // wall outlets single and double (which match their standard-pole
            // tier per-instance: WallMk1=4, WallMk2=7, WallMk3=10), and any
            // future tiers or modded pole classes. The earlier string-match on
            // "PowerPoleMk2"/"PowerPoleMk3" missed wall-outlet classes named
            // "PowerPoleWallMk2"/"PowerPoleWallMk3" (the "Wall" token breaks the
            // substring check), capping them at 4 and making ValidatePowerCapacity
            // over-restrictive for wall-outlet manifolds. (Issue #288)
            TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
            PowerPole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);
            int32 ConnectedCount = 0;
            int32 MaxConnectionsOnPole = 4;  // Safe default if the pole has no circuit conns (shouldn't happen)
            for (UFGCircuitConnectionComponent* Conn : PoleCircuitConns)
            {
                if (!Conn) continue;
                MaxConnectionsOnPole = FMath::Max(MaxConnectionsOnPole, Conn->GetMaxNumConnections());
                if (Conn->IsConnected())
                {
                    ConnectedCount++;
                }
            }
            PowerNode.MaxConnections = MaxConnectionsOnPole;
            PowerNode.SourceFreeConnections = PowerNode.MaxConnections - ConnectedCount;
            PowerNode.bSourceHasFreeConnections = (PowerNode.SourceFreeConnections > 0);
            
            CachedTopology.PowerPoles.Add(PowerNode);
            
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!:   Power pole %s at (%.0f,%.0f,%.0f) INSIDE bounds - captured (tier max=%d, used=%d, free=%d)"),
                *PowerPole->GetName(), PoleLocation.X, PoleLocation.Y, PoleLocation.Z,
                PowerNode.MaxConnections, ConnectedCount, PowerNode.SourceFreeConnections);
        }
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("Smart!: Power walk complete - found %d power pole(s) within manifold bounds"),
        CachedTopology.PowerPoles.Num());
}

void USFExtendTopologyService::DiscoverPipePassthroughs(AFGBuildable* SourceBuilding)
{
    if (!IsValid(SourceBuilding))
    {
        return;
    }
    
    UWorld* World = SourceBuilding->GetWorld();
    if (!World)
    {
        return;
    }
    
    // Use manifold bounds to define search area (same as power pole discovery)
    FBox ManifoldBounds = CalculateManifoldBounds(500.0f); // 5m padding
    if (!ManifoldBounds.IsValid)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Passthrough discovery skipped - no valid manifold bounds"));
        return;
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Discovering passthroughs near factory (bounds: min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f))"),
        ManifoldBounds.Min.X, ManifoldBounds.Min.Y, ManifoldBounds.Min.Z,
        ManifoldBounds.Max.X, ManifoldBounds.Max.Y, ManifoldBounds.Max.Z);

    // Issue #283: Build a TSet of every conveyor/pipe actor that belongs to one of the chains
    // we already walked for THIS source. We use this set to filter the spatial sweep below: a
    // passthrough is only relevant to the current Extend if at least one of its snapped
    // connections points at a chain member we've just captured. Previously, every passthrough
    // inside the manifold bounds was cloned — so the first Extend's freshly-built passthroughs
    // sat inside the bounds of a second Extend, got re-discovered, and duplicated once more per
    // extension step. Filtering here makes each Extend clone exactly the passthroughs that
    // belong to the manifold the player is pointing at, regardless of neighbouring clones.
    TSet<const AActor*> ChainMembers;
    auto AddChainConveyors = [&ChainMembers](const TArray<FSFConnectionChainNode>& Chains)
    {
        for (const FSFConnectionChainNode& Chain : Chains)
        {
            for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Weak : Chain.Conveyors)
            {
                if (AActor* Actor = Weak.Get())
                {
                    ChainMembers.Add(Actor);
                }
            }
        }
    };
    auto AddChainPipes = [&ChainMembers](const TArray<FSFPipeConnectionChainNode>& Chains)
    {
        for (const FSFPipeConnectionChainNode& Chain : Chains)
        {
            for (const TWeakObjectPtr<AFGBuildablePipeline>& Weak : Chain.Pipelines)
            {
                if (AActor* Actor = Weak.Get())
                {
                    ChainMembers.Add(Actor);
                }
            }
        }
    };
    AddChainConveyors(CachedTopology.InputChains);
    AddChainConveyors(CachedTopology.OutputChains);
    AddChainPipes(CachedTopology.PipeInputChains);
    AddChainPipes(CachedTopology.PipeOutputChains);

    // Issue #260: Discover ALL passthroughs (both pipe and lift floor holes) within bounds.
    // Pipe floor holes are NOT in the pipe connection chain — pipes pass through physically.
    // Lift floor holes are NOT auto-created by Extend-cloned lifts (different construction flow).
    // Both types need explicit spatial discovery and cloning.
    int32 SkippedForeign = 0;
    for (TActorIterator<AFGBuildablePassthrough> It(World); It; ++It)
    {
        AFGBuildablePassthrough* Passthrough = *It;
        if (!IsValid(Passthrough))
        {
            continue;
        }
        
        FVector PassLocation = Passthrough->GetActorLocation();
        
        // Check if within manifold bounds
        if (!ManifoldBounds.IsInside(PassLocation))
        {
            continue;
        }

        // Issue #283: Verify this passthrough is actually snapped to a conveyor/pipe that's
        // part of THIS source's chains. If both snap slots are empty or both point at actors
        // outside our manifold (e.g. belts/pipes owned by a neighbouring clone from a prior
        // Extend), this passthrough is a bystander — do not clone it.
        const UFGConnectionComponent* TopConn = Passthrough->GetTopSnappedConnection<UFGConnectionComponent>();
        const UFGConnectionComponent* BottomConn = Passthrough->GetBottomSnappedConnection<UFGConnectionComponent>();

        const AActor* TopOwner = TopConn ? TopConn->GetOwner() : nullptr;
        const AActor* BottomOwner = BottomConn ? BottomConn->GetOwner() : nullptr;

        const bool bTopOwnedByChain = TopOwner && ChainMembers.Contains(TopOwner);
        const bool bBottomOwnedByChain = BottomOwner && ChainMembers.Contains(BottomOwner);

        if (!bTopOwnedByChain && !bBottomOwnedByChain)
        {
            SkippedForeign++;
            UE_LOG(LogSmartFoundations, Verbose,
                TEXT("🔧 EXTEND: Skipping foreign passthrough %s at (%.0f,%.0f,%.0f) — snapped to %s/%s, neither in current chains (#283)"),
                *Passthrough->GetName(), PassLocation.X, PassLocation.Y, PassLocation.Z,
                TopOwner ? *TopOwner->GetName() : TEXT("none"),
                BottomOwner ? *BottomOwner->GetName() : TEXT("none"));
            continue;
        }

        CachedTopology.PipePassthroughs.Add(Passthrough);
        
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Discovered passthrough %s (class=%s) at (%.0f,%.0f,%.0f)"),
            *Passthrough->GetName(), *Passthrough->GetClass()->GetName(),
            PassLocation.X, PassLocation.Y, PassLocation.Z);
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Passthrough discovery complete - found %d floor hole(s) (skipped %d foreign passthrough(s) not owned by current manifold, #283)"),
        CachedTopology.PipePassthroughs.Num(), SkippedForeign);
}

void USFExtendTopologyService::DiscoverWallPassthroughs(AFGBuildable* SourceBuilding)
{
    if (!IsValid(SourceBuilding))
    {
        return;
    }

    UWorld* World = SourceBuilding->GetWorld();
    if (!World)
    {
        return;
    }

    // Wall holes (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C) are Blueprint-only
    // decorator buildables — no factory/pipe connection components, no passthrough base class.
    // They snap beside a belt or pipe that passes through a wall and occupy a snap point on the
    // wall itself. We cannot use connection-based ownership (#283 pattern) because there are no
    // connections; instead we use spatial overlap with captured chain belts/pipes.
    FBox ManifoldBounds = CalculateManifoldBounds(500.0f); // 5m padding
    if (!ManifoldBounds.IsValid)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Wall hole discovery skipped - no valid manifold bounds"));
        return;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Discovering wall holes near factory (bounds: min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f))"),
        ManifoldBounds.Min.X, ManifoldBounds.Min.Y, ManifoldBounds.Min.Z,
        ManifoldBounds.Max.X, ManifoldBounds.Max.Y, ManifoldBounds.Max.Z);

    // Step 1: Gather every connection-point world location in this source's chains.
    //
    // When a player builds a belt or pipe through a wall, they place the wall hole first,
    // then snap belt A to one face and belt B to the opposite face. Belt A's Output connector
    // and Belt B's Input connector end up co-located at the wall hole's center plane, and
    // are directly wired connector-to-connector. The wall hole is a SnapOnly decoration that
    // sits at that shared connector location but is not part of the connection graph.
    //
    // So: probe every connector on every chain belt/pipe actor. A wall hole for our manifold
    // will sit within ~150cm of one of these points (belt connectors are ~50cm from the wall
    // hole's center plane; safe margin covers Mk variants and Pipeline support offsets).
    TArray<FVector> JunctionPoints;
    JunctionPoints.Reserve(64);

    auto GatherBeltConnectors = [&JunctionPoints](const TArray<FSFConnectionChainNode>& Chains)
    {
        for (const FSFConnectionChainNode& Chain : Chains)
        {
            for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Weak : Chain.Conveyors)
            {
                AFGBuildableConveyorBase* Belt = Weak.Get();
                if (!IsValid(Belt)) continue;

                TArray<UFGFactoryConnectionComponent*> Connectors;
                Belt->GetComponents<UFGFactoryConnectionComponent>(Connectors);
                for (UFGFactoryConnectionComponent* C : Connectors)
                {
                    if (C)
                    {
                        JunctionPoints.Add(C->GetComponentLocation());
                    }
                }
            }
        }
    };
    auto GatherPipeConnectors = [&JunctionPoints](const TArray<FSFPipeConnectionChainNode>& Chains)
    {
        for (const FSFPipeConnectionChainNode& Chain : Chains)
        {
            for (const TWeakObjectPtr<AFGBuildablePipeline>& Weak : Chain.Pipelines)
            {
                AFGBuildablePipeline* Pipe = Weak.Get();
                if (!IsValid(Pipe)) continue;

                TArray<UFGPipeConnectionComponent*> Connectors;
                Pipe->GetComponents<UFGPipeConnectionComponent>(Connectors);
                for (UFGPipeConnectionComponent* C : Connectors)
                {
                    if (C)
                    {
                        JunctionPoints.Add(C->GetComponentLocation());
                    }
                }
            }
        }
    };
    GatherBeltConnectors(CachedTopology.InputChains);
    GatherBeltConnectors(CachedTopology.OutputChains);
    GatherPipeConnectors(CachedTopology.PipeInputChains);
    GatherPipeConnectors(CachedTopology.PipeOutputChains);

    if (JunctionPoints.Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Wall hole discovery skipped - no chain connectors to probe"));
        return;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Probing %d chain connector(s) for adjacent wall holes"),
        JunctionPoints.Num());

    // Step 2: Single world sweep to collect wall-hole candidates inside the manifold bounds.
    // Class-name suffix filter covers Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C,
    // and any future Mk variants ending in "WallHole_C".
    TArray<AFGBuildable*> Candidates;
    int32 Scanned = 0;
    for (TActorIterator<AFGBuildable> It(World); It; ++It)
    {
        AFGBuildable* Buildable = *It;
        if (!IsValid(Buildable))
        {
            continue;
        }
        Scanned++;

        if (!Buildable->GetClass()->GetName().EndsWith(TEXT("WallHole_C")))
        {
            continue;
        }

        if (!ManifoldBounds.IsInside(Buildable->GetActorLocation()))
        {
            continue;
        }

        Candidates.Add(Buildable);
    }

    if (Candidates.Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Wall hole discovery complete - no wall-hole candidates in manifold bounds (scanned %d buildables)"),
            Scanned);
        return;
    }

    // Step 3: Accept a wall hole if ANY chain connector is within ProbeRadius of its center.
    // 150cm covers belt-connector offset (~50cm from wall-hole center plane) plus some margin
    // for Pipeline support variants and snap tolerances.
    constexpr float ProbeRadiusCm = 150.0f;
    constexpr float ProbeRadiusSqCm = ProbeRadiusCm * ProbeRadiusCm;

    int32 SkippedNoConnector = 0;
    for (AFGBuildable* Candidate : Candidates)
    {
        const FVector WallLoc = Candidate->GetActorLocation();

        // Find the nearest chain connector.
        float BestDistSq = TNumericLimits<float>::Max();
        for (const FVector& ConnectorLoc : JunctionPoints)
        {
            const float DistSq = FVector::DistSquared(WallLoc, ConnectorLoc);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
            }
        }

        if (BestDistSq > ProbeRadiusSqCm)
        {
            SkippedNoConnector++;
            UE_LOG(LogSmartFoundations, Verbose,
                TEXT("🔧 EXTEND: Skipping wall hole %s (class=%s) at (%.0f,%.0f,%.0f) — nearest chain connector %.0fcm away (> %.0fcm)"),
                *Candidate->GetName(), *Candidate->GetClass()->GetName(),
                WallLoc.X, WallLoc.Y, WallLoc.Z,
                FMath::Sqrt(BestDistSq), ProbeRadiusCm);
            continue;
        }

        CachedTopology.WallPassthroughs.Add(Candidate);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Discovered wall hole %s (class=%s) at (%.0f,%.0f,%.0f) — nearest connector %.0fcm away"),
            *Candidate->GetName(), *Candidate->GetClass()->GetName(),
            WallLoc.X, WallLoc.Y, WallLoc.Z, FMath::Sqrt(BestDistSq));
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Wall hole discovery complete - found %d wall hole(s) (scanned %d buildables, %d candidates in bounds, skipped %d without nearby connector)"),
        CachedTopology.WallPassthroughs.Num(), Scanned, Candidates.Num(), SkippedNoConnector);
}
