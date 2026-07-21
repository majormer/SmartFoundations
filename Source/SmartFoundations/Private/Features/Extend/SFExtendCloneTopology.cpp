// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFExtendService.h"
#include "FGFactoryColoringTypes.h"  // [#477] FFactoryCustomizationData capture
#include "Misc/DateTime.h"

// Satisfactory includes
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"  // Issue #288: valves/pumps
#include "Buildables/FGBuildablePassthrough.h"
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
#include "Shared/Conduits/SFDistributorTopology.h"

// Issue #345: approximate height (cm) of a power connector above a pole/factory origin, used to lift
// the previewed cable catenary off the base so it reads like the real pole-top wiring. Tuned in-game.
static constexpr float SFWireConnectorHeightCm = 600.0f;

// ============================================================================
// FSFCloneTopology
// ============================================================================

void FSFCloneTopology::ApplyRigidYawRotation(const FRotator& RotOffset, const FVector& Center, const FVector& WorldOffsetToClone)
{
    for (FSFCloneHologram& Holo : ChildHolograms)
    {
        if (Holo.bIsLaneSegment)
        {
            // Lane segments are ADAPTIVE — only rotate the clone-side endpoint; source-side stays fixed.
            if (Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
            {
                FVector StartWorld = Holo.SplineData.Points[0].World.ToFVector();
                FVector EndWorld = Holo.SplineData.Points.Last().World.ToFVector();
                FVector LaneDir = EndWorld - StartWorld;
                bool bCloneAtEnd = (FVector::DotProduct(LaneDir, WorldOffsetToClone) > 0.0f);

                if (bCloneAtEnd)
                {
                    FVector RelEnd = EndWorld - Center;
                    FVector RotatedEnd = Center + RotOffset.RotateVector(RelEnd);
                    Holo.SplineData.Points.Last().World = FSFVec3(RotatedEnd);
                    Holo.LaneEndNormal = FSFVec3(RotOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                }
                else
                {
                    FVector RelStart = StartWorld - Center;
                    FVector RotatedStart = Center + RotOffset.RotateVector(RelStart);
                    Holo.SplineData.Points[0].World = FSFVec3(RotatedStart);
                    Holo.LaneStartNormal = FSFVec3(RotOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                }

                FVector NewStart = Holo.SplineData.Points[0].World.ToFVector();
                FVector NewEnd = Holo.SplineData.Points.Last().World.ToFVector();
                Holo.SplineData.Length = FVector::Dist(NewStart, NewEnd);
                Holo.Transform = FSFTransform(NewStart, (NewEnd - NewStart).Rotation());
            }
            continue;
        }

        // Internal segments rotate rigidly around the factory center.
        FVector HoloPos = Holo.Transform.Location.ToFVector();
        FVector RelPos = HoloPos - Center;
        FVector RotatedPos = Center + RotOffset.RotateVector(RelPos);
        FRotator RotatedRot = Holo.Transform.Rotation.ToFRotator() + RotOffset;
        Holo.Transform = FSFTransform(RotatedPos, RotatedRot);

        if (Holo.bHasSplineData)
        {
            for (FSFSplinePoint& Point : Holo.SplineData.Points)
            {
                FVector WorldPt = Point.World.ToFVector();
                FVector RelPt = WorldPt - Center;
                Point.World = FSFVec3(Center + RotOffset.RotateVector(RelPt));
            }
        }

        if (Holo.bHasLiftData)
        {
            FVector BotPos = Holo.LiftData.BottomTransform.Location.ToFVector();
            FVector BotRel = BotPos - Center;
            Holo.LiftData.BottomTransform.Location = FSFVec3(Center + RotOffset.RotateVector(BotRel));
            Holo.LiftData.BottomTransform.Rotation = FSFRot3(Holo.LiftData.BottomTransform.Rotation.ToFRotator() + RotOffset);
        }
    }
}

FSFCloneTopology FSFCloneTopology::FromSource(const FSFSourceTopology& Source, const FVector& Offset, const FVector& PrincipalAxisWorld)
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
    
    auto ResolveDistributorTopology = [](const FSFSourceChain& Chain)
    {
        return FSFDistributorTopologyResolver::Resolve(
            Chain.Distributor.Class, FName(*Chain.Distributor.ConnectorUsed));
    };

    // A recognized orientation with no complete perpendicular lane is not a partial clone: the
    // distributor and every segment in that branch must be absent from preview, cost, and wiring.
    TSet<FString> InvalidDistributorIds;
    auto FindInvalidDistributors = [&InvalidDistributorIds, &ResolveDistributorTopology](const TArray<FSFSourceChain>& Chains)
    {
        for (const FSFSourceChain& Chain : Chains)
        {
            const FSFDistributorPortTopology Topology = ResolveDistributorTopology(Chain);
            if (Topology.bRecognized && !Topology.bValidManifold && !Chain.Distributor.Id.IsEmpty())
            {
                InvalidDistributorIds.Add(Chain.Distributor.Id);
            }
        }
    };
    FindInvalidDistributors(Source.BeltInputChains);
    FindInvalidDistributors(Source.BeltOutputChains);
    FindInvalidDistributors(Source.PipeInputChains);
    FindInvalidDistributors(Source.PipeOutputChains);

    auto IsDistributorBranchInvalid = [&InvalidDistributorIds](const FSFSourceChain& Chain)
    {
        return InvalidDistributorIds.Contains(Chain.Distributor.Id);
    };

    auto IsBeltDistributorLaneBlocked = [&ResolveDistributorTopology](const FSFSourceChain& Chain) -> bool
    {
        const FSFDistributorPortTopology Topology = ResolveDistributorTopology(Chain);
        if (!Topology.bRecognized || !FSFDistributorTopologyResolver::IsBelt(Topology.Kind)) return false;
        return Chain.Distributor.ConnectedConnectors.Contains(Topology.LaneOutputPort.ToString()) &&
            Chain.Distributor.ConnectedConnectors.Contains(Topology.LaneInputPort.ToString());
    };

    TSet<FString> ExcludedSegmentIds;
    auto CollectExcludedSegmentIds = [&ExcludedSegmentIds, &IsDistributorBranchInvalid, &IsBeltDistributorLaneBlocked](const TArray<FSFSourceChain>& Chains)
    {
        for (const FSFSourceChain& Chain : Chains)
        {
            const bool bExcluded = Chain.Distributor.Id.IsEmpty() ||
                IsDistributorBranchInvalid(Chain) || IsBeltDistributorLaneBlocked(Chain);
            if (!bExcluded) continue;
            for (const FSFSourceSegment& Segment : Chain.Segments)
            {
                if (!Segment.Id.IsEmpty()) ExcludedSegmentIds.Add(Segment.Id);
            }
        }
    };
    CollectExcludedSegmentIds(Source.BeltInputChains);
    CollectExcludedSegmentIds(Source.BeltOutputChains);
    CollectExcludedSegmentIds(Source.PipeInputChains);
    CollectExcludedSegmentIds(Source.PipeOutputChains);
    
    auto BuildMapping = [&](const FSFSourceChain& Chain)
    {
        // Skip chains without a physical distributor (Issue #277)
        // Belt chains must terminate at a splitter/merger, pipe chains at a junction
        if (Chain.Distributor.Id.IsEmpty())
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("\u26a0\ufe0f EXTEND: Skipping chain '%s' - no physical distributor (terminates directly at factory)"), *Chain.ChainId);
            return;
        }

        if (IsDistributorBranchInvalid(Chain))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("Extend: discarding invalid distributor branch %s.%s (missing complete perpendicular lane)"),
                *Chain.Distributor.Class, *Chain.Distributor.ConnectorUsed);
            return;
        }
        
        // Skip belt chains where both manifold lane connectors (Output1/Input1) are occupied (Issue #277)
        // A distributor with both lane connectors used by factory connections can't form a manifold lane
        if (IsBeltDistributorLaneBlocked(Chain))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("\u26a0\ufe0f EXTEND: Skipping chain '%s' - distributor '%s' has both lane connectors (Output1/Input1) occupied"), *Chain.ChainId, *Chain.Distributor.Id);
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

        if (IsDistributorBranchInvalid(Chain))
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
        DistHolo.Customization = Chain.Distributor.Customization;  // [#477]
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
            SegHolo.Customization = Seg.Customization;  // [#477]
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
    // [#498] Clone-space pole connector positions by pole HologramId, recorded while the poles are
    // emitted — PHASE 2.7 anchors each pump's cable preview to its own pole's connector.
    TMap<FString, FVector> PoleCloneConnectorWorldById;
    int32 PowerPoleIndex = 0;
    for (const FSFSourcePowerPole& SourcePole : Source.PowerPoles)
    {
        FSFCloneHologram PoleHolo;
        PoleHolo.HologramId = FString::Printf(TEXT("power_pole_%d"), PowerPoleIndex);
        PoleHolo.Role = TEXT("power_pole");
        PoleHolo.SourceId = SourcePole.Id;
        PoleHolo.Customization = SourcePole.Customization;  // [#477]
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

        // [#498] Record the clone pole's connector world position (same derivation the cable
        // previews below use) so PHASE 2.7 can anchor pump cables to it.
        PoleCloneConnectorWorldById.Add(PoleHolo.HologramId,
            SourcePole.bHasConnectorWorld
                ? SourcePole.PoleConnectorWorld.ToFVector() + Offset
                : PoleHolo.Transform.Location.ToFVector() + FVector(0.f, 0.f, SFWireConnectorHeightCm));

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

            // Issue #345: cable world endpoints for the visible preview (clone factory <-> clone pole).
            // Prefer the real captured source connectors (offset into the clone); fall back to a guessed
            // connector height only if they were not captured.
            {
                FVector FactoryConnW, PoleConnW;
                if (SourcePole.bHasConnectorWorld)
                {
                    FactoryConnW = SourcePole.FactoryConnectorWorld.ToFVector() + Offset;
                    PoleConnW    = SourcePole.PoleConnectorWorld.ToFVector() + Offset;
                }
                else
                {
                    const FVector ClonePoleLoc = PoleHolo.Transform.Location.ToFVector();
                    PoleConnW    = ClonePoleLoc + FVector(0.f, 0.f, SFWireConnectorHeightCm);
                    FactoryConnW = (ClonePoleLoc - SourcePole.RelativeOffset.ToFVector()) + FVector(0.f, 0.f, SFWireConnectorHeightCm);
                }
                FSFSplinePoint PtFactory; PtFactory.World = FSFVec3(FactoryConnW);
                FSFSplinePoint PtPole;    PtPole.World    = FSFVec3(PoleConnW);
                WireHolo.SplineData.Points.Add(PtFactory);
                WireHolo.SplineData.Points.Add(PtPole);
            }

            Result.ChildHolograms.Add(WireHolo);
        }

        // Wire cost hologram 2: Source pole ↔ Clone pole (only if source has free connections)
        if (SourcePole.bSourceHasFreeConnections)
        {
            float SourceToCloneDistance = Offset.Size();
            FSFCloneHologram WireHolo;
            WireHolo.HologramId = FString::Printf(TEXT("wire_source_clone_%d"), PowerPoleIndex);
            WireHolo.Role = TEXT("wire_cost");
            // Issue #345: this cable's source end must chain to the previous clone and only its clone end
            // rotates in Scaled Extend (handled by the lane rotation/chain passes via this flag).
            WireHolo.bIsSourceToCloneWire = true;
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

            // Issue #345: cable world endpoints (source pole <-> clone pole). The source pole connector
            // is real and unmoved; the clone pole connector is that same point offset into the clone.
            {
                FVector SourcePoleConnW, ClonePoleConnW;
                if (SourcePole.bHasConnectorWorld)
                {
                    SourcePoleConnW = SourcePole.PoleConnectorWorld.ToFVector();
                    ClonePoleConnW  = SourcePole.PoleConnectorWorld.ToFVector() + Offset;
                }
                else
                {
                    SourcePoleConnW = SourcePole.Transform.Location.ToFVector() + FVector(0.f, 0.f, SFWireConnectorHeightCm);
                    ClonePoleConnW  = PoleHolo.Transform.Location.ToFVector()  + FVector(0.f, 0.f, SFWireConnectorHeightCm);
                }
                FSFSplinePoint PtSource; PtSource.World = FSFVec3(SourcePoleConnW);
                FSFSplinePoint PtClone;  PtClone.World  = FSFVec3(ClonePoleConnW);
                WireHolo.SplineData.Points.Add(PtSource);
                WireHolo.SplineData.Points.Add(PtClone);
            }

            Result.ChildHolograms.Add(WireHolo);
        }
        
        PowerPoleIndex++;
    }

    // ========================================================================
    // PHASE 2.7: Pump → pole cable cost/preview holograms (#498)
    // ========================================================================
    // Powered pipe attachments (pumps) get their REAL power line at build time (Phase 3.8b wires
    // pump → its clone pole), but nothing represented that cable in the PREVIEW: clone pumps showed
    // bare power connectors and the extend quote missed the cable's length-based cost. Emit one
    // wire_cost child per powered pump — the same preview-only channel as the factory/daisy cables
    // above, so the visible catenary and the cost (endpoints drive GetWireLength → GetCost) ride the
    // established path. Both endpoints are clone-internal: the prefix pass scopes the id and the
    // rigid rotation passes translate/rotate it exactly like wire_factory_pole. Valves and pumps
    // whose source had no cable (empty ConnectedPowerPoleHologramId) emit nothing, mirroring 3.8b.
    {
        TArray<FSFCloneHologram> PumpCableHolos;
        for (const FSFCloneHologram& AttHolo : Result.ChildHolograms)
        {
            if (AttHolo.Role != TEXT("pipe_attachment") || AttHolo.ConnectedPowerPoleHologramId.IsEmpty())
            {
                continue;
            }
            const FVector* PoleConnW = PoleCloneConnectorWorldById.Find(AttHolo.ConnectedPowerPoleHologramId);
            if (!PoleConnW)
            {
                continue;  // pole outside the captured manifold — 3.8b skips these too
            }

            const FVector PumpW = AttHolo.Transform.Location.ToFVector();

            FSFCloneHologram WireHolo;
            WireHolo.HologramId = TEXT("wire_pump_") + AttHolo.HologramId;
            WireHolo.Role = TEXT("wire_cost");
            WireHolo.SourceId = AttHolo.SourceId;
            WireHolo.SourceClass = TEXT("Build_PowerLine_C");
            WireHolo.HologramClass = TEXT("ASFWireHologram");
            WireHolo.BuildClass = TEXT("Build_PowerLine_C");
            WireHolo.RecipeClass = TEXT("Recipe_PowerLine_C");
            WireHolo.Transform = AttHolo.Transform;
            WireHolo.bConstructible = false;
            WireHolo.bPreviewOnly = true;
            WireHolo.bHasSplineData = true;
            WireHolo.SplineData.Length = FVector::Dist(PumpW, *PoleConnW);

            FSFSplinePoint PtPump; PtPump.World = FSFVec3(PumpW);
            FSFSplinePoint PtPole; PtPole.World = FSFVec3(*PoleConnW);
            WireHolo.SplineData.Points.Add(PtPump);
            WireHolo.SplineData.Points.Add(PtPole);

            PumpCableHolos.Add(WireHolo);
        }
        Result.ChildHolograms.Append(PumpCableHolos);
    }

    // ========================================================================
    // PHASE 2.6: Generate Pipe Passthrough Clone Holograms (Issue #260)
    // ========================================================================
    // Pipe floor holes are NOT in pipe connection chains (pipes pass through physically,
    // not logically). They are discovered spatially and need explicit hologram cloning.
    int32 PassthroughCloneIndex = 0;
    for (const FSFSourceSegment& PassSeg : Source.PipePassthroughs)
    {
        bool bHasRelatedOwner = false;
        bool bHasKeptOwner = false;
        for (const FString& RelatedSourceId : PassSeg.RelatedSourceIds)
        {
            if (RelatedSourceId.IsEmpty()) continue;
            bHasRelatedOwner = true;
            if (!ExcludedSegmentIds.Contains(RelatedSourceId))
            {
                bHasKeptOwner = true;
                break;
            }
        }
        if (bHasRelatedOwner && !bHasKeptOwner)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("Extend: dropping floor hole %s because all snapped conduit owners belong to an excluded branch"),
                *PassSeg.Id);
            continue;
        }

        FSFCloneHologram PassHolo;
        PassHolo.HologramId = FString::Printf(TEXT("passthrough_%d"), PassthroughCloneIndex);
        PassHolo.Role = TEXT("passthrough");
        PassHolo.SourceId = PassSeg.Id;
        PassHolo.Customization = PassSeg.Customization;  // [#477]
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
    //
    // #wall-hole-orphan: a wall hole decorates one specific conduit segment. If that conduit's chain is
    // EXCLUDED from the clone — a blocked distributor lane (both Output1/Input1 occupied), so the
    // splitter + its belts never clone — the hole must drop with it, else it orphans onto the source
    // building, decorating whatever it lands on (e.g. a pipe, reading as a stray pipe wall hole). Match
    // by WORLD POSITION, not actor id: in MP the wall-hole discovery that would stamp an owner id runs on
    // the SERVER while this clone plan runs on the CLIENT, and the two use independent actor-id spaces
    // (server conduits Build_...460xxx, client chains Build_...469xxx) so an owner id never matches. World
    // coordinates ARE identical on both, so we drop a hole that physically sits on an excluded segment.
    struct FSFExclSeg { FVector A; FVector B; };
    TArray<FSFExclSeg> ExcludedSegs;
    auto CollectExcludedChainGeometry = [&ExcludedSegs, &IsBeltDistributorLaneBlocked, &IsDistributorBranchInvalid](const TArray<FSFSourceChain>& Chains)
    {
        for (const FSFSourceChain& Chain : Chains)
        {
            // Mirror ProcessChain's skip conditions (SFExtendCloneTopology.cpp ~261-270): a chain whose
            // distributor is absent, or whose Output1/Input1 lane connectors are both occupied, is
            // excluded from the clone entirely — its distributor + segments never build.
            const bool bExcluded = Chain.Distributor.Id.IsEmpty() ||
                IsDistributorBranchInvalid(Chain) || IsBeltDistributorLaneBlocked(Chain);
            if (!bExcluded) continue;
            for (const FSFSourceSegment& Seg : Chain.Segments)
            {
                if (Seg.bHasSplineData && Seg.SplineData.Points.Num() >= 2)
                {
                    ExcludedSegs.Add({ Seg.SplineData.Points[0].World.ToFVector(),
                                       Seg.SplineData.Points.Last().World.ToFVector() });
                }
                else
                {
                    const FVector C = Seg.Transform.Location.ToFVector();
                    ExcludedSegs.Add({ C, C });
                }
            }
        }
    };
    CollectExcludedChainGeometry(Source.BeltInputChains);
    CollectExcludedChainGeometry(Source.BeltOutputChains);
    CollectExcludedChainGeometry(Source.PipeInputChains);
    CollectExcludedChainGeometry(Source.PipeOutputChains);

    auto PointToSegDistSq = [](const FVector& P, const FVector& A, const FVector& B) -> float
    {
        const FVector AB = B - A;
        const double L2 = AB.SizeSquared();
        if (L2 < 1.0) return static_cast<float>(FVector::DistSquared(P, A));
        const double T = FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0);
        return static_cast<float>(FVector::DistSquared(P, A + T * AB));
    };
    // The hole's center sits on its conduit; 200cm covers wall-hole pivot/Z offset with margin while
    // staying well below the gap to any kept chain.
    constexpr float OnExcludedSqCm = 200.0f * 200.0f;

    int32 WallHoleCloneIndex = 0;
    for (const FSFSourceSegment& WallSeg : Source.WallHoles)
    {
        // #wall-hole-orphan: drop a hole that physically sits on an excluded chain's segment (blocked
        // lane) — else it orphans onto the source building after that belt/pipe fails to clone. (Logged
        // via SF_EXTEND_DIAGNOSTIC_LOG, which forces Verbose — runtime-filtered in shipping.)
        const FVector HoleLoc = WallSeg.Transform.Location.ToFVector();
        float BestSq = TNumericLimits<float>::Max();
        for (const FSFExclSeg& S : ExcludedSegs)
        {
            BestSq = FMath::Min(BestSq, PointToSegDistSq(HoleLoc, S.A, S.B));
        }
        const bool bOnExcluded = !ExcludedSegs.IsEmpty() && BestSq <= OnExcludedSqCm;
        if (bOnExcluded)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("🔧 EXTEND: Dropping wall hole %s (class=%s) — sits on an excluded (blocked-lane) chain segment (%.0fcm)"),
                *WallSeg.Id, *WallSeg.Class, FMath::Sqrt(BestSq));
            continue;
        }

        FSFCloneHologram WallHolo;
        WallHolo.HologramId = FString::Printf(TEXT("wall_hole_%d"), WallHoleCloneIndex);
        WallHolo.Role = TEXT("wall_hole");
        WallHolo.SourceId = WallSeg.Id;
        WallHolo.Customization = WallSeg.Customization;  // [#477]
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
    // Lane-eligible connections are resolved from the distributor class + factory-side named port.
    // Geometry only orders the two mapped lane ports toward/away from the clone.
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

        if (IsDistributorBranchInvalid(Chain))
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
        // For PIPE JUNCTIONS (geometry-driven, Issue #320 - no Cross-specific assumptions):
        //   - ConnectorUsed tells us which connector goes to the factory chain (excluded)
        //   - Each connector's facing = (its captured world pos - junction centre), normalized
        //   - Source backbone port = free connector facing toward the clone (along the manifold axis)
        //   - Clone backbone port = connector facing back toward the source (the clone reuses the
        //     source's connector layout at the clone transform)
        //   - Works for the 4-port Cross, the 3-port T-Junction, and any future junction
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
            
            const FString FactoryConnector = Distributor.ConnectorUsed;
            const FVector ManifoldAxis = (CloneDistLocation - SourceDistCenter).GetSafeNormal();
            const FVector SourceFacingAxis = PrincipalAxisWorld.IsNearlyZero()
                ? ManifoldAxis : PrincipalAxisWorld.GetSafeNormal();
            const float FacingThreshold = 0.30f;
            FString BestSourceConn, BestCloneConn;
            FVector BestSourcePos = FVector::ZeroVector, BestClonePos = FVector::ZeroVector;
            FVector BestSourceNormal = FVector::ZeroVector, BestCloneNormal = FVector::ZeroVector;
            float BestSourceFacing = FacingThreshold;

            const FSFDistributorPortTopology Topology = FSFDistributorTopologyResolver::Resolve(
                Distributor.Class, FName(*FactoryConnector));
            TArray<FString> LaneCandidates;
            if (Topology.bRecognized)
            {
                LaneCandidates = { Topology.LanePortA.ToString(), Topology.LanePortB.ToString() };
            }
            else
            {
                SourceConnectorPositions.GetKeys(LaneCandidates);
                LaneCandidates.Remove(FactoryConnector);
            }

            for (const FString& ConnectorName : LaneCandidates)
            {
                const FVector* ConnectorPosition = SourceConnectorPositions.Find(ConnectorName);
                if (!ConnectorPosition || Distributor.ConnectedConnectors.Contains(ConnectorName)) continue;
                const FVector Normal = (*ConnectorPosition - SourceDistCenter).GetSafeNormal();
                const float Facing = FVector::DotProduct(Normal, SourceFacingAxis);
                if (Facing > BestSourceFacing)
                {
                    BestSourceFacing = Facing;
                    BestSourceConn = ConnectorName;
                    BestSourcePos = *ConnectorPosition;
                    BestSourceNormal = Normal;
                }
            }

            float BestCloneOpposition = -2.0f;
            for (const FString& ConnectorName : LaneCandidates)
            {
                const FVector* ConnectorPosition = SourceConnectorPositions.Find(ConnectorName);
                if (!ConnectorPosition || ConnectorName == BestSourceConn) continue;
                const FVector SourceLayoutNormal = (*ConnectorPosition - SourceDistCenter).GetSafeNormal();
                const float Opposition = FVector::DotProduct(SourceLayoutNormal, -BestSourceNormal);
                if (Opposition > BestCloneOpposition)
                {
                    BestCloneOpposition = Opposition;
                    const FVector LocalOffset = SourceRotation.UnrotateVector(*ConnectorPosition - SourceDistCenter);
                    const FVector CloneWorld = CloneDistLocation + CloneDistRotation.RotateVector(LocalOffset);
                    BestCloneConn = ConnectorName;
                    BestClonePos = CloneWorld;
                    BestCloneNormal = (CloneWorld - CloneDistLocation).GetSafeNormal();
                }
            }

            // Skip this junction's lane if either backbone connector is unavailable (e.g. the
            // facing port is consumed) - avoids emitting garbage lane data.
            if (BestSourceConn.IsEmpty() || BestCloneConn.IsEmpty())
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("PIPE LANE: No backbone connector facing the manifold axis for junction %s (factory=%s, connected=%d) - skipping lane segment"),
                    *DistributorId, *FactoryConnector, Distributor.ConnectedConnectors.Num());
                continue;
            }

            LaneType = TEXT("pipe");
            SplineStartPos = BestSourcePos;
            SplineEndPos = BestClonePos;
            Conn0Target = FString::Printf(TEXT("source:%s"), *DistributorId);
            Conn0Connector = BestSourceConn;
            Conn1Target = DistributorHologramId;
            Conn1Connector = BestCloneConn;
            LaneStartNormal = BestSourceNormal;
            LaneEndNormal = BestCloneNormal;

            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("PIPE LANE (named) %s: axis=(%.2f,%.2f,%.2f) Source.%s facing=%.2f -> Clone.%s opposition=%.2f (cloneRot=%.0f)"),
                *DistributorId, ManifoldAxis.X, ManifoldAxis.Y, ManifoldAxis.Z,
                *BestSourceConn, BestSourceFacing,
                *BestCloneConn, BestCloneOpposition, CloneDistRotation.Yaw);
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
            
            const FSFDistributorPortTopology Topology = FSFDistributorTopologyResolver::Resolve(
                Distributor.Class, FName(*Distributor.ConnectorUsed));
            const FString OutputPort = Topology.bRecognized ? Topology.LaneOutputPort.ToString() : TEXT("Output1");
            const FString InputPort = Topology.bRecognized ? Topology.LaneInputPort.ToString() : TEXT("Input1");
            const FVector* CapturedOutput = Distributor.ConnectorWorldPositions.Find(OutputPort);
            const FVector* CapturedInput = Distributor.ConnectorWorldPositions.Find(InputPort);
            if ((Topology.bRecognized && !Topology.bValidManifold) || !CapturedOutput || !CapturedInput)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("BELT LANE: Missing named topology/positions for %s.%s"),
                    *Distributor.Class, *Distributor.ConnectorUsed);
                continue;
            }

            const FVector SourceOutput1 = *CapturedOutput;
            const FVector SourceInput1 = *CapturedInput;
            const FVector LocalOutputOffset = SourceRotation.UnrotateVector(SourceOutput1 - SourceDistCenter);
            const FVector LocalInputOffset = SourceRotation.UnrotateVector(SourceInput1 - SourceDistCenter);
            const FVector CloneOutput1 = CloneDistLocation + CloneDistRotation.RotateVector(LocalOutputOffset);
            const FVector CloneInput1 = CloneDistLocation + CloneDistRotation.RotateVector(LocalInputOffset);
            
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ BELT LANE: Source center=(%.0f,%.0f) rot=%.0f, Clone center=(%.0f,%.0f) rot=%.0f"),
                SourceDistCenter.X, SourceDistCenter.Y, SourceRotation.Yaw,
                CloneDistLocation.X, CloneDistLocation.Y, CloneDistRotation.Yaw);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ BELT LANE: Source.Output1=(%.0f,%.0f), Source.Input1=(%.0f,%.0f)"),
                SourceOutput1.X, SourceOutput1.Y, SourceInput1.X, SourceInput1.Y);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ BELT LANE: Clone.Output1=(%.0f,%.0f), Clone.Input1=(%.0f,%.0f)"),
                CloneOutput1.X, CloneOutput1.Y, CloneInput1.X, CloneInput1.Y);
            
            // Find closest Output→Input pair
            // Option 1: Source Output1 → Clone Input1
            // Option 2: Clone Output1 → Source Input1
            float Dist1 = FVector::Dist(SourceOutput1, CloneInput1);
            float Dist2 = FVector::Dist(CloneOutput1, SourceInput1);
            
            // Check if source connectors are already connected (skip manifold if so)
            bool bSourceOutput1Connected = Distributor.ConnectedConnectors.Contains(OutputPort);
            bool bSourceInput1Connected = Distributor.ConnectedConnectors.Contains(InputPort);
            
            // Determine which option is valid based on source connector availability
            bool bOption1Valid = !bSourceOutput1Connected;  // Source Output1 → Clone Input1
            bool bOption2Valid = !bSourceInput1Connected;   // Clone Output1 → Source Input1
            
            // If neither option is valid, skip this distributor's lane segment
            if (!bOption1Valid && !bOption2Valid)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🛤️ BELT LANE: Skipping %s - both Output1 (connected=%d) and Input1 (connected=%d) unavailable"),
                    *DistributorId, bSourceOutput1Connected, bSourceInput1Connected);
                continue;  // Skip to next distributor
            }
            
            LaneType = TEXT("belt");
            
            const FVector OutputNormalLocal = SourceRotation.UnrotateVector((SourceOutput1 - SourceDistCenter).GetSafeNormal());
            const FVector InputNormalLocal = SourceRotation.UnrotateVector((SourceInput1 - SourceDistCenter).GetSafeNormal());
            
            // Choose the closest valid option
            if (bOption1Valid && (!bOption2Valid || Dist1 <= Dist2))
            {
                // Source Output1 → Clone Input1 (source feeds clone)
                SplineStartPos = SourceOutput1;
                SplineEndPos = CloneInput1;
                Conn0Target = FString::Printf(TEXT("source:%s"), *DistributorId);
                Conn0Connector = OutputPort;
                Conn1Target = DistributorHologramId;
                Conn1Connector = InputPort;
                
                // Start normal = Source Output1 facing direction
                // End normal = Clone Input1 facing direction
                LaneStartNormal = SourceRotation.RotateVector(OutputNormalLocal);
                LaneEndNormal = CloneDistRotation.RotateVector(InputNormalLocal);
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ BELT LANE: Source.Output1(%.0f,%.0f)→Clone.Input1(%.0f,%.0f) (dist=%.1f vs %.1f)"),
                    SourceOutput1.X, SourceOutput1.Y, CloneInput1.X, CloneInput1.Y, Dist1, Dist2);
            }
            else
            {
                // Clone Output1 → Source Input1 (clone feeds source)
                SplineStartPos = CloneOutput1;
                SplineEndPos = SourceInput1;
                Conn0Target = DistributorHologramId;
                Conn0Connector = OutputPort;
                Conn1Target = FString::Printf(TEXT("source:%s"), *DistributorId);
                Conn1Connector = InputPort;
                
                // Start normal = Clone Output1 facing direction
                // End normal = Source Input1 facing direction
                LaneStartNormal = CloneDistRotation.RotateVector(OutputNormalLocal);
                LaneEndNormal = SourceRotation.RotateVector(InputNormalLocal);
                
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ BELT LANE: Clone.Output1(%.0f,%.0f)→Source.Input1(%.0f,%.0f) (dist=%.1f vs %.1f)"),
                    CloneOutput1.X, CloneOutput1.Y, SourceInput1.X, SourceInput1.Y, Dist2, Dist1);
            }
        }
        
        // Generate lane segment
        FSFCloneHologram LaneHolo;
        LaneHolo.HologramId = FString::Printf(TEXT("lane_segment_%d"), LaneSegmentIndex++);
        LaneHolo.Role = TEXT("lane_segment");
        LaneHolo.SourceId = TEXT("");  // Generated, not captured
        LaneHolo.SourceClass = TEXT("");
        // [#477] Lanes have no source actor; their appearance was resolved at capture time from
        // the first belt/pipe of this distributor's chain (LaneCustomization on the source record).
        LaneHolo.Customization = Distributor.LaneCustomization;
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
        
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ LANE: Generated %s lane %s.%s → %s.%s (type=%s)"),
            *LaneHolo.HologramId,
            *Conn0Target, *Conn0Connector,
            *Conn1Target, *Conn1Connector,
            *LaneHolo.LaneSegmentType);
    }
    
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ LANE: Generated %d manifold lane segments"), LaneSegmentIndex);
    
    return Result;
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

    // [#477] Class -> path string (empty for null); keeps records JSON/MP serializable.
    FString ClassPathOrEmpty(const UClass* Cls)
    {
        return Cls ? Cls->GetPathName() : FString();
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
                
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("📋 CAPTURE: Lift %s has %d passthroughs: [%s]"),
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
            Result.Distributor.Customization.CaptureFrom(Dist->GetCustomizationData_Implementation());
            // [#477] Lane appearance: sample the chain's first belt NOW (same source the live
            // LaneColorMap uses) - lanes are generated segments and can't be resampled later.
            if (Chain.Conveyors.Num() > 0 && Chain.Conveyors[0].IsValid())
            {
                Result.Distributor.LaneCustomization.CaptureFrom(Chain.Conveyors[0]->GetCustomizationData_Implementation());
            }
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
                if (DistConn)
                {
                    const FString ConnectorName = DistConn->GetFName().ToString();
                    Result.Distributor.ConnectorWorldPositions.Add(ConnectorName, DistConn->GetComponentLocation());
                    if (DistConn->IsConnected())
                    {
                        Result.Distributor.ConnectedConnectors.Add(ConnectorName);
                    }
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
            Seg.Customization.CaptureFrom(Conveyor->GetCustomizationData_Implementation());
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
            Result.Distributor.Customization.CaptureFrom(Junc->GetCustomizationData_Implementation());
            // [#477] Lane appearance from the chain's first pipe (see belt-distributor note).
            if (Chain.Pipelines.Num() > 0 && Chain.Pipelines[0].IsValid())
            {
                Result.Distributor.LaneCustomization.CaptureFrom(Chain.Pipelines[0]->GetCustomizationData_Implementation());
            }
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
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ PIPE CAPTURE: Junction %s at (%.0f,%.0f,%.0f), capturing %d connectors"),
                *Result.Distributor.Id, Junc->GetActorLocation().X, Junc->GetActorLocation().Y, Junc->GetActorLocation().Z, JuncConns.Num());
            for (UFGPipeConnectionComponentBase* JuncConn : JuncConns)
            {
                if (JuncConn)
                {
                    FString ConnName = JuncConn->GetFName().ToString();
                    FVector ConnPos = JuncConn->GetComponentLocation();
                    
                    // Store world position for lane spline generation
                    Result.Distributor.ConnectorWorldPositions.Add(ConnName, ConnPos);
                    
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ PIPE CAPTURE:   %s @ (%.0f,%.0f,%.0f) connected=%d"),
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
            Seg.Customization.CaptureFrom(Pipe->GetCustomizationData_Implementation());
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
            PassSeg.Customization.CaptureFrom(Passthrough->GetCustomizationData_Implementation());
            PassSeg.RecipeClass = GetRecipeClassName(Passthrough->GetBuiltWithRecipe());
            PassSeg.Transform = FSFTransform(Passthrough->GetActorLocation(), Passthrough->GetActorRotation());
            
            Result.Segments.Add(PassSeg);
            
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND CAPTURE: Pipe passthrough %s at %s"),
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
            AttSeg.Customization.CaptureFrom(Attachment->GetCustomizationData_Implementation());
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
            
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND CAPTURE: Pipe attachment %s (class=%s) UserFlowLimit=%.3f PowerPole=%s at %s"),
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
        SourcePole.Customization.CaptureFrom(Pole->GetCustomizationData_Implementation());
        SourcePole.RecipeClass = GetRecipeClassName(Pole->GetBuiltWithRecipe());
        SourcePole.Transform = FSFTransform(Pole->GetActorLocation(), Pole->GetActorRotation());
        SourcePole.RelativeOffset = FSFVec3(PowerNode.RelativeOffset);
        SourcePole.bSourceHasFreeConnections = PowerNode.bSourceHasFreeConnections;
        SourcePole.SourceFreeConnections = PowerNode.SourceFreeConnections;
        SourcePole.MaxConnections = PowerNode.MaxConnections;

        // Issue #345: capture the real source connector world positions for an accurate cable preview.
        if (PowerNode.PoleConnector.IsValid())
        {
            SourcePole.PoleConnectorWorld = FSFVec3(PowerNode.PoleConnector->GetComponentLocation());
        }
        if (PowerNode.FactoryConnector.IsValid())
        {
            SourcePole.FactoryConnectorWorld = FSFVec3(PowerNode.FactoryConnector->GetComponentLocation());
        }
        SourcePole.bHasConnectorWorld = PowerNode.PoleConnector.IsValid() && PowerNode.FactoryConnector.IsValid();

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
        PassSeg.Customization.CaptureFrom(Passthrough->GetCustomizationData_Implementation());
        PassSeg.RecipeClass = GetRecipeClassName(Passthrough->GetBuiltWithRecipe());
        PassSeg.Transform = FSFTransform(Passthrough->GetActorLocation(), Passthrough->GetActorRotation());

        if (AFGBuildablePassthrough* TypedPassthrough = Cast<AFGBuildablePassthrough>(Passthrough))
        {
            const UFGConnectionComponent* TopConnection = TypedPassthrough->GetTopSnappedConnection<UFGConnectionComponent>();
            const UFGConnectionComponent* BottomConnection = TypedPassthrough->GetBottomSnappedConnection<UFGConnectionComponent>();
            if (AActor* TopOwner = TopConnection ? TopConnection->GetOwner() : nullptr)
            {
                PassSeg.RelatedSourceIds.AddUnique(GetActorId(TopOwner));
            }
            if (AActor* BottomOwner = BottomConnection ? BottomConnection->GetOwner() : nullptr)
            {
                PassSeg.RelatedSourceIds.AddUnique(GetActorId(BottomOwner));
            }
        }
        
        // Read mSnappedBuildingThickness via reflection (protected member on AFGBuildablePassthrough)
        FFloatProperty* ThickProp = CastField<FFloatProperty>(
            Passthrough->GetClass()->FindPropertyByName(FName(TEXT("mSnappedBuildingThickness"))));
        if (ThickProp)
        {
            PassSeg.Thickness = ThickProp->GetPropertyValue_InContainer(Passthrough);
        }
        
        Result.PipePassthroughs.Add(PassSeg);
        
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND CAPTURE: Passthrough %s (class=%s, thickness=%.0f) at %s"),
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
        WallSeg.Customization.CaptureFrom(WallHole->GetCustomizationData_Implementation());
        WallSeg.RecipeClass = GetRecipeClassName(WallHole->GetBuiltWithRecipe());
        WallSeg.Transform = FSFTransform(WallHole->GetActorLocation(), WallHole->GetActorRotation());

        Result.WallHoles.Add(WallSeg);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND CAPTURE: Wall hole %s (class=%s) at %s"),
            *WallSeg.Id, *WallSeg.Class, *WallHole->GetActorLocation().ToString());
    }

    return Result;
}

