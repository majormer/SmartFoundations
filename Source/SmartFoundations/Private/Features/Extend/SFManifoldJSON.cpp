#include "Features/Extend/SFManifoldJSON.h"
#include "Features/Extend/SFExtendService.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Satisfactory includes
#include "FGBuildableConveyorBelt.h"
#include "FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"  // Issue #288: valves/pumps
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGPowerConnectionComponent.h"  // Issue #288: capture pump's power-pole linkage
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGConveyorChainActor.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGPlayerController.h"
#include "Components/SplineComponent.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "FGFactoryColoringTypes.h"

// Smart includes
#include "SmartFoundations.h"
#include "Holograms/Logistics/SFConveyorAttachmentChildHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Holograms/Logistics/SFPipeAttachmentChildHologram.h"  // Issue #288: valves/pumps
#include "Holograms/Logistics/SFWallHoleChildHologram.h"
#include "Holograms/Power/SFPowerPoleChildHologram.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFHologramDataRegistry.h"

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

namespace ManifoldJSONHelpers
{
    TSharedPtr<FJsonObject> Vec3ToJson(const FSFVec3& V)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), V.X);
        Obj->SetNumberField(TEXT("y"), V.Y);
        Obj->SetNumberField(TEXT("z"), V.Z);
        return Obj;
    }
    
    TSharedPtr<FJsonObject> Rot3ToJson(const FSFRot3& R)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("pitch"), R.Pitch);
        Obj->SetNumberField(TEXT("yaw"), R.Yaw);
        Obj->SetNumberField(TEXT("roll"), R.Roll);
        return Obj;
    }
    
    TSharedPtr<FJsonObject> TransformToJson(const FSFTransform& T)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("location"), Vec3ToJson(T.Location));
        Obj->SetObjectField(TEXT("rotation"), Rot3ToJson(T.Rotation));
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SplinePointToJson(const FSFSplinePoint& P)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("local"), Vec3ToJson(P.Local));
        Obj->SetObjectField(TEXT("world"), Vec3ToJson(P.World));
        Obj->SetObjectField(TEXT("arrive_tangent"), Vec3ToJson(P.ArriveTangent));
        Obj->SetObjectField(TEXT("leave_tangent"), Vec3ToJson(P.LeaveTangent));
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SplineDataToJson(const FSFSplineData& S)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("length"), S.Length);
        
        TArray<TSharedPtr<FJsonValue>> PointsArray;
        for (const FSFSplinePoint& Point : S.Points)
        {
            PointsArray.Add(MakeShared<FJsonValueObject>(SplinePointToJson(Point)));
        }
        Obj->SetArrayField(TEXT("points"), PointsArray);
        return Obj;
    }
    
    TSharedPtr<FJsonObject> LiftDataToJson(const FSFLiftData& L)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("height"), L.Height);
        Obj->SetBoolField(TEXT("is_reversed"), L.bIsReversed);
        Obj->SetObjectField(TEXT("top_transform"), TransformToJson(L.TopTransform));
        Obj->SetObjectField(TEXT("bottom_transform"), TransformToJson(L.BottomTransform));
        return Obj;
    }
    
    TSharedPtr<FJsonObject> ConnectionRefToJson(const FSFConnectionRef& C)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("target"), C.Target);
        Obj->SetStringField(TEXT("connector"), C.Connector);
        return Obj;
    }
    
    TSharedPtr<FJsonObject> ConnectionsToJson(const FSFConnections& C)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetObjectField(TEXT("ConveyorAny0"), ConnectionRefToJson(C.ConveyorAny0));
        Obj->SetObjectField(TEXT("ConveyorAny1"), ConnectionRefToJson(C.ConveyorAny1));
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SourceSegmentToJson(const FSFSourceSegment& S)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("index"), S.Index);
        Obj->SetStringField(TEXT("type"), S.Type);
        Obj->SetStringField(TEXT("id"), S.Id);
        Obj->SetStringField(TEXT("class"), S.Class);
        Obj->SetStringField(TEXT("recipe_class"), S.RecipeClass);
        Obj->SetObjectField(TEXT("transform"), TransformToJson(S.Transform));
        Obj->SetObjectField(TEXT("connections"), ConnectionsToJson(S.Connections));
        
        // Chain actor info (for belts/lifts)
        if (!S.ChainActorName.IsEmpty())
        {
            Obj->SetStringField(TEXT("chain_actor_name"), S.ChainActorName);
            Obj->SetNumberField(TEXT("chain_segment_index"), S.ChainSegmentIndex);
        }
        
        if (S.bHasSplineData)
        {
            Obj->SetObjectField(TEXT("spline_data"), SplineDataToJson(S.SplineData));
        }
        if (S.bHasLiftData)
        {
            Obj->SetObjectField(TEXT("lift_data"), LiftDataToJson(S.LiftData));
        }
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SourceDistributorToJson(const FSFSourceDistributor& D)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), D.Id);
        Obj->SetStringField(TEXT("class"), D.Class);
        Obj->SetStringField(TEXT("recipe_class"), D.RecipeClass);
        Obj->SetObjectField(TEXT("transform"), TransformToJson(D.Transform));
        Obj->SetStringField(TEXT("connector_used"), D.ConnectorUsed);
        
        // Serialize connected connectors array
        TArray<TSharedPtr<FJsonValue>> ConnectedArray;
        for (const FString& Conn : D.ConnectedConnectors)
        {
            ConnectedArray.Add(MakeShared<FJsonValueString>(Conn));
        }
        Obj->SetArrayField(TEXT("connected_connectors"), ConnectedArray);
        
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SourceChainToJson(const FSFSourceChain& C)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("chain_id"), C.ChainId);
        Obj->SetStringField(TEXT("factory_connector"), C.FactoryConnector);
        Obj->SetObjectField(TEXT("distributor"), SourceDistributorToJson(C.Distributor));
        
        TArray<TSharedPtr<FJsonValue>> SegmentsArray;
        for (const FSFSourceSegment& Segment : C.Segments)
        {
            SegmentsArray.Add(MakeShared<FJsonValueObject>(SourceSegmentToJson(Segment)));
        }
        Obj->SetArrayField(TEXT("segments"), SegmentsArray);
        return Obj;
    }
    
    TSharedPtr<FJsonObject> SourceFactoryToJson(const FSFSourceFactory& F)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), F.Id);
        Obj->SetStringField(TEXT("class"), F.Class);
        Obj->SetObjectField(TEXT("transform"), TransformToJson(F.Transform));
        return Obj;
    }
    
    TSharedPtr<FJsonObject> CloneHologramToJson(const FSFCloneHologram& H)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        
        // Role and identification
        Obj->SetStringField(TEXT("hologram_id"), H.HologramId);
        Obj->SetStringField(TEXT("role"), H.Role);
        Obj->SetStringField(TEXT("source_id"), H.SourceId);
        Obj->SetStringField(TEXT("source_class"), H.SourceClass);
        Obj->SetStringField(TEXT("source_chain"), H.SourceChain);
        if (H.SourceSegmentIndex >= 0)
        {
            Obj->SetNumberField(TEXT("source_segment_index"), H.SourceSegmentIndex);
        }
        
        // Hologram spawning data
        Obj->SetStringField(TEXT("hologram_class"), H.HologramClass);
        Obj->SetStringField(TEXT("build_class"), H.BuildClass);
        Obj->SetStringField(TEXT("recipe_class"), H.RecipeClass);
        Obj->SetObjectField(TEXT("transform"), TransformToJson(H.Transform));
        
        // Type-specific data
        if (H.bHasSplineData)
        {
            Obj->SetObjectField(TEXT("spline_data"), SplineDataToJson(H.SplineData));
        }
        if (H.bHasLiftData)
        {
            Obj->SetObjectField(TEXT("lift_data"), LiftDataToJson(H.LiftData));
        }
        
        // Connections
        Obj->SetObjectField(TEXT("source_connections"), ConnectionsToJson(H.SourceConnections));
        Obj->SetObjectField(TEXT("clone_connections"), ConnectionsToJson(H.CloneConnections));
        
        // Behavior flags
        Obj->SetBoolField(TEXT("constructible"), H.bConstructible);
        Obj->SetBoolField(TEXT("preview_only"), H.bPreviewOnly);
        
        // Lane segment metadata (only for role="lane_segment")
        if (H.bIsLaneSegment)
        {
            Obj->SetBoolField(TEXT("is_lane_segment"), true);
            Obj->SetStringField(TEXT("lane_from_distributor_id"), H.LaneFromDistributorId);
            Obj->SetStringField(TEXT("lane_from_connector"), H.LaneFromConnector);
            Obj->SetStringField(TEXT("lane_to_distributor_id"), H.LaneToDistributorId);
            Obj->SetStringField(TEXT("lane_to_connector"), H.LaneToConnector);
            Obj->SetStringField(TEXT("lane_segment_type"), H.LaneSegmentType);
        }
        
        return Obj;
    }
}

// ============================================================================
// FSFSourceTopology
// ============================================================================

