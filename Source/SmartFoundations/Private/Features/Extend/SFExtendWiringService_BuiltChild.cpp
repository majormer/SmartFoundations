// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendWiringService - Built-child registration + WireBuiltChildConnections (slice E2 unit F).
 * Moved verbatim from SFExtendService; operate on its shared registry maps via ExtendService->.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Core/Helpers/SFBuildEffectHelper.h"
#include "FGDismantleInterface.h"
#include "Shared/Power/SFWireDesignerRegistration.h"  // [#421] designer containment for direct-spawned wires

bool USFExtendWiringService::HasPendingPostBuildWiring() const
{
    return ExtendService->BuiltChainElements.Num() > 0 ||
        ExtendService->BuiltConveyorsByChain.Num() > 0 ||
        ExtendService->BuiltDistributorsByChain.Num() > 0 ||
        ExtendService->BuiltJunctionsByChain.Num() > 0 ||
        ExtendService->BuiltPipesByChain.Num() > 0 ||
        ExtendService->JsonBuiltActors.Num() > 0 ||
        ExtendService->JsonSpawnedHolograms.Num() > 0 ||
        ExtendService->PowerPoleWiringData.Num() > 0 ||
        ExtendService->ScaledExtendClones.Num() > 0 ||
        (ExtendService->StoredCloneTopology.IsValid() && ExtendService->StoredCloneTopology->ChildHolograms.Num() > 0);
}

void USFExtendWiringService::RegisterBuiltConveyor(int32 ChainId, int32 ChainIndex, AFGBuildableConveyorBase* BuiltConveyor, bool bIsInputChain)
{
    if (!BuiltConveyor) return;

    // Get or create the index map for ExtendService chain
    TMap<int32, AFGBuildableConveyorBase*>& ChainConveyors = ExtendService->BuiltConveyorsByChain.FindOrAdd(ChainId);
    ChainConveyors.Add(ChainIndex, BuiltConveyor);

    // Track chain direction
    if (!ExtendService->BuiltChainIsInputMap.Contains(ChainId))
    {
        ExtendService->BuiltChainIsInputMap.Add(ChainId, bIsInputChain);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built conveyor %s in chain %d at index %d (now %d conveyors, isInput=%d)"),
        *BuiltConveyor->GetName(), ChainId, ChainIndex, ChainConveyors.Num(), bIsInputChain);
}

AFGBuildableConveyorBase* USFExtendWiringService::GetBuiltConveyor(int32 ChainId, int32 ChainIndex) const
{
    const TMap<int32, AFGBuildableConveyorBase*>* ChainConveyors = ExtendService->BuiltConveyorsByChain.Find(ChainId);
    if (ChainConveyors)
    {
        AFGBuildableConveyorBase* const* Conveyor = ChainConveyors->Find(ChainIndex);
        if (Conveyor)
        {
            return *Conveyor;
        }
    }
    return nullptr;
}

void USFExtendWiringService::RegisterBuiltDistributor(int32 ChainId, AFGBuildable* BuiltDistributor)
{
    if (!BuiltDistributor) return;

    ExtendService->BuiltDistributorsByChain.Add(ChainId, BuiltDistributor);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built distributor %s for chain %d"),
        *BuiltDistributor->GetName(), ChainId);
}

AFGBuildable* USFExtendWiringService::GetBuiltDistributor(int32 ChainId) const
{
    AFGBuildable* const* Distributor = ExtendService->BuiltDistributorsByChain.Find(ChainId);
    return Distributor ? *Distributor : nullptr;
}

FName USFExtendWiringService::GetDistributorConnectorName(int32 ChainId) const
{
    const FName* ConnectorName = ExtendService->DistributorConnectorNameByChain.Find(ChainId);
    return ConnectorName ? *ConnectorName : NAME_None;
}

void USFExtendWiringService::SetDistributorConnectorName(int32 ChainId, FName ConnectorName)
{
    ExtendService->DistributorConnectorNameByChain.Add(ChainId, ConnectorName);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Stored distributor connector name '%s' for chain %d"),
        *ConnectorName.ToString(), ChainId);
}

