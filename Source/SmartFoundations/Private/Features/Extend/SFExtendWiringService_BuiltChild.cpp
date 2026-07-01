// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendWiringService - Built-child registration + WireBuiltChildConnections (slice E2 unit F).
 * Moved verbatim from SFExtendService; operate on its shared registry maps via ExtendService->.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"
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
    // Issues #298, #299, #301: When Extend clones an adjacent configurable distributor, the new
    // actor is constructed from the same recipe as the source but its user-configured state
    // (sort rules for Smart/Programmable Splitters, per-input priority numbers for Priority
    // Mergers) is left at class defaults. We explicitly transfer that state from the source to
    // each clone after construction so configuration survives.
    //
    // Smart Splitter (Build_ConveyorAttachmentSplitterSmart) and Programmable Splitter
    // (Build_ConveyorAttachmentSplitterProgrammable) share AFGBuildableSplitterSmart as their
    // C++ base and the same mSortRules backing array, so one cast + SetSortRules covers both
    // families plus their Lift variants (SplitterLiftSmart / SplitterLiftProgrammable).
    //
    // Priority Merger (Build_PriorityMerger) uses AFGBuildableMergerPriority with a
    // mInputPriorities int32 array (one priority per input connection). SetInputPriorities is
    // BlueprintAuthorityOnly; Extend post-build wiring is server-authoritative, so the call is
    // safe here. The replacement array must have the same size as the clone's input count —
    // since source and clone share the same class (and therefore the same input count), the
    // source's array fits directly.
    //
    // Vanilla 3-way splitters and plain mergers have no configurable state — both casts return
    // null and we skip them silently. Runtime state (mItemToLastOutputMap, mCurrentOutputIndex,
    // mCurrentInputIndices, mCurrentInputPriorityGroupIndex, etc.) is intentionally NOT copied:
    // only user-configured state is transferred so items flow fresh through the clone.
    //
    // Implementation: the active Extend pipeline spawns clones via JSON-based topology and
    // registers them in ExtendService->JsonBuiltActors keyed by a symbolic HologramId (e.g. "distributor_0").
    // ExtendService->StoredCloneTopology->ChildHolograms links each clone HologramId to its source actor name
    // via SourceId. We walk ChildHolograms for every entry with Role=="distributor", resolve
    // source → clone via that mapping, and copy user state. The legacy ExtendService->BuiltDistributorsByChain
    // map is still consulted afterwards as a fallback for older code paths that don't populate
    // the JSON topology.

    int32 CopiedSmartCount = 0;
    int32 CopiedPriorityCount = 0;
    int32 SkippedNonConfigurable = 0;
    int32 SkippedUnresolved = 0;

    auto CopyFromSourceToClone = [&](AFGBuildable* Source, AFGBuildable* Clone, const FString& Context)
    {
        if (!IsValid(Source) || !IsValid(Clone))
        {
            return false;
        }

        // Smart / Programmable Splitter — copy sort rules (Issues #298, #299).
        // SetSortRules broadcasts OnSortRulesChanged and replicates via OnRep_SortRules,
        // so multiplayer clients see the updated filters on the clone immediately.
        if (AFGBuildableSplitterSmart* SourceSmart = Cast<AFGBuildableSplitterSmart>(Source))
        {
            if (AFGBuildableSplitterSmart* CloneSmart = Cast<AFGBuildableSplitterSmart>(Clone))
            {
                const TArray<FSplitterSortRule> SourceRules = SourceSmart->GetSortRules();
                CloneSmart->SetSortRules(SourceRules);
                CopiedSmartCount++;

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("🔧 EXTEND Config Copy: Copied %d sort rule(s) from Smart Splitter %s → %s (%s)"),
                    SourceRules.Num(), *SourceSmart->GetName(), *CloneSmart->GetName(), *Context);
                return true;
            }
        }

        // Priority Merger — copy per-input priority numbers (Issue #301). SetInputPriorities is
        // BlueprintAuthorityOnly (server-only), which Extend post-build wiring satisfies. The
        // source array has one entry per input connection and the clone shares the same class
        // (and therefore the same input count), so the array fits directly. SetInputPriorities
        // broadcasts OnInputPrioritiesChanged and replicates via OnRep_InputPriorities.
        if (AFGBuildableMergerPriority* SourcePriority = Cast<AFGBuildableMergerPriority>(Source))
        {
            if (AFGBuildableMergerPriority* ClonePriority = Cast<AFGBuildableMergerPriority>(Clone))
            {
                const TArray<int32> SourcePriorities = SourcePriority->GetInputPriorities();
                const int32 CloneInputCount = ClonePriority->GetInputConnections().Num();

                // Defensive size check — SetInputPriorities requires exact input-count match or
                // the call is silently rejected. Source and clone should always agree in practice.
                if (SourcePriorities.Num() == CloneInputCount)
                {
                    ClonePriority->SetInputPriorities(SourcePriorities);
                    CopiedPriorityCount++;

                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("🔧 EXTEND Config Copy: Copied %d input priority value(s) from Priority Merger %s → %s (%s)"),
                        SourcePriorities.Num(), *SourcePriority->GetName(), *ClonePriority->GetName(), *Context);
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                        TEXT("🔧 EXTEND Config Copy: Priority Merger input count mismatch (source=%d, clone=%d) on %s → %s — skipping"),
                        SourcePriorities.Num(), CloneInputCount, *SourcePriority->GetName(), *ClonePriority->GetName());
                }
                return true;
            }
        }

        // Unrecognized class — vanilla splitter / plain merger / something new with no
        // user-configured state to transfer. Caller counts these as "skipped non-configurable".
        return false;
    };

    // ==================== Primary path: JSON clone topology ====================
    // Active Extend pipeline: iterate ChildHolograms and resolve source/clone via HologramId
    // + SourceId. SourceId is the source actor's object name (see FSFCloneHologram), which
    // GetSourceBuildableByName looks up via world iteration.
    if (ExtendService->StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& ChildHolo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (ChildHolo.Role != TEXT("distributor"))
            {
                continue;
            }

            AFGBuildable* CloneDistributor = GetBuiltActorByCloneId(ChildHolo.HologramId);
            if (!IsValid(CloneDistributor))
            {
                UE_LOG(LogSmartExtend, Verbose,
                    TEXT("🔧 EXTEND Config Copy: No clone actor registered for HologramId=%s; skipping"),
                    *ChildHolo.HologramId);
                SkippedUnresolved++;
                continue;
            }

            AFGBuildable* SourceDistributor = GetSourceBuildableByName(ChildHolo.SourceId);
            if (!IsValid(SourceDistributor))
            {
                UE_LOG(LogSmartExtend, Verbose,
                    TEXT("🔧 EXTEND Config Copy: No source actor resolved for SourceId=%s (clone=%s); skipping"),
                    *ChildHolo.SourceId, *CloneDistributor->GetName());
                SkippedUnresolved++;
                continue;
            }

            const FString Context = FString::Printf(TEXT("HologramId=%s"), *ChildHolo.HologramId);
            if (!CopyFromSourceToClone(SourceDistributor, CloneDistributor, Context))
            {
                SkippedNonConfigurable++;
            }
        }
    }

    // ==================== Fallback path: legacy ChainId map ====================
    // Older code paths that don't populate ExtendService->StoredCloneTopology may still populate
    // ExtendService->BuiltDistributorsByChain directly from ASFConveyorAttachmentChildHologram::Construct().
    // Walk it as a safety net. ChainId layout: [0..NumInputChains) indexes InputChains;
    // [NumInputChains..NumInputChains+NumOutputChains) indexes OutputChains. Mirrors the
    // ChainId convention used elsewhere in WireBuiltChildConnections for pipe/belt chains.
    if (!ExtendService->BuiltDistributorsByChain.IsEmpty())
    {
        const FSFExtendTopology& Topology = ExtendService->GetCurrentTopology();
        const int32 NumInputChains = Topology.InputChains.Num();
        const int32 NumOutputChains = Topology.OutputChains.Num();

        for (const TPair<int32, AFGBuildable*>& ClonePair : ExtendService->BuiltDistributorsByChain)
        {
            const int32 ChainId = ClonePair.Key;
            AFGBuildable* CloneDistributor = ClonePair.Value;
            if (!IsValid(CloneDistributor))
            {
                continue;
            }

            AFGBuildable* SourceDistributor = nullptr;
            if (ChainId >= 0 && ChainId < NumInputChains)
            {
                SourceDistributor = Topology.InputChains[ChainId].Distributor.Get();
            }
            else if (ChainId >= NumInputChains && ChainId < NumInputChains + NumOutputChains)
            {
                SourceDistributor = Topology.OutputChains[ChainId - NumInputChains].Distributor.Get();
            }

            if (!IsValid(SourceDistributor))
            {
                SkippedUnresolved++;
                continue;
            }

            const FString Context = FString::Printf(TEXT("ChainId=%d"), ChainId);
            if (!CopyFromSourceToClone(SourceDistributor, CloneDistributor, Context))
            {
                SkippedNonConfigurable++;
            }
        }
    }

    const int32 TotalCopied = CopiedSmartCount + CopiedPriorityCount;
    if (TotalCopied > 0 || SkippedNonConfigurable > 0 || SkippedUnresolved > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("🔧 EXTEND Config Copy: %d Smart Splitter + %d Priority Merger clone(s) received source configuration (%d non-configurable, %d unresolved)"),
            CopiedSmartCount, CopiedPriorityCount, SkippedNonConfigurable, SkippedUnresolved);
    }

    return TotalCopied;
}