bool FSFSourceTopology::SaveToFile(const FString& FilePath) const
{
    using namespace ManifoldJSONHelpers;
    
    TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
    RootObj->SetStringField(TEXT("schema_version"), SchemaVersion);
    RootObj->SetStringField(TEXT("capture_timestamp"), CaptureTimestamp);
    RootObj->SetObjectField(TEXT("factory"), SourceFactoryToJson(Factory));
    
    // Chains object
    TSharedPtr<FJsonObject> ChainsObj = MakeShared<FJsonObject>();
    
    // Belt input chains
    TArray<TSharedPtr<FJsonValue>> BeltInputArray;
    for (const FSFSourceChain& Chain : BeltInputChains)
    {
        BeltInputArray.Add(MakeShared<FJsonValueObject>(SourceChainToJson(Chain)));
    }
    ChainsObj->SetArrayField(TEXT("belt_input"), BeltInputArray);
    
    // Belt output chains
    TArray<TSharedPtr<FJsonValue>> BeltOutputArray;
    for (const FSFSourceChain& Chain : BeltOutputChains)
    {
        BeltOutputArray.Add(MakeShared<FJsonValueObject>(SourceChainToJson(Chain)));
    }
    ChainsObj->SetArrayField(TEXT("belt_output"), BeltOutputArray);
    
    // Pipe input chains
    TArray<TSharedPtr<FJsonValue>> PipeInputArray;
    for (const FSFSourceChain& Chain : PipeInputChains)
    {
        PipeInputArray.Add(MakeShared<FJsonValueObject>(SourceChainToJson(Chain)));
    }
    ChainsObj->SetArrayField(TEXT("pipe_input"), PipeInputArray);
    
    // Pipe output chains
    TArray<TSharedPtr<FJsonValue>> PipeOutputArray;
    for (const FSFSourceChain& Chain : PipeOutputChains)
    {
        PipeOutputArray.Add(MakeShared<FJsonValueObject>(SourceChainToJson(Chain)));
    }
    ChainsObj->SetArrayField(TEXT("pipe_output"), PipeOutputArray);
    
    RootObj->SetObjectField(TEXT("chains"), ChainsObj);
    
    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = 
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
    
    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

// ============================================================================
// FSFCloneTopology
// ============================================================================

FSFCloneTopology FSFCloneTopology::FromSource(const FSFSourceTopology& Source, const FVector& Offset)
{
    FSFCloneTopology Result;
    Result.SchemaVersion = TEXT("1.0");
    Result.WorldOffset = FSFVec3(Offset);
    Result.SourceFactoryId = Source.Factory.Id;
    Result.ParentBuildClass = Source.Factory.Class;
    Result.ParentTransform = Source.Factory.Transform.WithOffset(Result.WorldOffset);
    
    // Phase 1: Build source ID → hologram identifier mapping
    // Maps source actor IDs to hologram identifiers like "distributor_0", "belt_segment_1", etc.
    TMap<FString, FString> SourceIdToHologramId;
    
    // Factory maps to "parent"
    SourceIdToHologramId.Add(Source.Factory.Id, TEXT("parent"));
    
    // Pre-scan all chains to build the mapping
    int32 DistributorIndex = 0;
    int32 BeltSegmentIndex = 0;
    int32 LiftSegmentIndex = 0;
    int32 PipeSegmentIndex = 0;
    int32 PassthroughIndex = 0;
    int32 PipeAttachmentIndex = 0;  // Issue #288
    
    // Helper: check if a belt distributor's manifold lane connectors are both occupied (Issue #277)
    // Splitters/mergers use Output1/Input1 for manifold lanes. If both are occupied by factory
    // connections, no lane segment can be created and the chain should be excluded entirely.
    // Does not apply to pipe junctions (they use geometric connector selection).
    auto IsBeltDistributorLaneBlocked = [](const FSFSourceChain& Chain) -> bool
    {
        // Only check belt distributors (splitters/mergers), not pipe junctions
        if (Chain.Distributor.Class.Contains(TEXT("Junction"))) return false;
        
        bool bOutput1Occupied = Chain.Distributor.ConnectedConnectors.Contains(TEXT("Output1"));
        bool bInput1Occupied = Chain.Distributor.ConnectedConnectors.Contains(TEXT("Input1"));
        return bOutput1Occupied && bInput1Occupied;
    };
    
    auto BuildMapping = [&](const FSFSourceChain& Chain)
    {
        // Skip chains without a physical distributor (Issue #277)
        // Belt chains must terminate at a splitter/merger, pipe chains at a junction
        if (Chain.Distributor.Id.IsEmpty())
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("\u26a0\ufe0f EXTEND: Skipping chain '%s' - no physical distributor (terminates directly at factory)"), *Chain.ChainId);
            return;
        }
        
        // Skip belt chains where both manifold lane connectors (Output1/Input1) are occupied (Issue #277)
        // A distributor with both lane connectors used by factory connections can't form a manifold lane
        if (IsBeltDistributorLaneBlocked(Chain))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("\u26a0\ufe0f EXTEND: Skipping chain '%s' - distributor '%s' has both lane connectors (Output1/Input1) occupied"), *Chain.ChainId, *Chain.Distributor.Id);
            return;
        }
        
        // Distributor
        FString DistId = FString::Printf(TEXT("distributor_%d"), DistributorIndex++);
        SourceIdToHologramId.Add(Chain.Distributor.Id, DistId);
        
        // Segments
        for (const FSFSourceSegment& Seg : Chain.Segments)
        {
            FString SegId;
            if (Seg.Type == TEXT("belt"))
            {
                SegId = FString::Printf(TEXT("belt_segment_%d"), BeltSegmentIndex++);
            }
            else if (Seg.Type == TEXT("lift"))
            {
                SegId = FString::Printf(TEXT("lift_segment_%d"), LiftSegmentIndex++);
            }
            else if (Seg.Type == TEXT("pipe"))
            {
                SegId = FString::Printf(TEXT("pipe_segment_%d"), PipeSegmentIndex++);
            }
            else if (Seg.Type == TEXT("passthrough"))
            {
                SegId = FString::Printf(TEXT("passthrough_%d"), PassthroughIndex++);
            }
            else if (Seg.Type == TEXT("pipe_attachment"))
            {
                // Issue #288: hologram IDs for valves/pumps so pipes that snap
                // to them can resolve their target via ResolveConnection.
                SegId = FString::Printf(TEXT("pipe_attachment_%d"), PipeAttachmentIndex++);
            }
            SourceIdToHologramId.Add(Seg.Id, SegId);
        }
    };
    
    for (const FSFSourceChain& Chain : Source.BeltInputChains) { BuildMapping(Chain); }
    for (const FSFSourceChain& Chain : Source.BeltOutputChains) { BuildMapping(Chain); }
    for (const FSFSourceChain& Chain : Source.PipeInputChains) { BuildMapping(Chain); }
    for (const FSFSourceChain& Chain : Source.PipeOutputChains) { BuildMapping(Chain); }
    
    // Issue #288: Pre-allocate power_pole_N HologramIds so that pipe_attachment
    // segments emitted later in ProcessChain can resolve their ConnectedPowerPoleSourceId
    // via SourceIdToHologramId.Find(). Without this pre-pass, the pump → pole
    // source-id lookup fires before PHASE 2.5 populates the map, leaving every
    // cloned pump with an empty ConnectedPowerPoleHologramId — so Phase 3.8b
    // post-build wiring would skip every pump on a no-op match. The index format
    // here ("power_pole_%d") MUST stay in sync with PHASE 2.5 below.
    for (int32 PoleIdx = 0; PoleIdx < Source.PowerPoles.Num(); PoleIdx++)
    {
        SourceIdToHologramId.Add(Source.PowerPoles[PoleIdx].Id,
            FString::Printf(TEXT("power_pole_%d"), PoleIdx));
    }
    
    // Helper to resolve a connection reference to hologram identifier
    auto ResolveConnection = [&SourceIdToHologramId](const FSFConnectionRef& SourceConn) -> FSFConnectionRef
    {
        FSFConnectionRef CloneConn;
        CloneConn.Connector = SourceConn.Connector;  // Connector name stays the same
        
        // Try to find the target in our mapping
        if (const FString* HoloId = SourceIdToHologramId.Find(SourceConn.Target))
        {
            CloneConn.Target = *HoloId;
        }
        else if (!SourceConn.Target.IsEmpty())
        {
            // Target not in our topology - could be external or the parent factory
            // Check if it matches the factory class pattern
            if (SourceConn.Target.Contains(TEXT("OilRefinery")) || 
                SourceConn.Target.Contains(TEXT("Assembler")) ||
                SourceConn.Target.Contains(TEXT("Constructor")) ||
                SourceConn.Target.Contains(TEXT("Manufacturer")) ||
                SourceConn.Target.Contains(TEXT("Smelter")) ||
                SourceConn.Target.Contains(TEXT("Foundry")))
            {
                CloneConn.Target = TEXT("parent");
            }
            else
            {
                // External connection - mark as such
                CloneConn.Target = TEXT("external");
            }
        }
        return CloneConn;
    };
    
    // Phase 2: Create holograms with resolved connections
    auto ProcessChain = [&](const FSFSourceChain& Chain, bool bIsInput)
    {
        // Skip chains without a physical distributor (Issue #277)
        if (Chain.Distributor.Id.IsEmpty())
        {
            return;
        }
        
        // Skip belt chains with both manifold lane connectors occupied (Issue #277)
        if (IsBeltDistributorLaneBlocked(Chain))
        {
            return;
        }
        
        // Add distributor hologram
        FSFCloneHologram DistHolo;
        DistHolo.Role = TEXT("distributor");
        DistHolo.SourceId = Chain.Distributor.Id;
        DistHolo.SourceClass = Chain.Distributor.Class;
        DistHolo.SourceChain = Chain.ChainId;
        DistHolo.SourceSegmentIndex = -1;
        DistHolo.HologramClass = TEXT("ASFConveyorAttachmentChildHologram");
        DistHolo.BuildClass = Chain.Distributor.Class;
        DistHolo.RecipeClass = Chain.Distributor.RecipeClass;
        DistHolo.Transform = Chain.Distributor.Transform.WithOffset(FSFVec3(Offset));
        DistHolo.bConstructible = true;
        DistHolo.bPreviewOnly = false;
        
        // Resolve hologram ID for this distributor
        if (const FString* HoloId = SourceIdToHologramId.Find(Chain.Distributor.Id))
        {
            DistHolo.HologramId = *HoloId;
        }
        
        Result.ChildHolograms.Add(DistHolo);
        
        // Add segment holograms
        for (int32 i = 0; i < Chain.Segments.Num(); i++)
        {
            const FSFSourceSegment& Seg = Chain.Segments[i];
            
            FSFCloneHologram SegHolo;
            SegHolo.SourceId = Seg.Id;
            SegHolo.SourceClass = Seg.Class;
            SegHolo.SourceChain = Chain.ChainId;
            SegHolo.SourceSegmentIndex = i;
            SegHolo.BuildClass = Seg.Class;
            SegHolo.RecipeClass = Seg.RecipeClass;
            SegHolo.Transform = Seg.Transform.WithOffset(FSFVec3(Offset));
            SegHolo.SourceConnections = Seg.Connections;
            SegHolo.bConstructible = true;
            SegHolo.bPreviewOnly = false;
            
            // Resolve hologram ID for this segment
            if (const FString* HoloId = SourceIdToHologramId.Find(Seg.Id))
            {
                SegHolo.HologramId = *HoloId;
            }
            
            if (Seg.Type == TEXT("belt"))
            {
                SegHolo.Role = TEXT("belt_segment");
                SegHolo.HologramClass = TEXT("ASFConveyorBeltHologram");
                SegHolo.bHasSplineData = true;
                SegHolo.SplineData = Seg.SplineData.WithOffset(FSFVec3(Offset));
            }
            else if (Seg.Type == TEXT("lift"))
            {
                SegHolo.Role = TEXT("lift_segment");
                SegHolo.HologramClass = TEXT("ASFConveyorLiftHologram");
                SegHolo.bHasLiftData = true;
                SegHolo.LiftData = Seg.LiftData.WithOffset(FSFVec3(Offset));
            }
            else if (Seg.Type == TEXT("pipe"))
            {
                SegHolo.Role = TEXT("pipe_segment");
                SegHolo.HologramClass = TEXT("ASFPipelineHologram");
                SegHolo.bHasSplineData = true;
                SegHolo.SplineData = Seg.SplineData.WithOffset(FSFVec3(Offset));
            }
            else if (Seg.Type == TEXT("passthrough"))
            {
                // Issue #260: Pipe floor holes need explicit cloning (unlike lift floor holes
                // which are auto-created by conveyor lift construction)
                SegHolo.Role = TEXT("passthrough");
                SegHolo.HologramClass = TEXT("ASFPassthroughChildHologram");
            }
            else if (Seg.Type == TEXT("pipe_attachment"))
            {
                // Issue #288: Valves and pumps are inline pipe attachments (share
                // AFGBuildablePipelinePump). The spawn handler uses vanilla
                // AFGPipelineAttachmentHologram via SetBuildClass, then post-build
                // copies mUserFlowLimit from the source (-1 = unlimited).
                SegHolo.Role = TEXT("pipe_attachment");
                SegHolo.UserFlowLimit = Seg.UserFlowLimit;
                
                // Issue #288: Resolve the clone pole HologramId for pump power wiring.
                // Empty in three cases: (a) valve (no PowerInput component), (b) pump
                // with no cable in source, (c) pump connected to a pole outside the
                // manifold — the SourceIdToHologramId lookup fails because Power
                // Extend didn't capture that pole. All three correctly short-circuit
                // both the preview-time capacity validation and the post-build wiring.
                if (!Seg.ConnectedPowerPoleSourceId.IsEmpty())
                {
                    if (const FString* PoleHoloId = SourceIdToHologramId.Find(Seg.ConnectedPowerPoleSourceId))
                    {
                        SegHolo.ConnectedPowerPoleHologramId = *PoleHoloId;
                    }
                }
            }
            
            // Resolve clone_connections to hologram identifiers
            SegHolo.CloneConnections.ConveyorAny0 = ResolveConnection(Seg.Connections.ConveyorAny0);
            SegHolo.CloneConnections.ConveyorAny1 = ResolveConnection(Seg.Connections.ConveyorAny1);
            
            Result.ChildHolograms.Add(SegHolo);
        }
    };
    
    // Process all chains
    for (const FSFSourceChain& Chain : Source.BeltInputChains)
    {
        ProcessChain(Chain, true);
    }
    for (const FSFSourceChain& Chain : Source.BeltOutputChains)
    {
        ProcessChain(Chain, false);
    }
    for (const FSFSourceChain& Chain : Source.PipeInputChains)
    {
        ProcessChain(Chain, true);
    }
    for (const FSFSourceChain& Chain : Source.PipeOutputChains)
    {
        ProcessChain(Chain, false);
    }
    
    // ========================================================================
    // PHASE 2.5: Generate Power Pole Clone Holograms (Issue #229)
    // ========================================================================
    int32 PowerPoleIndex = 0;
    for (const FSFSourcePowerPole& SourcePole : Source.PowerPoles)
    {
        FSFCloneHologram PoleHolo;
        PoleHolo.HologramId = FString::Printf(TEXT("power_pole_%d"), PowerPoleIndex);
        PoleHolo.Role = TEXT("power_pole");
        PoleHolo.SourceId = SourcePole.Id;
        PoleHolo.SourceClass = SourcePole.Class;
        PoleHolo.SourceChain = TEXT("");  // Not part of a chain
        PoleHolo.SourceSegmentIndex = -1;
        PoleHolo.HologramClass = TEXT("ASFPowerPoleChildHologram");
        PoleHolo.BuildClass = SourcePole.Class;
        PoleHolo.RecipeClass = SourcePole.RecipeClass;
        PoleHolo.Transform = SourcePole.Transform.WithOffset(FSFVec3(Offset));
        PoleHolo.bConstructible = true;
        PoleHolo.bPreviewOnly = false;
        PoleHolo.PowerPoleMaxConnections = SourcePole.MaxConnections;  // Issue #288
        
        // Store source pole metadata in a way the spawner can access
        // We encode free connection info into the HologramId for later use
        // (The actual wiring logic uses the registered built actors)
        
        SourceIdToHologramId.Add(SourcePole.Id, PoleHolo.HologramId);
        Result.ChildHolograms.Add(PoleHolo);
        
        // Wire cost hologram 1: Clone factory ↔ Clone pole
        // Distance = RelativeOffset magnitude (same relative position in clone)
        {
            float FactoryToPoleDistance = SourcePole.RelativeOffset.ToFVector().Size();
            FSFCloneHologram WireHolo;
            WireHolo.HologramId = FString::Printf(TEXT("wire_factory_pole_%d"), PowerPoleIndex);
            WireHolo.Role = TEXT("wire_cost");
            WireHolo.SourceId = SourcePole.Id;
            WireHolo.SourceClass = TEXT("Build_PowerLine_C");
            WireHolo.HologramClass = TEXT("ASFWireHologram");
            WireHolo.BuildClass = TEXT("Build_PowerLine_C");
            WireHolo.RecipeClass = TEXT("Recipe_PowerLine_C");
            WireHolo.Transform = PoleHolo.Transform;  // Position doesn't matter for cost-only
            WireHolo.bConstructible = false;
            WireHolo.bPreviewOnly = true;
            // Store wire distance in SplineData.Length for the spawner to read
            WireHolo.bHasSplineData = true;
            WireHolo.SplineData.Length = FactoryToPoleDistance;
            Result.ChildHolograms.Add(WireHolo);
        }
        
        // Wire cost hologram 2: Source pole ↔ Clone pole (only if source has free connections)
        if (SourcePole.bSourceHasFreeConnections)
        {
            float SourceToCloneDistance = Offset.Size();
            FSFCloneHologram WireHolo;
            WireHolo.HologramId = FString::Printf(TEXT("wire_source_clone_%d"), PowerPoleIndex);
            WireHolo.Role = TEXT("wire_cost");
            WireHolo.SourceId = SourcePole.Id;
            WireHolo.SourceClass = TEXT("Build_PowerLine_C");
            WireHolo.HologramClass = TEXT("ASFWireHologram");
            WireHolo.BuildClass = TEXT("Build_PowerLine_C");
            WireHolo.RecipeClass = TEXT("Recipe_PowerLine_C");
            WireHolo.Transform = PoleHolo.Transform;
            WireHolo.bConstructible = false;
            WireHolo.bPreviewOnly = true;
            WireHolo.bHasSplineData = true;
            WireHolo.SplineData.Length = SourceToCloneDistance;
            Result.ChildHolograms.Add(WireHolo);
        }
        
        PowerPoleIndex++;
    }
    
    // ========================================================================
    // PHASE 2.6: Generate Pipe Passthrough Clone Holograms (Issue #260)
    // ========================================================================
    // Pipe floor holes are NOT in pipe connection chains (pipes pass through physically,
    // not logically). They are discovered spatially and need explicit hologram cloning.
    int32 PassthroughCloneIndex = 0;
    for (const FSFSourceSegment& PassSeg : Source.PipePassthroughs)
    {
        FSFCloneHologram PassHolo;
        PassHolo.HologramId = FString::Printf(TEXT("passthrough_%d"), PassthroughCloneIndex);
        PassHolo.Role = TEXT("passthrough");
        PassHolo.SourceId = PassSeg.Id;
        PassHolo.SourceClass = PassSeg.Class;
        PassHolo.SourceChain = TEXT("");  // Not part of a chain
        PassHolo.SourceSegmentIndex = -1;
        PassHolo.HologramClass = TEXT("ASFPassthroughChildHologram");
        PassHolo.BuildClass = PassSeg.Class;
        PassHolo.RecipeClass = PassSeg.RecipeClass;
        PassHolo.Transform = PassSeg.Transform.WithOffset(FSFVec3(Offset));
        PassHolo.bConstructible = true;
        PassHolo.bPreviewOnly = false;
        PassHolo.Thickness = PassSeg.Thickness;
        
        SourceIdToHologramId.Add(PassSeg.Id, PassHolo.HologramId);
        Result.ChildHolograms.Add(PassHolo);
        PassthroughCloneIndex++;
    }

    // ========================================================================
    // PHASE 2.7: Generate Wall Hole Clone Holograms
    // ========================================================================
    // Wall holes (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C) are cosmetic
    // Blueprint decorators with no connections. They need to be cloned at their world-offset
    // transform so the cloned belts/pipes pass through matching decorations and snap-point
    // consumption happens on the cloned wall.
    int32 WallHoleCloneIndex = 0;
    for (const FSFSourceSegment& WallSeg : Source.WallHoles)
    {
        FSFCloneHologram WallHolo;
        WallHolo.HologramId = FString::Printf(TEXT("wall_hole_%d"), WallHoleCloneIndex);
        WallHolo.Role = TEXT("wall_hole");
        WallHolo.SourceId = WallSeg.Id;
        WallHolo.SourceClass = WallSeg.Class;
        WallHolo.SourceChain = TEXT("");  // Not part of a chain
        WallHolo.SourceSegmentIndex = -1;
        WallHolo.HologramClass = TEXT("ASFWallHoleChildHologram");  // Spawn handler to be added in Phase 2
        WallHolo.BuildClass = WallSeg.Class;
        WallHolo.RecipeClass = WallSeg.RecipeClass;
        WallHolo.Transform = WallSeg.Transform.WithOffset(FSFVec3(Offset));
        WallHolo.bConstructible = true;
        WallHolo.bPreviewOnly = false;

        SourceIdToHologramId.Add(WallSeg.Id, WallHolo.HologramId);
        Result.ChildHolograms.Add(WallHolo);
        WallHoleCloneIndex++;
    }
    
    // ========================================================================
    // PHASE 3: Generate Manifold Lane Segments (Source → Clone connections)
    // ========================================================================
    // For each unique distributor, check if it has an open lane-eligible connection.
    // If so, generate a lane segment to connect the source distributor to its clone.
    // 
    // Lane-eligible connections:
    // - Splitters/Mergers: Output1 (source) → Input1 (clone)
    // - Pipeline Junctions: Closest open connection pair (geometric)
    // ========================================================================
    
    // Collect unique distributors (avoid duplicates from multiple chains)
    TMap<FString, FSFSourceDistributor> UniqueDistributors;
    TMap<FString, FString> DistributorIdToHologramId;
    TMap<FString, bool> DistributorIsPipe;  // Track if distributor is a pipe junction
    
    auto CollectDistributor = [&](const FSFSourceChain& Chain, bool bIsPipe)
    {
        // Skip chains without a physical distributor (Issue #277)
        if (Chain.Distributor.Id.IsEmpty())
        {
            return;
        }
        
        // Skip belt chains with both manifold lane connectors occupied (Issue #277)
        if (!bIsPipe && IsBeltDistributorLaneBlocked(Chain))
        {
            return;
        }
        
        if (!UniqueDistributors.Contains(Chain.Distributor.Id))
        {
            UniqueDistributors.Add(Chain.Distributor.Id, Chain.Distributor);
            DistributorIsPipe.Add(Chain.Distributor.Id, bIsPipe);
            
            // Find the hologram ID for this distributor
            if (const FString* HoloId = SourceIdToHologramId.Find(Chain.Distributor.Id))
            {
                DistributorIdToHologramId.Add(Chain.Distributor.Id, *HoloId);
            }
        }
    };
    
    for (const FSFSourceChain& Chain : Source.BeltInputChains) { CollectDistributor(Chain, false); }
    for (const FSFSourceChain& Chain : Source.BeltOutputChains) { CollectDistributor(Chain, false); }
    for (const FSFSourceChain& Chain : Source.PipeInputChains) { CollectDistributor(Chain, true); }
    for (const FSFSourceChain& Chain : Source.PipeOutputChains) { CollectDistributor(Chain, true); }
    
    int32 LaneSegmentIndex = 0;
    
    for (const auto& DistPair : UniqueDistributors)
    {
        const FString& DistributorId = DistPair.Key;
        const FSFSourceDistributor& Distributor = DistPair.Value;
        const bool bIsPipeJunction = DistributorIsPipe.FindRef(DistributorId);
        
        // Get the hologram ID for this distributor
        const FString* HologramIdPtr = DistributorIdToHologramId.Find(DistributorId);
        if (!HologramIdPtr)
        {
            continue;
        }
        const FString& DistributorHologramId = *HologramIdPtr;
        
        // ==========================================================================
        // LANE CONNECTOR SELECTION
        // ==========================================================================
        // We need to find the closest pair of connectors between source and clone
        // that are valid for a manifold lane connection.
        //
        // For PIPE JUNCTIONS:
        //   - ConnectorUsed tells us which connector goes to factory chain
        //   - Opposite pairs: Connection0↔Connection1, Connection2↔Connection3
        //   - Exclude factory connector AND its opposite
        //   - Find closest pair from remaining 2 connectors
        //
        // For BELT DISTRIBUTORS (Splitter/Merger):
        //   - Lane always uses Output1 (output) and Input1 (input)
        //   - Find which Output1/Input1 pair is closest
        //   - Flow: always Output → Input
        // ==========================================================================
        
        FVector SourceDistCenter = Distributor.Transform.Location.ToFVector();
        FVector CloneDistCenter = SourceDistCenter + Offset;
        FRotator SourceRotation = Distributor.Transform.Rotation.ToFRotator();
        
        // Determine flow direction and connector positions based on distributor type
        FString LaneType;
        FVector SplineStartPos;   // Output end (where items come FROM)
        FVector SplineEndPos;     // Input end (where items go TO)
        FString Conn0Target;      // Actor ID for output connection
        FString Conn0Connector;   // Connector name on output target
        FString Conn1Target;      // Actor ID for input connection
        FString Conn1Connector;   // Connector name on input target
        FVector LaneStartNormal = FVector::ForwardVector;   // Connector facing direction at start
        FVector LaneEndNormal = -FVector::ForwardVector;    // Connector facing direction at end
        
        bool bIsMerger = Distributor.Class.Contains(TEXT("Merger"));
        
        if (bIsPipeJunction)
        {
            // Look up the clone's actual transform from the already-generated hologram
            FVector CloneDistLocation = CloneDistCenter;
            FRotator CloneDistRotation = SourceRotation;
            
            for (const FSFCloneHologram& Holo : Result.ChildHolograms)
            {
                if (Holo.HologramId == DistributorHologramId)
                {
                    CloneDistLocation = Holo.Transform.Location.ToFVector();
                    CloneDistRotation = Holo.Transform.Rotation.ToFRotator();
                    break;
                }
            }
            
            // Use ACTUAL connector world positions captured from the source junction
            // instead of hardcoded offsets - this ensures accurate spline generation
            const TMap<FString, FVector>& SourceConnectorPositions = Distributor.ConnectorWorldPositions;
            
            // Opposite connector mapping (for selecting facing connectors)
            TMap<FString, FString> OppositeConnector;
            OppositeConnector.Add(TEXT("Connection0"), TEXT("Connection1"));
            OppositeConnector.Add(TEXT("Connection1"), TEXT("Connection0"));
            OppositeConnector.Add(TEXT("Connection2"), TEXT("Connection3"));
            OppositeConnector.Add(TEXT("Connection3"), TEXT("Connection2"));
            
            // Local offsets for clone (which doesn't exist yet, so we calculate from center)
            // These are the ACTUAL local-space offsets for pipeline junction cross connectors
            // Connection0/1 are on the X axis, Connection2/3 are on the Y axis
            TMap<FString, FVector> LocalOffsets;
            LocalOffsets.Add(TEXT("Connection0"), FVector(-100.0f, 0.0f, 0.0f));
            LocalOffsets.Add(TEXT("Connection1"), FVector(100.0f, 0.0f, 0.0f));
            LocalOffsets.Add(TEXT("Connection2"), FVector(0.0f, 100.0f, 0.0f));
            LocalOffsets.Add(TEXT("Connection3"), FVector(0.0f, -100.0f, 0.0f));
            
            // Exclude factory-connected connector and its opposite (they're reserved for factory chain)
            FString FactoryConnector = Distributor.ConnectorUsed;
            FString ExcludedOpposite = OppositeConnector.FindRef(FactoryConnector);
            
            // Find valid connectors for SOURCE (not factory, not opposite, and not already connected)
            TArray<FString> ValidSourceConnectors;
            for (const auto& Pair : SourceConnectorPositions)
            {
                if (Pair.Key != FactoryConnector && Pair.Key != ExcludedOpposite)
                {
                    // Source: check if this connector is already connected to something
                    if (!Distributor.ConnectedConnectors.Contains(Pair.Key))
                    {
                        ValidSourceConnectors.Add(Pair.Key);
                    }
                    else
                    {
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ PIPE LANE: Excluding source %s (already connected)"), *Pair.Key);
                    }
                }
            }
            
            // Find valid connectors for CLONE (not factory, not opposite - clone is new, no existing connections)
            TArray<FString> ValidCloneConnectors;
            for (const auto& Pair : LocalOffsets)
            {
                if (Pair.Key != FactoryConnector && Pair.Key != ExcludedOpposite)
                {
                    ValidCloneConnectors.Add(Pair.Key);
                }
            }
            
            // If no valid source connectors available, skip this junction's lane segment
            if (ValidSourceConnectors.Num() == 0)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🛤️ PIPE LANE: No available connectors on source junction %s (factory=%s, connected=%d) - skipping lane segment"),
                    *DistributorId, *FactoryConnector, Distributor.ConnectedConnectors.Num());
                continue;  // CRITICAL: Skip to next distributor to avoid garbage lane data
            }
            
            // Find the CLOSEST connector pair where connectors FACE EACH OTHER
            // Source uses ACTUAL world position, Clone uses calculated position from center + offset
            
            float BestDistance = FLT_MAX;
            FString BestSourceConn, BestCloneConn;
            FVector BestSourcePos, BestClonePos;
            
            for (const FString& SourceConn : ValidSourceConnectors)
            {
                // Use ACTUAL world position from captured connector data
                const FVector* SourcePosPtr = SourceConnectorPositions.Find(SourceConn);
                if (!SourcePosPtr) continue;
                FVector SourcePos = *SourcePosPtr;
                
                // For the clone, we need the OPPOSITE connector so they face each other
                FString CloneConn = OppositeConnector.FindRef(SourceConn);
                
                // Make sure the opposite connector is valid for the clone
                if (!ValidCloneConnectors.Contains(CloneConn))
                {
                    continue;
                }
                
                // Clone connector position calculated from center + ROTATED offset (clone doesn't exist yet)
                // The local offset must be rotated by the clone junction's rotation to get correct world position
                FVector RotatedOffset = CloneDistRotation.RotateVector(LocalOffsets[CloneConn]);
                FVector ClonePos = CloneDistLocation + RotatedOffset;
                
                // Find the closest pair
                float Distance = FVector::Dist(SourcePos, ClonePos);
                
                if (Distance < BestDistance)
                {
                    BestDistance = Distance;
                    BestSourceConn = SourceConn;
                    BestCloneConn = CloneConn;
                    BestSourcePos = SourcePos;
                    BestClonePos = ClonePos;
                }
            }
            
            // Safety check: if no valid connector pair was found, skip this lane segment
            if (BestSourceConn.IsEmpty() || BestCloneConn.IsEmpty())
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🛤️ PIPE LANE: No valid connector pair found for junction %s - skipping lane segment"),
                    *DistributorId);
                continue;
            }
            
            LaneType = TEXT("pipe");
            SplineStartPos = BestSourcePos;
            SplineEndPos = BestClonePos;
            Conn0Target = FString::Printf(TEXT("source:%s"), *DistributorId);
            Conn0Connector = BestSourceConn;
            Conn1Target = DistributorHologramId;
            Conn1Connector = BestCloneConn;
            
            // Calculate connector normals (outward-facing direction from junction center)
            // The normal is the normalized local offset direction, rotated by the junction's world rotation
            FVector SourceConnLocalDir = LocalOffsets[BestSourceConn].GetSafeNormal();
            FVector CloneConnLocalDir = LocalOffsets[BestCloneConn].GetSafeNormal();
            LaneStartNormal = SourceRotation.RotateVector(SourceConnLocalDir);
            LaneEndNormal = CloneDistRotation.RotateVector(CloneConnLocalDir);
            
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ PIPE LANE: Factory uses %s, excluded opposite %s, chose Source.%s(%.0f,%.0f,%.0f)→Clone.%s(%.0f,%.0f,%.0f) (dist=%.0f) normals: start=(%.2f,%.2f,%.2f) end=(%.2f,%.2f,%.2f)"),
                *FactoryConnector, *ExcludedOpposite,
                *BestSourceConn, BestSourcePos.X, BestSourcePos.Y, BestSourcePos.Z,
                *BestCloneConn, BestClonePos.X, BestClonePos.Y, BestClonePos.Z, BestDistance,
                LaneStartNormal.X, LaneStartNormal.Y, LaneStartNormal.Z,
                LaneEndNormal.X, LaneEndNormal.Y, LaneEndNormal.Z);
        }
        else
        {
            // Belt distributor: lane uses Output1 and Input1
            // Look up the clone's actual transform from the already-generated hologram
            FVector CloneDistLocation = CloneDistCenter;
            FRotator CloneDistRotation = SourceRotation;
            
            for (const FSFCloneHologram& Holo : Result.ChildHolograms)
            {
                if (Holo.HologramId == DistributorHologramId)
                {
                    CloneDistLocation = Holo.Transform.Location.ToFVector();
                    CloneDistRotation = Holo.Transform.Rotation.ToFRotator();
                    break;
                }
            }
            
            // Connector offsets in local space for splitters/mergers:
            // Input1 = -100 X (back), Output1 = +100 X (front, opposite Input1)
            // Output2 = +100 Y (right), Output3 = -100 Y (left)
            // Per REF_Manifold_Technical.md: Lane always uses Output1 and Input1
            FVector LocalOutput1Offset = FVector(100.0f, 0.0f, 0.0f);
            FVector LocalInput1Offset = FVector(-100.0f, 0.0f, 0.0f);
            
            // Calculate connector world positions using each distributor's actual rotation
            FVector SourceOutput1 = SourceDistCenter + SourceRotation.RotateVector(LocalOutput1Offset);
            FVector SourceInput1 = SourceDistCenter + SourceRotation.RotateVector(LocalInput1Offset);
            FVector CloneOutput1 = CloneDistLocation + CloneDistRotation.RotateVector(LocalOutput1Offset);
            FVector CloneInput1 = CloneDistLocation + CloneDistRotation.RotateVector(LocalInput1Offset);
            
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ BELT LANE: Source center=(%.0f,%.0f) rot=%.0f, Clone center=(%.0f,%.0f) rot=%.0f"),
                SourceDistCenter.X, SourceDistCenter.Y, SourceRotation.Yaw,
                CloneDistLocation.X, CloneDistLocation.Y, CloneDistRotation.Yaw);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ BELT LANE: Source.Output1=(%.0f,%.0f), Source.Input1=(%.0f,%.0f)"),
                SourceOutput1.X, SourceOutput1.Y, SourceInput1.X, SourceInput1.Y);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ BELT LANE: Clone.Output1=(%.0f,%.0f), Clone.Input1=(%.0f,%.0f)"),
                CloneOutput1.X, CloneOutput1.Y, CloneInput1.X, CloneInput1.Y);
            
            // Find closest Output→Input pair
            // Option 1: Source Output1 → Clone Input1
            // Option 2: Clone Output1 → Source Input1
            float Dist1 = FVector::Dist(SourceOutput1, CloneInput1);
            float Dist2 = FVector::Dist(CloneOutput1, SourceInput1);
            
            // Check if source connectors are already connected (skip manifold if so)
            bool bSourceOutput1Connected = Distributor.ConnectedConnectors.Contains(TEXT("Output1"));
            bool bSourceInput1Connected = Distributor.ConnectedConnectors.Contains(TEXT("Input1"));
            
            // Determine which option is valid based on source connector availability
            bool bOption1Valid = !bSourceOutput1Connected;  // Source Output1 → Clone Input1
            bool bOption2Valid = !bSourceInput1Connected;   // Clone Output1 → Source Input1
            
            // If neither option is valid, skip this distributor's lane segment
            if (!bOption1Valid && !bOption2Valid)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🛤️ BELT LANE: Skipping %s - both Output1 (connected=%d) and Input1 (connected=%d) unavailable"),
                    *DistributorId, bSourceOutput1Connected, bSourceInput1Connected);
                continue;  // Skip to next distributor
            }
            
            LaneType = TEXT("belt");
            
            // Connector normal directions in local space
            // Output1 faces +X (forward), Input1 faces -X (backward)
            FVector Output1NormalLocal = FVector(1.0f, 0.0f, 0.0f);
            FVector Input1NormalLocal = FVector(-1.0f, 0.0f, 0.0f);
            
            // Choose the closest valid option
            if (bOption1Valid && (!bOption2Valid || Dist1 <= Dist2))
            {
                // Source Output1 → Clone Input1 (source feeds clone)
                SplineStartPos = SourceOutput1;
                SplineEndPos = CloneInput1;
                Conn0Target = FString::Printf(TEXT("source:%s"), *DistributorId);
                Conn0Connector = TEXT("Output1");
                Conn1Target = DistributorHologramId;
                Conn1Connector = TEXT("Input1");
                
                // Start normal = Source Output1 facing direction
                // End normal = Clone Input1 facing direction
                LaneStartNormal = SourceRotation.RotateVector(Output1NormalLocal);
                LaneEndNormal = CloneDistRotation.RotateVector(Input1NormalLocal);
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ BELT LANE: Source.Output1(%.0f,%.0f)→Clone.Input1(%.0f,%.0f) (dist=%.1f vs %.1f)"),
                    SourceOutput1.X, SourceOutput1.Y, CloneInput1.X, CloneInput1.Y, Dist1, Dist2);
            }
            else
            {
                // Clone Output1 → Source Input1 (clone feeds source)
                SplineStartPos = CloneOutput1;
                SplineEndPos = SourceInput1;
                Conn0Target = DistributorHologramId;
                Conn0Connector = TEXT("Output1");
                Conn1Target = FString::Printf(TEXT("source:%s"), *DistributorId);
                Conn1Connector = TEXT("Input1");
                
                // Start normal = Clone Output1 facing direction
                // End normal = Source Input1 facing direction
                LaneStartNormal = CloneDistRotation.RotateVector(Output1NormalLocal);
                LaneEndNormal = SourceRotation.RotateVector(Input1NormalLocal);
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ BELT LANE: Clone.Output1(%.0f,%.0f)→Source.Input1(%.0f,%.0f) (dist=%.1f vs %.1f)"),
                    CloneOutput1.X, CloneOutput1.Y, SourceInput1.X, SourceInput1.Y, Dist2, Dist1);
            }
        }
        
        // Generate lane segment
        FSFCloneHologram LaneHolo;
        LaneHolo.HologramId = FString::Printf(TEXT("lane_segment_%d"), LaneSegmentIndex++);
        LaneHolo.Role = TEXT("lane_segment");
        LaneHolo.SourceId = TEXT("");  // Generated, not captured
        LaneHolo.SourceClass = TEXT("");
        LaneHolo.SourceChain = TEXT("");
        LaneHolo.SourceSegmentIndex = -1;
        LaneHolo.bConstructible = true;
        LaneHolo.bPreviewOnly = false;
        
        // Lane segment metadata
        LaneHolo.bIsLaneSegment = true;
        LaneHolo.LaneFromDistributorId = Conn0Target.Contains(TEXT("source:")) ? DistributorId : DistributorHologramId;
        LaneHolo.LaneFromConnector = Conn0Connector;
        LaneHolo.LaneToDistributorId = Conn1Target.Contains(TEXT("source:")) ? DistributorId : DistributorHologramId;
        LaneHolo.LaneToConnector = Conn1Connector;
        LaneHolo.LaneSegmentType = LaneType;
        LaneHolo.LaneStartNormal = FSFVec3(LaneStartNormal);
        LaneHolo.LaneEndNormal = FSFVec3(LaneEndNormal);
        
        // Lane segments between distributors are ALWAYS belts (or pipes).
        // Conveyor lifts don't make sense for manifold connections — belts handle
        // the Z-delta from stepping just fine. Only captured source topology segments use lifts.
        if (LaneType == TEXT("pipe"))
        {
            // Pipe lane
            LaneHolo.HologramClass = TEXT("ASFPipelineHologram");
            LaneHolo.BuildClass = TEXT("Build_PipelineMK2_C");  // Will be resolved from settings
            LaneHolo.bHasSplineData = true;
            
            // Generate simple 2-point spline
            // Use X-axis aligned local coordinates (like belts) - the hologram rotation
            // will orient the pipe correctly in world space
            float SplineLength = FVector::Dist(SplineStartPos, SplineEndPos);
            LaneHolo.SplineData.Length = SplineLength;
            
            FSFSplinePoint StartPoint;
            StartPoint.World = FSFVec3(SplineStartPos);
            StartPoint.Local = FSFVec3(0, 0, 0);  // Start at origin in local space
            // Tangents point forward in local space (+X)
            StartPoint.LeaveTangent = FSFVec3(FVector(100.0f, 0, 0));
            StartPoint.ArriveTangent = FSFVec3(FVector(100.0f, 0, 0));
            
            FSFSplinePoint EndPoint;
            EndPoint.World = FSFVec3(SplineEndPos);
            // End point is at +X in local space (forward along pipe)
            EndPoint.Local = FSFVec3(FVector(SplineLength, 0, 0));
            EndPoint.LeaveTangent = FSFVec3(FVector(100.0f, 0, 0));
            EndPoint.ArriveTangent = FSFVec3(FVector(100.0f, 0, 0));
            
            LaneHolo.SplineData.Points.Add(StartPoint);
            LaneHolo.SplineData.Points.Add(EndPoint);
        }
        else
        {
            // Belt lane
            LaneHolo.HologramClass = TEXT("ASFConveyorBeltHologram");
            LaneHolo.BuildClass = TEXT("");  // Resolved at spawn time from player's unlocked tiers
            LaneHolo.bHasSplineData = true;
            
            // Generate simple 2-point spline from Conn0 (output) to Conn1 (input)
            // Local coordinates are in hologram space - X is forward along the belt
            float SplineLength = FVector::Dist(SplineStartPos, SplineEndPos);
            LaneHolo.SplineData.Length = SplineLength;
            
            FSFSplinePoint StartPoint;
            StartPoint.World = FSFVec3(SplineStartPos);
            StartPoint.Local = FSFVec3(0, 0, 0);  // Start at origin in local space
            // Tangents point forward in local space (+X)
            StartPoint.LeaveTangent = FSFVec3(FVector(100.0f, 0, 0));
            StartPoint.ArriveTangent = FSFVec3(FVector(100.0f, 0, 0));
            
            FSFSplinePoint EndPoint;
            EndPoint.World = FSFVec3(SplineEndPos);
            // End point is at +X in local space (forward along belt)
            EndPoint.Local = FSFVec3(FVector(SplineLength, 0, 0));
            EndPoint.LeaveTangent = FSFVec3(FVector(100.0f, 0, 0));
            EndPoint.ArriveTangent = FSFVec3(FVector(100.0f, 0, 0));
            
            LaneHolo.SplineData.Points.Add(StartPoint);
            LaneHolo.SplineData.Points.Add(EndPoint);
        }
        
        // Set transform to START of spline (Conn0/output position)
        // Rotation points from start toward end - this orients the local +X axis
        FRotator LaneRotation = (SplineEndPos - SplineStartPos).Rotation();
        LaneHolo.Transform = FSFTransform(SplineStartPos, LaneRotation);
        
        // Set clone connections for wiring
        // Conn0 = output end (where items come from)
        // Conn1 = input end (where items go to)
        LaneHolo.CloneConnections.ConveyorAny0.Target = Conn0Target;
        LaneHolo.CloneConnections.ConveyorAny0.Connector = Conn0Connector;
        LaneHolo.CloneConnections.ConveyorAny1.Target = Conn1Target;
        LaneHolo.CloneConnections.ConveyorAny1.Connector = Conn1Connector;
        
        Result.ChildHolograms.Add(LaneHolo);
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE: Generated %s lane %s.%s → %s.%s (type=%s, merger=%d)"),
            *LaneHolo.HologramId,
            *Conn0Target, *Conn0Connector,
            *Conn1Target, *Conn1Connector,
            *LaneHolo.LaneSegmentType,
            bIsMerger ? 1 : 0);
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE: Generated %d manifold lane segments"), LaneSegmentIndex);
    
    return Result;
}