void USFExtendWiringService::RegisterBuiltJunction(int32 ChainId, AFGBuildable* BuiltJunction)
{
    if (!BuiltJunction) return;

    ExtendService->BuiltJunctionsByChain.Add(ChainId, BuiltJunction);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built junction %s for pipe chain %d"),
        *BuiltJunction->GetName(), ChainId);
}

void USFExtendWiringService::RegisterBuiltPipe(int32 ChainId, int32 ChainIndex, AFGBuildablePipeline* BuiltPipe, bool bIsInputChain)
{
    if (!BuiltPipe) return;

    // Create chain entry if it doesn't exist
    if (!ExtendService->BuiltPipesByChain.Contains(ChainId))
    {
        ExtendService->BuiltPipesByChain.Add(ChainId, TMap<int32, AFGBuildablePipeline*>());
    }

    // Add pipe to chain at index
    ExtendService->BuiltPipesByChain[ChainId].Add(ChainIndex, BuiltPipe);

    // Track chain direction
    if (!ExtendService->BuiltPipeChainIsInputMap.Contains(ChainId))
    {
        ExtendService->BuiltPipeChainIsInputMap.Add(ChainId, bIsInputChain);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built pipe %s for pipe chain %d, index %d (%s)"),
        *BuiltPipe->GetName(), ChainId, ChainIndex, bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));
}

int32 USFExtendWiringService::CopyDistributorConfigurations()
{
    if (ExtendService->IsExecutingOwnedNativeInput())
        return ExtendService->ApplyOwnedDistributorSettings() ? 1 : 0;
    const FSFCloneTopology* Captured = ExtendService->GetCapturedWiringTopology();
    if (!Captured) Captured = ExtendService->StoredCloneTopology.Get();
    if (!Captured) return 0;
    // The unconverted caller still needs operation ownership, but it must consume
    // the same value intent. Never resolve a historical source name to copy state.
    const FSFCloneTopology Values = *Captured;
    int32 Applied = 0;
    for (const auto& Child : Values.ChildHolograms)
    {
        if (Child.Role != TEXT("distributor") || Child.bPreviewOnly || !Child.bConstructible) continue;
        AFGBuildable* Built = GetBuiltActorByCloneId(Child.HologramId);
        if (!IsValid(Built)) continue;
        const bool bConfigurable = Built->IsA<AFGBuildableSplitterSmart>() || Built->IsA<AFGBuildableMergerPriority>();
        if (!bConfigurable && !Child.DistributorSettings.bCaptured) continue;
        if (Child.DistributorSettings.ApplyAndVerify(Built)) ++Applied;
        else UE_LOG(LogSmartExtend, Warning, TEXT("Extend/Restore distributor settings not verified for %s; captured values were not substituted with live source state."), *Child.HologramId);
    }
    return Applied;
}