// ============================================================================
// FSFCloneTopology - Spawn child holograms
// ============================================================================


int32 FSFCloneTopology::WireChildHologramConnections(
    const TMap<FString, AFGHologram*>& SpawnedHolograms,
    AFGHologram* ParentHologram) const
{
    
    if (!ParentHologram)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRE: No parent hologram provided"));
        return 0;
    }
    
    int32 WiredCount = 0;
    
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: Starting connection wiring for %d child holograms"), ChildHolograms.Num());
    
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
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRE: Hologram %s not found in spawned map"), *ChildData.HologramId);
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
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: %s Conn0 -> parent.%s"), 
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
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: %s Conn0 -> %s.%s"), 
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
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: %s Conn1 -> parent.%s"), 
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
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: %s Conn1 -> %s.%s"), 
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
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: Skipping belt %s (spline-based, will wire post-build)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("lift_segment"))
        {
            // SKIP lift wiring - setting mSnappedConnectionComponents causes vanilla to wire lifts,
            // but each lift gets its own chain actor. We need ConfigureComponents to handle wiring
            // so it can do Add/Remove/Add to unify chains (same approach as belts).
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: Skipping lift %s (will wire in ConfigureComponents for chain unification)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("pipe_segment"))
        {
            // SKIP pipe wiring - vanilla recalculates spline when mSnappedConnectionComponents is set
            // Connections will be established post-build instead
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: Skipping pipe %s (spline-based, will wire post-build)"),
                *ChildData.HologramId);
        }
        else if (ChildData.Role == TEXT("lane_segment"))
        {
            // Lane segments connect source distributor (existing buildable) to clone distributor (hologram)
            // They use the same wiring strategy as their segment type (belt/lift/pipe)
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🛤️ WIRE: Skipping lane %s (type=%s, will wire in ConfigureComponents)"),
                *ChildData.HologramId, *ChildData.LaneSegmentType);
        }
    }
    
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 WIRE: Completed - wired %d connections"), WiredCount);
    
    return WiredCount;
}