bool FSFCloneTopology::SaveToFile(const FString& FilePath) const
{
    using namespace ManifoldJSONHelpers;
    
    TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
    RootObj->SetStringField(TEXT("schema_version"), SchemaVersion);
    RootObj->SetObjectField(TEXT("world_offset"), Vec3ToJson(WorldOffset));
    RootObj->SetStringField(TEXT("source_factory_id"), SourceFactoryId);
    RootObj->SetStringField(TEXT("parent_build_class"), ParentBuildClass);
    RootObj->SetObjectField(TEXT("parent_transform"), TransformToJson(ParentTransform));
    
    TArray<TSharedPtr<FJsonValue>> ChildArray;
    for (const FSFCloneHologram& Child : ChildHolograms)
    {
        ChildArray.Add(MakeShared<FJsonValueObject>(CloneHologramToJson(Child)));
    }
    RootObj->SetArrayField(TEXT("child_holograms"), ChildArray);
    
    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = 
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
    
    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

// ============================================================================
// FSFSourceTopology - Capture from existing topology
// ============================================================================

namespace CaptureHelpers
{
    FString GetActorId(AActor* Actor)
    {
        if (!Actor) return TEXT("");
        // Use GetName() to match the format used by connection targets
        return Actor->GetName();
    }
    
    FString GetRecipeClassName(TSubclassOf<UFGRecipe> Recipe)
    {
        if (!Recipe) return TEXT("");
        return Recipe->GetName();
    }
    
    FSFSplineData CaptureSplineFromBelt(AFGBuildableConveyorBelt* Belt)
    {
        FSFSplineData Result;
        if (!Belt) return Result;
        
        USplineComponent* Spline = Belt->GetSplineComponent();
        if (!Spline) return Result;
        
        Result.Length = Spline->GetSplineLength();
        
        int32 NumPoints = Spline->GetNumberOfSplinePoints();
        for (int32 i = 0; i < NumPoints; i++)
        {
            FSFSplinePoint Point;
            Point.Local = FSFVec3(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Point.World = FSFVec3(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
            Point.ArriveTangent = FSFVec3(Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Point.LeaveTangent = FSFVec3(Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Result.Points.Add(Point);
        }
        
        return Result;
    }
    
    FSFSplineData CaptureSplineFromPipe(AFGBuildablePipeline* Pipe)
    {
        FSFSplineData Result;
        if (!Pipe) return Result;
        
        USplineComponent* Spline = Pipe->GetSplineComponent();
        if (!Spline) return Result;
        
        Result.Length = Spline->GetSplineLength();
        
        int32 NumPoints = Spline->GetNumberOfSplinePoints();
        for (int32 i = 0; i < NumPoints; i++)
        {
            FSFSplinePoint Point;
            Point.Local = FSFVec3(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Point.World = FSFVec3(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
            Point.ArriveTangent = FSFVec3(Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Point.LeaveTangent = FSFVec3(Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
            Result.Points.Add(Point);
        }
        
        return Result;
    }
    
    FSFLiftData CaptureLiftData(AFGBuildableConveyorLift* Lift)
    {
        FSFLiftData Result;
        if (!Lift) return Result;
        
        Result.Height = Lift->GetHeight();
        Result.bIsReversed = Lift->GetIsReversed();
        
        FTransform TopTransform = Lift->GetTopTransform();
        Result.TopTransform = FSFTransform(TopTransform);
        
        FTransform BottomTransform = Lift->GetActorTransform();
        Result.BottomTransform = FSFTransform(BottomTransform);
        
        // Issue #260: Capture passthrough references for half-height lift rendering
        // Use reflection to access private mSnappedPassthroughs array
        FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
        if (SnappedProp)
        {
            TArray<AFGBuildablePassthrough*>* PassthroughArray = 
                SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(Lift);
            
            if (PassthroughArray && PassthroughArray->Num() > 0)
            {
                Result.PassthroughCloneIds.SetNum(PassthroughArray->Num());
                for (int32 i = 0; i < PassthroughArray->Num(); ++i)
                {
                    AFGBuildablePassthrough* PT = (*PassthroughArray)[i];
                    if (PT)
                    {
                        // Store passthrough actor name as CloneId (will be matched during spawn)
                        Result.PassthroughCloneIds[i] = PT->GetName();
                    }
                }
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("📋 CAPTURE: Lift %s has %d passthroughs: [%s]"),
                    *Lift->GetName(), Result.PassthroughCloneIds.Num(),
                    *FString::Join(Result.PassthroughCloneIds, TEXT(", ")));
            }
        }
        
        return Result;
    }
    
    FSFSourceChain CaptureBeltChain(const FSFConnectionChainNode& Chain, const FString& ChainId, bool bIsInput)
    {
        FSFSourceChain Result;
        Result.ChainId = ChainId;
        
        // Factory connector
        if (Chain.SourceConnector.IsValid())
        {
            Result.FactoryConnector = Chain.SourceConnector->GetFName().ToString();
        }
        
        // Distributor
        if (Chain.Distributor.IsValid())
        {
            AFGBuildable* Dist = Chain.Distributor.Get();
            Result.Distributor.Id = GetActorId(Dist);
            Result.Distributor.Class = Dist->GetClass()->GetName();
            // Get recipe directly from distributor actor (Chain.DistributorRecipe may not be populated)
            Result.Distributor.RecipeClass = GetRecipeClassName(Dist->GetBuiltWithRecipe());
            Result.Distributor.Transform = FSFTransform(Dist->GetActorLocation(), Dist->GetActorRotation());
            
            if (Chain.DistributorConnector.IsValid())
            {
                Result.Distributor.ConnectorUsed = Chain.DistributorConnector->GetFName().ToString();
            }
            
            // Capture which connectors are already connected (for manifold lane selection)
            TArray<UFGFactoryConnectionComponent*> DistConnectors;
            Dist->GetComponents<UFGFactoryConnectionComponent>(DistConnectors);
            for (UFGFactoryConnectionComponent* DistConn : DistConnectors)
            {
                if (DistConn && DistConn->IsConnected())
                {
                    Result.Distributor.ConnectedConnectors.Add(DistConn->GetFName().ToString());
                }
            }
        }
        
        // Segments (conveyors - belts and lifts)
        for (int32 i = 0; i < Chain.Conveyors.Num(); i++)
        {
            AFGBuildableConveyorBase* Conveyor = Chain.Conveyors[i].Get();
            if (!Conveyor) continue;
            
            FSFSourceSegment Seg;
            Seg.Index = i;
            Seg.Id = GetActorId(Conveyor);
            Seg.Class = Conveyor->GetClass()->GetName();
            Seg.RecipeClass = GetRecipeClassName(Conveyor->GetBuiltWithRecipe());
            Seg.Transform = FSFTransform(Conveyor->GetActorLocation(), Conveyor->GetActorRotation());
            
            // Capture connections
            UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
            UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();
            
            if (Conn0 && Conn0->GetConnection())
            {
                AActor* ConnectedActor = Conn0->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny0.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny0.Connector = Conn0->GetConnection()->GetFName().ToString();
            }
            if (Conn1 && Conn1->GetConnection())
            {
                AActor* ConnectedActor = Conn1->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny1.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny1.Connector = Conn1->GetConnection()->GetFName().ToString();
            }
            
            // Capture chain actor info (for debugging chain groupings)
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            if (ChainActor)
            {
                Seg.ChainActorName = ChainActor->GetName();
                // Get segment index within the chain
                FConveyorChainSplineSegment* ChainSegment = ChainActor->GetSegmentForConveyorBase(Conveyor);
                if (ChainSegment)
                {
                    // The segment index is the position in the chain's segment array
                    const TArray<FConveyorChainSplineSegment>& Segments = ChainActor->GetChainSegments();
                    for (int32 SegIdx = 0; SegIdx < Segments.Num(); SegIdx++)
                    {
                        if (&Segments[SegIdx] == ChainSegment)
                        {
                            Seg.ChainSegmentIndex = SegIdx;
                            break;
                        }
                    }
                }
            }
            
            // Check if belt or lift
            if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Conveyor))
            {
                Seg.Type = TEXT("belt");
                Seg.bHasSplineData = true;
                Seg.SplineData = CaptureSplineFromBelt(Belt);
            }
            else if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Conveyor))
            {
                Seg.Type = TEXT("lift");
                Seg.bHasLiftData = true;
                Seg.LiftData = CaptureLiftData(Lift);
            }
            
            Result.Segments.Add(Seg);
        }
        
        return Result;
    }
    
    FSFSourceChain CapturePipeChain(const FSFPipeConnectionChainNode& Chain, const FString& ChainId, bool bIsInput)
    {
        FSFSourceChain Result;
        Result.ChainId = ChainId;
        
        // Factory connector
        if (Chain.SourceConnector.IsValid())
        {
            Result.FactoryConnector = Chain.SourceConnector->GetFName().ToString();
        }
        
        // Junction (distributor equivalent for pipes)
        if (Chain.Junction.IsValid())
        {
            AFGBuildable* Junc = Chain.Junction.Get();
            Result.Distributor.Id = GetActorId(Junc);
            Result.Distributor.Class = Junc->GetClass()->GetName();
            // Get recipe directly from junction actor (Chain.JunctionRecipe may not be populated)
            Result.Distributor.RecipeClass = GetRecipeClassName(Junc->GetBuiltWithRecipe());
            Result.Distributor.Transform = FSFTransform(Junc->GetActorLocation(), Junc->GetActorRotation());
            
            if (Chain.JunctionConnector.IsValid())
            {
                Result.Distributor.ConnectorUsed = Chain.JunctionConnector->GetFName().ToString();
            }
            
            // Capture connector info: which are connected and their world positions
            TArray<UFGPipeConnectionComponentBase*> JuncConns;
            Junc->GetComponents<UFGPipeConnectionComponentBase>(JuncConns);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ PIPE CAPTURE: Junction %s at (%.0f,%.0f,%.0f), capturing %d connectors"),
                *Result.Distributor.Id, Junc->GetActorLocation().X, Junc->GetActorLocation().Y, Junc->GetActorLocation().Z, JuncConns.Num());
            for (UFGPipeConnectionComponentBase* JuncConn : JuncConns)
            {
                if (JuncConn)
                {
                    FString ConnName = JuncConn->GetFName().ToString();
                    FVector ConnPos = JuncConn->GetComponentLocation();
                    
                    // Store world position for lane spline generation
                    Result.Distributor.ConnectorWorldPositions.Add(ConnName, ConnPos);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ PIPE CAPTURE:   %s @ (%.0f,%.0f,%.0f) connected=%d"),
                        *ConnName, ConnPos.X, ConnPos.Y, ConnPos.Z, JuncConn->IsConnected() ? 1 : 0);
                    
                    // Track which are already connected
                    if (JuncConn->IsConnected())
                    {
                        Result.Distributor.ConnectedConnectors.Add(ConnName);
                    }
                }
            }
        }
        
        // Segments (pipelines)
        for (int32 i = 0; i < Chain.Pipelines.Num(); i++)
        {
            AFGBuildablePipeline* Pipe = Chain.Pipelines[i].Get();
            if (!Pipe) continue;
            
            FSFSourceSegment Seg;
            Seg.Index = i;
            Seg.Type = TEXT("pipe");
            Seg.Id = GetActorId(Pipe);
            Seg.Class = Pipe->GetClass()->GetName();
            Seg.RecipeClass = GetRecipeClassName(Pipe->GetBuiltWithRecipe());
            Seg.Transform = FSFTransform(Pipe->GetActorLocation(), Pipe->GetActorRotation());
            
            // Capture spline data
            Seg.bHasSplineData = true;
            Seg.SplineData = CaptureSplineFromPipe(Pipe);
            
            // Capture pipe connections (uses UFGPipeConnectionComponentBase API)
            UFGPipeConnectionComponentBase* PipeConn0 = Pipe->GetPipeConnection0();
            UFGPipeConnectionComponentBase* PipeConn1 = Pipe->GetPipeConnection1();
            
            if (PipeConn0 && PipeConn0->GetConnection())
            {
                AActor* ConnectedActor = PipeConn0->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny0.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny0.Connector = PipeConn0->GetConnection()->GetFName().ToString();
            }
            if (PipeConn1 && PipeConn1->GetConnection())
            {
                AActor* ConnectedActor = PipeConn1->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny1.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny1.Connector = PipeConn1->GetConnection()->GetFName().ToString();
            }
            
            Result.Segments.Add(Seg);
        }
        
        // Issue #260: Capture pipe passthroughs (floor holes) as special segments
        for (int32 i = 0; i < Chain.Passthroughs.Num(); i++)
        {
            AFGBuildable* Passthrough = Chain.Passthroughs[i].Get();
            if (!Passthrough) continue;
            
            FSFSourceSegment PassSeg;
            PassSeg.Index = Result.Segments.Num();  // After pipe segments
            PassSeg.Type = TEXT("passthrough");
            PassSeg.Id = GetActorId(Passthrough);
            PassSeg.Class = Passthrough->GetClass()->GetName();
            PassSeg.RecipeClass = GetRecipeClassName(Passthrough->GetBuiltWithRecipe());
            PassSeg.Transform = FSFTransform(Passthrough->GetActorLocation(), Passthrough->GetActorRotation());
            
            Result.Segments.Add(PassSeg);
            
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND CAPTURE: Pipe passthrough %s at %s"),
                *PassSeg.Id, *Passthrough->GetActorLocation().ToString());
        }
        
        // Issue #288: Capture pipe attachments (valves, pumps) as special segments.
        // AFGBuildablePipelinePump is the common base; the user-configured state we need
        // to preserve is mUserFlowLimit (-1 = unlimited / valve fully open).
        for (int32 i = 0; i < Chain.PipeAttachments.Num(); i++)
        {
            AFGBuildable* Attachment = Chain.PipeAttachments[i].Get();
            if (!Attachment) continue;
            
            FSFSourceSegment AttSeg;
            AttSeg.Index = Result.Segments.Num();
            AttSeg.Type = TEXT("pipe_attachment");
            AttSeg.Id = GetActorId(Attachment);
            AttSeg.Class = Attachment->GetClass()->GetName();
            AttSeg.RecipeClass = GetRecipeClassName(Attachment->GetBuiltWithRecipe());
            AttSeg.Transform = FSFTransform(Attachment->GetActorLocation(), Attachment->GetActorRotation());
            
            if (AFGBuildablePipelinePump* Pump = Cast<AFGBuildablePipelinePump>(Attachment))
            {
                AttSeg.UserFlowLimit = Pump->GetUserFlowLimit();
                
                // Issue #288: record the source pole this pump is directly connected to.
                // Only captured if the pole is itself present in the caller's manifold
                // power-pole set (checked at emit-time via SourceIdToHologramId — a pole
                // that isn't in the clone topology can never produce a valid clone wiring
                // target, so skipping it keeps the validator honest). Here we simply
                // record the pole's actor id; membership resolution happens later.
                UFGPowerConnectionComponent* PumpPowerInput = Pump->FindComponentByClass<UFGPowerConnectionComponent>();
                if (PumpPowerInput)
                {
                    TArray<UFGCircuitConnectionComponent*> PowerConns;
                    PumpPowerInput->GetConnections(PowerConns);
                    for (UFGCircuitConnectionComponent* Conn : PowerConns)
                    {
                        if (!Conn) continue;
                        if (AFGBuildablePowerPole* ConnectedPole = Cast<AFGBuildablePowerPole>(Conn->GetOwner()))
                        {
                            AttSeg.ConnectedPowerPoleSourceId = GetActorId(ConnectedPole);
                            break;  // pumps have exactly one power connection slot
                        }
                    }
                }
            }
            
            Result.Segments.Add(AttSeg);
            
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND CAPTURE: Pipe attachment %s (class=%s) UserFlowLimit=%.3f PowerPole=%s at %s"),
                *AttSeg.Id, *AttSeg.Class, AttSeg.UserFlowLimit,
                AttSeg.ConnectedPowerPoleSourceId.IsEmpty() ? TEXT("<none>") : *AttSeg.ConnectedPowerPoleSourceId,
                *Attachment->GetActorLocation().ToString());
        }
        
        return Result;
    }
}