void USFExtendWiringService::WireBuiltChildConnections(AFGBuildableFactory* NewFactory)
{
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

    // ==================== PUMP POWER WIRING (Issue #288, Phase 3.8b) ====================
    // Source-linked pump → pole wiring: for each pipe_attachment clone whose source
    // pump was directly connected to an in-manifold power pole, we spawn a power
    // line from the clone pump's PowerInput to the clone pole's power connector.
    // Runs BEFORE GenerateAndExecuteWiring because that function resets
    // ExtendService->StoredCloneTopology and empties ExtendService->JsonBuiltActors at its end.
    if (ExtendService->StoredCloneTopology.IsValid())
    {
        UClass* PumpWireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
        int32 PumpsWired = 0;
        int32 PumpsSkipped = 0;

        int32 AttachmentTotal = 0;
        int32 AttachmentLinked = 0;
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (Holo.Role == TEXT("pipe_attachment"))
            {
                AttachmentTotal++;
                if (!Holo.ConnectedPowerPoleHologramId.IsEmpty()) AttachmentLinked++;
                UE_LOG(LogSmartExtend, VeryVerbose,
                    TEXT("⚡ EXTEND Phase 3.8b (#288) inventory: %s class=%s PowerPoleClone=%s"),
                    *Holo.HologramId, *Holo.BuildClass,
                    Holo.ConnectedPowerPoleHologramId.IsEmpty() ? TEXT("<none>") : *Holo.ConnectedPowerPoleHologramId);
            }
        }
        UE_LOG(LogSmartExtend, VeryVerbose,
            TEXT("⚡ EXTEND Phase 3.8b (#288) start: %d pipe_attachment(s), %d with pole linkage, JsonBuiltActors=%d"),
            AttachmentTotal, AttachmentLinked, ExtendService->JsonBuiltActors.Num());

        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (Holo.Role != TEXT("pipe_attachment")) continue;
            if (Holo.ConnectedPowerPoleHologramId.IsEmpty()) continue;  // valve or unpowered pump

            const TObjectPtr<AActor>* PumpActorPtr = ExtendService->JsonBuiltActors.Find(Holo.HologramId);
            const TObjectPtr<AActor>* PoleActorPtr = ExtendService->JsonBuiltActors.Find(Holo.ConnectedPowerPoleHologramId);
            if (!PumpActorPtr || !*PumpActorPtr || !PoleActorPtr || !*PoleActorPtr)
            {
                continue;
            }

            AFGBuildablePipelinePump* ClonePump = Cast<AFGBuildablePipelinePump>(*PumpActorPtr);
            AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(*PoleActorPtr);
            if (!ClonePump || !ClonePole) continue;  // Valve (no PowerInput) or non-pole

            UFGPowerConnectionComponent* PumpPowerConn = ClonePump->FindComponentByClass<UFGPowerConnectionComponent>();
            if (!PumpPowerConn) continue;
            if (PumpPowerConn->IsConnected()) continue;

            TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);
            if (PoleCircuitConns.Num() == 0) { ++PumpsSkipped; continue; }
            UFGCircuitConnectionComponent* PoleConn = PoleCircuitConns[0];

            if (PoleConn->GetNumConnections() >= PoleConn->GetMaxNumConnections())
            {
                ++PumpsSkipped;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                    TEXT("⚡ EXTEND Phase 3.8b (#288): clone pole %s reached capacity (%d/%d) — skipping pump %s"),
                    *ClonePole->GetName(), PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections(),
                    *ClonePump->GetName());
                continue;
            }

            if (!PumpWireClass)
            {
                ++PumpsSkipped;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Phase 3.8b (#288): Build_PowerLine_C class not loadable — skipping pump %s"), *ClonePump->GetName());
                continue;
            }

            FActorSpawnParameters WireSpawnParams;
            WireSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                PumpWireClass, ClonePump->GetActorLocation(), FRotator::ZeroRotator, WireSpawnParams);
            if (!NewWire)
            {
                ++PumpsSkipped;
                continue;
            }

            if (NewWire->Connect(PumpPowerConn, PoleConn))
            {
                SFWireDesigner::RegisterSpawnedWire(NewWire);  // [#421] designer containment (no-op outside a designer)
                ++PumpsWired;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ EXTEND Phase 3.8b (#288): wired pump %s → pole %s (pole now at %d/%d)"),
                    *ClonePump->GetName(), *ClonePole->GetName(),
                    PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections());
            }
            else
            {
                ++PumpsSkipped;
                // [NULL-WIRE GUARD] Dismantle, not Destroy: a failed Connect may still have
				// registered one side; bare Destroy leaves a dead entry in that connection's
				// SaveGame'd wire list (asserts on the owner's next dismantle / after reload).
				IFGDismantleInterface::Execute_Dismantle(NewWire);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Phase 3.8b (#288): Wire->Connect() failed for pump %s → pole %s"),
                    *ClonePump->GetName(), *ClonePole->GetName());
            }
        }

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ EXTEND Phase 3.8b (#288): pump power wiring complete — wired %d, skipped %d"),
            PumpsWired, PumpsSkipped);
    }
    else
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ EXTEND Phase 3.8b (#288): StoredCloneTopology invalid at pre-wire checkpoint — skipping pump power wiring"));
    }

    // ==================== PHASE 5/6: JSON-Based Wiring ====================
    // Try JSON-based wiring first (for JSON-spawned holograms)
    // This runs regardless of whether chain-based wiring has data
    // NOTE: GenerateAndExecuteWiring resets ExtendService->StoredCloneTopology and ExtendService->JsonBuiltActors
    // at its end — phases 3.8a/3.8b above must run before ExtendService call.
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

    // ==================== CONVEYOR CHAIN WIRING ====================
    // HYBRID APPROACH:
    // - Conveyor-to-conveyor connections are handled by snapped connections at build time
    //   (ExtendService creates unified chains)
    // - Endpoint connections (distributor↔first conveyor, last conveyor↔factory) are done
    //   post-build because snapped connections don't work for belt→distributor
    //
    // The snapped connections are set in SFConveyorBeltHologram::Construct() BEFORE
    // Super::Construct() is called, pointing to already-built conveyors.
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring ENDPOINT connections only (conveyor↔conveyor handled by snapped connections)"));

    for (auto& ChainPair : ExtendService->BuiltConveyorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ConveyorsByIndex = ChainPair.Value;
        bool bIsInputChain = ExtendService->BuiltChainIsInputMap.FindRef(ChainId);

        // Sort indices to get conveyors in order
        TArray<int32> SortedIndices;
        ConveyorsByIndex.GetKeys(SortedIndices);
        SortedIndices.Sort();

        // Build ordered array of conveyors
        TArray<AFGBuildableConveyorBase*> OrderedConveyors;
        for (int32 Index : SortedIndices)
        {
            if (AFGBuildableConveyorBase* Conveyor = ConveyorsByIndex.FindRef(Index))
            {
                OrderedConveyors.Add(Conveyor);
            }
        }

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Processing conveyor chain %d with %d conveyors (%s), indices: %s"),
            ChainId, OrderedConveyors.Num(), bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"),
            *FString::JoinBy(SortedIndices, TEXT(", "), [](int32 i) { return FString::FromInt(i); }));

        // Connect consecutive conveyors in the chain
        // Note: Snapped connections create unified chains for lifts, but belts still need post-build wiring
        // to establish the actual connection (snapped connections don't work for belt-to-belt)
        for (int32 i = 0; i < OrderedConveyors.Num() - 1; i++)
        {
            AFGBuildableConveyorBase* Current = OrderedConveyors[i];
            AFGBuildableConveyorBase* Next = OrderedConveyors[i + 1];

            if (!Current || !Next) continue;

            // Connect Current.Connection1 (output) to Next.Connection0 (input)
            UFGFactoryConnectionComponent* CurrentConn1 = Current->GetConnection1();
            UFGFactoryConnectionComponent* NextConn0 = Next->GetConnection0();

            if (CurrentConn1 && NextConn0 && !CurrentConn1->IsConnected() && !NextConn0->IsConnected())
            {
                CurrentConn1->SetConnection(NextConn0);
                TotalConnections++;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s.Conn1 → %s.Conn0"),
                    *Current->GetName(), *Next->GetName());
            }
        }

        // === CHAIN INDEX CONVENTION (after INPUT chain reversal fix) ===
        //
        // Both INPUT and OUTPUT chains now use the SAME index convention:
        //   [0] = SOURCE end (where items ENTER the chain)
        //   [N-1] = DESTINATION end (where items EXIT the chain)
        //
        // For OUTPUT chains: Source = Factory, Destination = Distributor (merger)
        //   - [0].Conn0 ← Factory OUTPUT
        //   - [N-1].Conn1 → Distributor INPUT
        //
        // For INPUT chains: Source = Distributor (splitter), Destination = Factory
        //   - [0].Conn0 ← Distributor OUTPUT
        //   - [N-1].Conn1 → Factory INPUT
        //
        // Items always flow: Conn0 → Conn1 through each conveyor

        // Connect conveyor to factory
        if (OrderedConveyors.Num() > 0)
        {
            // INPUT: last conveyor (index N-1) connects to factory (destination)
            // OUTPUT: first conveyor (index 0) connects to factory (source)
            AFGBuildableConveyorBase* FactoryConveyor = bIsInputChain ? OrderedConveyors.Last() : OrderedConveyors[0];

            // Find matching factory connector by proximity
            TArray<UFGFactoryConnectionComponent*> FactoryConnectors;
            NewFactory->GetComponents<UFGFactoryConnectionComponent>(FactoryConnectors);

            // INPUT: conveyor's Conn1 (output) → Factory INPUT (items enter factory)
            // OUTPUT: conveyor's Conn0 (input) ← Factory OUTPUT (items leave factory)
            UFGFactoryConnectionComponent* ConveyorConn = bIsInputChain ? FactoryConveyor->GetConnection1() : FactoryConveyor->GetConnection0();
            EFactoryConnectionDirection NeededDirection = bIsInputChain ? EFactoryConnectionDirection::FCD_INPUT : EFactoryConnectionDirection::FCD_OUTPUT;

            if (ConveyorConn && !ConveyorConn->IsConnected())
            {
                UFGFactoryConnectionComponent* BestFactoryConn = nullptr;
                float BestDistance = FLT_MAX;

                for (UFGFactoryConnectionComponent* FactoryConn : FactoryConnectors)
                {
                    if (!FactoryConn || FactoryConn->IsConnected()) continue;
                    if (FactoryConn->GetDirection() != NeededDirection) continue;

                    float Distance = FVector::Dist(ConveyorConn->GetComponentLocation(), FactoryConn->GetComponentLocation());
                    if (Distance < BestDistance && Distance <= 350.0f)  // 350cm to handle edge cases at exactly 300cm
                    {
                        BestDistance = Distance;
                        BestFactoryConn = FactoryConn;
                    }
                }

                if (BestFactoryConn)
                {
                    ConveyorConn->SetConnection(BestFactoryConn);
                    TotalConnections++;
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s.%s → Factory.%s (distance=%.1f cm)"),
                        *FactoryConveyor->GetName(), bIsInputChain ? TEXT("Conn1") : TEXT("Conn0"),
                        *BestFactoryConn->GetName(), BestDistance);
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No factory %s connector found within 300cm of %s.%s"),
                        bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"),
                        *FactoryConveyor->GetName(), bIsInputChain ? TEXT("Conn1") : TEXT("Conn0"));
                }
            }
        }

        // Connect conveyor to distributor
        // INPUT: first conveyor (index 0) connects to distributor (source)
        // OUTPUT: last conveyor (index N-1) connects to distributor (destination)
        if (OrderedConveyors.Num() > 0)
        {
            AFGBuildableConveyorBase* DistributorConveyor = bIsInputChain ? OrderedConveyors[0] : OrderedConveyors.Last();

            // Get the built distributor for ExtendService chain
            AFGBuildable** DistPtr = ExtendService->BuiltDistributorsByChain.Find(ChainId);
            if (DistPtr && *DistPtr)
            {
                AFGBuildable* BuiltDistributor = *DistPtr;

                // Find matching distributor connector by proximity
                TArray<UFGFactoryConnectionComponent*> DistConnectors;
                BuiltDistributor->GetComponents<UFGFactoryConnectionComponent>(DistConnectors);

                // INPUT: conveyor's Conn0 (input) ← Distributor OUTPUT (items leave distributor/splitter)
                // OUTPUT: conveyor's Conn1 (output) → Distributor INPUT (items enter distributor/merger)
                UFGFactoryConnectionComponent* ConveyorConn = bIsInputChain ? DistributorConveyor->GetConnection0() : DistributorConveyor->GetConnection1();
                EFactoryConnectionDirection NeededDirection = bIsInputChain ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT;

                if (ConveyorConn && !ConveyorConn->IsConnected())
                {
                    UFGFactoryConnectionComponent* BestDistConn = nullptr;
                    float BestDistance = FLT_MAX;

                    for (UFGFactoryConnectionComponent* DistConn : DistConnectors)
                    {
                        if (!DistConn || DistConn->IsConnected()) continue;
                        if (DistConn->GetDirection() != NeededDirection) continue;

                        float Distance = FVector::Dist(ConveyorConn->GetComponentLocation(), DistConn->GetComponentLocation());
                        if (Distance < BestDistance && Distance <= 350.0f)  // 350cm to handle edge cases at exactly 300cm
                        {
                            BestDistance = Distance;
                            BestDistConn = DistConn;
                        }
                    }

                    if (BestDistConn)
                    {
                        ConveyorConn->SetConnection(BestDistConn);
                        TotalConnections++;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s.%s → Distributor.%s (distance=%.1f cm)"),
                            *DistributorConveyor->GetName(), bIsInputChain ? TEXT("Conn0") : TEXT("Conn1"),
                            *BestDistConn->GetName(), BestDistance);
                    }
                    else
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No distributor %s connector found within 300cm of %s.%s"),
                            bIsInputChain ? TEXT("OUTPUT") : TEXT("INPUT"),
                            *DistributorConveyor->GetName(), bIsInputChain ? TEXT("Conn0") : TEXT("Conn1"));
                    }
                }
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No built distributor found for chain %d"), ChainId);
            }
        }
    }

    // (Phases 3.8a and 3.8b formerly lived here — moved to before GenerateAndExecuteWiring above
    //  so that ExtendService->StoredCloneTopology and ExtendService->JsonBuiltActors are still populated when they run.)

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

    // ============================================================
    // CHAIN FIX: Delete built belts and respawn using SOURCE topology
    // ============================================================
    // PROBLEM: Belts built via holograms create fragmented chain actors because
    // each belt creates its own chain at BeginPlay before connections exist.
    //
    // SOLUTION: Delete the fragmented belts and respawn them in FORWARD order
    // using the SOURCE topology data (which has correct lift configurations,
    // spline data, etc.) plus the offset to the clone position.

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Deleting fragmented belts and respawning from source topology..."));

    // Log topology info for debugging
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Topology has %d input chains, %d output chains"),
        ExtendService->GetCurrentTopology().InputChains.Num(), ExtendService->GetCurrentTopology().OutputChains.Num());
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: BuiltConveyorsByChain has %d chains:"), ExtendService->BuiltConveyorsByChain.Num());
    for (auto& DebugPair : ExtendService->BuiltConveyorsByChain)
    {
        bool bDebugIsInput = ExtendService->BuiltChainIsInputMap.FindRef(DebugPair.Key);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix:   ChainId=%d, IsInput=%d, ConveyorCount=%d"),
            DebugPair.Key, bDebugIsInput ? 1 : 0, DebugPair.Value.Num());
    }

    // Calculate offset from source to clone factory
    FVector CloneOffset = FVector::ZeroVector;
    if (ExtendService->GetCurrentTopology().SourceBuilding.IsValid() && NewFactory)
    {
        CloneOffset = NewFactory->GetActorLocation() - ExtendService->GetCurrentTopology().SourceBuilding->GetActorLocation();
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Clone offset = %s"), *CloneOffset.ToString());
    }
    else
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Cannot calculate offset - missing source or new factory!"));
    }

    // Process each conveyor chain
    for (auto& ChainPair : ExtendService->BuiltConveyorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ConveyorMap = ChainPair.Value;
        bool bIsInput = ExtendService->BuiltChainIsInputMap.FindRef(ChainId);

        // Get the SOURCE chain from cached topology
        const FSFConnectionChainNode* SourceChain = nullptr;
        if (bIsInput && ChainId < ExtendService->GetCurrentTopology().InputChains.Num())
        {
            SourceChain = &ExtendService->GetCurrentTopology().InputChains[ChainId];
        }
        else if (!bIsInput)
        {
            // Output chains use ChainId offset by input chain count
            int32 OutputChainIndex = ChainId - ExtendService->GetCurrentTopology().InputChains.Num();
            if (OutputChainIndex >= 0 && OutputChainIndex < ExtendService->GetCurrentTopology().OutputChains.Num())
            {
                SourceChain = &ExtendService->GetCurrentTopology().OutputChains[OutputChainIndex];
            }
        }

        if (!SourceChain || SourceChain->Conveyors.Num() == 0)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Chain %d: No source chain found in topology, skipping respawn"),
                ChainId);
            continue;
        }

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d (%s): Found %d source conveyors, %d built conveyors"),
            ChainId, bIsInput ? TEXT("INPUT") : TEXT("OUTPUT"),
            SourceChain->Conveyors.Num(), ConveyorMap.Num());

        // Collect endpoint connections before deletion (for first/last belt connections)
        // These are the external connections at each end of the chain
        TWeakObjectPtr<UFGFactoryConnectionComponent> FactoryEndConn;   // At topology[0] - factory side
        TWeakObjectPtr<UFGFactoryConnectionComponent> DistributorEndConn; // At topology[N-1] - distributor side

        // Sort indices to find first and last
        TArray<int32> SortedIndices;
        ConveyorMap.GetKeys(SortedIndices);
        SortedIndices.Sort();

        if (SortedIndices.Num() > 0)
        {
            // Get factory-end connection DIRECTLY from clone factory (not from belt)
            // Belt connections may not have been established due to fragmented chains
            if (NewFactory)
            {
                // Get the connector that should connect to the belt chain
                // For INPUT chains: factory INPUT receives from belt chain
                // For OUTPUT chains: factory OUTPUT feeds into belt chain
                TArray<UFGFactoryConnectionComponent*> FactoryConnections;
                NewFactory->GetComponents<UFGFactoryConnectionComponent>(FactoryConnections);

                // Find the correct factory connector based on chain type and source topology
                // The source chain has a SourceConnector that we need to match by name
                FName SourceConnectorName = NAME_None;
                if (SourceChain && SourceChain->SourceConnector.IsValid())
                {
                    SourceConnectorName = SourceChain->SourceConnector->GetFName();
                }

                for (UFGFactoryConnectionComponent* FactoryConn : FactoryConnections)
                {
                    if (!FactoryConn) continue;

                    // For INPUT chains: need INPUT connector (factory receives FROM belt chain)
                    // For OUTPUT chains: need OUTPUT connector (factory feeds INTO belt chain)
                    bool bWantInput = bIsInput;
                    bool bIsInput_Conn = (FactoryConn->GetDirection() == EFactoryConnectionDirection::FCD_INPUT);

                    // Match by name if we have source connector name, otherwise match by direction
                    // NOTE: Don't check IsConnected() - factory may be connected to old belt we're about to delete
                    bool bNameMatch = (SourceConnectorName == NAME_None) || (FactoryConn->GetFName() == SourceConnectorName);

                    if (bWantInput == bIsInput_Conn && bNameMatch)
                    {
                        FactoryEndConn = FactoryConn;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Got factory connector %s from clone %s (connected=%d)"),
            ChainId, *FactoryConn->GetName(), *NewFactory->GetName(), FactoryConn->IsConnected());
                        break;
                    }
                }
            }

            // Get distributor-end connection DIRECTLY from clone distributor (not from belt)
            // Belt connections may not have been established due to fragmented chains
            AFGBuildable** CloneDistributorPtr = ExtendService->BuiltDistributorsByChain.Find(ChainId);
            if (CloneDistributorPtr && *CloneDistributorPtr)
            {
                AFGBuildable* CloneDistributor = *CloneDistributorPtr;

                // Get the connector that should connect to the belt chain
                // For INPUT chains (splitter): use an OUTPUT connector to feed into belts
                // For OUTPUT chains (merger): use an INPUT connector to receive from belts
                TArray<UFGFactoryConnectionComponent*> Connections;
                CloneDistributor->GetComponents<UFGFactoryConnectionComponent>(Connections);

                for (UFGFactoryConnectionComponent* DistConn : Connections)
                {
                    if (!DistConn) continue;

                    // For INPUT chains: need OUTPUT connector (splitter feeds INTO belt chain)
                    // For OUTPUT chains: need INPUT connector (merger receives FROM belt chain)
                    bool bWantOutput = bIsInput;
                    bool bIsOutput = (DistConn->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT);

                    if (bWantOutput == bIsOutput && !DistConn->IsConnected())
                    {
                        DistributorEndConn = DistConn;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Got distributor connector %s from clone %s"),
                            ChainId, *DistConn->GetName(), *CloneDistributor->GetName());
                        break;
                    }
                }
            }
        }

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Endpoints - FactoryEnd=%s, DistributorEnd=%s"),
            ChainId,
            FactoryEndConn.IsValid() ? *FactoryEndConn->GetName() : TEXT("NULL"),
            DistributorEndConn.IsValid() ? *DistributorEndConn->GetName() : TEXT("NULL"));

        // Delete all fragmented conveyors (belts and lifts)
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Deleting %d fragmented conveyors..."),
            ChainId, SortedIndices.Num());

        for (int32 Index : SortedIndices)
        {
            AFGBuildableConveyorBase* Conveyor = ConveyorMap[Index];
            if (Conveyor)
            {
                // Clear connections first
                UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
                UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();
                if (Conn0) Conn0->ClearConnection();
                if (Conn1) Conn1->ClearConnection();
                Conveyor->Destroy();
            }
        }
        ConveyorMap.Empty();

        // Respawn conveyors using SOURCE topology data
        // For INPUT chains: items flow Splitter → Factory, so spawn in REVERSE order (N-1 → 0)
        //   because topology[0] is at factory, topology[N-1] is at splitter
        // For OUTPUT chains: items flow Factory → Merger, so spawn in FORWARD order (0 → N)
        //   because topology[0] is at factory, topology[N-1] is at merger
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain %d (%s): Respawning %d conveyors (order: %s)..."),
            ChainId, bIsInput ? TEXT("INPUT") : TEXT("OUTPUT"), SourceChain->Conveyors.Num(),
            bIsInput ? TEXT("REVERSE N→0") : TEXT("FORWARD 0→N"));

        UWorld* World = GetWorld();
        if (!World) continue;

        AFGBuildableConveyorBase* PreviousConveyor = nullptr;
        int32 NumSourceConveyors = SourceChain->Conveyors.Num();

        for (int32 iter = 0; iter < NumSourceConveyors; iter++)
        {
            // For INPUT chains, iterate in reverse; for OUTPUT chains, iterate forward
            int32 i = bIsInput ? (NumSourceConveyors - 1 - iter) : iter;
            AFGBuildableConveyorBase* SourceConveyor = SourceChain->Conveyors[i].Get();
            if (!SourceConveyor)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND   [%d] Source conveyor is null, skipping"), i);
                continue;
            }

            AFGBuildableConveyorBase* NewConveyor = nullptr;

            // Check if source is a lift or belt
            AFGBuildableConveyorLift* SourceLift = Cast<AFGBuildableConveyorLift>(SourceConveyor);
            AFGBuildableConveyorBelt* SourceBelt = Cast<AFGBuildableConveyorBelt>(SourceConveyor);

            if (SourceLift)
            {
                // Spawn lift at clone location using same approach as belts
                FVector SourceLocation = SourceLift->GetActorLocation();
                FVector CloneLocation = SourceLocation + CloneOffset;
                FTransform CloneTransform = SourceLift->GetActorTransform();
                CloneTransform.SetLocation(CloneLocation);

                // mTopTransform is LOCAL (relative to actor), so it stays the same
                FTransform SourceTopTransform = SourceLift->GetTopTransform();

                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND   [%d] Spawning lift:"), i);
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Source: %s at %s, Height=%.1f"),
                    *SourceLift->GetName(), *SourceLocation.ToString(), SourceLift->GetHeight());
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Clone location: %s"), *CloneLocation.ToString());
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Source TopTransform: %s (LOCAL)"), *SourceTopTransform.ToString());

                // Spawn at clone transform
                AFGBuildableConveyorLift* NewLift = World->SpawnActorDeferred<AFGBuildableConveyorLift>(
                    SourceLift->GetClass(),
                    CloneTransform,
                    nullptr,
                    nullptr,
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

                if (NewLift)
                {
                    // Set mTopTransform via reflection BEFORE FinishSpawning (it's private)
                    // mTopTransform is LOCAL - same relative offset as source
                    FProperty* TopTransformProp = NewLift->GetClass()->FindPropertyByName(TEXT("mTopTransform"));
                    if (TopTransformProp)
                    {
                        FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(NewLift);
                        if (TopTransformPtr)
                        {
                            *TopTransformPtr = SourceTopTransform;
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Set mTopTransform via reflection"));
                        }
                    }

                    // Set snapped connections BEFORE FinishSpawning so chain actor registers correctly
                    // mSnappedConnectionComponents[0] = Conn0 partner, [1] = Conn1 partner
                    FProperty* SnappedConnProp = NewLift->GetClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
                    if (SnappedConnProp)
                    {
                        void* ArrayPtr = SnappedConnProp->ContainerPtrToValuePtr<void>(NewLift);
                        FArrayProperty* ArrayProp = CastField<FArrayProperty>(SnappedConnProp);
                        if (ArrayPtr && ArrayProp)
                        {
                            FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

                            // Resize array to 2 if needed (it starts empty)
                            if (ArrayHelper.Num() < 2)
                            {
                                ArrayHelper.Resize(2);
                            }

                            UFGFactoryConnectionComponent** Conn0Ptr = (UFGFactoryConnectionComponent**)ArrayHelper.GetRawPtr(0);

                            // Conn0 partner (previous conveyor's Conn1 or endpoint)
                            if (PreviousConveyor)
                            {
                                *Conn0Ptr = PreviousConveyor->GetConnection1();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to previous Conn1"));
                            }
                            else if (bIsInput && DistributorEndConn.IsValid())
                            {
                                *Conn0Ptr = DistributorEndConn.Get();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to distributor endpoint"));
                            }
                            else if (!bIsInput && FactoryEndConn.IsValid())
                            {
                                *Conn0Ptr = FactoryEndConn.Get();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to factory endpoint"));
                            }
                        }
                    }

                    UGameplayStatics::FinishSpawningActor(NewLift, CloneTransform);
                    NewLift->SetupConnections();

                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND   [%d] Spawned lift %s at %s, Height=%.1f"),
                        i, *NewLift->GetName(), *NewLift->GetActorLocation().ToString(), NewLift->GetHeight());

                    // Also set connections via SetConnection for immediate effect
                    if (PreviousConveyor)
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        UFGFactoryConnectionComponent* PrevConn1 = PreviousConveyor->GetConnection1();
                        if (OurConn0 && PrevConn1)
                        {
                            OurConn0->SetConnection(PrevConn1);
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to previous Conn1"));
                        }
                    }
                    else if (bIsInput && DistributorEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(DistributorEndConn.Get());
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to distributor endpoint %s"),
                                *DistributorEndConn->GetName());
                        }
                    }
                    else if (!bIsInput && FactoryEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(FactoryEndConn.Get());
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to factory endpoint %s"),
                                *FactoryEndConn->GetName());
                        }
                    }

                    NewConveyor = NewLift;
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND   [%d] SpawnActorDeferred FAILED"), i);
                }
            }
            else if (SourceBelt)
            {
                // Get source belt's spline data and offset to clone position
                const TArray<FSplinePointData>& SourceSplineData = SourceBelt->GetSplinePointData();
                TArray<FSplinePointData> CloneSplineData;
                CloneSplineData.Reserve(SourceSplineData.Num());
                for (const FSplinePointData& Point : SourceSplineData)
                {
                    FSplinePointData ClonePoint = Point;
                    ClonePoint.Location += CloneOffset;
                    CloneSplineData.Add(ClonePoint);
                }

                FVector SourceLocation = SourceBelt->GetActorLocation();
                FVector CloneLocation = SourceLocation + CloneOffset;

                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND   [%d] Respawning belt:"), i);
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Source: %s at %s"),
                    *SourceBelt->GetName(), *SourceLocation.ToString());

                // Spawn belt at source transform, then use Respline with offset spline data
                AFGBuildableConveyorBelt* NewBelt = World->SpawnActorDeferred<AFGBuildableConveyorBelt>(
                    SourceBelt->GetClass(),
                    SourceBelt->GetActorTransform(),
                    nullptr,
                    nullptr,
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

                if (NewBelt)
                {
                    // Set snapped connections BEFORE FinishSpawning so chain actor registers correctly
                    // mSnappedConnectionComponents[0] = Conn0 partner, [1] = Conn1 partner
                    FProperty* SnappedConnProp = NewBelt->GetClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
                    if (SnappedConnProp)
                    {
                        void* ArrayPtr = SnappedConnProp->ContainerPtrToValuePtr<void>(NewBelt);
                        FArrayProperty* ArrayProp = CastField<FArrayProperty>(SnappedConnProp);
                        if (ArrayPtr && ArrayProp)
                        {
                            FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

                            // Resize array to 2 if needed (it starts empty)
                            if (ArrayHelper.Num() < 2)
                            {
                                ArrayHelper.Resize(2);
                            }

                            UFGFactoryConnectionComponent** Conn0Ptr = (UFGFactoryConnectionComponent**)ArrayHelper.GetRawPtr(0);

                            if (PreviousConveyor)
                            {
                                *Conn0Ptr = PreviousConveyor->GetConnection1();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to previous Conn1"));
                            }
                            else if (bIsInput && DistributorEndConn.IsValid())
                            {
                                *Conn0Ptr = DistributorEndConn.Get();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to distributor endpoint"));
                            }
                            else if (!bIsInput && FactoryEndConn.IsValid())
                            {
                                *Conn0Ptr = FactoryEndConn.Get();
                                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to factory endpoint"));
                            }
                        }
                    }

                    UGameplayStatics::FinishSpawningActor(NewBelt, SourceBelt->GetActorTransform());

                    // Apply offset spline data using Respline
                    if (CloneSplineData.Num() >= 2)
                    {
                        AFGBuildableConveyorBelt* ResplinedBelt = AFGBuildableConveyorBelt::Respline(NewBelt, CloneSplineData);
                        if (ResplinedBelt)
                        {
                            NewBelt = ResplinedBelt;
                        }
                    }

                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND   [%d] Respawned belt %s -> %s at %s"),
                        i, *SourceBelt->GetName(), *NewBelt->GetName(), *NewBelt->GetActorLocation().ToString());

                    // Also set connections via SetConnection for immediate effect
                    if (PreviousConveyor)
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        UFGFactoryConnectionComponent* PrevConn1 = PreviousConveyor->GetConnection1();
                        if (OurConn0 && PrevConn1)
                        {
                            OurConn0->SetConnection(PrevConn1);
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to previous Conn1"));
                        }
                    }
                    else if (bIsInput && DistributorEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(DistributorEndConn.Get());
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to distributor endpoint %s"),
                                *DistributorEndConn->GetName());
                        }
                    }
                    else if (!bIsInput && FactoryEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(FactoryEndConn.Get());
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to factory endpoint %s"),
                                *FactoryEndConn->GetName());
                        }
                    }

                    NewBelt->OnBuildEffectFinished();

                    // NOTE: Don't call AddConveyor here - the belt chain is still being built.
                    // CreateChainActors() in SFWiringManifest will handle chain creation
                    // AFTER all belts are spawned and connected via Respline.

                    NewConveyor = NewBelt;
                }
            }

            if (NewConveyor)
            {
                // Register the new conveyor
                ConveyorMap.Add(i, NewConveyor);

                // Connect last spawned conveyor's Conn1 to endpoint
                // For INPUT chains (reverse): last spawned is at factory end → connect to FactoryEndConn
                // For OUTPUT chains (forward): last spawned is at distributor end → connect to DistributorEndConn
                if (iter == NumSourceConveyors - 1)
                {
                    UFGFactoryConnectionComponent* OurConn1 = NewConveyor->GetConnection1();
                    if (bIsInput && FactoryEndConn.IsValid() && OurConn1)
                    {
                        OurConn1->SetConnection(FactoryEndConn.Get());
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn1 to factory endpoint %s"),
                            *FactoryEndConn->GetName());
                    }
                    else if (!bIsInput && DistributorEndConn.IsValid() && OurConn1)
                    {
                        OurConn1->SetConnection(DistributorEndConn.Get());
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn1 to distributor endpoint %s"),
                            *DistributorEndConn->GetName());
                    }
                }

                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND   [%d] Chain=%s"),
                    i,
                    NewConveyor->GetConveyorChainActor() ? *NewConveyor->GetConveyorChainActor()->GetName() : TEXT("NULL"));

                PreviousConveyor = NewConveyor;
            }
        }

        // Log final chain state
        TSet<AFGConveyorChainActor*> FinalChains;
        for (auto& ConveyorPair : ConveyorMap)
        {
            if (ConveyorPair.Value)
            {
                AFGConveyorChainActor* Chain = ConveyorPair.Value->GetConveyorChainActor();
                if (Chain)
                {
                    FinalChains.Add(Chain);
                }
            }
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Chain %d: Respawn complete - %d conveyors now have %d chain actor(s)"),
            ChainId, ConveyorMap.Num(), FinalChains.Num());
    }

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