void USFExtendWiringService::WireBuiltChildConnections(AFGBuildableFactory* NewFactory)
{
    if (!ExtendService || !ExtendService->IsExecutingOwnedConveyorWiring()) return;

    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called with null factory"));
        return;
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called for %s (StoredCloneTopology=%s, JsonBuiltActors=%d)"),
        *NewFactory->GetName(),
        (ExtendService->StoredCloneTopology.IsValid() && ExtendService->StoredCloneTopology->ChildHolograms.Num() > 0) ? TEXT("VALID") : TEXT("INVALID"),
        ExtendService->JsonBuiltActors.Num());

    if (ExtendService->bRestoredCloneTopologyActive || ExtendService->bRestoredScaledWiringDeferred)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Post-build wiring start: factory=%s storedChildren=%d jsonBuilt=%d jsonSpawned=%d previewFactories=%d parentValid=%d retry=%d/%d deferred=%d"),
            *NewFactory->GetName(),
            ExtendService->StoredCloneTopology.IsValid() ? ExtendService->StoredCloneTopology->ChildHolograms.Num() : 0,
            ExtendService->JsonBuiltActors.Num(),
            ExtendService->JsonSpawnedHolograms.Num(),
            ExtendService->RestoredScaledFactoryPreviewLocations.Num(),
            ExtendService->RestoredCloneParentHologram.IsValid() ? 1 : 0,
            ExtendService->RestoredScaledWiringRetryAttempts,
            5,
            ExtendService->bRestoredScaledWiringDeferred ? 1 : 0);
    }

    // Issues #298, #299: Copy Smart/Programmable Splitter filter configuration from every
    // source distributor to its cloned counterpart before wiring brings belts online. Done
    // here (rather than at individual Construct() time) so the source → clone pairing has
    // the full topology available for lookup.
    CopyDistributorConfigurations();

    int32 TotalConveyorChains = ExtendService->BuiltConveyorsByChain.Num();
    int32 TotalPipeChains = ExtendService->BuiltPipesByChain.Num();

    // ==================== PIPE ATTACHMENT PRE-WIRING (Issue #288) ====================
    // Wire cloned valves/pumps to their neighbouring pipe connectors BEFORE the JSON
    // wiring pass in GenerateAndExecuteWiring consumes the topology. Each attachment's
    // two UFGPipeConnectionComponent endpoints coincide with the adjacent cloned pipes'
    // terminal connectors, so a small-radius proximity match reliably links them.
    // IMPORTANT: ExtendService must run BEFORE GenerateAndExecuteWiring — that function clears
    // ExtendService->StoredCloneTopology / ExtendService->JsonBuiltActors on completion, which would otherwise leave
    // both 3.8a and 3.8b with nothing to iterate.
    {
        constexpr float AttachmentConnectorProximitySqCm = 25.0f * 25.0f;  // 25 cm radius
        int32 AttachmentsWired = 0;
        for (const TPair<FString, TObjectPtr<AActor>>& Entry : ExtendService->JsonBuiltActors)
        {
            if (!Entry.Key.StartsWith(TEXT("pipe_attachment_"))) continue;
            AActor* AttachmentActor = Entry.Value;
            if (!IsValid(AttachmentActor)) continue;

            TArray<UFGPipeConnectionComponent*> AttachmentConns;
            AttachmentActor->GetComponents<UFGPipeConnectionComponent>(AttachmentConns);

            for (UFGPipeConnectionComponent* AttConn : AttachmentConns)
            {
                if (!AttConn || AttConn->IsConnected()) continue;
                const FVector AttLoc = AttConn->GetComponentLocation();

                UFGPipeConnectionComponent* BestPipeConn = nullptr;
                float BestDistSq = AttachmentConnectorProximitySqCm;

                for (const TPair<FString, TObjectPtr<AActor>>& PipeEntry : ExtendService->JsonBuiltActors)
                {
                    AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(PipeEntry.Value);
                    if (!IsValid(Pipe)) continue;

                    for (UFGPipeConnectionComponent* PipeConn : { Pipe->GetPipeConnection0(), Pipe->GetPipeConnection1() })
                    {
                        if (!PipeConn || PipeConn->IsConnected()) continue;
                        const float DistSq = FVector::DistSquared(AttLoc, PipeConn->GetComponentLocation());
                        if (DistSq < BestDistSq)
                        {
                            BestDistSq = DistSq;
                            BestPipeConn = PipeConn;
                        }
                    }
                }

                if (BestPipeConn)
                {
                    AttConn->SetConnection(BestPipeConn);
                    ++AttachmentsWired;
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   🔧 Pipe attachment wired: %s.%s ↔ %s.%s (dist=%.1f cm)"),
                        *AttachmentActor->GetName(), *AttConn->GetName(),
                        *BestPipeConn->GetOwner()->GetName(), *BestPipeConn->GetName(),
                        FMath::Sqrt(BestDistSq));
                }
            }
        }
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8a (#288): wired %d pipe-attachment endpoint(s) to neighbouring cloned pipes (JsonBuiltActors=%d)"),
            AttachmentsWired, ExtendService->JsonBuiltActors.Num());
    }

    // Pump cables are part of the priced endpoint plan consumed by JSON wiring.

    // ==================== PHASE 5/6: JSON-Based Wiring ====================
    // Try JSON-based wiring first (for JSON-spawned holograms)
    // This runs regardless of whether chain-based wiring has data
    // NOTE: GenerateAndExecuteWiring resets ExtendService->StoredCloneTopology and ExtendService->JsonBuiltActors
    // at its end — phase 3.8a above must run before this call.
    int32 JsonWiredCount = GenerateAndExecuteWiring(NewFactory);
    if (ExtendService->bRestoredScaledWiringDeferred)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] JSON wiring deferred; leaving post-build tracking intact for retry (jsonBuilt=%d, jsonSpawned=%d, storedChildren=%d)"),
            ExtendService->JsonBuiltActors.Num(),
            ExtendService->JsonSpawnedHolograms.Num(),
            ExtendService->StoredCloneTopology.IsValid() ? ExtendService->StoredCloneTopology->ChildHolograms.Num() : 0);
        return;
    }
    if (JsonWiredCount > 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 5/6: JSON-based wiring completed - %d connections"), JsonWiredCount);
    }

    // NOTE (Issue #288): Previously ExtendService function early-returned here when
    // ExtendService->BuiltConveyorsByChain and ExtendService->BuiltPipesByChain were both empty. That gated
    // phases 3.8a (pipe-attachment pre-wiring) and 3.8b (pump→pole power wiring)
    // behind the legacy chain maps, even though both phases only read from
    // ExtendService->JsonBuiltActors / ExtendService->StoredCloneTopology. For JSON-only topologies (e.g. an
    // oil refinery whose pipes/valves/pumps live entirely in the JSON-spawned
    // set) the early return caused Phase 3.8b to never run, leaving cloned
    // pumps unwired from their clone power pole. The downstream conveyor and
    // pipe chain loops below are already safe no-ops when their maps are empty.
    if (TotalConveyorChains == 0 && TotalPipeChains == 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: No legacy built chains — continuing to JSON-based attachment phases (JSON wiring: %d)"), JsonWiredCount);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring built child connections for %s (Conveyor chains: %d, Pipe chains: %d)"),
        *NewFactory->GetName(), TotalConveyorChains, TotalPipeChains);

    int32 TotalConnections = 0;
    int32 FailedConnections = 0;

    // Owned conveyor links were applied once by the merged native-phase adapter.
 // legacy conveyor wiring

    // ==================== PIPE CHAIN WIRING ====================
    // Process pipe chains similarly to belt chains

    if (TotalPipeChains > 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring %d pipe chains"), TotalPipeChains);

        for (auto& PipeChainPair : ExtendService->BuiltPipesByChain)
        {
            int32 PipeChainId = PipeChainPair.Key;
            TMap<int32, AFGBuildablePipeline*>& PipesByIndex = PipeChainPair.Value;
            bool bIsInputChain = ExtendService->BuiltPipeChainIsInputMap.FindRef(PipeChainId);

            // Sort indices to get pipes in order
            TArray<int32> SortedPipeIndices;
            PipesByIndex.GetKeys(SortedPipeIndices);
            SortedPipeIndices.Sort();

            // Build ordered array of pipes
            TArray<AFGBuildablePipeline*> OrderedPipes;
            for (int32 Index : SortedPipeIndices)
            {
                if (AFGBuildablePipeline* Pipe = PipesByIndex.FindRef(Index))
                {
                    OrderedPipes.Add(Pipe);
                }
            }

            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Processing pipe chain %d with %d pipes (%s), indices: %s"),
                PipeChainId, OrderedPipes.Num(), bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"),
                *FString::JoinBy(SortedPipeIndices, TEXT(", "), [](int32 i) { return FString::FromInt(i); }));

            // Connect consecutive pipes in the chain
            // Physical topology: Pipe[i].Conn1 meets Pipe[i+1].Conn0 at the same location
            // This is the same for both INPUT and OUTPUT chains (flow direction is handled by pumps)
            for (int32 i = 0; i < OrderedPipes.Num() - 1; i++)
            {
                AFGBuildablePipeline* CurrentPipe = OrderedPipes[i];
                AFGBuildablePipeline* NextPipe = OrderedPipes[i + 1];

                if (!CurrentPipe || !NextPipe) continue;

                // Physical connection: CurrentPipe.Conn1 → NextPipe.Conn0
                UFGPipeConnectionComponent* FromConn = CurrentPipe->GetPipeConnection1();
                UFGPipeConnectionComponent* ToConn = NextPipe->GetPipeConnection0();

                if (FromConn && ToConn && !FromConn->IsConnected() && !ToConn->IsConnected())
                {
                    FromConn->SetConnection(ToConn);
                    TotalConnections++;
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s.Conn1 → %s.Conn0"),
                        *CurrentPipe->GetName(), *NextPipe->GetName());
                }
                else
                {
                    FailedConnections++;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Could not connect %s.Conn1 → %s.Conn0 (From.Connected=%d, To.Connected=%d)"),
                        *CurrentPipe->GetName(), *NextPipe->GetName(),
                        FromConn ? FromConn->IsConnected() : -1,
                        ToConn ? ToConn->IsConnected() : -1);
                }
            }

            // Connect pipe to factory using SOURCE topology connector name (1:1 mapping)
            if (OrderedPipes.Num() > 0)
            {
                AFGBuildablePipeline* FactoryPipe = OrderedPipes[0];  // First pipe is at factory end

                // Get the source factory connector name from topology
                FName SourceFactoryConnectorName = NAME_None;
                if (bIsInputChain && PipeChainId < ExtendService->GetCurrentTopology().PipeInputChains.Num())
                {
                    if (ExtendService->GetCurrentTopology().PipeInputChains[PipeChainId].SourceConnector.IsValid())
                    {
                        SourceFactoryConnectorName = ExtendService->GetCurrentTopology().PipeInputChains[PipeChainId].SourceConnector->GetFName();
                    }
                }
                else if (!bIsInputChain)
                {
                    int32 OutputChainIndex = PipeChainId - ExtendService->GetCurrentTopology().PipeInputChains.Num();
                    if (OutputChainIndex >= 0 && OutputChainIndex < ExtendService->GetCurrentTopology().PipeOutputChains.Num())
                    {
                        if (ExtendService->GetCurrentTopology().PipeOutputChains[OutputChainIndex].SourceConnector.IsValid())
                        {
                            SourceFactoryConnectorName = ExtendService->GetCurrentTopology().PipeOutputChains[OutputChainIndex].SourceConnector->GetFName();
                        }
                    }
                }

                // Find the clone factory connector with the SAME NAME as the source
                TArray<UFGPipeConnectionComponent*> FactoryPipeConnectors;
                NewFactory->GetComponents<UFGPipeConnectionComponent>(FactoryPipeConnectors);

                UFGPipeConnectionComponent* TargetFactoryConn = nullptr;
                for (UFGPipeConnectionComponent* FactoryConn : FactoryPipeConnectors)
                {
                    if (FactoryConn && FactoryConn->GetFName() == SourceFactoryConnectorName)
                    {
                        TargetFactoryConn = FactoryConn;
                        break;
                    }
                }

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔍 Factory wiring: %s chain %d, Pipe=%s, TargetConnector=%s"),
                    bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"), PipeChainId,
                    *FactoryPipe->GetName(), *SourceFactoryConnectorName.ToString());

                // Find which pipe connector (Conn0 or Conn1) is closest to the target factory connector
                UFGPipeConnectionComponent* PipeConn = nullptr;
                if (TargetFactoryConn)
                {
                    FVector TargetLoc = TargetFactoryConn->GetComponentLocation();
                    UFGPipeConnectionComponent* Conn0 = FactoryPipe->GetPipeConnection0();
                    UFGPipeConnectionComponent* Conn1 = FactoryPipe->GetPipeConnection1();

                    float Dist0 = Conn0 ? FVector::Dist(Conn0->GetComponentLocation(), TargetLoc) : FLT_MAX;
                    float Dist1 = Conn1 ? FVector::Dist(Conn1->GetComponentLocation(), TargetLoc) : FLT_MAX;

                    PipeConn = (Dist0 < Dist1) ? Conn0 : Conn1;

                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔍 Pipe Conn0 dist=%.1f cm, Conn1 dist=%.1f cm, using %s"),
                        Dist0, Dist1, (Dist0 < Dist1) ? TEXT("Conn0") : TEXT("Conn1"));
                }

                if (PipeConn && TargetFactoryConn && !PipeConn->IsConnected() && !TargetFactoryConn->IsConnected())
                {
                    float Distance = FVector::Dist(PipeConn->GetComponentLocation(), TargetFactoryConn->GetComponentLocation());
                    PipeConn->SetConnection(TargetFactoryConn);
                    TotalConnections++;
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s → Factory.%s (distance=%.1f cm)"),
                        *FactoryPipe->GetName(), *TargetFactoryConn->GetName(), Distance);
                }
                else if (!TargetFactoryConn)
                {
                    FailedConnections++;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Could not find factory connector '%s' on clone factory"),
                        *SourceFactoryConnectorName.ToString());
                }
                else if (PipeConn && PipeConn->IsConnected())
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ℹ️ Pipe connector already connected, skipping factory wiring"));
                }
                else if (TargetFactoryConn && TargetFactoryConn->IsConnected())
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ℹ️ Factory connector '%s' already connected, skipping"),
                        *TargetFactoryConn->GetName());
                }
            }

            // Connect pipe to junction using SOURCE topology to determine connector names
            // The source junction connector tells us which connector name to use on the clone
            if (OrderedPipes.Num() > 0)
            {
                AFGBuildablePipeline* JunctionPipe = OrderedPipes.Last();  // Last pipe is at junction end

                // Get the CLONE junction for ExtendService chain
                AFGBuildable** JunctionPtr = ExtendService->BuiltJunctionsByChain.Find(PipeChainId);
                if (JunctionPtr && *JunctionPtr)
                {
                    AFGBuildable* CloneJunction = *JunctionPtr;

                    // Get the source junction connector name from topology
                    FName SourceJunctionConnectorName = NAME_None;
                    if (bIsInputChain && PipeChainId < ExtendService->GetCurrentTopology().PipeInputChains.Num())
                    {
                        if (ExtendService->GetCurrentTopology().PipeInputChains[PipeChainId].JunctionConnector.IsValid())
                        {
                            SourceJunctionConnectorName = ExtendService->GetCurrentTopology().PipeInputChains[PipeChainId].JunctionConnector->GetFName();
                        }
                    }
                    else if (!bIsInputChain)
                    {
                        // For output chains, PipeChainId is offset by the number of input chains
                        int32 OutputChainIndex = PipeChainId - ExtendService->GetCurrentTopology().PipeInputChains.Num();
                        if (OutputChainIndex >= 0 && OutputChainIndex < ExtendService->GetCurrentTopology().PipeOutputChains.Num())
                        {
                            if (ExtendService->GetCurrentTopology().PipeOutputChains[OutputChainIndex].JunctionConnector.IsValid())
                            {
                                SourceJunctionConnectorName = ExtendService->GetCurrentTopology().PipeOutputChains[OutputChainIndex].JunctionConnector->GetFName();
                            }
                        }
                    }

                    // Find the clone junction connector with the SAME NAME as the source
                    TArray<UFGPipeConnectionComponent*> JunctionConnectors;
                    CloneJunction->GetComponents<UFGPipeConnectionComponent>(JunctionConnectors);

                    UFGPipeConnectionComponent* TargetJunctionConn = nullptr;
                    for (UFGPipeConnectionComponent* JunctionConn : JunctionConnectors)
                    {
                        if (JunctionConn && JunctionConn->GetFName() == SourceJunctionConnectorName)
                        {
                            TargetJunctionConn = JunctionConn;
                            break;
                        }
                    }

                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔍 Junction wiring: %s chain %d, Pipe=%s, Junction=%s (CLONE), TargetConnector=%s"),
                        bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"), PipeChainId,
                        *JunctionPipe->GetName(), *CloneJunction->GetName(),
                        *SourceJunctionConnectorName.ToString());

                    // Find which pipe connector (Conn0 or Conn1) is closest to the target junction connector
                    UFGPipeConnectionComponent* PipeConn = nullptr;
                    if (TargetJunctionConn)
                    {
                        FVector TargetLoc = TargetJunctionConn->GetComponentLocation();
                        UFGPipeConnectionComponent* Conn0 = JunctionPipe->GetPipeConnection0();
                        UFGPipeConnectionComponent* Conn1 = JunctionPipe->GetPipeConnection1();

                        float Dist0 = Conn0 ? FVector::Dist(Conn0->GetComponentLocation(), TargetLoc) : FLT_MAX;
                        float Dist1 = Conn1 ? FVector::Dist(Conn1->GetComponentLocation(), TargetLoc) : FLT_MAX;

                        PipeConn = (Dist0 < Dist1) ? Conn0 : Conn1;

                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔍 Target junction connector %s @ (%.1f, %.1f, %.1f)"),
                            *SourceJunctionConnectorName.ToString(), TargetLoc.X, TargetLoc.Y, TargetLoc.Z);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔍 Pipe Conn0 dist=%.1f cm, Conn1 dist=%.1f cm, using %s"),
                            Dist0, Dist1, (Dist0 < Dist1) ? TEXT("Conn0") : TEXT("Conn1"));
                    }

                    if (PipeConn && TargetJunctionConn && !PipeConn->IsConnected() && !TargetJunctionConn->IsConnected())
                    {
                        float Distance = FVector::Dist(PipeConn->GetComponentLocation(), TargetJunctionConn->GetComponentLocation());
                        PipeConn->SetConnection(TargetJunctionConn);
                        TotalConnections++;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s → Junction.%s (distance=%.1f cm, verified: PipeConnected=%d, JunctionConnected=%d)"),
                            *JunctionPipe->GetName(), *TargetJunctionConn->GetName(), Distance,
                            PipeConn->IsConnected(), TargetJunctionConn->IsConnected());
                    }
                    else if (!TargetJunctionConn)
                    {
                        FailedConnections++;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Could not find junction connector '%s' on clone junction"),
                            *SourceJunctionConnectorName.ToString());
                    }
                    else if (PipeConn && PipeConn->IsConnected())
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ℹ️ Pipe connector already connected, skipping junction wiring"));
                    }
                    else if (TargetJunctionConn && TargetJunctionConn->IsConnected())
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ℹ️ Junction connector '%s' already connected, skipping"),
                            *TargetJunctionConn->GetName());
                    }
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No built junction found for pipe chain %d"), PipeChainId);
                }
            }
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring complete - %d succeeded, %d failed"),
        TotalConnections, FailedConnections);

    // Trigger pipe network rebuild for all connected pipes
    // This ensures the pipe subsystem recognizes the new connections we just made
    if (NewFactory)
    {
        UWorld* World = NewFactory->GetWorld();
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
        {
            TSet<int32> NetworksToRebuild;

            // Collect all unique network IDs from built pipes
            for (auto& PipeChainPair : ExtendService->BuiltPipesByChain)
            {
                for (auto& PipeIndexPair : PipeChainPair.Value)
                {
                    AFGBuildablePipeline* Pipe = PipeIndexPair.Value;
                    if (Pipe)
                    {
                        UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
                        UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();
                        if (Conn0 && Conn0->GetPipeNetworkID() != INDEX_NONE)
                        {
                            NetworksToRebuild.Add(Conn0->GetPipeNetworkID());
                        }
                        if (Conn1 && Conn1->GetPipeNetworkID() != INDEX_NONE)
                        {
                            NetworksToRebuild.Add(Conn1->GetPipeNetworkID());
                        }
                    }
                }
            }

            // Mark all involved networks for rebuild
            for (int32 NetworkID : NetworksToRebuild)
            {
                AFGPipeNetwork* Network = PipeSubsystem->FindPipeNetwork(NetworkID);
                if (Network)
                {
                    Network->MarkForFullRebuild();
                }
            }

            if (NetworksToRebuild.Num() > 0)
            {
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Marked %d pipe networks for rebuild"),
                    NetworksToRebuild.Num());
            }
        }
    }

    // Owned conveyor links were applied once by the merged native-phase adapter.
 // legacy conveyor respawn

    // CRITICAL: Clear tracking maps after wiring to prevent duplicate wiring attempts
    // The deferred timer fires for EVERY factory built (including junctions), so without
    // clearing these maps, subsequent calls would try to re-wire already-connected elements
    ExtendService->BuiltConveyorsByChain.Empty();
    ExtendService->BuiltDistributorsByChain.Empty();
    ExtendService->BuiltJunctionsByChain.Empty();
    ExtendService->BuiltPipesByChain.Empty();
    ExtendService->BuiltChainIsInputMap.Empty();
    ExtendService->BuiltPipeChainIsInputMap.Empty();

    // Also clear source tracking maps (used for pipe junction connections)
    ExtendService->SourceDistributorsByChain.Empty();
    ExtendService->SourceJunctionsByChain.Empty();
}

// ==================== Manifold Connections ====================
