#include "Features/Extend/SFWiringManifest.h"
#include "Features/Extend/SFManifoldJSON.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFChainActorService.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "FGFluidIntegrantInterface.h"
#include "FGConveyorChainActor.h"
#include "FGBuildableSubsystem.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// ============================================================================
// FSFWiringManifest - Generate from clone topology
// ============================================================================

FSFWiringManifest FSFWiringManifest::Generate(
    const FSFCloneTopology& CloneTopology,
    const TMap<FString, AActor*>& CloneIdToBuildable,
    AFGBuildableFactory* ParentFactory)
{
    FSFWiringManifest Manifest;
    
    // Set timestamp
    Manifest.Timestamp = FDateTime::UtcNow().ToIso8601();
    
    // Set parent factory info
    if (ParentFactory)
    {
        Manifest.ParentActorName = ParentFactory->GetName();
        Manifest.ParentClass = ParentFactory->GetClass()->GetName();
        Manifest.ParentLocation = ParentFactory->GetActorLocation();
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRING MANIFEST: Generating from %d child holograms, %d mapped buildables"),
        CloneTopology.ChildHolograms.Num(), CloneIdToBuildable.Num());
    
    // Build the BuiltBuildables array from the mapping
    for (const auto& Pair : CloneIdToBuildable)
    {
        const FString& CloneId = Pair.Key;
        AActor* Buildable = Pair.Value;
        
        if (!Buildable)
        {
            continue;
        }
        
        FSFBuiltBuildable Entry;
        Entry.CloneId = CloneId;
        Entry.ActorName = Buildable->GetName();
        Entry.BuildClass = Buildable->GetClass()->GetName();
        Entry.Location = Buildable->GetActorLocation();
        Entry.ResolvedActor = Buildable;
        
        // Determine role from class
        if (Buildable->IsA<AFGBuildableConveyorBelt>())
        {
            Entry.Role = TEXT("belt_segment");
            Manifest.BeltSegments++;
        }
        else if (Buildable->IsA<AFGBuildableConveyorLift>())
        {
            Entry.Role = TEXT("lift_segment");
            Manifest.LiftSegments++;
        }
        else if (Buildable->GetClass()->GetName().Contains(TEXT("Splitter")) ||
                 Buildable->GetClass()->GetName().Contains(TEXT("Merger")))
        {
            Entry.Role = TEXT("distributor");
            Manifest.Distributors++;
        }
        else if (Buildable->IsA<AFGBuildablePipeline>())
        {
            Entry.Role = TEXT("pipe_segment");
            Manifest.PipeSegments++;
        }
        else if (Buildable->GetClass()->GetName().Contains(TEXT("Junction")))
        {
            Entry.Role = TEXT("junction");
            Manifest.Junctions++;
        }
        else if (CloneId == TEXT("parent"))
        {
            Entry.Role = TEXT("parent_factory");
        }
        else
        {
            Entry.Role = TEXT("unknown");
        }
        
        Manifest.BuiltBuildables.Add(Entry);
    }
    
    Manifest.TotalBuildables = Manifest.BuiltBuildables.Num();
    
    // Generate connections from CloneTopology
    for (const FSFCloneHologram& ChildData : CloneTopology.ChildHolograms)
    {
        // Skip if this buildable wasn't mapped
        AActor* const* SourceBuildablePtr = CloneIdToBuildable.Find(ChildData.HologramId);
        if (!SourceBuildablePtr || !*SourceBuildablePtr)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING MANIFEST: Clone %s not found in buildable map"),
                *ChildData.HologramId);
            continue;
        }
        
        AActor* SourceBuildable = *SourceBuildablePtr;
        const FString& SourceActorName = SourceBuildable->GetName();
        
        // Process belt/lift connections (including belt lane segments)
        bool bIsBeltLane = (ChildData.Role == TEXT("lane_segment") && ChildData.LaneSegmentType == TEXT("belt"));
        if (ChildData.Role == TEXT("belt_segment") || ChildData.Role == TEXT("lift_segment") || bIsBeltLane)
        {
            // ConveyorAny0 connection
            if (!ChildData.CloneConnections.ConveyorAny0.Target.IsEmpty() &&
                ChildData.CloneConnections.ConveyorAny0.Target != TEXT("external"))
            {
                const FString& TargetCloneId = ChildData.CloneConnections.ConveyorAny0.Target;
                AActor* const* TargetBuildablePtr = CloneIdToBuildable.Find(TargetCloneId);
                
                if (TargetBuildablePtr && *TargetBuildablePtr)
                {
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("ConveyorAny0"));
                    FSFWiringEndpoint Target(TargetCloneId, (*TargetBuildablePtr)->GetName(), 
                        ChildData.CloneConnections.ConveyorAny0.Connector);
                    
                    Source.ResolvedActor = SourceBuildable;
                    Target.ResolvedActor = *TargetBuildablePtr;
                    
                    Manifest.BeltConnections.Add(FSFWiringConnection(Source, Target));
                }
            }
            
            // ConveyorAny1 connection
            if (!ChildData.CloneConnections.ConveyorAny1.Target.IsEmpty() &&
                ChildData.CloneConnections.ConveyorAny1.Target != TEXT("external"))
            {
                const FString& TargetCloneId = ChildData.CloneConnections.ConveyorAny1.Target;
                AActor* const* TargetBuildablePtr = CloneIdToBuildable.Find(TargetCloneId);
                
                if (TargetBuildablePtr && *TargetBuildablePtr)
                {
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("ConveyorAny1"));
                    FSFWiringEndpoint Target(TargetCloneId, (*TargetBuildablePtr)->GetName(),
                        ChildData.CloneConnections.ConveyorAny1.Connector);
                    
                    Source.ResolvedActor = SourceBuildable;
                    Target.ResolvedActor = *TargetBuildablePtr;
                    
                    Manifest.BeltConnections.Add(FSFWiringConnection(Source, Target));
                }
            }
        }
        // Process pipe connections (pipe_segment and pipe lane_segments only)
        else if (ChildData.Role == TEXT("pipe_segment") || (ChildData.Role == TEXT("lane_segment") && !bIsBeltLane))
        {
            // For lane_segment, check if it's a pipe lane (has pipe connections)
            // Lane segments connecting to source junctions need special handling
            bool bIsPipeLane = ChildData.Role == TEXT("lane_segment") && 
                (ChildData.CloneConnections.ConveyorAny0.Target.StartsWith(TEXT("source:")) ||
                 ChildData.CloneConnections.ConveyorAny1.Target.StartsWith(TEXT("source:")));
            
            // PipelineConnection0
            if (!ChildData.CloneConnections.ConveyorAny0.Target.IsEmpty() &&
                ChildData.CloneConnections.ConveyorAny0.Target != TEXT("external"))
            {
                const FString& TargetCloneId = ChildData.CloneConnections.ConveyorAny0.Target;
                AActor* const* TargetBuildablePtr = CloneIdToBuildable.Find(TargetCloneId);
                
                if (TargetBuildablePtr && *TargetBuildablePtr)
                {
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("PipelineConnection0"));
                    FSFWiringEndpoint Target(TargetCloneId, (*TargetBuildablePtr)->GetName(),
                        ChildData.CloneConnections.ConveyorAny0.Connector);
                    
                    Source.ResolvedActor = SourceBuildable;
                    Target.ResolvedActor = *TargetBuildablePtr;
                    
                    Manifest.PipeConnections.Add(FSFWiringConnection(Source, Target));
                }
                else if (TargetCloneId.StartsWith(TEXT("source:")))
                {
                    // Lane segment connecting to source junction - mark for deferred resolution
                    // The target is a source buildable, not a cloned one
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("PipelineConnection0"));
                    FSFWiringEndpoint Target(TargetCloneId, TEXT(""), ChildData.CloneConnections.ConveyorAny0.Connector);
                    Target.bIsSourceBuildable = true;  // Mark as source buildable for Execute() to resolve
                    
                    Source.ResolvedActor = SourceBuildable;
                    // Target.ResolvedActor will be resolved during Execute() using ExtendService
                    
                    Manifest.PipeConnections.Add(FSFWiringConnection(Source, Target));
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRING MANIFEST: Added lane->source connection %s.Conn0 -> %s.%s"),
                        *ChildData.HologramId, *TargetCloneId, *ChildData.CloneConnections.ConveyorAny0.Connector);
                }
            }
            
            // PipelineConnection1
            if (!ChildData.CloneConnections.ConveyorAny1.Target.IsEmpty() &&
                ChildData.CloneConnections.ConveyorAny1.Target != TEXT("external"))
            {
                const FString& TargetCloneId = ChildData.CloneConnections.ConveyorAny1.Target;
                AActor* const* TargetBuildablePtr = CloneIdToBuildable.Find(TargetCloneId);
                
                if (TargetBuildablePtr && *TargetBuildablePtr)
                {
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("PipelineConnection1"));
                    FSFWiringEndpoint Target(TargetCloneId, (*TargetBuildablePtr)->GetName(),
                        ChildData.CloneConnections.ConveyorAny1.Connector);
                    
                    Source.ResolvedActor = SourceBuildable;
                    Target.ResolvedActor = *TargetBuildablePtr;
                    
                    Manifest.PipeConnections.Add(FSFWiringConnection(Source, Target));
                }
                else if (TargetCloneId.StartsWith(TEXT("source:")))
                {
                    // Lane segment connecting to source junction - mark for deferred resolution
                    FSFWiringEndpoint Source(ChildData.HologramId, SourceActorName, TEXT("PipelineConnection1"));
                    FSFWiringEndpoint Target(TargetCloneId, TEXT(""), ChildData.CloneConnections.ConveyorAny1.Connector);
                    Target.bIsSourceBuildable = true;
                    
                    Source.ResolvedActor = SourceBuildable;
                    
                    Manifest.PipeConnections.Add(FSFWiringConnection(Source, Target));
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRING MANIFEST: Added lane->source connection %s.Conn1 -> %s.%s"),
                        *ChildData.HologramId, *TargetCloneId, *ChildData.CloneConnections.ConveyorAny1.Connector);
                }
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRING MANIFEST: Generated %d belt connections, %d pipe connections"),
        Manifest.BeltConnections.Num(), Manifest.PipeConnections.Num());
    
    return Manifest;
}