FSFSourceTopology FSFSourceTopology::CaptureFromTopology(const FSFExtendTopology& Topology)
{
    using namespace CaptureHelpers;
    
    FSFSourceTopology Result;
    Result.SchemaVersion = TEXT("1.0");
    Result.CaptureTimestamp = FDateTime::UtcNow().ToIso8601();
    
    // Factory
    if (Topology.SourceBuilding.IsValid())
    {
        AFGBuildable* Factory = Topology.SourceBuilding.Get();
        Result.Factory.Id = GetActorId(Factory);
        Result.Factory.Class = Factory->GetClass()->GetName();
        Result.Factory.Transform = FSFTransform(Factory->GetActorLocation(), Factory->GetActorRotation());
    }
    
    // Belt input chains
    for (int32 i = 0; i < Topology.InputChains.Num(); i++)
    {
        FString ChainId = FString::Printf(TEXT("belt_input_%d"), i);
        Result.BeltInputChains.Add(CaptureBeltChain(Topology.InputChains[i], ChainId, true));
    }
    
    // Belt output chains
    for (int32 i = 0; i < Topology.OutputChains.Num(); i++)
    {
        FString ChainId = FString::Printf(TEXT("belt_output_%d"), i);
        Result.BeltOutputChains.Add(CaptureBeltChain(Topology.OutputChains[i], ChainId, false));
    }
    
    // Pipe input chains
    for (int32 i = 0; i < Topology.PipeInputChains.Num(); i++)
    {
        FString ChainId = FString::Printf(TEXT("pipe_input_%d"), i);
        Result.PipeInputChains.Add(CapturePipeChain(Topology.PipeInputChains[i], ChainId, true));
    }
    
    // Pipe output chains
    for (int32 i = 0; i < Topology.PipeOutputChains.Num(); i++)
    {
        FString ChainId = FString::Printf(TEXT("pipe_output_%d"), i);
        Result.PipeOutputChains.Add(CapturePipeChain(Topology.PipeOutputChains[i], ChainId, false));
    }
    
    // Power poles (Issue #229)
    for (int32 i = 0; i < Topology.PowerPoles.Num(); i++)
    {
        const FSFPowerChainNode& PowerNode = Topology.PowerPoles[i];
        if (!PowerNode.PowerPole.IsValid())
        {
            continue;
        }
        
        AFGBuildablePowerPole* Pole = PowerNode.PowerPole.Get();
        FSFSourcePowerPole SourcePole;
        SourcePole.Id = GetActorId(Pole);
        SourcePole.Class = Pole->GetClass()->GetName();
        SourcePole.RecipeClass = GetRecipeClassName(Pole->GetBuiltWithRecipe());
        SourcePole.Transform = FSFTransform(Pole->GetActorLocation(), Pole->GetActorRotation());
        SourcePole.RelativeOffset = FSFVec3(PowerNode.RelativeOffset);
        SourcePole.bSourceHasFreeConnections = PowerNode.bSourceHasFreeConnections;
        SourcePole.SourceFreeConnections = PowerNode.SourceFreeConnections;
        SourcePole.MaxConnections = PowerNode.MaxConnections;
        
        Result.PowerPoles.Add(SourcePole);
    }
    
    // Issue #260: Passthroughs (spatially discovered — both pipe and lift floor holes)
    for (int32 i = 0; i < Topology.PipePassthroughs.Num(); i++)
    {
        AFGBuildable* Passthrough = Topology.PipePassthroughs[i].Get();
        if (!Passthrough) continue;
        
        FSFSourceSegment PassSeg;
        PassSeg.Index = i;
        PassSeg.Type = TEXT("passthrough");
        PassSeg.Id = GetActorId(Passthrough);
        PassSeg.Class = Passthrough->GetClass()->GetName();
        PassSeg.RecipeClass = GetRecipeClassName(Passthrough->GetBuiltWithRecipe());
        PassSeg.Transform = FSFTransform(Passthrough->GetActorLocation(), Passthrough->GetActorRotation());
        
        // Read mSnappedBuildingThickness via reflection (protected member on AFGBuildablePassthrough)
        FFloatProperty* ThickProp = CastField<FFloatProperty>(
            Passthrough->GetClass()->FindPropertyByName(FName(TEXT("mSnappedBuildingThickness"))));
        if (ThickProp)
        {
            PassSeg.Thickness = ThickProp->GetPropertyValue_InContainer(Passthrough);
        }
        
        Result.PipePassthroughs.Add(PassSeg);
        
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND CAPTURE: Passthrough %s (class=%s, thickness=%.0f) at %s"),
            *PassSeg.Id, *PassSeg.Class, PassSeg.Thickness, *Passthrough->GetActorLocation().ToString());
    }

    // Wall holes (spatially discovered — Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C,
    // any *WallHole_C variants). These are Blueprint-only SnapOnly decorators with no connection
    // components and no thickness field; we just need their class, recipe, and transform to clone them.
    for (int32 i = 0; i < Topology.WallPassthroughs.Num(); i++)
    {
        AFGBuildable* WallHole = Topology.WallPassthroughs[i].Get();
        if (!WallHole) continue;

        FSFSourceSegment WallSeg;
        WallSeg.Index = i;
        WallSeg.Type = TEXT("wall_hole");
        WallSeg.Id = GetActorId(WallHole);
        WallSeg.Class = WallHole->GetClass()->GetName();
        WallSeg.RecipeClass = GetRecipeClassName(WallHole->GetBuiltWithRecipe());
        WallSeg.Transform = FSFTransform(WallHole->GetActorLocation(), WallHole->GetActorRotation());

        Result.WallHoles.Add(WallSeg);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND CAPTURE: Wall hole %s (class=%s) at %s"),
            *WallSeg.Id, *WallSeg.Class, *WallHole->GetActorLocation().ToString());
    }

    return Result;
}