// ============================================================================
// [#477] FSFCapturedCustomization - capture/apply (see SFExtendCloneTopology.h)
// ============================================================================

void FSFCapturedCustomization::CaptureFrom(const FFactoryCustomizationData& Data)
{
	bCaptured = true;
	SwatchClass = Data.SwatchDesc ? Data.SwatchDesc->GetPathName() : FString();
	PatternClass = Data.PatternDesc ? Data.PatternDesc->GetPathName() : FString();
	MaterialClass = Data.MaterialDesc ? Data.MaterialDesc->GetPathName() : FString();
	SkinClass = Data.SkinDesc ? Data.SkinDesc->GetPathName() : FString();
	PaintFinishClass = Data.OverrideColorData.PaintFinish ? Data.OverrideColorData.PaintFinish->GetPathName() : FString();
	OverridePrimary = Data.OverrideColorData.PrimaryColor;
	OverrideSecondary = Data.OverrideColorData.SecondaryColor;
	PatternRotation = Data.PatternRotation;
}

bool FSFCapturedCustomization::ApplyTo(FFactoryCustomizationData& Out) const
{
	if (!bCaptured)
	{
		return false;
	}

	// Typed loads: the class must exist AND be the right descriptor kind (guards hand-edited or
	// mod-removed paths). If ANY non-empty path fails to resolve, report failure instead of a
	// partial look - the caller then tries the live-actor harvest (correct in-session) or ends
	// at honest defaults (post-restart with a missing descriptor mod). [#477 review finding]
	FFactoryCustomizationData Result;
	bool bAllResolved = true;
	if (!SwatchClass.IsEmpty())
	{
		Result.SwatchDesc = FSoftClassPath(SwatchClass).TryLoadClass<UFGFactoryCustomizationDescriptor_Swatch>();
		bAllResolved &= (Result.SwatchDesc != nullptr);
	}
	if (!PatternClass.IsEmpty())
	{
		Result.PatternDesc = FSoftClassPath(PatternClass).TryLoadClass<UFGFactoryCustomizationDescriptor_Pattern>();
		bAllResolved &= (Result.PatternDesc != nullptr);
	}
	if (!MaterialClass.IsEmpty())
	{
		Result.MaterialDesc = FSoftClassPath(MaterialClass).TryLoadClass<UFGFactoryCustomizationDescriptor_Material>();
		bAllResolved &= (Result.MaterialDesc != nullptr);
	}
	if (!SkinClass.IsEmpty())
	{
		Result.SkinDesc = FSoftClassPath(SkinClass).TryLoadClass<UFGFactoryCustomizationDescriptor_Skin>();
		bAllResolved &= (Result.SkinDesc != nullptr);
	}
	if (!PaintFinishClass.IsEmpty())
	{
		Result.OverrideColorData.PaintFinish = FSoftClassPath(PaintFinishClass).TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
		bAllResolved &= (Result.OverrideColorData.PaintFinish != nullptr);
	}
	if (!bAllResolved)
	{
		UE_LOG(LogSmartExtend, Verbose,
			TEXT("[#477] Captured customization has unresolvable descriptor path(s) (swatch='%s' pattern='%s' material='%s' skin='%s' finish='%s') - falling back."),
			*SwatchClass, *PatternClass, *MaterialClass, *SkinClass, *PaintFinishClass);
		return false;
	}
	Result.OverrideColorData.PrimaryColor = OverridePrimary;
	Result.OverrideColorData.SecondaryColor = OverrideSecondary;
	Result.PatternRotation = PatternRotation;
	Out = Result;
	return true;
}