// ============================================================================
// FSFWiringManifest - Save to JSON
// ============================================================================

bool FSFWiringManifest::SaveToFile(const FString& FilePath) const
{
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    
    RootObject->SetStringField(TEXT("schema_version"), SchemaVersion);
    RootObject->SetStringField(TEXT("timestamp"), Timestamp);
    
    // Parent factory
    TSharedRef<FJsonObject> ParentObj = MakeShared<FJsonObject>();
    ParentObj->SetStringField(TEXT("actor_name"), ParentActorName);
    ParentObj->SetStringField(TEXT("class"), ParentClass);
    TSharedRef<FJsonObject> ParentLocObj = MakeShared<FJsonObject>();
    ParentLocObj->SetNumberField(TEXT("x"), ParentLocation.X);
    ParentLocObj->SetNumberField(TEXT("y"), ParentLocation.Y);
    ParentLocObj->SetNumberField(TEXT("z"), ParentLocation.Z);
    ParentObj->SetObjectField(TEXT("location"), ParentLocObj);
    RootObject->SetObjectField(TEXT("parent_factory"), ParentObj);
    
    // Built buildables
    TArray<TSharedPtr<FJsonValue>> BuildablesArray;
    for (const FSFBuiltBuildable& Entry : BuiltBuildables)
    {
        TSharedRef<FJsonObject> EntryObj = MakeShared<FJsonObject>();
        EntryObj->SetStringField(TEXT("clone_id"), Entry.CloneId);
        EntryObj->SetStringField(TEXT("actor_name"), Entry.ActorName);
        EntryObj->SetStringField(TEXT("class"), Entry.BuildClass);
        EntryObj->SetStringField(TEXT("role"), Entry.Role);
        TSharedRef<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Entry.Location.X);
        LocObj->SetNumberField(TEXT("y"), Entry.Location.Y);
        LocObj->SetNumberField(TEXT("z"), Entry.Location.Z);
        EntryObj->SetObjectField(TEXT("location"), LocObj);
        BuildablesArray.Add(MakeShared<FJsonValueObject>(EntryObj));
    }
    RootObject->SetArrayField(TEXT("built_buildables"), BuildablesArray);
    
    // Helper lambda for connection serialization
    auto SerializeConnection = [](const FSFWiringConnection& Conn) -> TSharedRef<FJsonObject>
    {
        TSharedRef<FJsonObject> ConnObj = MakeShared<FJsonObject>();
        ConnObj->SetStringField(TEXT("description"), Conn.Description);
        
        TSharedRef<FJsonObject> SourceObj = MakeShared<FJsonObject>();
        SourceObj->SetStringField(TEXT("clone_id"), Conn.Source.CloneId);
        SourceObj->SetStringField(TEXT("actor_name"), Conn.Source.ActorName);
        SourceObj->SetStringField(TEXT("connector"), Conn.Source.Connector);
        ConnObj->SetObjectField(TEXT("source"), SourceObj);
        
        TSharedRef<FJsonObject> TargetObj = MakeShared<FJsonObject>();
        TargetObj->SetStringField(TEXT("clone_id"), Conn.Target.CloneId);
        TargetObj->SetStringField(TEXT("actor_name"), Conn.Target.ActorName);
        TargetObj->SetStringField(TEXT("connector"), Conn.Target.Connector);
        ConnObj->SetObjectField(TEXT("target"), TargetObj);
        
        return ConnObj;
    };
    
    // Belt connections
    TArray<TSharedPtr<FJsonValue>> BeltConnsArray;
    for (const FSFWiringConnection& Conn : BeltConnections)
    {
        BeltConnsArray.Add(MakeShared<FJsonValueObject>(SerializeConnection(Conn)));
    }
    RootObject->SetArrayField(TEXT("belt_connections"), BeltConnsArray);
    
    // Pipe connections
    TArray<TSharedPtr<FJsonValue>> PipeConnsArray;
    for (const FSFWiringConnection& Conn : PipeConnections)
    {
        PipeConnsArray.Add(MakeShared<FJsonValueObject>(SerializeConnection(Conn)));
    }
    RootObject->SetArrayField(TEXT("pipe_connections"), PipeConnsArray);
    
    // Statistics
    TSharedRef<FJsonObject> StatsObj = MakeShared<FJsonObject>();
    StatsObj->SetNumberField(TEXT("total_buildables"), TotalBuildables);
    StatsObj->SetNumberField(TEXT("belt_segments"), BeltSegments);
    StatsObj->SetNumberField(TEXT("lift_segments"), LiftSegments);
    StatsObj->SetNumberField(TEXT("distributors"), Distributors);
    StatsObj->SetNumberField(TEXT("pipe_segments"), PipeSegments);
    StatsObj->SetNumberField(TEXT("junctions"), Junctions);
    StatsObj->SetNumberField(TEXT("belt_connections_to_wire"), BeltConnections.Num());
    StatsObj->SetNumberField(TEXT("pipe_connections_to_wire"), PipeConnections.Num());
    RootObject->SetObjectField(TEXT("statistics"), StatsObj);
    
    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject, Writer);
    
    // Save to file
    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