FSFSourceTopology FSFSourceTopology::CaptureFromBuiltFactory(AFGBuildableFactory* Factory)
{
    using namespace CaptureHelpers;
    
    FSFSourceTopology Result;
    Result.SchemaVersion = TEXT("1.0");
    Result.CaptureTimestamp = FDateTime::UtcNow().ToIso8601();
    
    if (!Factory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("📋 CaptureFromBuiltFactory: No factory provided"));
        return Result;
    }
    
    // Factory info
    Result.Factory.Id = GetActorId(Factory);
    Result.Factory.Class = Factory->GetClass()->GetName();
    Result.Factory.Transform = FSFTransform(Factory->GetActorLocation(), Factory->GetActorRotation());
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("📋 CaptureFromBuiltFactory: Capturing topology for %s"), *Factory->GetName());
    
    // Walk belt connections from factory
    TArray<UFGFactoryConnectionComponent*> FactoryConnections;
    Factory->GetComponents<UFGFactoryConnectionComponent>(FactoryConnections);
    
    int32 BeltInputIdx = 0;
    int32 BeltOutputIdx = 0;
    
    for (UFGFactoryConnectionComponent* FactoryConn : FactoryConnections)
    {
        if (!FactoryConn || !FactoryConn->IsConnected())
        {
            continue;
        }
        
        // Get connected component
        UFGFactoryConnectionComponent* ConnectedComp = FactoryConn->GetConnection();
        if (!ConnectedComp)
        {
            continue;
        }
        
        // Check if connected to a conveyor
        AFGBuildableConveyorBase* FirstConveyor = Cast<AFGBuildableConveyorBase>(ConnectedComp->GetOwner());
        if (!FirstConveyor)
        {
            continue;
        }
        
        // Determine if input or output chain based on factory connector direction
        bool bIsInput = (FactoryConn->GetDirection() == EFactoryConnectionDirection::FCD_INPUT);
        
        FSFSourceChain Chain;
        Chain.ChainId = bIsInput ? FString::Printf(TEXT("belt_input_%d"), BeltInputIdx++) 
                                  : FString::Printf(TEXT("belt_output_%d"), BeltOutputIdx++);
        Chain.FactoryConnector = FactoryConn->GetFName().ToString();
        
        // Walk the chain to find distributor and all segments
        TArray<AFGBuildableConveyorBase*> ChainConveyors;
        AFGBuildableConveyorBase* Current = FirstConveyor;
        TSet<AFGBuildableConveyorBase*> Visited;
        AFGBuildable* Distributor = nullptr;
        UFGFactoryConnectionComponent* DistributorConnector = nullptr;
        
        while (Current && !Visited.Contains(Current))
        {
            Visited.Add(Current);
            ChainConveyors.Add(Current);
            
            // Get the "other" connection (away from factory)
            UFGFactoryConnectionComponent* Conn0 = Current->GetConnection0();
            UFGFactoryConnectionComponent* Conn1 = Current->GetConnection1();
            
            // Determine which connection leads away from factory
            UFGFactoryConnectionComponent* NextConn = nullptr;
            if (bIsInput)
            {
                // For input chains, walk backward (Conn0 direction)
                NextConn = Conn0;
            }
            else
            {
                // For output chains, walk forward (Conn1 direction)
                NextConn = Conn1;
            }
            
            if (!NextConn || !NextConn->IsConnected())
            {
                break;
            }
            
            UFGFactoryConnectionComponent* OtherConn = NextConn->GetConnection();
            if (!OtherConn)
            {
                break;
            }
            
            AActor* OtherOwner = OtherConn->GetOwner();
            
            // Check if it's another conveyor
            AFGBuildableConveyorBase* NextConveyor = Cast<AFGBuildableConveyorBase>(OtherOwner);
            if (NextConveyor)
            {
                Current = NextConveyor;
                continue;
            }
            
            // Check if it's a distributor (splitter/merger)
            if (OtherOwner && (OtherOwner->GetClass()->GetName().Contains(TEXT("Splitter")) ||
                               OtherOwner->GetClass()->GetName().Contains(TEXT("Merger"))))
            {
                Distributor = Cast<AFGBuildable>(OtherOwner);
                DistributorConnector = OtherConn;
            }
            
            break;
        }
        
        // Capture distributor info
        if (Distributor)
        {
            Chain.Distributor.Id = GetActorId(Distributor);
            Chain.Distributor.Class = Distributor->GetClass()->GetName();
            Chain.Distributor.RecipeClass = GetRecipeClassName(Distributor->GetBuiltWithRecipe());
            Chain.Distributor.Transform = FSFTransform(Distributor->GetActorLocation(), Distributor->GetActorRotation());
            if (DistributorConnector)
            {
                Chain.Distributor.ConnectorUsed = DistributorConnector->GetFName().ToString();
            }
        }
        
        // Capture segments
        for (int32 i = 0; i < ChainConveyors.Num(); i++)
        {
            AFGBuildableConveyorBase* Conveyor = ChainConveyors[i];
            
            FSFSourceSegment Seg;
            Seg.Index = i;
            Seg.Id = GetActorId(Conveyor);
            Seg.Class = Conveyor->GetClass()->GetName();
            Seg.RecipeClass = GetRecipeClassName(Conveyor->GetBuiltWithRecipe());
            Seg.Transform = FSFTransform(Conveyor->GetActorLocation(), Conveyor->GetActorRotation());
            
            // Capture connections
            UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
            UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();
            
            if (Conn0 && Conn0->GetConnection())
            {
                AActor* ConnectedActor = Conn0->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny0.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny0.Connector = Conn0->GetConnection()->GetFName().ToString();
            }
            if (Conn1 && Conn1->GetConnection())
            {
                AActor* ConnectedActor = Conn1->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny1.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny1.Connector = Conn1->GetConnection()->GetFName().ToString();
            }
            
            // Capture chain actor info
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            if (ChainActor)
            {
                Seg.ChainActorName = ChainActor->GetName();
                FConveyorChainSplineSegment* ChainSegment = ChainActor->GetSegmentForConveyorBase(Conveyor);
                if (ChainSegment)
                {
                    const TArray<FConveyorChainSplineSegment>& Segments = ChainActor->GetChainSegments();
                    for (int32 SegIdx = 0; SegIdx < Segments.Num(); SegIdx++)
                    {
                        if (&Segments[SegIdx] == ChainSegment)
                        {
                            Seg.ChainSegmentIndex = SegIdx;
                            break;
                        }
                    }
                }
            }
            
            // Check if belt or lift
            if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Conveyor))
            {
                Seg.Type = TEXT("belt");
                Seg.bHasSplineData = true;
                Seg.SplineData = CaptureSplineFromBelt(Belt);
            }
            else if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Conveyor))
            {
                Seg.Type = TEXT("lift");
                Seg.bHasLiftData = true;
                Seg.LiftData = CaptureLiftData(Lift);
            }
            
            Chain.Segments.Add(Seg);
        }
        
        // Add to appropriate chain array
        if (bIsInput)
        {
            Result.BeltInputChains.Add(Chain);
        }
        else
        {
            Result.BeltOutputChains.Add(Chain);
        }
    }
    
    // Walk pipe connections from factory
    TArray<UFGPipeConnectionComponentBase*> PipeConnections;
    Factory->GetComponents<UFGPipeConnectionComponentBase>(PipeConnections);
    
    int32 PipeInputIdx = 0;
    int32 PipeOutputIdx = 0;
    
    for (UFGPipeConnectionComponentBase* FactoryPipeConn : PipeConnections)
    {
        if (!FactoryPipeConn || !FactoryPipeConn->IsConnected())
        {
            continue;
        }
        
        UFGPipeConnectionComponentBase* ConnectedPipeComp = FactoryPipeConn->GetConnection();
        if (!ConnectedPipeComp)
        {
            continue;
        }
        
        AFGBuildablePipeline* FirstPipe = Cast<AFGBuildablePipeline>(ConnectedPipeComp->GetOwner());
        if (!FirstPipe)
        {
            continue;
        }
        
        // Determine if input or output based on pipe type
        bool bIsInput = (FactoryPipeConn->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER);
        
        FSFSourceChain Chain;
        Chain.ChainId = bIsInput ? FString::Printf(TEXT("pipe_input_%d"), PipeInputIdx++) 
                                  : FString::Printf(TEXT("pipe_output_%d"), PipeOutputIdx++);
        Chain.FactoryConnector = FactoryPipeConn->GetFName().ToString();
        
        // Walk the pipe chain
        TArray<AFGBuildablePipeline*> ChainPipes;
        AFGBuildablePipeline* CurrentPipe = FirstPipe;
        TSet<AFGBuildablePipeline*> VisitedPipes;
        AFGBuildable* Junction = nullptr;
        UFGPipeConnectionComponentBase* JunctionConnector = nullptr;
        
        while (CurrentPipe && !VisitedPipes.Contains(CurrentPipe))
        {
            VisitedPipes.Add(CurrentPipe);
            ChainPipes.Add(CurrentPipe);
            
            UFGPipeConnectionComponentBase* PipeConn0 = CurrentPipe->GetPipeConnection0();
            UFGPipeConnectionComponentBase* PipeConn1 = CurrentPipe->GetPipeConnection1();
            
            // Find the connection that leads away from factory
            UFGPipeConnectionComponentBase* NextPipeConn = nullptr;
            if (PipeConn0 && PipeConn0->IsConnected())
            {
                AActor* Conn0Owner = PipeConn0->GetConnection()->GetOwner();
                if (Conn0Owner != Factory && !VisitedPipes.Contains(Cast<AFGBuildablePipeline>(Conn0Owner)))
                {
                    NextPipeConn = PipeConn0;
                }
            }
            if (!NextPipeConn && PipeConn1 && PipeConn1->IsConnected())
            {
                AActor* Conn1Owner = PipeConn1->GetConnection()->GetOwner();
                if (Conn1Owner != Factory && !VisitedPipes.Contains(Cast<AFGBuildablePipeline>(Conn1Owner)))
                {
                    NextPipeConn = PipeConn1;
                }
            }
            
            if (!NextPipeConn)
            {
                break;
            }
            
            UFGPipeConnectionComponentBase* OtherPipeConn = NextPipeConn->GetConnection();
            if (!OtherPipeConn)
            {
                break;
            }
            
            AActor* OtherOwner = OtherPipeConn->GetOwner();
            
            AFGBuildablePipeline* NextPipe = Cast<AFGBuildablePipeline>(OtherOwner);
            if (NextPipe)
            {
                CurrentPipe = NextPipe;
                continue;
            }
            
            // Check if it's a junction
            if (OtherOwner && OtherOwner->GetClass()->GetName().Contains(TEXT("Junction")))
            {
                Junction = Cast<AFGBuildable>(OtherOwner);
                JunctionConnector = OtherPipeConn;
            }
            
            break;
        }
        
        // Capture junction info
        if (Junction)
        {
            Chain.Distributor.Id = GetActorId(Junction);
            Chain.Distributor.Class = Junction->GetClass()->GetName();
            Chain.Distributor.RecipeClass = GetRecipeClassName(Junction->GetBuiltWithRecipe());
            Chain.Distributor.Transform = FSFTransform(Junction->GetActorLocation(), Junction->GetActorRotation());
            if (JunctionConnector)
            {
                Chain.Distributor.ConnectorUsed = JunctionConnector->GetFName().ToString();
            }
        }
        
        // Capture pipe segments
        for (int32 i = 0; i < ChainPipes.Num(); i++)
        {
            AFGBuildablePipeline* Pipe = ChainPipes[i];
            
            FSFSourceSegment Seg;
            Seg.Index = i;
            Seg.Type = TEXT("pipe");
            Seg.Id = GetActorId(Pipe);
            Seg.Class = Pipe->GetClass()->GetName();
            Seg.RecipeClass = GetRecipeClassName(Pipe->GetBuiltWithRecipe());
            Seg.Transform = FSFTransform(Pipe->GetActorLocation(), Pipe->GetActorRotation());
            
            Seg.bHasSplineData = true;
            Seg.SplineData = CaptureSplineFromPipe(Pipe);
            
            // Capture pipe connections
            UFGPipeConnectionComponentBase* PipeConn0 = Pipe->GetPipeConnection0();
            UFGPipeConnectionComponentBase* PipeConn1 = Pipe->GetPipeConnection1();
            
            if (PipeConn0 && PipeConn0->GetConnection())
            {
                AActor* ConnectedActor = PipeConn0->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny0.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny0.Connector = PipeConn0->GetConnection()->GetFName().ToString();
            }
            if (PipeConn1 && PipeConn1->GetConnection())
            {
                AActor* ConnectedActor = PipeConn1->GetConnection()->GetOwner();
                Seg.Connections.ConveyorAny1.Target = ConnectedActor ? ConnectedActor->GetName() : TEXT("");
                Seg.Connections.ConveyorAny1.Connector = PipeConn1->GetConnection()->GetFName().ToString();
            }
            
            Chain.Segments.Add(Seg);
        }
        
        // Add to appropriate chain array
        if (bIsInput)
        {
            Result.PipeInputChains.Add(Chain);
        }
        else
        {
            Result.PipeOutputChains.Add(Chain);
        }
    }
    
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("📋 CaptureFromBuiltFactory: Captured %d belt input, %d belt output, %d pipe input, %d pipe output chains"),
        Result.BeltInputChains.Num(), Result.BeltOutputChains.Num(),
        Result.PipeInputChains.Num(), Result.PipeOutputChains.Num());
    
    return Result;
}

