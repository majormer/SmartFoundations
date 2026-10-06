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
    ExtendService->bRestoredScaledWiringDeferred = false;

    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: GenerateAndExecuteWiring called with null factory"));
        return 0;
    }

    // Check if we have stored clone topology from JSON spawning
    if (!ExtendService->StoredCloneTopology.IsValid() || ExtendService->StoredCloneTopology->ChildHolograms.Num() == 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: No stored clone topology - skipping JSON wiring"));
        return 0;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Generating wiring manifest from %d child holograms, %d registered built actors"),
        ExtendService->StoredCloneTopology->ChildHolograms.Num(), ExtendService->JsonBuiltActors.Num());

    // Build clone_id -> buildable mapping from ExtendService->JsonBuiltActors (populated during Construct())
    // Holograms are destroyed after Construct(), so we can't use ExtendService->JsonSpawnedHolograms here
    TMap<FString, AActor*> CloneIdToBuildable;

    // Add parent factory
    CloneIdToBuildable.Add(TEXT("parent"), NewFactory);

    // Copy all registered built actors
    for (const auto& Pair : ExtendService->JsonBuiltActors)
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
    if (ExtendService->StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
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
        *ExtendService->StoredCloneTopology,
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
    int32 WiredCount = WiringManifest.ExecuteWiring(GetWorld());

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Wiring complete - %d connections established"), WiredCount);

    // Create chain actors for wired belts (prevents crash in Factory_UpdateRadioactivity)
    // Pass ExtendService->JsonBuiltActors to include lane segments that were wired in ConfigureComponents
    // JsonBuiltActors stores TObjectPtr (GC-safe); build a transient raw view for the wiring-manifest API.
    TMap<FString, AActor*> BuiltActorsRaw;
    BuiltActorsRaw.Reserve(ExtendService->JsonBuiltActors.Num());
    for (const TPair<FString, TObjectPtr<AActor>>& Pair : ExtendService->JsonBuiltActors)
    {
        BuiltActorsRaw.Add(Pair.Key, Pair.Value);
    }
    int32 ChainsCreated = WiringManifest.CreateChainActors(GetWorld(), BuiltActorsRaw);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Chain actors created - %d chains"), ChainsCreated);

    // Rebuild pipe networks to ensure fluid flow between source and clone manifolds (same raw view)
    int32 NetworksRebuilt = WiringManifest.RebuildPipeNetworks(GetWorld(), BuiltActorsRaw);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Pipe networks rebuilt - %d networks"), NetworksRebuilt);

    // ==================== Lift ↔ Passthrough Linking (Issue #260) ====================
    // Built lifts need mSnappedPassthroughs set so they render as half-height
    // when connected to floor holes. Uses world search (TActorIterator) to find
    // passthroughs near each lift — no dependency on ExtendService->JsonBuiltActors registration.
    {
        // Step 1: Collect ALL built lifts from ExtendService->JsonBuiltActors
        TArray<AFGBuildableConveyorLift*> BuiltLifts;
        for (const auto& Pair : ExtendService->JsonBuiltActors)
        {
            if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Pair.Value))
            {
                BuiltLifts.AddUnique(Lift);
            }
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d built lifts in JsonBuiltActors (%d total entries)"),
            BuiltLifts.Num(), ExtendService->JsonBuiltActors.Num());

        if (BuiltLifts.Num() > 0)
        {
            // Step 2: Collect ALL passthroughs in the world near the build area (via TActorIterator)
            TArray<AFGBuildablePassthrough*> NearbyPassthroughs;
            FVector BuildCenter = NewFactory ? NewFactory->GetActorLocation() : FVector::ZeroVector;

            for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (IsValid(PT) && FVector::Dist(PT->GetActorLocation(), BuildCenter) < 10000.0f) // 100m radius
                {
                    NearbyPassthroughs.Add(PT);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d passthroughs within 100m of factory at %s"),
                NearbyPassthroughs.Num(), *BuildCenter.ToString());

            // Step 3: Get reflection property
            FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: mSnappedPassthroughs property %s"),
                SnappedProp ? TEXT("FOUND") : TEXT("NOT FOUND"));

            // Step 4: For each lift, find closest passthrough at bottom or top position
            int32 LinkedCount = 0;
            const float SnapDistance = 100.0f; // 1m tolerance

            for (AFGBuildableConveyorLift* Lift : BuiltLifts)
            {
                if (!IsValid(Lift) || !SnappedProp) continue;

                FVector LiftLoc = Lift->GetActorLocation();
                FTransform TopXform = Lift->GetTopTransform();
                FVector LiftTop = Lift->GetActorTransform().TransformPosition(TopXform.GetTranslation());

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Lift %s bottom=(%s) top=(%s)"),
                    *Lift->GetName(), *LiftLoc.ToString(), *LiftTop.ToString());

                AFGBuildablePassthrough* BottomPT = nullptr;
                AFGBuildablePassthrough* TopPT = nullptr;
                float BestBottomDist = SnapDistance;
                float BestTopDist = SnapDistance;

                for (AFGBuildablePassthrough* PT : NearbyPassthroughs)
                {
                    FVector PTLoc = PT->GetActorLocation();

                    float DistBottom = FVector::Dist(PTLoc, LiftLoc);
                    float DistTop = FVector::Dist(PTLoc, LiftTop);

                    if (DistBottom < BestBottomDist)
                    {
                        BestBottomDist = DistBottom;
                        BottomPT = PT;
                    }
                    if (DistTop < BestTopDist)
                    {
                        BestTopDist = DistTop;
                        TopPT = PT;
                    }
                }

                if (BottomPT || TopPT)
                {
                    // Use reflection to access private mSnappedPassthroughs
                    TArray<AFGBuildablePassthrough*>* PassthroughArray =
                        SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(Lift);

                    if (PassthroughArray)
                    {
                        if (PassthroughArray->Num() < 2) PassthroughArray->SetNum(2);

                        if (BottomPT)
                        {
                            (*PassthroughArray)[0] = BottomPT;
                            BottomPT->SetTopSnappedConnection(Lift->GetConnection0());
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → bottom=%s (dist=%.1f)"),
                                *BottomPT->GetName(), BestBottomDist);
                        }
                        if (TopPT)
                        {
                            (*PassthroughArray)[1] = TopPT;
                            TopPT->SetBottomSnappedConnection(Lift->GetConnection1());
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → top=%s (dist=%.1f)"),
                                *TopPT->GetName(), BestTopDist);
                        }

                        // Trigger mesh rebuild via OnRep
                        UFunction* OnRepFunc = Lift->FindFunction(TEXT("OnRep_SnappedPassthroughs"));
                        if (OnRepFunc)
                        {
                            Lift->ProcessEvent(OnRepFunc, nullptr);
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → OnRep fired ✅"));
                        }
                        else
                        {
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → OnRep_SnappedPassthroughs NOT FOUND as UFunction"));
                        }

                        LinkedCount++;
                    }
                    else
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → ContainerPtrToValuePtr returned null!"));
                    }
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → no passthrough within %.0fcm"), SnapDistance);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Linked %d/%d lifts to passthroughs"),
                LinkedCount, BuiltLifts.Num());
        }
    }

    // Only captured floor holes in this construction transaction may receive pipe links.
    SFExtendPassthroughLinks::Apply(*ExtendService->StoredCloneTopology, BuiltActorsRaw);

    // VERIFICATION: Log chain actor status for all built conveyors
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: Checking %d built actors for chain actor status"), ExtendService->JsonBuiltActors.Num());
    int32 ValidChains = 0;
    int32 NullChains = 0;
    for (const auto& Pair : ExtendService->JsonBuiltActors)
    {
        if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Pair.Value))
        {
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            if (ChainActor)
            {
                ValidChains++;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY: ✅ %s -> Chain=%s (segments=%d)"),
                    *Conveyor->GetName(), *ChainActor->GetName(), ChainActor->GetNumChainSegments());
            }
            else
            {
                NullChains++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("⛓️ VERIFY: ❌ %s -> ChainActor=NULL (CRASH RISK!)"),
                    *Conveyor->GetName());
            }
        }
    }
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: %d valid, %d NULL (crash risk if NULL > 0)"), ValidChains, NullChains);

    UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);

    // ==================== Scaled Extend: Additional Clone Wiring (Issue #265) ====================
    // Process wiring for each additional clone set beyond clone 1
    int32 ScaledExtendWiredCount = 0;
    // [EXTEND-MP] Display level while MP Extend validates (Verbose is stripped on the dedi).
    UE_LOG(LogSmartFoundations, Verbose,
        TEXT("[EXTEND-MP] Scaled clone wiring: %d clone set(s), jsonBuilt=%d, sourceTarget=%s."),
        ExtendService->ScaledExtendClones.Num(), ExtendService->JsonBuiltActors.Num(),
        *GetNameSafe(ExtendService->CurrentExtendTarget.Get()));
    for (int32 CloneIdx = 0; CloneIdx < ExtendService->ScaledExtendClones.Num(); CloneIdx++)
    {
        FSFScaledExtendClone& Clone = ExtendService->ScaledExtendClones[CloneIdx];
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
        const TObjectPtr<AActor>* Registered = ExtendService->JsonBuiltActors.Find(CellPrefix + TEXT("factory"));
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
        for (const auto& Pair : ExtendService->JsonBuiltActors)
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

        int32 CloneWired = CloneManifest.ExecuteWiring(GetWorld());
        int32 CloneChains = CloneManifest.CreateChainActors(GetWorld(), CloneBuiltActors);
        int32 ClonePipes = CloneManifest.RebuildPipeNetworks(GetWorld(), CloneBuiltActors);

        // [EXTEND-MP] Display level while MP Extend validates.
        UE_LOG(LogSmartFoundations, Verbose,
            TEXT("[EXTEND-MP] Scaled clone wiring: Clone[%d] factory=%s mapped=%d manifestBelts=%d manifestPipes=%d wired=%d chains=%d."),
            CloneIdx, *CloneFactory->GetName(), CloneBuiltActors.Num(),
            CloneManifest.BeltConnections.Num(), CloneManifest.PipeConnections.Num(), CloneWired, CloneChains);

        // Lift ↔ Passthrough linking for scaled clone (Issue #260) — world search approach
        {
            TArray<AFGBuildableConveyorLift*> CloneLifts;
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(BuiltPair.Value))
                    CloneLifts.AddUnique(Lift);
            }
            if (CloneLifts.Num() > 0)
            {
                FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
                TArray<AFGBuildablePassthrough*> NearbyPTs;
                FVector Center = CloneFactory->GetActorLocation();
                for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
                {
                    if (IsValid(*It) && FVector::Dist(It->GetActorLocation(), Center) < 10000.0f)
                        NearbyPTs.Add(*It);
                }
                for (AFGBuildableConveyorLift* Lift : CloneLifts)
                {
                    if (!IsValid(Lift) || !SnappedProp) continue;
                    FVector LiftLoc = Lift->GetActorLocation();
                    FVector LiftTop = Lift->GetActorTransform().TransformPosition(Lift->GetTopTransform().GetTranslation());
                    AFGBuildablePassthrough* BottomPT = nullptr;
                    AFGBuildablePassthrough* TopPT = nullptr;
                    float BestBD = 100.0f, BestTD = 100.0f;
                    for (AFGBuildablePassthrough* PT : NearbyPTs)
                    {
                        float DB = FVector::Dist(PT->GetActorLocation(), LiftLoc);
                        float DT = FVector::Dist(PT->GetActorLocation(), LiftTop);
                        if (DB < BestBD) { BestBD = DB; BottomPT = PT; }
                        if (DT < BestTD) { BestTD = DT; TopPT = PT; }
                    }
                    if (BottomPT || TopPT)
                    {
                        TArray<AFGBuildablePassthrough*>* Arr = SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(Lift);
                        if (Arr)
                        {
                            if (Arr->Num() < 2) Arr->SetNum(2);
                            if (BottomPT) { (*Arr)[0] = BottomPT; BottomPT->SetTopSnappedConnection(Lift->GetConnection0()); }
                            if (TopPT) { (*Arr)[1] = TopPT; TopPT->SetBottomSnappedConnection(Lift->GetConnection1()); }
                            if (UFunction* Fn = Lift->FindFunction(TEXT("OnRep_SnappedPassthroughs"))) Lift->ProcessEvent(Fn, nullptr);
                        }
                    }
                }
            }
        }

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
            ScaledExtendWiredCount, ExtendService->ScaledExtendClones.Num());
        WiredCount += ScaledExtendWiredCount;
    }

    // [MP-AUTH] Consume the same named endpoint plan as preview/cost. Live server
    // reconstruction has no client-only PowerPoleWiringData; this also covers it.
    // Legacy templates are promoted before quoting; no additional post-build cable inference.
    if (ExtendService->StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (Holo.Role != TEXT("wire_cost") || Holo.PowerFrom.Target.IsEmpty() || Holo.PowerTo.Target.IsEmpty()) continue;
            WiredCount += SFExtendPowerConnections::Connect(GetWorld(), WireClass,
                SFExtendPowerConnections::Resolve(CloneIdToBuildable.FindRef(Holo.PowerFrom.Target), Holo.PowerFrom.Connector),
                SFExtendPowerConnections::Resolve(CloneIdToBuildable.FindRef(Holo.PowerTo.Target), Holo.PowerTo.Connector)) ? 1 : 0;
        }
    }

    // Factory daisy chains use the same priced endpoint plans above.

    // Clear stored topology and built actors after wiring
    ExtendService->StoredCloneTopology.Reset();
    ExtendService->JsonSpawnedHolograms.Empty();
    ExtendService->JsonBuiltActors.Empty();
    ExtendService->PowerPoleWiringData.Empty();

    // Clear scaled extend clones (their topologies were consumed by wiring)
    ExtendService->ScaledExtendClones.Empty();

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

    // Check ExtendService->JsonBuiltActors map
    if (const TObjectPtr<AActor>* FoundActor = ExtendService->JsonBuiltActors.Find(CloneId))
    {
        return Cast<AFGBuildable>(*FoundActor);
    }

    return nullptr;
}

AFGBuildable* USFExtendWiringService::GetSourceBuildableByName(const FString& ActorName) const
{
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