// ============================================================================
// FSFWiringManifest - Resolve actors
// ============================================================================

int32 FSFWiringManifest::ResolveActors(UWorld* World)
{
    if (!World)
    {
        return 0;
    }
    
    int32 ResolvedCount = 0;
    
    // Build a map of actor names to actors for fast lookup
    TMap<FString, AActor*> ActorNameMap;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor)
        {
            ActorNameMap.Add(Actor->GetName(), Actor);
        }
    }
    
    // Resolve built buildables
    for (FSFBuiltBuildable& Entry : BuiltBuildables)
    {
        if (!Entry.ResolvedActor)
        {
            if (AActor** Found = ActorNameMap.Find(Entry.ActorName))
            {
                Entry.ResolvedActor = *Found;
                ResolvedCount++;
            }
        }
    }
    
    // Resolve belt connections
    for (FSFWiringConnection& Conn : BeltConnections)
    {
        if (!Conn.Source.ResolvedActor)
        {
            if (AActor** Found = ActorNameMap.Find(Conn.Source.ActorName))
            {
                Conn.Source.ResolvedActor = *Found;
                ResolvedCount++;
            }
        }
        if (!Conn.Target.ResolvedActor)
        {
            if (AActor** Found = ActorNameMap.Find(Conn.Target.ActorName))
            {
                Conn.Target.ResolvedActor = *Found;
                ResolvedCount++;
            }
        }
    }
    
    // Resolve pipe connections
    for (FSFWiringConnection& Conn : PipeConnections)
    {
        if (!Conn.Source.ResolvedActor)
        {
            if (AActor** Found = ActorNameMap.Find(Conn.Source.ActorName))
            {
                Conn.Source.ResolvedActor = *Found;
                ResolvedCount++;
            }
        }
        if (!Conn.Target.ResolvedActor)
        {
            if (AActor** Found = ActorNameMap.Find(Conn.Target.ActorName))
            {
                Conn.Target.ResolvedActor = *Found;
                ResolvedCount++;
            }
        }
    }
    
    return ResolvedCount;
}

// ============================================================================
// FSFWiringManifest - Execute wiring
// ============================================================================