// ============================================================================
// FSFCloneTopology - Spawn child holograms
// ============================================================================

namespace SpawnHelpers
{
    // Find recipe by class name
    TSubclassOf<UFGRecipe> FindRecipeByName(UWorld* World, const FString& RecipeClassName)
    {
        if (RecipeClassName.IsEmpty() || !World)
        {
            return nullptr;
        }
        
        // Get recipe manager
        AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
        if (!RecipeManager)
        {
            return nullptr;
        }
        
        // Search available recipes
        TArray<TSubclassOf<UFGRecipe>> AllRecipes;
        RecipeManager->GetAllAvailableRecipes(AllRecipes);
        
        for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
        {
            if (Recipe && Recipe->GetName() == RecipeClassName)
            {
                return Recipe;
            }
        }
        
        return nullptr;
    }
    
    // Find build class by name
    TSubclassOf<AFGBuildable> FindBuildClassByName(const FString& BuildClassName)
    {
        if (BuildClassName.IsEmpty())
        {
            return nullptr;
        }
        
        // Try to find the class by name using FindFirstObject (replaces deprecated ANY_PACKAGE)
        UClass* FoundClass = FindFirstObject<UClass>(*BuildClassName, EFindFirstObjectOptions::ExactClass);
        if (!FoundClass)
        {
            // Try without ExactClass flag
            FoundClass = FindFirstObject<UClass>(*BuildClassName, EFindFirstObjectOptions::None);
        }
        
        if (FoundClass && FoundClass->IsChildOf(AFGBuildable::StaticClass()))
        {
            return TSubclassOf<AFGBuildable>(FoundClass);
        }
        
        return nullptr;
    }
    
    // Convert FSFSplineData to TArray<FSplinePointData>
    TArray<FSplinePointData> ConvertSplineData(const FSFSplineData& SplineData)
    {
        TArray<FSplinePointData> Result;
        for (const FSFSplinePoint& Point : SplineData.Points)
        {
            FSplinePointData PointData;
            PointData.Location = Point.Local.ToFVector();
            PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
            PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
            Result.Add(PointData);
        }
        return Result;
    }
}

