// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendWiringService - JSON-based post-build wiring (slice E2 unit I).
 * GenerateAndExecuteWiring + JSON built-actor registry moved verbatim from SFExtendService;
 * operate on its shared state via ExtendService->. Subsystem uses this service own back-ref.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/SFExtendPassthroughLinks.h"
#include "SFScaledExtendGrid.h"
#include "Features/Extend/SFExtendControlFrame.h"
#include "Features/Extend/SFRestoreGrid.h"

int32 USFExtendWiringService::GenerateAndExecuteWiring(AFGBuildableFactory* NewFactory)
{
    if (!ExtendService || !ExtendService->IsExecutingOwnedConveyorWiring()) return 0;

    const FSFCloneTopology* WiringTopology = ExtendService->GetCapturedWiringTopology();
    if (!WiringTopology) WiringTopology = ExtendService->StoredCloneTopology.Get();
    const auto* CapturedActors = ExtendService->GetCapturedWiringActors();
    const auto& WiringActors = CapturedActors ? *CapturedActors : ExtendService->JsonBuiltActors;
    const auto* CapturedScaled = ExtendService->GetCapturedScaledWiringPlans();
    const auto& WiringScaledPlans = CapturedScaled ? *CapturedScaled : ExtendService->ScaledExtendClones;
    ExtendService->bRestoredScaledWiringDeferred = false;

    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: GenerateAndExecuteWiring called with null factory"));
        return 0;
    }

    // Check if we have stored clone topology from JSON spawning
    if (!(WiringTopology != nullptr) || WiringTopology->ChildHolograms.Num() == 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: No stored clone topology - skipping JSON wiring"));
        return 0;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Generating wiring manifest from %d child holograms, %d registered built actors"),
        WiringTopology->ChildHolograms.Num(), WiringActors.Num());

    // Build clone_id -> buildable mapping from WiringActors (populated during Construct())
    // Holograms are destroyed after Construct(), so we can't use ExtendService->JsonSpawnedHolograms here
    TMap<FString, AActor*> CloneIdToBuildable;

    // Add parent factory
    CloneIdToBuildable.Add(TEXT("parent"), NewFactory);

    // Copy all registered built actors
    for (const auto& Pair : WiringActors)
    {
        const FString& CloneId = Pair.Key;
        AActor* BuiltActor = Pair.Value;

        if (IsValid(BuiltActor))
        {
            CloneIdToBuildable.Add(CloneId, BuiltActor);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Using registered actor %s -> %s"),
                *CloneId, *BuiltActor->GetName());
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: Registered actor for %s is no longer valid"), *CloneId);
        }
    }

    // Pre-resolve "source:" targets from topology into CloneIdToBuildable.
    // Clone 1's lane segments have "source:ActorName" targets pointing to the SOURCE
    // building's distributors (real world actors, not clones). Without ExtendService, Generate()
    // can't find them in CloneIdToBuildable and skips the connection.
    if ((WiringTopology != nullptr))
    {
        for (const FSFCloneHologram& Holo : WiringTopology->ChildHolograms)
        {
            auto ResolveSourceTarget = [&](const FString& Target)
            {
                if (Target.StartsWith(TEXT("source:")) && !CloneIdToBuildable.Contains(Target))
                {
                    FString SourceActorName = Target.Mid(7);
                    AFGBuildable* SourceBuildable = GetSourceBuildableByName(SourceActorName);
                    if (SourceBuildable)
                    {
                        CloneIdToBuildable.Add(Target, SourceBuildable);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Pre-resolved source target '%s' → %s"),
                            *Target, *SourceBuildable->GetName());
                    }
                }
            };

            ResolveSourceTarget(Holo.PowerFrom.Target);
            ResolveSourceTarget(Holo.PowerTo.Target);
            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny0.Target);
            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny1.Target);
        }
    }

    // Factory IDs are registered at ConfigureActor/Construct. A missing ID must
    // remain missing: searching existing world factories can mutate another build.
    ExtendService->bRestoredScaledWiringRetryScheduled = false;
    ExtendService->RestoredScaledWiringRetryAttempts = 0;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Mapped %d buildables (including parent + source targets)"), CloneIdToBuildable.Num());

    // Generate wiring manifest
    FSFWiringManifest WiringManifest = FSFWiringManifest::Generate(
        *WiringTopology,
        CloneIdToBuildable,
        NewFactory);

    // Resolve source buildable targets for lane segments connecting to source junctions
    // These have bIsSourceBuildable=true and need resolution via GetSourceBuildableByName
    for (FSFWiringConnection& PipeConn : WiringManifest.PipeConnections)
    {
        if (PipeConn.Target.bIsSourceBuildable && !PipeConn.Target.ResolvedActor)
        {
            // Target.CloneId is "source:ActorName.ConnectorName" format
            FString SourceRef = PipeConn.Target.CloneId;
            if (SourceRef.StartsWith(TEXT("source:")))
            {
                FString SourceActorName = SourceRef.Mid(7);  // Remove "source:" prefix
                AFGBuildable* SourceBuildable = GetSourceBuildableByName(SourceActorName);
                if (SourceBuildable)
                {
                    PipeConn.Target.ResolvedActor = SourceBuildable;
                    PipeConn.Target.ActorName = SourceBuildable->GetName();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Resolved source buildable '%s' -> %s"),
                        *SourceActorName, *SourceBuildable->GetName());
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: Failed to resolve source buildable '%s'"),
                        *SourceActorName);
                }
            }
        }
    }

    // Save manifest for debugging
    FString LogDir = FPaths::ProjectLogDir();
    WiringManifest.SaveToFile(LogDir / TEXT("WiringManifest.json"));

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Generated manifest - %d belt connections, %d pipe connections"),
        WiringManifest.BeltConnections.Num(), WiringManifest.PipeConnections.Num());

    // Execute all wiring in single tick
    const bool bOwnedConveyors = ExtendService->IsExecutingOwnedConveyorWiring();
    // The owner has already applied and checked the merged conveyor plan. Keep
    // pipe/power behavior here, but never run a second conveyor writer/rebuild.
    if (bOwnedConveyors) WiringManifest.BeltConnections.Reset();
    int32 WiredCount = WiringManifest.ExecuteWiring(GetWorld());

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Wiring complete - %d connections established"), WiredCount);

    // Create chain actors for wired belts (prevents crash in Factory_UpdateRadioactivity)
    // Pass WiringActors to include lane segments that were wired in ConfigureComponents
    // JsonBuiltActors stores TObjectPtr (GC-safe); build a transient raw view for the wiring-manifest API.
    TMap<FString, AActor*> BuiltActorsRaw;
    BuiltActorsRaw.Reserve(WiringActors.Num());
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : WiringActors)
    {
        BuiltActorsRaw.Add(Pair.Key, Pair.Value);
    }
    const int32 ChainsCreated = 0; // Native migration is observed by the retained operation.

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Chain actors created - %d chains"), ChainsCreated);

    // Rebuild pipe networks to ensure fluid flow between source and clone manifolds (same raw view)
    int32 NetworksRebuilt = WiringManifest.RebuildPipeNetworks(GetWorld(), BuiltActorsRaw);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Pipe networks rebuilt - %d networks"), NetworksRebuilt);

    // ==================== Lift ↔ Passthrough Linking (Issue #260) ====================
    // Conveyor lift attachments are established by native construction using
    // exact clone roles and verified by the retained operation. No post-build
    // nearby-hole assignment or synthetic OnRep belongs here.

    // Only captured floor holes in this construction transaction may receive pipe links.
    SFExtendPassthroughLinks::Apply(*WiringTopology, BuiltActorsRaw);

    UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);

    // ==================== Scaled Extend: Additional Clone Wiring (Issue #265) ====================
    // Process wiring for each additional clone set beyond clone 1
    int32 ScaledExtendWiredCount = 0;
    // [EXTEND-MP] Display level while MP Extend validates (Verbose is stripped on the dedi).
    UE_LOG(LogSmartFoundations, Verbose,
        TEXT("[EXTEND-MP] Scaled clone wiring: %d clone set(s), jsonBuilt=%d, sourceTarget=%s."),
        WiringScaledPlans.Num(), WiringActors.Num(),
        *GetNameSafe(ExtendService->CurrentExtendTarget.Get()));
    for (int32 CloneIdx = 0; CloneIdx < WiringScaledPlans.Num(); CloneIdx++)
    {
        const FSFScaledExtendClone& Clone = WiringScaledPlans[CloneIdx];
        if (!Clone.CloneTopology.IsValid() || Clone.CloneTopology->ChildHolograms.Num() == 0)
        {
            UE_LOG(LogSmartFoundations, Verbose,
                TEXT("[EXTEND-MP] Scaled clone wiring: Clone[%d] SKIPPED - topology %s."),
                CloneIdx, Clone.CloneTopology.IsValid() ? TEXT("empty") : TEXT("null"));
            continue;
        }

        // Find ExtendService clone's factory building from built actors
        // The factory hologram was registered as "factory" in the clone's SpawnedHolograms
        // But we need to find the BUILT factory, not the hologram
        const FString CellPrefix = SFScaledExtendGrid::Prefix(Clone.GridX, Clone.GridY, Clone.GridZ);
        const TObjectPtr<AActor>* Registered = WiringActors.Find(CellPrefix + TEXT("factory"));
        AFGBuildableFactory* CloneFactory = Registered ? Cast<AFGBuildableFactory>(*Registered) : nullptr;
        if (!IsValid(CloneFactory)) CloneFactory = nullptr;

        if (!CloneFactory)
        {
            UE_LOG(LogSmartExtend, Warning, TEXT("Extend wiring has no constructed factory for %s; no nearby actor substituted."),
                *CellPrefix);
            continue;
        }

        // Per-clone actor map with PREFIXED keys (scopes chain/network rebuilds to this clone).
        TMap<FString, AActor*> CloneBuiltActors;
        FString ClonePrefix = SFScaledExtendGrid::Prefix(Clone.GridX, Clone.GridY, Clone.GridZ);
        for (const auto& Pair : WiringActors)
        {
            if (Pair.Key.StartsWith(ClonePrefix) && IsValid(Pair.Value))
            {
                CloneBuiltActors.Add(Pair.Key, Pair.Value);
            }
        }
        CloneBuiltActors.Add(ClonePrefix + TEXT("factory"), CloneFactory);
        // Share the exact constructed owner with later cells and the power plan.
        CloneIdToBuildable.Add(ClonePrefix + TEXT("factory"), CloneFactory);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d built actors mapped (factory=%s)"),
            CloneIdx, CloneBuiltActors.Num(), *CloneFactory->GetName());

        // Generate the clone's manifest from its PREFIXED topology against the GLOBAL id map.
        // The spawn-time prefix pass rewrites every connection TARGET into the prefixed
        // namespace ("parent" -> "sc{i}_factory", internal refs -> "sc{i}_X", lane source-sides
        // -> "sc{i-1}_X" or the parent set's UNPREFIXED ids). The old code stripped prefixes
        // from the map keys and hologram ids but NEVER from the targets, so every lookup missed
        // and the per-clone manifests came out empty (live 2026-06-10: manifestBelts=0 for every
        // clone, with clone-0's lanes mis-resolving into its own namespace). The global map
        // resolves all of it: parent-set ids, every prefixed clone id, and the parent factory.
        TMap<FString, AActor*> CloneGenMap = CloneIdToBuildable;
        for (const auto& Pair : CloneBuiltActors)
        {
            CloneGenMap.Add(Pair.Key, Pair.Value);
        }

        FSFWiringManifest CloneManifest = FSFWiringManifest::Generate(
            *Clone.CloneTopology, CloneGenMap, CloneFactory);

        if (bOwnedConveyors) CloneManifest.BeltConnections.Reset();
        int32 CloneWired = CloneManifest.ExecuteWiring(GetWorld());
        const int32 CloneChains = 0; // No independent chain creation backend.
        int32 ClonePipes = CloneManifest.RebuildPipeNetworks(GetWorld(), CloneBuiltActors);

        // [EXTEND-MP] Display level while MP Extend validates.
        UE_LOG(LogSmartFoundations, Verbose,
            TEXT("[EXTEND-MP] Scaled clone wiring: Clone[%d] factory=%s mapped=%d manifestBelts=%d manifestPipes=%d wired=%d chains=%d."),
            CloneIdx, *CloneFactory->GetName(), CloneBuiltActors.Num(),
            CloneManifest.BeltConnections.Num(), CloneManifest.PipeConnections.Num(), CloneWired, CloneChains);

        // Lift-hole bindings are native construction inputs, verified from exact clone roles.
        SFExtendPassthroughLinks::Apply(*Clone.CloneTopology, CloneBuiltActors);

        // Power is materialized once, from the merged priced endpoint plan below.
        const int32 ClonePowerWired = 0;

        ScaledExtendWiredCount += CloneWired + ClonePowerWired;

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d belt/pipe, %d power, %d chains, %d pipe networks%s"),
            CloneIdx, CloneWired, ClonePowerWired, CloneChains, ClonePipes, Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
    }

    if (ScaledExtendWiredCount > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("⚡ SCALED EXTEND Wire: Total %d additional connections across %d clones"),
            ScaledExtendWiredCount, WiringScaledPlans.Num());
        WiredCount += ScaledExtendWiredCount;
    }

    // [MP-AUTH] Consume the same named endpoint plan as preview/cost. Live server
    // reconstruction has no client-only PowerPoleWiringData; this also covers it.
    // Legacy templates are promoted before quoting; no additional post-build cable inference.
    if ((WiringTopology != nullptr))
    {
        for (const FSFCloneHologram& Holo : WiringTopology->ChildHolograms)
        {
            if (Holo.Role != TEXT("wire_cost") || Holo.PowerFrom.Target.IsEmpty() || Holo.PowerTo.Target.IsEmpty()) continue;
            auto* From = SFExtendPowerConnections::Resolve(CloneIdToBuildable.FindRef(Holo.PowerFrom.Target), Holo.PowerFrom.Connector);
            auto* To = SFExtendPowerConnections::Resolve(CloneIdToBuildable.FindRef(Holo.PowerTo.Target), Holo.PowerTo.Connector);
            WiredCount += (bOwnedConveyors ? ExtendService->ExecuteOwnedPowerWire(Holo, WireClass, From, To)
                : SFExtendPowerConnections::Connect(GetWorld(), WireClass, From, To)) ? 1 : 0;
        }
    }

    // Factory daisy chains use the same priced endpoint plans above.

    // Captured work owns its inputs. A delayed invocation must not erase a newer preview.
    if (!ExtendService->IsExecutingCapturedWiring())
    {
        ExtendService->StoredCloneTopology.Reset();
        ExtendService->JsonSpawnedHolograms.Empty();
        ExtendService->JsonBuiltActors.Empty();
        ExtendService->PowerPoleWiringData.Empty();
        ExtendService->ScaledExtendClones.Empty();
    }

    return WiredCount;
}