UFGFactoryConnectionComponent* FSFWiringManifest::FindBeltConnection(AActor* Actor, const FString& ConnectorName)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TArray<UFGFactoryConnectionComponent*> Connections;
    Actor->GetComponents<UFGFactoryConnectionComponent>(Connections);
    
    for (UFGFactoryConnectionComponent* Conn : Connections)
    {
        if (Conn && Conn->GetFName().ToString() == ConnectorName)
        {
            return Conn;
        }
    }
    
    // Try partial match for common patterns
    for (UFGFactoryConnectionComponent* Conn : Connections)
    {
        if (Conn)
        {
            FString ConnName = Conn->GetFName().ToString();
            if (ConnName.Contains(ConnectorName) || ConnectorName.Contains(ConnName))
            {
                return Conn;
            }
        }
    }
    
    return nullptr;
}

UFGPipeConnectionComponentBase* FSFWiringManifest::FindPipeConnection(AActor* Actor, const FString& ConnectorName)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TArray<UFGPipeConnectionComponentBase*> Connections;
    Actor->GetComponents<UFGPipeConnectionComponentBase>(Connections);
    
    for (UFGPipeConnectionComponentBase* Conn : Connections)
    {
        if (Conn && Conn->GetFName().ToString() == ConnectorName)
        {
            return Conn;
        }
    }
    
    // Try partial match
    for (UFGPipeConnectionComponentBase* Conn : Connections)
    {
        if (Conn)
        {
            FString ConnName = Conn->GetFName().ToString();
            if (ConnName.Contains(ConnectorName) || ConnectorName.Contains(ConnName))
            {
                return Conn;
            }
        }
    }
    
    return nullptr;
}

bool FSFWiringManifest::WireBeltConnection(const FSFWiringConnection& Connection, UWorld* World) const
{
    AActor* SourceActor = Connection.Source.ResolvedActor;
    AActor* TargetActor = Connection.Target.ResolvedActor;
    
    if (!SourceActor || !TargetActor)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE BELT: Failed - actors not resolved for %s"),
            *Connection.Description);
        return false;
    }
    
    UFGFactoryConnectionComponent* SourceConn = FindBeltConnection(SourceActor, Connection.Source.Connector);
    UFGFactoryConnectionComponent* TargetConn = FindBeltConnection(TargetActor, Connection.Target.Connector);
    
    if (!SourceConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE BELT: Source connector %s not found on %s"),
            *Connection.Source.Connector, *SourceActor->GetName());
        return false;
    }
    
    if (!TargetConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE BELT: Target connector %s not found on %s"),
            *Connection.Target.Connector, *TargetActor->GetName());
        return false;
    }
    
    // Check if already connected
    if (SourceConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE BELT: %s already connected, skipping"),
            *Connection.Description);
        return true; // Not a failure, just already done
    }
    
    // Check compatibility
    if (!SourceConn->CanConnectTo(TargetConn))
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE BELT: Cannot connect %s - incompatible"),
            *Connection.Description);
        return false;
    }
    
    // Wire the connection
    SourceConn->SetConnection(TargetConn);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE BELT: ✅ %s"), *Connection.Description);
    return true;
}