int32 FSFCloneTopology::SpawnChildHolograms(
    AFGHologram* ParentHologram,
    USFExtendService* ExtendService,
    TMap<FString, AFGHologram*>& OutSpawnedHolograms) const
{
    using namespace SpawnHelpers;
    
    if (!ParentHologram || !ExtendService)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Invalid parent hologram or extend service"));
        return 0;
    }
    
    UWorld* World = ParentHologram->GetWorld();
    if (!World)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: No world available"));
        return 0;
    }
    
    int32 SpawnedCount = 0;
    static int32 JsonSpawnCounter = 0;
    
    // Build customization lookup from source actors so clones inherit source colors, not parent's
    TMap<FString, FFactoryCustomizationData> SourceCustomizationMap;
    const FSFExtendTopology& Topology = ExtendService->GetCurrentTopology();
    auto CaptureCustomization = [&](AFGBuildable* Actor)
    {
        if (Actor && IsValid(Actor))
        {
            SourceCustomizationMap.Add(Actor->GetName(), Actor->GetCustomizationData_Implementation());
        }
    };
    for (const FSFConnectionChainNode& Chain : Topology.InputChains)
    {
        if (Chain.Distributor.IsValid()) CaptureCustomization(Chain.Distributor.Get());
        for (const auto& Conv : Chain.Conveyors) { if (Conv.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Conv.Get())); }
    }
    for (const FSFConnectionChainNode& Chain : Topology.OutputChains)
    {
        if (Chain.Distributor.IsValid()) CaptureCustomization(Chain.Distributor.Get());
        for (const auto& Conv : Chain.Conveyors) { if (Conv.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Conv.Get())); }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeInputChains)
    {
        if (Chain.Junction.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Chain.Junction.Get()));
        for (const auto& Pipe : Chain.Pipelines) { if (Pipe.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Pipe.Get())); }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeOutputChains)
    {
        if (Chain.Junction.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Chain.Junction.Get()));
        for (const auto& Pipe : Chain.Pipelines) { if (Pipe.IsValid()) CaptureCustomization(Cast<AFGBuildable>(Pipe.Get())); }
    }
    for (const FSFPowerChainNode& PoleNode : Topology.PowerPoles)
    {
        if (PoleNode.PowerPole.IsValid()) CaptureCustomization(Cast<AFGBuildable>(PoleNode.PowerPole.Get()));
    }
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🎨 JSON SPAWN: Captured customization data from %d source actors"), SourceCustomizationMap.Num());
    
    // Build lane segment color lookup: distributor name → customization from the first belt/pipe
    // on the "other side" of the distributor (away from factory). Lane segments should inherit
    // colors from existing infrastructure, NOT from the factory building. If no belt/pipe exists
    // on the other side, we leave the entry absent so lane segments get default colors.
    TMap<FString, FFactoryCustomizationData> LaneColorMap;
    for (const FSFConnectionChainNode& Chain : Topology.InputChains)
    {
        if (Chain.Distributor.IsValid() && Chain.Conveyors.Num() > 0)
        {
            if (AFGBuildableConveyorBase* FirstConv = Chain.Conveyors[0].Get())
            {
                LaneColorMap.Add(Chain.Distributor->GetName(), Cast<AFGBuildable>(FirstConv)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFConnectionChainNode& Chain : Topology.OutputChains)
    {
        if (Chain.Distributor.IsValid() && Chain.Conveyors.Num() > 0)
        {
            if (AFGBuildableConveyorBase* FirstConv = Chain.Conveyors[0].Get())
            {
                LaneColorMap.Add(Chain.Distributor->GetName(), Cast<AFGBuildable>(FirstConv)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeInputChains)
    {
        if (Chain.Junction.IsValid() && Chain.Pipelines.Num() > 0)
        {
            if (AFGBuildablePipeline* FirstPipe = Chain.Pipelines[0].Get())
            {
                LaneColorMap.Add(Chain.Junction->GetName(), Cast<AFGBuildable>(FirstPipe)->GetCustomizationData_Implementation());
            }
        }
    }
    for (const FSFPipeConnectionChainNode& Chain : Topology.PipeOutputChains)
    {
        if (Chain.Junction.IsValid() && Chain.Pipelines.Num() > 0)
        {
            if (AFGBuildablePipeline* FirstPipe = Chain.Pipelines[0].Get())
            {
                LaneColorMap.Add(Chain.Junction->GetName(), Cast<AFGBuildable>(FirstPipe)->GetCustomizationData_Implementation());
            }
        }
    }
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🎨 JSON SPAWN: Built lane color map from %d distributors with existing infrastructure"), LaneColorMap.Num());
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Starting spawn of %d child holograms"), ChildHolograms.Num());
    
    for (const FSFCloneHologram& ChildData : ChildHolograms)
    {
        AFGHologram* SpawnedHologram = nullptr;
        
        // Get transform
        FVector Location = ChildData.Transform.Location.ToFVector();
        FRotator Rotation = ChildData.Transform.Rotation.ToFRotator();
        
        // Find recipe
        TSubclassOf<UFGRecipe> Recipe = FindRecipeByName(World, ChildData.RecipeClass);
        
        // Find build class
        TSubclassOf<AFGBuildable> BuildClass = FindBuildClassByName(ChildData.BuildClass);
        
        if (ChildData.Role == TEXT("distributor"))
        {
            // Spawn distributor child hologram
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for distributor %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonDistributor_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorAttachmentChildHologram* DistributorChild = World->SpawnActor<ASFConveyorAttachmentChildHologram>(
                ASFConveyorAttachmentChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (DistributorChild)
            {
                // Match working EXTEND code order exactly
                DistributorChild->SetBuildClass(BuildClass);
                DistributorChild->SetRecipe(Recipe);
                
                DistributorChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Add as child IMMEDIATELY after FinishSpawning (matches working EXTEND code)
                ParentHologram->AddChild(DistributorChild, ChildName);
                
                // NOTE: Do NOT call ProvideFloorHitResult here - it calls SetHologramLocationAndRotation
                // which adjusts the Z position and causes distributors to be raised ~100 units
                
                // Disable validation AFTER AddChild
                USFHologramDataService::DisableValidation(DistributorChild);
                USFHologramDataService::MarkAsChild(DistributorChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                // Configure visibility
                if (DistributorChild->IsHologramLocked())
                {
                    DistributorChild->LockHologramPosition(false);
                }
                DistributorChild->SetActorHiddenInGame(false);
                DistributorChild->SetActorEnableCollision(false);
                
                // Disable collision on ALL primitive components
                TArray<UPrimitiveComponent*> Primitives;
                DistributorChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                // Disable tick to prevent validation from running
                DistributorChild->SetActorTickEnabled(false);
                DistributorChild->RegisterAllComponents();
                DistributorChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                DistributorChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = DistributorChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned distributor %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("belt_segment"))
        {
            // Spawn belt child hologram
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Processing belt_segment %s, bHasSplineData=%d, SplinePoints=%d"), 
                *ChildData.HologramId, ChildData.bHasSplineData, ChildData.SplineData.Points.Num());
            
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing build class for belt %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonBelt_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorBeltHologram* BeltChild = World->SpawnActor<ASFConveyorBeltHologram>(
                ASFConveyorBeltHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (BeltChild)
            {
                BeltChild->SetReplicates(false);
                BeltChild->SetReplicateMovement(false);
                BeltChild->SetBuildClass(BuildClass);
                
                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                BeltChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                USFHologramDataService::DisableValidation(BeltChild);
                USFHologramDataService::MarkAsChild(BeltChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                BeltChild->FinishSpawning(FTransform(Rotation, Location));
                BeltChild->SetActorLocation(Location);
                BeltChild->SetActorRotation(Rotation);
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Belt %s spawned, checking spline data..."), *ChildName.ToString());
                
                // Set spline data AFTER FinishSpawning (mSplineComponent now exists)
                if (ChildData.bHasSplineData)
                {
                    TArray<FSplinePointData> SplinePoints = ConvertSplineData(ChildData.SplineData);
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Converted %d spline points for belt %s"), 
                        SplinePoints.Num(), *ChildName.ToString());
                    
                    BeltChild->SetSplineDataAndUpdate(SplinePoints);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: SetSplineDataAndUpdate completed for belt %s"), 
                        *ChildName.ToString());
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: bHasSplineData=FALSE for belt %s - NO SPLINE DATA!"),
                        *ChildName.ToString());
                }
                
                // Force unlock to ensure visibility
                if (BeltChild->IsHologramLocked())
                {
                    BeltChild->LockHologramPosition(false);
                }
                BeltChild->SetActorHiddenInGame(false);
                BeltChild->SetActorEnableCollision(false);
                BeltChild->SetActorTickEnabled(false);
                BeltChild->RegisterAllComponents();
                BeltChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                // Add as child for vanilla construction (belt Construct() calls Super::Construct when tagged)
                ParentHologram->AddChild(BeltChild, ChildName);
                
                // CRITICAL: Trigger mesh generation AFTER AddChild and spline data set
                // This creates the visible spline mesh components
                if (ChildData.bHasSplineData)
                {
                    // DEFENSIVE: Re-apply spline data AFTER AddChild in case vanilla code reset it
                    // This mirrors the pipe hologram's backup restoration mechanism
                    TArray<FSplinePointData> SplinePointsAgain = ConvertSplineData(ChildData.SplineData);
                    BeltChild->SetSplineDataAndUpdate(SplinePointsAgain);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Re-applied spline data after AddChild, triggering mesh generation for belt %s"), *ChildName.ToString());
                    BeltChild->TriggerMeshGeneration();
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Mesh generation complete for belt %s"), *ChildName.ToString());
                }
                
                // Force hologram material (required for spline mesh visibility)
                BeltChild->ForceApplyHologramMaterial();
                
                SpawnedHologram = BeltChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned belt %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("lift_segment"))
        {
            // Spawn lift child hologram
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing build class for lift %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonLift_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFConveyorLiftHologram* LiftChild = World->SpawnActor<ASFConveyorLiftHologram>(
                ASFConveyorLiftHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (LiftChild)
            {
                LiftChild->SetReplicates(false);
                LiftChild->SetReplicateMovement(false);
                LiftChild->SetBuildClass(BuildClass);
                
                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                LiftChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                USFHologramDataService::DisableValidation(LiftChild);
                USFHologramDataService::MarkAsChild(LiftChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                LiftChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Apply lift top transform and force mesh rebuild (critical for visibility)
                if (ChildData.bHasLiftData)
                {
                    FTransform TopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                    LiftChild->SetTopTransform(TopTransform);
                    
                    // Force mesh rebuild by simulating a location update
                    FHitResult DummyHit;
                    DummyHit.Location = Location;
                    DummyHit.ImpactPoint = Location;
                    DummyHit.ImpactNormal = FVector::UpVector;
                    LiftChild->SetHologramLocationAndRotation(DummyHit);
                    
                    // SetHologramLocationAndRotation may have reset mTopTransform - restore it
                    LiftChild->SetTopTransform(TopTransform);
                    
                    // CRITICAL: SetHologramLocationAndRotation adjusts Z position - restore exact location
                    LiftChild->SetActorLocation(Location);
                    LiftChild->SetActorRotation(Rotation);
                    
                    // Call UpdateConnectionDirections to update the visual mesh orientations
                    if (UFunction* UpdateFunc = AFGConveyorLiftHologram::StaticClass()->FindFunctionByName(TEXT("UpdateConnectionDirections")))
                    {
                        LiftChild->ProcessEvent(UpdateFunc, nullptr);
                    }
                }
                
                // Store lift data in registry BEFORE AddChild (matches working EXTEND code order)
                if (ChildData.bHasLiftData)
                {
                    FSFHologramData* HoloData = USFHologramDataRegistry::GetData(LiftChild);
                    if (!HoloData)
                    {
                        HoloData = USFHologramDataRegistry::AttachData(LiftChild);
                    }
                    if (HoloData)
                    {
                        HoloData->bHasLiftData = true;
                        HoloData->LiftHeight = ChildData.LiftData.Height;
                        HoloData->bLiftIsReversed = ChildData.LiftData.bIsReversed;
                        HoloData->LiftTopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                        HoloData->LiftBottomTransform = ChildData.LiftData.BottomTransform.ToFTransform();
                        
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Stored lift data for %s (height=%.1f, reversed=%d)"), 
                            *ChildData.HologramId, 
                            ChildData.LiftData.Height, 
                            ChildData.LiftData.bIsReversed ? 1 : 0);
                    }
                    
                    // Issue #260: Passthrough linking is now done in SECOND PASS after all holograms are spawned
                    // (see end of SpawnChildHolograms function)
                }
                
                // Configure visibility and state (matches working EXTEND code order)
                if (LiftChild->IsHologramLocked())
                {
                    LiftChild->LockHologramPosition(false);
                }
                LiftChild->SetActorHiddenInGame(false);
                LiftChild->SetActorTickEnabled(false);
                LiftChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                LiftChild->SetActorEnableCollision(false);
                
                LiftChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                // Add as child (matches working EXTEND code order - after visibility, before ForceApplyHologramMaterial)
                ParentHologram->AddChild(LiftChild, ChildName);
                
                // Force hologram material AFTER AddChild (required for lift visibility)
                LiftChild->ForceApplyHologramMaterial();
                
                SpawnedHologram = LiftChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned lift %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("pipe_segment"))
        {
            // Spawn pipe child hologram
            if (!BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing build class for pipe %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPipe_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
                ASFPipelineHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PipeChild)
            {
                PipeChild->SetReplicates(false);
                PipeChild->SetReplicateMovement(false);
                PipeChild->SetBuildClass(BuildClass);
                
                // CRITICAL: Add tag BEFORE FinishSpawning so CheckValidPlacement can detect it
                PipeChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                USFHologramDataService::DisableValidation(PipeChild);
                USFHologramDataService::MarkAsChild(PipeChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                PipeChild->FinishSpawning(FTransform(Rotation, Location));
                PipeChild->SetActorLocation(Location);
                PipeChild->SetActorRotation(Rotation);
                
                // Set spline data AFTER FinishSpawning (mSplineComponent now exists)
                if (ChildData.bHasSplineData)
                {
                    TArray<FSplinePointData> SplinePoints = ConvertSplineData(ChildData.SplineData);
                    PipeChild->SetSplineDataAndUpdate(SplinePoints);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Set %d spline points for pipe %s"), 
                        SplinePoints.Num(), *ChildData.HologramId);
                }
                
                // Force unlock to ensure visibility
                if (PipeChild->IsHologramLocked())
                {
                    PipeChild->LockHologramPosition(false);
                }
                PipeChild->SetActorHiddenInGame(false);
                PipeChild->SetActorEnableCollision(false);
                PipeChild->SetActorTickEnabled(false);
                PipeChild->RegisterAllComponents();
                PipeChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                // Add as child for vanilla construction (pipe Construct() calls Super::Construct when tagged)
                ParentHologram->AddChild(PipeChild, ChildName);
                
                // CRITICAL: Trigger mesh generation AFTER AddChild and spline data set
                if (ChildData.bHasSplineData)
                {
                    PipeChild->TriggerMeshGeneration();
                }
                
                // Force hologram material (required for spline mesh visibility)
                PipeChild->ForceApplyHologramMaterial();
                
                SpawnedHologram = PipeChild;
                SpawnedCount++;
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Spawned pipe %s at %s"), 
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("passthrough"))
        {
            // Issue #260: Spawn pipe floor hole hologram for extend cloning
            // Pipe passthroughs need explicit spawning (unlike lift floor holes which are
            // auto-created by conveyor lift construction)
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for passthrough %s"), *ChildData.HologramId);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPassthrough_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPassthroughChildHologram* PassChild = World->SpawnActor<ASFPassthroughChildHologram>(
                ASFPassthroughChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PassChild)
            {
                PassChild->SetBuildClass(BuildClass);
                PassChild->SetRecipe(Recipe);
                
                PassChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(PassChild, ChildName);
                
                USFHologramDataService::DisableValidation(PassChild);
                USFHologramDataService::MarkAsChild(PassChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                if (PassChild->IsHologramLocked())
                {
                    PassChild->LockHologramPosition(false);
                }
                PassChild->SetActorHiddenInGame(false);
                PassChild->SetActorEnableCollision(false);
                
                TArray<UPrimitiveComponent*> Primitives;
                PassChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                PassChild->SetActorTickEnabled(false);
                PassChild->RegisterAllComponents();
                PassChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                PassChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                // Issue #260: Set foundation thickness captured from source passthrough
                if (ChildData.Thickness > 0.0f)
                {
                    PassChild->SetSnappedThickness(ChildData.Thickness);
                }
                else
                {
                    PassChild->SetSnappedThickness(400.0f); // Fallback default 4m
                }
                
                SpawnedHologram = PassChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 JSON SPAWN: Spawned passthrough %s at %s"),
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("pipe_attachment"))
        {
            // Issue #288: Clone inline pipe attachments (valves, pumps). Uses
            // ASFPipeAttachmentChildHologram → AFGPipelineAttachmentHologram. The
            // source UserFlowLimit was captured during topology walk and is applied
            // to the built AFGBuildablePipelinePump post-Construct (via reflection
            // on the hologram registry data).
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 JSON SPAWN: Missing recipe/build class for pipe attachment %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPipeAttachment_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPipeAttachmentChildHologram* AttChild = World->SpawnActor<ASFPipeAttachmentChildHologram>(
                ASFPipeAttachmentChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (AttChild)
            {
                AttChild->SetBuildClass(BuildClass);
                AttChild->SetRecipe(Recipe);
                
                AttChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(AttChild, ChildName);
                
                USFHologramDataService::DisableValidation(AttChild);
                USFHologramDataService::MarkAsChild(AttChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                // Stash the clone metadata (UserFlowLimit + JsonCloneId) on the
                // registry so Construct can apply the flow limit after the
                // AFGBuildablePipelinePump actor is spawned.
                if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(AttChild))
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                    HoloData->PipeAttachmentUserFlowLimit = ChildData.UserFlowLimit;
                    HoloData->bIsPipeAttachmentClone = true;
                }
                
                if (AttChild->IsHologramLocked())
                {
                    AttChild->LockHologramPosition(false);
                }
                AttChild->SetActorHiddenInGame(false);
                AttChild->SetActorEnableCollision(false);
                
                TArray<UPrimitiveComponent*> Primitives;
                AttChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                AttChild->SetActorTickEnabled(false);
                AttChild->RegisterAllComponents();
                AttChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                AttChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = AttChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🔧 JSON SPAWN: Spawned pipe attachment %s (class=%s) UserFlowLimit=%.3f at %s"),
                    *ChildData.HologramId, *ChildData.BuildClass, ChildData.UserFlowLimit, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("power_pole"))
        {
            // Issue #229: Spawn power pole child hologram for extend
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("⚡ JSON SPAWN: Missing recipe/build class for power pole %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonPowerPole_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFPowerPoleChildHologram* PoleChild = World->SpawnActor<ASFPowerPoleChildHologram>(
                ASFPowerPoleChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (PoleChild)
            {
                PoleChild->SetBuildClass(BuildClass);
                PoleChild->SetRecipe(Recipe);
                
                PoleChild->FinishSpawning(FTransform(Rotation, Location));
                
                // Add as child IMMEDIATELY after FinishSpawning
                ParentHologram->AddChild(PoleChild, ChildName);
                
                // Disable validation AFTER AddChild
                USFHologramDataService::DisableValidation(PoleChild);
                USFHologramDataService::MarkAsChild(PoleChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                // Store JsonCloneId for post-build registration
                FSFHologramData* HoloData = USFHologramDataRegistry::GetData(PoleChild);
                if (!HoloData)
                {
                    HoloData = USFHologramDataRegistry::AttachData(PoleChild);
                }
                if (HoloData)
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                }
                
                // Configure visibility
                if (PoleChild->IsHologramLocked())
                {
                    PoleChild->LockHologramPosition(false);
                }
                PoleChild->SetActorHiddenInGame(false);
                PoleChild->SetActorEnableCollision(false);
                
                // Disable collision on ALL primitive components
                TArray<UPrimitiveComponent*> Primitives;
                PoleChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                
                PoleChild->SetActorTickEnabled(false);
                PoleChild->RegisterAllComponents();
                PoleChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                PoleChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                SpawnedHologram = PoleChild;
                SpawnedCount++;
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("⚡ JSON SPAWN: Spawned power pole %s at %s"),
                    *ChildData.HologramId, *Location.ToString());
            }
        }
        else if (ChildData.Role == TEXT("wire_cost"))
        {
            // Issue #229: Wire hologram for cable cost — uses ASFWireHologram (same as PowerAutoConnect)
            // Construct() returns a real wire actor → no nullptr crash in InternalConstructHologram
            FName ChildName(*FString::Printf(TEXT("JsonWire_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            ASFWireHologram* WireChild = World->SpawnActor<ASFWireHologram>(
                ASFWireHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );
            
            if (WireChild)
            {
                // Set power line build class and recipe (matches PowerLinePreviewHelper pattern)
                UClass* PowerLineClass = LoadObject<UClass>(nullptr, 
                    TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
                if (PowerLineClass)
                {
                    WireChild->SetBuildClass(PowerLineClass);
                }
                
                TSubclassOf<UFGRecipe> WireRecipe = LoadClass<UFGRecipe>(nullptr, 
                    TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
                if (WireRecipe)
                {
                    WireChild->SetRecipe(WireRecipe);
                }
                
                WireChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                
                WireChild->FinishSpawning(FTransform(Rotation, Location));
                
                ParentHologram->AddChild(WireChild, ChildName);
                
                USFHologramDataService::DisableValidation(WireChild);
                USFHologramDataService::MarkAsChild(WireChild, ParentHologram, ESFChildHologramType::ExtendClone);
                
                // Store JsonCloneId so the wire registers on build
                FSFHologramData* HoloData = USFHologramDataRegistry::GetData(WireChild);
                if (!HoloData)
                {
                    HoloData = USFHologramDataRegistry::AttachData(WireChild);
                }
                if (HoloData)
                {
                    HoloData->JsonCloneId = ChildData.HologramId;
                }
                
                // Set wire endpoints for cost calculation (GetWireLength() → GetCost())
                float WireDistance = ChildData.bHasSplineData ? ChildData.SplineData.Length : 0.0f;
                WireChild->SetWireEndpoints(FVector::ZeroVector, FVector(WireDistance, 0, 0));
                
                WireChild->SetActorHiddenInGame(true);  // Hidden — post-build wiring creates the real connection
                WireChild->SetActorEnableCollision(false);
                WireChild->SetActorTickEnabled(false);
                WireChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                
                SpawnedHologram = WireChild;
                SpawnedCount++;
                
                int32 CableCount = FMath::Max(1, FMath::CeilToInt((WireDistance / 100.0f) / 25.0f));
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("⚡ JSON SPAWN: Spawned wire %s (distance=%.0fcm, cables=%d)"),
                    *ChildData.HologramId, WireDistance, CableCount);
            }
        }
        else if (ChildData.Role == TEXT("lane_segment"))
        {
            // Spawn lane segment - generated belt/lift/pipe connecting adjacent clones' distributors
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🛤️ LANE SPAWN: %s type=%s at (%.0f,%.0f,%.0f) rot=(%.0f)"),
                *ChildData.HologramId, *ChildData.LaneSegmentType,
                Location.X, Location.Y, Location.Z, Rotation.Yaw);
            if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
            {
                FVector StartW = ChildData.SplineData.Points[0].World.ToFVector();
                FVector EndW = ChildData.SplineData.Points.Last().World.ToFVector();
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🛤️ LANE SPAWN: %s spline Start(%.0f,%.0f,%.0f)→End(%.0f,%.0f,%.0f) len=%.0f"),
                    *ChildData.HologramId,
                    StartW.X, StartW.Y, StartW.Z, EndW.X, EndW.Y, EndW.Z,
                    ChildData.SplineData.Length);
            }
            
            FName ChildName(*FString::Printf(TEXT("JsonLane_%d"), JsonSpawnCounter++));
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;
            
            // Get belt/pipe tier from auto-connect settings
            USFSubsystem* Subsystem = USFSubsystem::Get(World);
            AFGPlayerController* PC = World ? World->GetFirstPlayerController<AFGPlayerController>() : nullptr;
            
            if (ChildData.LaneSegmentType == TEXT("pipe"))
            {
                // Spawn pipe lane using auto-connect global config setting for manifold lanes
                TSubclassOf<AFGBuildable> PipeBuildClass = nullptr;
                if (Subsystem && PC)
                {
                    // Use GetPipeClassFromConfig which handles Auto mode (0) and validates unlocks
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    PipeBuildClass = Subsystem->GetPipeClassFromConfig(Settings.PipeTierMain, Settings.bPipeIndicator, PC);
                }
                if (!PipeBuildClass)
                {
                    // Ultimate fallback: Mk1 is always available
                    PipeBuildClass = FindBuildClassByName(TEXT("Build_Pipeline_C"));
                }
                
                ASFPipelineHologram* PipeLane = World->SpawnActor<ASFPipelineHologram>(
                    ASFPipelineHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (PipeLane)
                {
                    PipeLane->SetReplicates(false);
                    PipeLane->SetReplicateMovement(false);
                    PipeLane->SetBuildClass(PipeBuildClass);
                    PipeLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    PipeLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));
                    
                    USFHologramDataService::DisableValidation(PipeLane);
                    USFHologramDataService::MarkAsChild(PipeLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    
                    PipeLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    // Use TryUseBuildModeRouting for pipe lanes (matches belt lane pattern)
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        PipeLane->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal);
                    }
                    
                    PipeLane->SetActorHiddenInGame(false);
                    PipeLane->SetActorEnableCollision(false);
                    PipeLane->SetActorTickEnabled(false);
                    PipeLane->RegisterAllComponents();
                    PipeLane->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                    
                    ParentHologram->AddChild(PipeLane, ChildName);
                    
                    if (ChildData.bHasSplineData)
                    {
                        PipeLane->TriggerMeshGeneration();
                    }
                    PipeLane->ForceApplyHologramMaterial();
                    
                    SpawnedHologram = PipeLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned pipe lane %s"), *ChildData.HologramId);
                }
            }
            else if (ChildData.LaneSegmentType == TEXT("lift"))
            {
                // Spawn lift lane
                TSubclassOf<AFGBuildable> LiftBuildClass = nullptr;
                if (Subsystem)
                {
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    int32 BeltTier = Settings.BeltTierMain;
                    if (BeltTier == 0)
                    {
                        // Auto mode: use highest unlocked belt tier
                        BeltTier = Subsystem->GetHighestUnlockedBeltTier(PC);
                    }
                    // Use belt tier for lift tier (lifts follow belt tiers)
                    LiftBuildClass = Subsystem->GetBeltClassForTier(BeltTier, PC);
                    // Convert belt class to lift class
                    if (LiftBuildClass)
                    {
                        FString LiftClassName = LiftBuildClass->GetName().Replace(TEXT("ConveyorBelt"), TEXT("ConveyorLift"));
                        LiftBuildClass = FindBuildClassByName(LiftClassName);
                    }
                }
                if (!LiftBuildClass)
                {
                    LiftBuildClass = FindBuildClassByName(TEXT("Build_ConveyorLiftMk1_C"));
                }
                
                ASFConveyorLiftHologram* LiftLane = World->SpawnActor<ASFConveyorLiftHologram>(
                    ASFConveyorLiftHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (LiftLane)
                {
                    LiftLane->SetReplicates(false);
                    LiftLane->SetReplicateMovement(false);
                    LiftLane->SetBuildClass(LiftBuildClass);
                    LiftLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    LiftLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));
                    
                    USFHologramDataService::DisableValidation(LiftLane);
                    USFHologramDataService::MarkAsChild(LiftLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    
                    LiftLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    if (ChildData.bHasLiftData)
                    {
                        FTransform TopTransform = ChildData.LiftData.TopTransform.ToFTransform();
                        LiftLane->SetTopTransform(TopTransform);
                    }
                    
                    LiftLane->SetActorHiddenInGame(false);
                    LiftLane->SetActorEnableCollision(false);
                    LiftLane->SetActorTickEnabled(false);
                    LiftLane->RegisterAllComponents();
                    LiftLane->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                    
                    ParentHologram->AddChild(LiftLane, ChildName);
                    LiftLane->ForceApplyHologramMaterial();
                    
                    SpawnedHologram = LiftLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned lift lane %s (height=%.1f)"), 
                        *ChildData.HologramId, ChildData.LiftData.Height);
                }
            }
            else
            {
                // Spawn belt lane using auto-connect global config setting for manifold lanes
                TSubclassOf<AFGBuildable> BeltBuildClass = nullptr;
                if (Subsystem && PC)
                {
                    // Use GetBeltClassFromConfig which handles Auto mode (0) and validates unlocks
                    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
                    BeltBuildClass = Subsystem->GetBeltClassFromConfig(Settings.BeltTierMain, PC);
                }
                if (!BeltBuildClass)
                {
                    // Ultimate fallback: Mk1 is always available
                    BeltBuildClass = FindBuildClassByName(TEXT("Build_ConveyorBeltMk1_C"));
                }
                
                ASFConveyorBeltHologram* BeltLane = World->SpawnActor<ASFConveyorBeltHologram>(
                    ASFConveyorBeltHologram::StaticClass(),
                    Location, Rotation, SpawnParams);
                
                if (BeltLane)
                {
                    BeltLane->SetReplicates(false);
                    BeltLane->SetReplicateMovement(false);
                    BeltLane->SetBuildClass(BeltBuildClass);
                    BeltLane->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
                    BeltLane->Tags.AddUnique(FName(TEXT("SF_LaneSegment")));
                    
                    USFHologramDataService::DisableValidation(BeltLane);
                    USFHologramDataService::MarkAsChild(BeltLane, ParentHologram, ESFChildHologramType::ExtendClone);
                    
                    BeltLane->FinishSpawning(FTransform(Rotation, Location));
                    
                    // Use AutoRouteSplineWithNormals for proper curved spline routing
                    // (same approach as stackable conveyor pole auto-connect)
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        BeltLane->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
                    }
                    
                    BeltLane->SetActorHiddenInGame(false);
                    BeltLane->SetActorEnableCollision(false);
                    BeltLane->SetActorTickEnabled(false);
                    BeltLane->RegisterAllComponents();
                    BeltLane->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
                    
                    ParentHologram->AddChild(BeltLane, ChildName);
                    
                    // Re-apply spline routing after AddChild (which repositions the actor)
                    if (ChildData.bHasSplineData && ChildData.SplineData.Points.Num() >= 2)
                    {
                        FVector StartPos = ChildData.SplineData.Points[0].World.ToFVector();
                        FVector EndPos = ChildData.SplineData.Points.Last().World.ToFVector();
                        FVector StartNormal = ChildData.LaneStartNormal.ToFVector();
                        FVector EndNormal = ChildData.LaneEndNormal.ToFVector();
                        
                        BeltLane->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
                        BeltLane->TriggerMeshGeneration();
                    }
                    BeltLane->ForceApplyHologramMaterial();
                    
                    SpawnedHologram = BeltLane;
                    SpawnedCount++;
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ JSON SPAWN: Spawned belt lane %s"), *ChildData.HologramId);
                }
            }
        }
        else if (ChildData.Role == TEXT("wall_hole"))
        {
            // Spawn wall hole child hologram (Build_ConveyorWallHole_C, Build_PipelineSupportWallHole_C,
            // any "*WallHole_C" variants). Pattern mirrors the passthrough handler above.
            if (!Recipe || !BuildClass)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🧱 JSON SPAWN: Missing recipe/build class for wall hole %s (recipe=%s, build=%s)"),
                    *ChildData.HologramId, *ChildData.RecipeClass, *ChildData.BuildClass);
                continue;
            }

            FName ChildName(*FString::Printf(TEXT("JsonWallHole_%d"), JsonSpawnCounter++));

            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = ChildName;
            SpawnParams.Owner = ParentHologram->GetOwner();
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.bDeferConstruction = true;

            ASFWallHoleChildHologram* WallChild = World->SpawnActor<ASFWallHoleChildHologram>(
                ASFWallHoleChildHologram::StaticClass(),
                Location,
                Rotation,
                SpawnParams
            );

            if (WallChild)
            {
                WallChild->SetBuildClass(BuildClass);
                WallChild->SetRecipe(Recipe);

                WallChild->FinishSpawning(FTransform(Rotation, Location));

                ParentHologram->AddChild(WallChild, ChildName);

                USFHologramDataService::DisableValidation(WallChild);
                USFHologramDataService::MarkAsChild(WallChild, ParentHologram, ESFChildHologramType::ExtendClone);

                if (WallChild->IsHologramLocked())
                {
                    WallChild->LockHologramPosition(false);
                }
                WallChild->SetActorHiddenInGame(false);
                WallChild->SetActorEnableCollision(false);

                TArray<UPrimitiveComponent*> Primitives;
                WallChild->GetComponents<UPrimitiveComponent>(Primitives);
                for (UPrimitiveComponent* PrimComp : Primitives)
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }

                WallChild->SetActorTickEnabled(false);
                WallChild->RegisterAllComponents();
                WallChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

                WallChild->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));

                SpawnedHologram = WallChild;
                SpawnedCount++;

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🧱 JSON SPAWN: Spawned wall hole %s (class=%s) at %s"),
                    *ChildData.HologramId, *ChildData.BuildClass, *Location.ToString());
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🧱 JSON SPAWN: Failed to spawn ASFWallHoleChildHologram for %s"),
                    *ChildData.HologramId);
            }
        }

        // Store in output map for connection wiring
        if (SpawnedHologram && !ChildData.HologramId.IsEmpty())
        {
            OutSpawnedHolograms.Add(ChildData.HologramId, SpawnedHologram);
            
            // Apply customization (color/swatch) to clone hologram
            // Without this, children inherit the parent hologram's color (e.g., refinery's caterium swatch)
            if (ChildData.bIsLaneSegment)
            {
                // Lane segments: inherit color from existing belt/pipe on the other side of the
                // source distributor. If no infrastructure exists there, use defaults instead of
                // the factory building's color. Look up by the source distributor's actor name.
                if (AFGBuildableHologram* BuildableHolo = Cast<AFGBuildableHologram>(SpawnedHologram))
                {
                    // Extract source distributor name from LaneFromDistributorId or LaneToDistributorId
                    // (whichever refers to the source, not the clone)
                    FString SourceDistName;
                    if (!ChildData.LaneFromDistributorId.IsEmpty() && !ChildData.LaneFromDistributorId.Contains(TEXT("clone_")))
                        SourceDistName = ChildData.LaneFromDistributorId;
                    else if (!ChildData.LaneToDistributorId.IsEmpty() && !ChildData.LaneToDistributorId.Contains(TEXT("clone_")))
                        SourceDistName = ChildData.LaneToDistributorId;
                    
                    if (const FFactoryCustomizationData* LaneCustom = LaneColorMap.Find(SourceDistName))
                    {
                        BuildableHolo->SetCustomizationData(*LaneCustom);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🎨 LANE COLOR: %s inherits color from belt/pipe near distributor %s"),
                            *ChildData.HologramId, *SourceDistName);
                    }
                    else
                    {
                        // No existing infrastructure to sample — use default customization
                        // (explicitly reset so lane doesn't inherit factory building's color)
                        BuildableHolo->SetCustomizationData(FFactoryCustomizationData());
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("🎨 LANE COLOR: %s using defaults (no infrastructure near distributor %s)"),
                            *ChildData.HologramId, *SourceDistName);
                    }
                }
            }
            else if (!ChildData.SourceId.IsEmpty())
            {
                // Non-lane clones: inherit color from their specific source actor
                if (const FFactoryCustomizationData* SourceCustom = SourceCustomizationMap.Find(ChildData.SourceId))
                {
                    if (AFGBuildableHologram* BuildableHolo = Cast<AFGBuildableHologram>(SpawnedHologram))
                    {
                        BuildableHolo->SetCustomizationData(*SourceCustom);
                    }
                }
            }
            
            // Store JsonCloneId and connection targets in hologram data
            FSFHologramData* HoloData = USFHologramDataRegistry::GetData(SpawnedHologram);
            if (!HoloData)
            {
                HoloData = USFHologramDataRegistry::AttachData(SpawnedHologram);
            }
            if (HoloData)
            {
                HoloData->JsonCloneId = ChildData.HologramId;
                
                // ================================================================
                // PRE-TICK CONNECTION TARGETS
                // Store connection targets from CloneConnections so ConfigureComponents
                // can establish connections during construction (like AutoLink)
                // ================================================================
                
                // Conn0 target (ConveyorAny0)
                if (!ChildData.CloneConnections.ConveyorAny0.Target.IsEmpty() && 
                    ChildData.CloneConnections.ConveyorAny0.Target != TEXT("external"))
                {
                    HoloData->Conn0TargetCloneId = ChildData.CloneConnections.ConveyorAny0.Target;
                    HoloData->Conn0TargetConnectorName = FName(*ChildData.CloneConnections.ConveyorAny0.Connector);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: %s Conn0 target set: %s.%s"),
                        *ChildData.HologramId,
                        *HoloData->Conn0TargetCloneId,
                        *HoloData->Conn0TargetConnectorName.ToString());
                }
                
                // Conn1 target (ConveyorAny1)
                if (!ChildData.CloneConnections.ConveyorAny1.Target.IsEmpty() && 
                    ChildData.CloneConnections.ConveyorAny1.Target != TEXT("external"))
                {
                    HoloData->Conn1TargetCloneId = ChildData.CloneConnections.ConveyorAny1.Target;
                    HoloData->Conn1TargetConnectorName = FName(*ChildData.CloneConnections.ConveyorAny1.Connector);
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: %s Conn1 target set: %s.%s"),
                        *ChildData.HologramId,
                        *HoloData->Conn1TargetCloneId,
                        *HoloData->Conn1TargetConnectorName.ToString());
                }
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 JSON SPAWN: Completed - spawned %d/%d holograms"), 
        SpawnedCount, ChildHolograms.Num());
    
    // ==================== SECOND PASS: Link Lifts to Passthroughs (Issue #260) ====================
    // Now that ALL holograms are spawned (including passthroughs), we can link lifts to passthroughs.
    // This must be done BEFORE AddChild/Construct is called on the lifts.
    int32 LiftsLinked = 0;
    for (const FSFCloneHologram& ChildData : ChildHolograms)
    {
        if (ChildData.Role == TEXT("lift_segment") && ChildData.bHasLiftData && 
            ChildData.LiftData.PassthroughCloneIds.Num() > 0)
        {
            // Find the spawned lift hologram
            AFGHologram** LiftHoloPtr = OutSpawnedHolograms.Find(ChildData.HologramId);
            if (!LiftHoloPtr || !*LiftHoloPtr) continue;
            
            AFGConveyorLiftHologram* LiftHolo = Cast<AFGConveyorLiftHologram>(*LiftHoloPtr);
            if (!LiftHolo) continue;
            
            // World search for passthroughs near the lift position
            FVector LiftBottom = LiftHolo->GetActorLocation();
            FVector LiftTop = LiftBottom + FVector(0, 0, ChildData.LiftData.Height);
            const float SnapDistance = 100.0f;  // 1m tolerance
            
            TArray<AFGBuildablePassthrough*> PassthroughActors;
            PassthroughActors.SetNum(2);  // [0]=bottom, [1]=top
            
            for (TActorIterator<AFGBuildablePassthrough> It(World); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (!IsValid(PT)) continue;
                
                FVector PTLoc = PT->GetActorLocation();
                float DistBottom = FVector::Dist(PTLoc, LiftBottom);
                float DistTop = FVector::Dist(PTLoc, LiftTop);
                
                if (DistBottom < SnapDistance && !PassthroughActors[0])
                {
                    PassthroughActors[0] = PT;
                }
                if (DistTop < SnapDistance && !PassthroughActors[1])
                {
                    PassthroughActors[1] = PT;
                }
                
                if (PassthroughActors[0] && PassthroughActors[1])
                    break;
            }
            
            // Set mSnappedPassthroughs on the hologram using reflection
            int32 ValidCount = (PassthroughActors[0] ? 1 : 0) + (PassthroughActors[1] ? 1 : 0);
            if (ValidCount > 0)
            {
                FProperty* SnappedProp = AFGConveyorLiftHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
                if (SnappedProp)
                {
                    TArray<AFGBuildablePassthrough*>* HoloPassthroughs = 
                        SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(LiftHolo);
                    
                    if (HoloPassthroughs)
                    {
                        *HoloPassthroughs = PassthroughActors;
                        LiftsLinked++;
                        
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔗 JSON SPAWN PASS 2: Linked %d passthroughs to lift %s (bottom=%s, top=%s)"),
                            ValidCount, *ChildData.HologramId,
                            PassthroughActors[0] ? *PassthroughActors[0]->GetName() : TEXT("none"),
                            PassthroughActors[1] ? *PassthroughActors[1]->GetName() : TEXT("none"));
                    }
                }
            }
        }
    }
    
    if (LiftsLinked > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔗 JSON SPAWN PASS 2: Linked %d lifts to passthroughs"), LiftsLinked);
    }
    
    return SpawnedCount;
}