void USFExtendWiringService::RegisterJsonBuiltActor(const FString& CloneId, AActor* BuiltActor)
{
    if (!BuiltActor || CloneId.IsEmpty())
    {
        return;
    }

    ExtendService->JsonBuiltActors.Add(CloneId, BuiltActor);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND: Registered built actor %s -> %s"),
        *CloneId, *BuiltActor->GetName());
}

AFGBuildable* USFExtendWiringService::GetBuiltActorByCloneId(const FString& CloneId) const
{
    if (CloneId.IsEmpty())
    {
        return nullptr;
    }

    const auto* Captured = ExtendService->GetCapturedWiringActors();
    const auto& Actors = Captured ? *Captured : ExtendService->JsonBuiltActors;
    if (const TObjectPtr<AActor>* FoundActor = Actors.Find(CloneId))
    {
        return Cast<AFGBuildable>(*FoundActor);
    }

    return nullptr;
}

AFGBuildable* USFExtendWiringService::GetSourceBuildableByName(const FString& ActorName) const
{
    if (ExtendService->IsExecutingCapturedWiring())
        return ExtendService->ResolveCapturedWiringSource(ActorName);
    if (ActorName.IsEmpty())
    {
        return nullptr;
    }

    // Search world for buildable with matching name
    // This is used by lane segments to find the source distributor (existing buildable)
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AFGBuildable> It(World); It; ++It)
    {
        AFGBuildable* Buildable = *It;
        if (Buildable && Buildable->GetName() == ActorName)
        {
            return Buildable;
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🛤️ LANE: Source buildable '%s' not found in world"), *ActorName);
    return nullptr;
}

// ==================== Scaled Extend (Issue #265) ====================