bool FSFWiringManifest::WirePipeConnection(const FSFWiringConnection& Connection, UWorld* World) const
{
    AActor* SourceActor = Connection.Source.ResolvedActor;
    AActor* TargetActor = Connection.Target.ResolvedActor;
    
    if (!SourceActor || !TargetActor)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: Failed - actors not resolved for %s"),
            *Connection.Description);
        return false;
    }
    
    UFGPipeConnectionComponentBase* SourceConn = FindPipeConnection(SourceActor, Connection.Source.Connector);
    UFGPipeConnectionComponentBase* TargetConn = FindPipeConnection(TargetActor, Connection.Target.Connector);
    
    if (!SourceConn)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: Source connector %s not found on %s"),
            *Connection.Source.Connector, *SourceActor->GetName());
        return false;
    }
    
    if (!TargetConn)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: Target connector %s not found on %s"),
            *Connection.Target.Connector, *TargetActor->GetName());
        return false;
    }
    
    // Check if already connected
    if (SourceConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: %s already connected, skipping"),
            *Connection.Description);
        return true;
    }
    
    // Check compatibility
    if (!SourceConn->CheckCompatibility(TargetConn, nullptr))
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: Cannot connect %s - incompatible"),
            *Connection.Description);
        return false;
    }
    
    // Wire the connection
    SourceConn->SetConnection(TargetConn);
    
    // Handle pipe network merging
    // Cast to UFGPipeConnectionComponent to access GetPipeNetworkID()
    UFGPipeConnectionComponent* SourceNetworkConn = Cast<UFGPipeConnectionComponent>(SourceConn);
    UFGPipeConnectionComponent* TargetNetworkConn = Cast<UFGPipeConnectionComponent>(TargetConn);
    
    if (World && SourceNetworkConn && TargetNetworkConn)
    {
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
        {
            int32 SourceNetworkID = SourceNetworkConn->GetPipeNetworkID();
            int32 TargetNetworkID = TargetNetworkConn->GetPipeNetworkID();
            
            if (SourceNetworkID != TargetNetworkID && SourceNetworkID != INDEX_NONE && TargetNetworkID != INDEX_NONE)
            {
                AFGPipeNetwork* SourceNetwork = PipeSubsystem->FindPipeNetwork(SourceNetworkID);
                AFGPipeNetwork* TargetNetwork = PipeSubsystem->FindPipeNetwork(TargetNetworkID);
                
                if (SourceNetwork && TargetNetwork && SourceNetwork->IsValidLowLevel() && TargetNetwork->IsValidLowLevel())
                {
                    SourceNetwork->MergeNetworks(TargetNetwork);
                    SourceNetwork->MarkForFullRebuild();
                }
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE PIPE: ✅ %s"), *Connection.Description);
    return true;
}

int32 FSFWiringManifest::ExecuteWiring(UWorld* World)
{
    int32 SuccessCount = 0;
    int32 FailCount = 0;
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXECUTE WIRING: Starting BATCH approach (Epp's AutoLink pattern) - %d belt connections, %d pipe connections"),
        BeltConnections.Num(), PipeConnections.Num());
    
    // ========================================================================
    // BATCH BELT WIRING: Epp's AutoLink approach
    // 1. Collect ALL conveyors involved
    // 2. Remove ALL from subsystem (invalidates chains)
    // 3. Make ALL connections
    // 4. Re-add ALL to subsystem (triggers single chain rebuild)
    // This is fastest - one chain rebuild at the end instead of per-connection
    // ========================================================================
    
    AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
    if (!BuildableSubsystem)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("🔌 EXECUTE WIRING: Failed to get BuildableSubsystem!"));
        return 0;
    }
    
    // Step 1: Collect all unique conveyors involved in belt connections
    TSet<AFGBuildableConveyorBase*> ConveyorsToRewire;
    TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>> ConnectionsToMake;
    
    for (const FSFWiringConnection& Conn : BeltConnections)
    {
        AActor* SourceActor = Conn.Source.ResolvedActor;
        AActor* TargetActor = Conn.Target.ResolvedActor;
        
        if (!SourceActor || !TargetActor)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 BATCH: Skipping %s - actors not resolved"), *Conn.Description);
            FailCount++;
            continue;
        }
        
        UFGFactoryConnectionComponent* SourceConn = FindBeltConnection(SourceActor, Conn.Source.Connector);
        UFGFactoryConnectionComponent* TargetConn = FindBeltConnection(TargetActor, Conn.Target.Connector);
        
        if (!SourceConn || !TargetConn)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 BATCH: Skipping %s - connectors not found"), *Conn.Description);
            FailCount++;
            continue;
        }
        
        // Skip if already connected
        if (SourceConn->IsConnected())
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 BATCH: %s already connected, skipping"), *Conn.Description);
            SuccessCount++;
            continue;
        }
        
        // Collect conveyors (belts and lifts only - not distributors/factories)
        AFGBuildableConveyorBase* SourceConveyor = Cast<AFGBuildableConveyorBase>(SourceActor);
        AFGBuildableConveyorBase* TargetConveyor = Cast<AFGBuildableConveyorBase>(TargetActor);
        
        if (SourceConveyor) ConveyorsToRewire.Add(SourceConveyor);
        if (TargetConveyor) ConveyorsToRewire.Add(TargetConveyor);
        
        // Queue the connection
        ConnectionsToMake.Add(TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>(SourceConn, TargetConn));
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 BATCH: Collected %d conveyors, %d connections to make"),
        ConveyorsToRewire.Num(), ConnectionsToMake.Num());
    
    // ========================================================================
    // HOOK-BASED APPROACH: Make connections NOW, chain rebuild via SML hook
    // 
    // Chain actor rebuilding is now handled by AFGBlueprintHologram::Construct
    // hook in SFGameInstanceModule. That hook runs DURING construction (like
    // AutoLink), before factory tick starts on these conveyors.
    // 
    // We just need to make the connections here - the hook will do Remove→Add.
    // ========================================================================
    
    // Step 2: Make ALL connections (chain rebuild handled by hook)
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ BATCH Step 2: Making %d connections (chain rebuild via hook)..."), ConnectionsToMake.Num());
    
    for (const auto& ConnPair : ConnectionsToMake)
    {
        UFGFactoryConnectionComponent* SourceConn = ConnPair.Key;
        UFGFactoryConnectionComponent* TargetConn = ConnPair.Value;
        
        if (!SourceConn || !TargetConn || SourceConn->IsConnected())
        {
            continue;
        }
        
        // Make the connection - chain rebuild handled by AFGBlueprintHologram::Construct hook
        SourceConn->SetConnection(TargetConn);
        SuccessCount++;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 BATCH: Connected %s → %s"),
            *SourceConn->GetOwner()->GetName(), *TargetConn->GetOwner()->GetName());
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ BATCH: %d connections made."), SuccessCount);
    
    // ========================================================================
    // NOTE: Chain rebuild is now handled in ConfigureComponents (pre-tick)
    // 
    // Belt holograms have connection target info stored in FSFHologramData.
    // During ConfigureComponents, they look up already-built targets and
    // establish connections + do Remove→Add. This is the same timing as
    // AutoLink and is safe from parallel factory tick crashes.
    // 
    // Any connections not made during ConfigureComponents (because target
    // wasn't built yet) will be handled here as a fallback, but without
    // the Remove→Add which causes crashes.
    // ========================================================================
    
    // ========================================================================
    // PIPE WIRING: Standard approach (no chain actor issues)
    // ========================================================================
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXECUTE WIRING: Now processing %d pipe connections"), PipeConnections.Num());
    for (const FSFWiringConnection& Conn : PipeConnections)
    {
        if (WirePipeConnection(Conn, World))
        {
            SuccessCount++;
        }
        else
        {
            FailCount++;
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXECUTE WIRING: Complete - %d succeeded, %d failed"),
        SuccessCount, FailCount);
    
    return SuccessCount;
}

// ============================================================================
// FSFWiringManifest - Create chain actors for wired belts
// ============================================================================