// ============================================================================
// FSFCloneTopology - Wire child hologram connections
// ============================================================================

int32 FSFCloneTopology::WireChildHologramConnections(
    const TMap<FString, AFGHologram*>& SpawnedHolograms,
    AFGHologram* ParentHologram) const
{
    using namespace SpawnHelpers;  // For ConvertSplineData
    
    if (!ParentHologram)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE: No parent hologram provided"));
        return 0;
    }
    
    int32 WiredCount = 0;
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: Starting connection wiring for %d child holograms"), ChildHolograms.Num());
    
    // Helper to get connection component from a hologram by connector name
    auto GetConnectionComponent = [](AFGHologram* Hologram, const FString& ConnectorName) -> UFGFactoryConnectionComponent*
    {
        if (!Hologram || ConnectorName.IsEmpty())
        {
            return nullptr;
        }
        
        // Try to find the connection component by name
        TArray<UFGFactoryConnectionComponent*> Connections;
        Hologram->GetComponents<UFGFactoryConnectionComponent>(Connections);
        
        for (UFGFactoryConnectionComponent* Conn : Connections)
        {
            if (Conn && Conn->GetFName().ToString() == ConnectorName)
            {
                return Conn;
            }
        }
        
        // Try common aliases
        if (ConnectorName == TEXT("ConveyorAny0") || ConnectorName == TEXT("Conn0"))
        {
            for (UFGFactoryConnectionComponent* Conn : Connections)
            {
                if (Conn && (Conn->GetFName().ToString().Contains(TEXT("0")) || 
                             Conn->GetFName().ToString().Contains(TEXT("Input"))))
                {
                    return Conn;
                }
            }
        }
        else if (ConnectorName == TEXT("ConveyorAny1") || ConnectorName == TEXT("Conn1"))
        {
            for (UFGFactoryConnectionComponent* Conn : Connections)
            {
                if (Conn && (Conn->GetFName().ToString().Contains(TEXT("1")) || 
                             Conn->GetFName().ToString().Contains(TEXT("Output"))))
                {
                    return Conn;
                }
            }
        }
        
        return nullptr;
    };
    
    // Helper to get pipe connection component from a hologram by connector name
    auto GetPipeConnectionComponent = [](AFGHologram* Hologram, const FString& ConnectorName) -> UFGPipeConnectionComponentBase*
    {
        if (!Hologram || ConnectorName.IsEmpty())
        {
            return nullptr;
        }
        
        TArray<UFGPipeConnectionComponentBase*> Connections;
        Hologram->GetComponents<UFGPipeConnectionComponentBase>(Connections);
        
        for (UFGPipeConnectionComponentBase* Conn : Connections)
        {
            if (Conn && Conn->GetFName().ToString() == ConnectorName)
            {
                return Conn;
            }
        }
        
        // Try index-based matching
        if (ConnectorName.Contains(TEXT("0")) && Connections.Num() > 0)
        {
            return Connections[0];
        }
        else if (ConnectorName.Contains(TEXT("1")) && Connections.Num() > 1)
        {
            return Connections[1];
        }
        
        return nullptr;
    };
    
    for (const FSFCloneHologram& ChildData : ChildHolograms)
    {
        // Only wire belt, lift, pipe, and lane segments (not distributors)
        if (ChildData.Role != TEXT("belt_segment") && 
            ChildData.Role != TEXT("lift_segment") && 
            ChildData.Role != TEXT("pipe_segment") &&
            ChildData.Role != TEXT("lane_segment"))
        {
            continue;
        }
        
        // Find the spawned hologram
        AFGHologram* const* FoundHologram = SpawnedHolograms.Find(ChildData.HologramId);
        if (!FoundHologram || !*FoundHologram)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRE: Hologram %s not found in spawned map"), *ChildData.HologramId);
            continue;
        }
        
        AFGHologram* Hologram = *FoundHologram;
        
        // Get connection targets from CloneConnections
        const FSFConnectionRef& Conn0Ref = ChildData.CloneConnections.ConveyorAny0;
        const FSFConnectionRef& Conn1Ref = ChildData.CloneConnections.ConveyorAny1;
        
        UFGFactoryConnectionComponent* Conn0Target = nullptr;
        UFGFactoryConnectionComponent* Conn1Target = nullptr;
        UFGPipeConnectionComponentBase* PipeConn0Target = nullptr;
        UFGPipeConnectionComponentBase* PipeConn1Target = nullptr;
        
        bool bIsPipe = (ChildData.Role == TEXT("pipe_segment"));
        
        // Resolve Conn0 target
        if (!Conn0Ref.Target.IsEmpty())
        {
            if (Conn0Ref.Target == TEXT("parent"))
            {
                // Connect to parent factory hologram
                if (bIsPipe)
                {
                    PipeConn0Target = GetPipeConnectionComponent(ParentHologram, Conn0Ref.Connector);
                }
                else
                {
                    Conn0Target = GetConnectionComponent(ParentHologram, Conn0Ref.Connector);
                }
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: %s Conn0 -> parent.%s"), 
                    *ChildData.HologramId, *Conn0Ref.Connector);
            }
            else if (Conn0Ref.Target != TEXT("external"))
            {
                // Connect to another cloned hologram
                AFGHologram* const* TargetHologram = SpawnedHolograms.Find(Conn0Ref.Target);
                if (TargetHologram && *TargetHologram)
                {
                    if (bIsPipe)
                    {
                        PipeConn0Target = GetPipeConnectionComponent(*TargetHologram, Conn0Ref.Connector);
                    }
                    else
                    {
                        Conn0Target = GetConnectionComponent(*TargetHologram, Conn0Ref.Connector);
                    }
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: %s Conn0 -> %s.%s"), 
                        *ChildData.HologramId, *Conn0Ref.Target, *Conn0Ref.Connector);
                }
            }
        }
        
        // Resolve Conn1 target
        if (!Conn1Ref.Target.IsEmpty())
        {
            if (Conn1Ref.Target == TEXT("parent"))
            {
                // Connect to parent factory hologram
                if (bIsPipe)
                {
                    PipeConn1Target = GetPipeConnectionComponent(ParentHologram, Conn1Ref.Connector);
                }
                else
                {
                    Conn1Target = GetConnectionComponent(ParentHologram, Conn1Ref.Connector);
                }
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: %s Conn1 -> parent.%s"), 
                    *ChildData.HologramId, *Conn1Ref.Connector);
            }
            else if (Conn1Ref.Target != TEXT("external"))
            {
                // Connect to another cloned hologram
                AFGHologram* const* TargetHologram = SpawnedHolograms.Find(Conn1Ref.Target);
                if (TargetHologram && *TargetHologram)
                {
                    if (bIsPipe)
                    {
                        PipeConn1Target = GetPipeConnectionComponent(*TargetHologram, Conn1Ref.Connector);
                    }
                    else
                    {
                        Conn1Target = GetConnectionComponent(*TargetHologram, Conn1Ref.Connector);
                    }
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: %s Conn1 -> %s.%s"), 
                        *ChildData.HologramId, *Conn1Ref.Target, *Conn1Ref.Connector);
                }
            }
        }
        
        // Apply snapped connections based on hologram type
        // NOTE: We SKIP belt and pipe wiring because setting mSnappedConnectionComponents
        // causes vanilla to continuously recalculate the spline on every tick, overwriting
        // our custom spline data. Belts/pipes with custom splines must be wired post-build.
        // Lifts don't have this issue since they don't use spline data.
        if (ChildData.Role == TEXT("belt_segment"))
        {
            // SKIP belt wiring - vanilla recalculates spline when mSnappedConnectionComponents is set
            // Connections will be established post-build instead
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: Skipping belt %s (spline-based, will wire post-build)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("lift_segment"))
        {
            // SKIP lift wiring - setting mSnappedConnectionComponents causes vanilla to wire lifts,
            // but each lift gets its own chain actor. We need ConfigureComponents to handle wiring
            // so it can do Add/Remove/Add to unify chains (same approach as belts).
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: Skipping lift %s (will wire in ConfigureComponents for chain unification)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("pipe_segment"))
        {
            // SKIP pipe wiring - vanilla recalculates spline when mSnappedConnectionComponents is set
            // Connections will be established post-build instead
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: Skipping pipe %s (spline-based, will wire post-build)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("lane_segment"))
        {
            // Lane segments connect source distributor (existing buildable) to clone distributor (hologram)
            // They use the same wiring strategy as their segment type (belt/lift/pipe)
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ WIRE: Skipping lane %s (type=%s, will wire in ConfigureComponents)"),
                *ChildData.HologramId, *ChildData.LaneSegmentType);
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 WIRE: Completed - wired %d connections"), WiredCount);
    
    return WiredCount;
}