int32 FSFWiringManifest::CreateChainActors(UWorld* World, const TMap<FString, AActor*>& AdditionalActors)
{
    // Issue #260: Use Archengius pattern (chain-level) instead of AddConveyor (bucket-level).
    //
    // Per CSS developer Archengius:
    //   "If you call RemoveChainActor on a chain it will remove the chain and
    //    automatically build a new one next frame"
    //
    // RemoveConveyorChainActor operates at the CHAIN level — the conveyor stays in its
    // tick group/bucket. Vanilla rebuilds from current topology on the next frame.
    // AddConveyor/RemoveConveyor operate at the BUCKET level — unsafe, causes
    // double-add corruption and bucket index -1 crashes.
    //
    // Strategy:
    // 1. Collect ALL built conveyors (belts + lifts)
    // 2. Collect their chain actors AND their neighbors' chain actors
    // 3. RemoveConveyorChainActor on each unique chain
    // 4. Vanilla rebuilds everything next frame
    
    if (!World)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ CHAIN REBUILD: No world context"));
        return 0;
    }
    
    AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
    if (!BuildableSubsystem)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ CHAIN REBUILD: No buildable subsystem"));
        return 0;
    }
    
    // Collect ALL built conveyors (belts AND lifts)
    TArray<AFGBuildableConveyorBase*> AllConveyors;
    
    // From BuiltBuildables (belt_segment, lift_segment from main manifold)
    for (const FSFBuiltBuildable& Entry : BuiltBuildables)
    {
        if (Entry.Role == TEXT("belt_segment") || Entry.Role == TEXT("lift_segment"))
        {
            if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Entry.ResolvedActor))
            {
                AllConveyors.AddUnique(Conveyor);
            }
        }
    }
    
    // From AdditionalActors (lane_segment, belt_segment, lift_segment from JSON build)
    for (const auto& Pair : AdditionalActors)
    {
        if (Pair.Key.StartsWith(TEXT("lane_segment")) || Pair.Key.StartsWith(TEXT("belt_segment")) || Pair.Key.StartsWith(TEXT("lift_segment")))
        {
            if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Pair.Value))
            {
                AllConveyors.AddUnique(Conveyor);
            }
        }
    }
    
    if (AllConveyors.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ CHAIN REBUILD: No conveyors to process"));
        return 0;
    }
    
    // Collect ALL affected chain actors — from our conveyors AND their neighbors —
    // and delegate invalidation + synchronous rebuild to USFChainActorService.
    // The service handles the Remove-then-Migrate-synchronously pattern proven
    // correct via live in-game testing (v29.2.2); see Services/SFChainActorService.h.
    TSet<AFGConveyorChainActor*> AffectedChains;

    for (AFGBuildableConveyorBase* Conveyor : AllConveyors)
    {
        if (!IsValid(Conveyor)) continue;

        if (AFGConveyorChainActor* Chain = Conveyor->GetConveyorChainActor())
        {
            AffectedChains.Add(Chain);
        }

        for (UFGFactoryConnectionComponent* Conn : { Conveyor->GetConnection0(), Conveyor->GetConnection1() })
        {
            if (Conn && Conn->GetConnection())
            {
                if (AFGBuildableConveyorBase* Neighbor = Cast<AFGBuildableConveyorBase>(Conn->GetConnection()->GetOwner()))
                {
                    if (AFGConveyorChainActor* NChain = Neighbor->GetConveyorChainActor())
                    {
                        AffectedChains.Add(NChain);
                    }
                }
            }
        }
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("⛓️ CHAIN REBUILD: %d conveyors, %d affected chains to rebuild"),
        AllConveyors.Num(), AffectedChains.Num());

    USFSubsystem* Subsystem = USFSubsystem::Get(World);
    USFChainActorService* ChainService = Subsystem ? Subsystem->GetChainActorService() : nullptr;
    if (!ChainService)
    {
        UE_LOG(LogSmartFoundations, Warning,
            TEXT("⛓️ CHAIN REBUILD: USFChainActorService unavailable — falling back to RemoveConveyorChainActor (vanilla rebuilds next frame; save-timing window not closed)"));
        int32 RemovedCount = 0;
        for (AFGConveyorChainActor* Chain : AffectedChains)
        {
            if (IsValid(Chain))
            {
                BuildableSubsystem->RemoveConveyorChainActor(Chain);
                ++RemovedCount;
            }
        }
        return RemovedCount;
    }

    const int32 MigratedCount = ChainService->InvalidateAndRebuildChains(AffectedChains);
    UE_LOG(LogSmartFoundations, Log, TEXT("⛓️ CHAIN REBUILD: rebuilt %d chain group(s) synchronously"), MigratedCount);

    return MigratedCount;
}

// ============================================================================
// FSFWiringManifest - Rebuild pipe networks after all pipes are built
// ============================================================================

int32 FSFWiringManifest::RebuildPipeNetworks(UWorld* World, const TMap<FString, AActor*>& AdditionalActors) const
{
    if (!World)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE NETWORKS: No world context"));
        return 0;
    }
    
    AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
    if (!PipeSubsystem)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE NETWORKS: No pipe subsystem"));
        return 0;
    }
    
    // Helper lambda to collect network IDs from an actor
    auto CollectNetworkIDs = [](AActor* Actor, TSet<int32>& OutNetworkIDs)
    {
        if (!Actor) return;
        
        // Try to get pipe connections from the actor
        TArray<UFGPipeConnectionComponent*> PipeConns;
        Actor->GetComponents<UFGPipeConnectionComponent>(PipeConns);
        
        for (UFGPipeConnectionComponent* PipeConn : PipeConns)
        {
            if (PipeConn)
            {
                int32 NetworkID = PipeConn->GetPipeNetworkID();
                if (NetworkID != INDEX_NONE)
                {
                    OutNetworkIDs.Add(NetworkID);
                }
            }
        }
    };
    
    // Collect all unique network IDs from pipes in this manifest
    TSet<int32> NetworkIDs;
    
    // From BuiltBuildables (pipes that went through WiringManifest)
    for (const FSFBuiltBuildable& Entry : BuiltBuildables)
    {
        if (Entry.Role == TEXT("pipe_segment") || Entry.Role == TEXT("junction"))
        {
            CollectNetworkIDs(Entry.ResolvedActor, NetworkIDs);
        }
    }
    
    // From AdditionalActors (includes lane segment pipes)
    for (const auto& Pair : AdditionalActors)
    {
        // Check if this is a pipe (lane segments have "lane_segment" in their ID)
        if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(Pair.Value))
        {
            CollectNetworkIDs(Pipeline, NetworkIDs);
        }
        // Also check for pipe junctions
        else if (Pair.Key.Contains(TEXT("distributor")) || Pair.Key.Contains(TEXT("junction")))
        {
            CollectNetworkIDs(Pair.Value, NetworkIDs);
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE NETWORKS: Found %d unique networks to rebuild"), NetworkIDs.Num());
    
    // Mark all networks for full rebuild
    int32 RebuiltCount = 0;
    for (int32 NetworkID : NetworkIDs)
    {
        AFGPipeNetwork* Network = PipeSubsystem->FindPipeNetwork(NetworkID);
        if (Network && Network->IsValidLowLevel())
        {
            Network->MarkForFullRebuild();
            RebuiltCount++;
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE NETWORKS: Marked network %d for rebuild"), NetworkID);
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE NETWORKS: Marked %d networks for rebuild"), RebuiltCount);
    return RebuiltCount;
}

// ============================================================================
// FSFWiringManifest - Organize connections by chain for sequential wiring
// ============================================================================

TArray<FSFOrderedBeltChain> FSFWiringManifest::OrganizeConnectionsByChain() const
{
    TArray<FSFOrderedBeltChain> OrderedChains;
    
    // Build lookup maps
    TMap<FString, const FSFWiringConnection*> ConnectionsByDescription;
    TSet<FString> ProcessedConnections;
    
    for (const FSFWiringConnection& Conn : BeltConnections)
    {
        ConnectionsByDescription.Add(Conn.Description, &Conn);
    }
    
    // Find all connections that involve distributors (chain endpoints)
    // and connections that involve the factory (parent)
    TArray<const FSFWiringConnection*> DistributorConnections;
    TArray<const FSFWiringConnection*> FactoryConnections;
    
    for (const FSFWiringConnection& Conn : BeltConnections)
    {
        if (Conn.Source.CloneId.StartsWith(TEXT("distributor_")) || 
            Conn.Target.CloneId.StartsWith(TEXT("distributor_")))
        {
            DistributorConnections.Add(&Conn);
        }
        if (Conn.Source.CloneId == TEXT("parent") || Conn.Target.CloneId == TEXT("parent"))
        {
            FactoryConnections.Add(&Conn);
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ ORGANIZE: Found %d distributor connections, %d factory connections"),
        DistributorConnections.Num(), FactoryConnections.Num());
    
    // For each distributor connection, trace the chain to the factory
    int32 ChainIndex = 0;
    for (const FSFWiringConnection* DistConn : DistributorConnections)
    {
        // Skip if already processed
        if (ProcessedConnections.Contains(DistConn->Description))
        {
            continue;
        }
        
        FSFOrderedBeltChain Chain;
        
        // Determine which end is the distributor
        FString DistributorId;
        FString FirstSegmentId;
        FString FirstSegmentConnector;
        
        if (DistConn->Source.CloneId.StartsWith(TEXT("distributor_")))
        {
            DistributorId = DistConn->Source.CloneId;
            FirstSegmentId = DistConn->Target.CloneId;
            FirstSegmentConnector = DistConn->Target.Connector;
            Chain.bIsInputChain = true; // Distributor outputs to belt -> input chain
        }
        else
        {
            DistributorId = DistConn->Target.CloneId;
            FirstSegmentId = DistConn->Source.CloneId;
            FirstSegmentConnector = DistConn->Source.Connector;
            Chain.bIsInputChain = false; // Belt outputs to distributor -> output chain
        }
        
        Chain.ChainId = FString::Printf(TEXT("%s_%d"), Chain.bIsInputChain ? TEXT("input") : TEXT("output"), ChainIndex++);
        Chain.DistributorCloneId = DistributorId;
        
        // Start with distributor connection
        Chain.OrderedConnections.Add(*DistConn);
        ProcessedConnections.Add(DistConn->Description);
        
        // Walk the chain from first segment toward factory
        FString CurrentSegmentId = FirstSegmentId;
        TSet<FString> VisitedSegments;
        VisitedSegments.Add(DistributorId);
        
        while (!CurrentSegmentId.IsEmpty() && CurrentSegmentId != TEXT("parent") && !VisitedSegments.Contains(CurrentSegmentId))
        {
            VisitedSegments.Add(CurrentSegmentId);
            
            // Find the next connection from this segment
            const FSFWiringConnection* NextConn = nullptr;
            FString NextSegmentId;
            
            for (const FSFWiringConnection& Conn : BeltConnections)
            {
                if (ProcessedConnections.Contains(Conn.Description))
                {
                    continue;
                }
                
                // Check if this connection involves current segment
                if (Conn.Source.CloneId == CurrentSegmentId && !VisitedSegments.Contains(Conn.Target.CloneId))
                {
                    NextConn = &Conn;
                    NextSegmentId = Conn.Target.CloneId;
                    break;
                }
                if (Conn.Target.CloneId == CurrentSegmentId && !VisitedSegments.Contains(Conn.Source.CloneId))
                {
                    NextConn = &Conn;
                    NextSegmentId = Conn.Source.CloneId;
                    break;
                }
            }
            
            if (NextConn)
            {
                Chain.OrderedConnections.Add(*NextConn);
                ProcessedConnections.Add(NextConn->Description);
                CurrentSegmentId = NextSegmentId;
            }
            else
            {
                break;
            }
        }
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ ORGANIZE: Chain %s has %d ordered connections (distributor=%s, isInput=%d)"),
            *Chain.ChainId, Chain.OrderedConnections.Num(), *Chain.DistributorCloneId, Chain.bIsInputChain ? 1 : 0);
        
        OrderedChains.Add(Chain);
    }
    
    return OrderedChains;
}

// ============================================================================
// FSFWiringManifest - Wire single belt with verification
// ============================================================================

bool FSFWiringManifest::WireSingleBeltAndVerify(const FSFWiringConnection& Connection, UWorld* World, 
    AFGConveyorChainActor*& OutChainActor) const
{
    OutChainActor = nullptr;
    
    // Resolve actors
    AActor* SourceActor = Connection.Source.ResolvedActor;
    AActor* TargetActor = Connection.Target.ResolvedActor;
    
    if (!SourceActor || !TargetActor)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ WIRE SINGLE: Failed - actors not resolved for %s"),
            *Connection.Description);
        return false;
    }
    
    UFGFactoryConnectionComponent* SourceConn = FindBeltConnection(SourceActor, Connection.Source.Connector);
    UFGFactoryConnectionComponent* TargetConn = FindBeltConnection(TargetActor, Connection.Target.Connector);
    
    if (!SourceConn || !TargetConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ WIRE SINGLE: Connectors not found for %s"),
            *Connection.Description);
        return false;
    }
    
    // Check if already connected
    if (SourceConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ WIRE SINGLE: %s already connected"),
            *Connection.Description);
        
        // Get chain actor from the conveyor
        AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(SourceActor);
        if (!Conveyor)
        {
            Conveyor = Cast<AFGBuildableConveyorBase>(TargetActor);
        }
        if (Conveyor)
        {
            OutChainActor = Conveyor->GetConveyorChainActor();
        }
        return true;
    }
    
    // Wire the connection
    SourceConn->SetConnection(TargetConn);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ WIRE SINGLE: ✅ Connected %s"), *Connection.Description);
    
    // Check chain actors on both sides
    AFGBuildableConveyorBase* SourceConveyor = Cast<AFGBuildableConveyorBase>(SourceActor);
    AFGBuildableConveyorBase* TargetConveyor = Cast<AFGBuildableConveyorBase>(TargetActor);
    
    AFGConveyorChainActor* SourceChain = SourceConveyor ? SourceConveyor->GetConveyorChainActor() : nullptr;
    AFGConveyorChainActor* TargetChain = TargetConveyor ? TargetConveyor->GetConveyorChainActor() : nullptr;
    
    // Log chain status
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ WIRE SINGLE: Chain status - Source=%s (segments=%d), Target=%s (segments=%d)"),
        SourceChain ? *SourceChain->GetName() : TEXT("NULL"),
        SourceChain ? SourceChain->GetNumChainSegments() : 0,
        TargetChain ? *TargetChain->GetName() : TEXT("NULL"),
        TargetChain ? TargetChain->GetNumChainSegments() : 0);
    
    // Check if chains are unified
    if (SourceChain && TargetChain)
    {
        if (SourceChain == TargetChain)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ WIRE SINGLE: ✅ UNIFIED - both conveyors share chain %s"),
                *SourceChain->GetName());
            OutChainActor = SourceChain;
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ WIRE SINGLE: ⚠️ FRAGMENTED - different chains! Source=%s, Target=%s"),
                *SourceChain->GetName(), *TargetChain->GetName());
            OutChainActor = SourceChain; // Return one of them
        }
    }
    else if (SourceChain)
    {
        OutChainActor = SourceChain;
    }
    else if (TargetChain)
    {
        OutChainActor = TargetChain;
    }
    
    return true;
}

// ============================================================================
// Deferred Chain Rebuild - runs on next frame to avoid factory tick race condition
// ============================================================================

void FSFWiringManifest::ScheduleDeferredChainRebuild(UWorld* World, const TArray<AFGBuildableConveyorBase*>& Conveyors)
{
    if (!World || Conveyors.Num() == 0)
    {
        return;
    }
    
    // Capture conveyors in weak pointers so they survive until the timer fires
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> WeakConveyors;
    for (AFGBuildableConveyorBase* Conveyor : Conveyors)
    {
        if (Conveyor && Conveyor->IsValidLowLevel())
        {
            WeakConveyors.Add(Conveyor);
        }
    }
    
    // Schedule on next tick using a timer
    FTimerHandle TimerHandle;
    World->GetTimerManager().SetTimerForNextTick([WeakConveyors, World]()
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ DEFERRED: Chain rebuild starting for %d conveyors..."), WeakConveyors.Num());
        
        AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
        if (!BuildableSubsystem)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ DEFERRED: Failed to get BuildableSubsystem!"));
            return;
        }
        
        // Collect valid conveyors
        TArray<AFGBuildableConveyorBase*> ValidConveyors;
        for (const TWeakObjectPtr<AFGBuildableConveyorBase>& WeakConveyor : WeakConveyors)
        {
            if (WeakConveyor.IsValid())
            {
                ValidConveyors.Add(WeakConveyor.Get());
            }
        }
        
        if (ValidConveyors.Num() == 0)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ DEFERRED: No valid conveyors remaining!"));
            return;
        }
        
        UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED: Step 1 - Removing %d conveyors from subsystem..."), ValidConveyors.Num());
        
        // Step 1: Remove all conveyors from subsystem
        for (AFGBuildableConveyorBase* Conveyor : ValidConveyors)
        {
            BuildableSubsystem->RemoveConveyor(Conveyor);
        }
        
        UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED: Step 2 - Re-adding %d conveyors to subsystem..."), ValidConveyors.Num());
        
        // Step 2: Re-add all conveyors to subsystem (triggers chain rebuild)
        for (AFGBuildableConveyorBase* Conveyor : ValidConveyors)
        {
            BuildableSubsystem->AddConveyor(Conveyor);
        }
        
        // Log chain status after rebuild
        TSet<AFGConveyorChainActor*> UniqueChains;
        for (AFGBuildableConveyorBase* Conveyor : ValidConveyors)
        {
            AFGConveyorChainActor* Chain = Conveyor->GetConveyorChainActor();
            if (Chain)
            {
                UniqueChains.Add(Chain);
            }
        }
        
        UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED: Chain rebuild complete! %d conveyors now belong to %d unique chain actors"),
            ValidConveyors.Num(), UniqueChains.Num());
        
        for (AFGConveyorChainActor* Chain : UniqueChains)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED:   Chain %s has %d segments"),
                *Chain->GetName(), Chain->GetNumChainSegments());
        }
        
        // Success check
        if (UniqueChains.Num() < ValidConveyors.Num())
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED: ✅ SUCCESS - chains unified from %d fragments to %d chains"),
                ValidConveyors.Num(), UniqueChains.Num());
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⛓️ DEFERRED: ⚠️ Chains still fragmented (%d conveyors, %d chains)"),
                ValidConveyors.Num(), UniqueChains.Num());
        }
    });
    
    UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ DEFERRED: Timer scheduled for next tick"));
}
