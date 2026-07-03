// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendWiringService - Manifold connections (slice E2 unit G).
 * Methods moved verbatim from SFExtendService; operate on its shared state via ExtendService->.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"

void USFExtendWiringService::WireManifoldConnections(AFGBuildableFactory* SourceFactory, AFGBuildableFactory* CloneFactory)
{
    if (!SourceFactory || !CloneFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold: Invalid factory pointers"));
        return;
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold: Wiring manifold connections between %s and %s"),
        *SourceFactory->GetName(), *CloneFactory->GetName());

    int32 BeltManifolds = 0;
    int32 PipeManifolds = 0;

    // Wire belt/lift manifold connections (distributor → distributor)
    for (auto& ChainPair : ExtendService->SourceDistributorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        AFGBuildable* SourceDistributor = ChainPair.Value;
        AFGBuildable** CloneDistPtr = ExtendService->BuiltDistributorsByChain.Find(ChainId);

        if (!SourceDistributor || !CloneDistPtr || !*CloneDistPtr)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Missing distributor for chain %d"), ChainId);
            continue;
        }

        AFGBuildable* CloneDistributor = *CloneDistPtr;
        bool bIsInputChain = ExtendService->BuiltChainIsInputMap.FindRef(ChainId);

        // Get connectors from both distributors
        TArray<UFGFactoryConnectionComponent*> SourceConnectors, CloneConnectors;
        SourceDistributor->GetComponents<UFGFactoryConnectionComponent>(SourceConnectors);
        CloneDistributor->GetComponents<UFGFactoryConnectionComponent>(CloneConnectors);

        // For INPUT chains (splitters): Source OUTPUT → Clone INPUT
        // For OUTPUT chains (mergers): Clone OUTPUT → Source INPUT
        EFactoryConnectionDirection FromDir = bIsInputChain ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_OUTPUT;
        EFactoryConnectionDirection ToDir = bIsInputChain ? EFactoryConnectionDirection::FCD_INPUT : EFactoryConnectionDirection::FCD_INPUT;

        TArray<UFGFactoryConnectionComponent*>& FromConnectors = bIsInputChain ? SourceConnectors : CloneConnectors;
        TArray<UFGFactoryConnectionComponent*>& ToConnectors = bIsInputChain ? CloneConnectors : SourceConnectors;
        AFGBuildable* FromDistributor = bIsInputChain ? SourceDistributor : CloneDistributor;
        AFGBuildable* ToDistributor = bIsInputChain ? CloneDistributor : SourceDistributor;

        // Find best pair: unconnected, correct direction, shortest distance
        UFGFactoryConnectionComponent* BestFrom = nullptr;
        UFGFactoryConnectionComponent* BestTo = nullptr;
        float BestDistance = FLT_MAX;

        for (UFGFactoryConnectionComponent* From : FromConnectors)
        {
            if (!From || From->IsConnected() || From->GetDirection() != FromDir) continue;

            for (UFGFactoryConnectionComponent* To : ToConnectors)
            {
                if (!To || To->IsConnected() || To->GetDirection() != ToDir) continue;

                float Distance = FVector::Dist(From->GetComponentLocation(), To->GetComponentLocation());
                if (Distance < BestDistance)
                {
                    BestDistance = Distance;
                    BestFrom = From;
                    BestTo = To;
                }
            }
        }

        if (BestFrom && BestTo)
        {
            if (CreateManifoldBelt(BestFrom, BestTo))
            {
                BeltManifolds++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ✅ Belt manifold: %s.%s → %s.%s (%.1f cm)"),
                    *FromDistributor->GetName(), *BestFrom->GetName(),
                    *ToDistributor->GetName(), *BestTo->GetName(), BestDistance);
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No available connectors for manifold on chain %d"), ChainId);
        }
    }

    // Wire pipe manifold connections (junction → junction)
    for (auto& ChainPair : ExtendService->SourceJunctionsByChain)
    {
        int32 ChainId = ChainPair.Key;
        AFGBuildable* SourceJunction = ChainPair.Value;
        AFGBuildable** CloneJunctionPtr = ExtendService->BuiltJunctionsByChain.Find(ChainId);

        if (!SourceJunction || !CloneJunctionPtr || !*CloneJunctionPtr)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Missing junction for pipe chain %d"), ChainId);
            continue;
        }

        AFGBuildable* CloneJunction = *CloneJunctionPtr;
        bool bIsInputChain = ExtendService->BuiltPipeChainIsInputMap.FindRef(ChainId);

        // Get pipe connectors from both junctions
        TArray<UFGPipeConnectionComponentBase*> SourceConnectors, CloneConnectors;
        SourceJunction->GetComponents<UFGPipeConnectionComponentBase>(SourceConnectors);
        CloneJunction->GetComponents<UFGPipeConnectionComponentBase>(CloneConnectors);

        // For junctions, direction is ANY - find the pair that faces each other
        TArray<UFGPipeConnectionComponentBase*>& FromConnectors = bIsInputChain ? SourceConnectors : CloneConnectors;
        TArray<UFGPipeConnectionComponentBase*>& ToConnectors = bIsInputChain ? CloneConnectors : SourceConnectors;
        AFGBuildable* FromJunction = bIsInputChain ? SourceJunction : CloneJunction;
        AFGBuildable* ToJunction = bIsInputChain ? CloneJunction : SourceJunction;

        // Find best pair: unconnected, shortest distance
        UFGPipeConnectionComponentBase* BestFrom = nullptr;
        UFGPipeConnectionComponentBase* BestTo = nullptr;
        float BestDistance = FLT_MAX;

        for (UFGPipeConnectionComponentBase* From : FromConnectors)
        {
            if (!From || From->IsConnected()) continue;

            for (UFGPipeConnectionComponentBase* To : ToConnectors)
            {
                if (!To || To->IsConnected()) continue;

                float Distance = FVector::Dist(From->GetComponentLocation(), To->GetComponentLocation());
                if (Distance < BestDistance)
                {
                    BestDistance = Distance;
                    BestFrom = From;
                    BestTo = To;
                }
            }
        }

        if (BestFrom && BestTo)
        {
            if (CreateManifoldPipe(BestFrom, BestTo))
            {
                PipeManifolds++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   ✅ Pipe manifold: %s.%s → %s.%s (%.1f cm)"),
                    *FromJunction->GetName(), *BestFrom->GetName(),
                    *ToJunction->GetName(), *BestTo->GetName(), BestDistance);
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ No available connectors for pipe manifold on chain %d"), ChainId);
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold: Created %d belt manifolds, %d pipe manifolds"),
        BeltManifolds, PipeManifolds);

    // Clear source tracking maps after manifold wiring
    ExtendService->SourceDistributorsByChain.Empty();
    ExtendService->SourceJunctionsByChain.Empty();
}

void USFExtendWiringService::WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId)
{
    if (!BuiltPipe || !SourceConnector)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Invalid parameters"));
        return;
    }

    // Get the pipe's two connectors
    UFGPipeConnectionComponentBase* PipeConn0 = BuiltPipe->GetPipeConnection0();
    UFGPipeConnectionComponentBase* PipeConn1 = BuiltPipe->GetPipeConnection1();

    if (!PipeConn0 || !PipeConn1)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Pipe %s missing connectors"), *BuiltPipe->GetName());
        return;
    }

    // Find which pipe connector is closer to source junction
    float Dist0ToSource = FVector::Dist(PipeConn0->GetComponentLocation(), SourceConnector->GetComponentLocation());
    float Dist1ToSource = FVector::Dist(PipeConn1->GetComponentLocation(), SourceConnector->GetComponentLocation());

    UFGPipeConnectionComponentBase* PipeToSource = (Dist0ToSource < Dist1ToSource) ? PipeConn0 : PipeConn1;
    UFGPipeConnectionComponentBase* PipeToClone = (Dist0ToSource < Dist1ToSource) ? PipeConn1 : PipeConn0;

    // Wire pipe to source junction
    if (!PipeToSource->IsConnected() && !SourceConnector->IsConnected())
    {
        PipeToSource->SetConnection(SourceConnector);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → source %s (verified: PipeConnected=%d, SourceConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToSource->GetName(), *SourceConnector->GetName(),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Source connection failed (PipeConnected=%d, SourceConnected=%d)"),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }

    // Find clone junction from ExtendService->BuiltJunctionsByChain
    AFGBuildable** CloneJunctionPtr = ExtendService->BuiltJunctionsByChain.Find(CloneChainId);
    if (!CloneJunctionPtr || !*CloneJunctionPtr)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone junction not found for chain %d"), CloneChainId);
        return;
    }

    AFGBuildable* CloneJunction = *CloneJunctionPtr;

    // Get clone junction's connectors
    TArray<UFGPipeConnectionComponentBase*> CloneConnectors;
    CloneJunction->GetComponents<UFGPipeConnectionComponentBase>(CloneConnectors);

    // ============================================================
    // OPPOSING CONNECTOR APPROACH FOR PIPE MANIFOLD WIRING
    // ============================================================
    // The manifold pipe should connect OPPOSING connectors on source and clone.
    // Find the clone connector that faces TOWARD the source junction.
    // This matches the spawning logic where we selected the source connector
    // facing toward the clone, and used the same relative position on clone.
    // ============================================================

    FVector CloneLocation = CloneJunction->GetActorLocation();
    FVector SourceLocation = SourceConnector->GetOwner()->GetActorLocation();
    FVector DirectionToSource = (SourceLocation - CloneLocation).GetSafeNormal();

    UFGPipeConnectionComponentBase* BestCloneConnector = nullptr;
    float BestAlignment = -FLT_MAX;

    for (UFGPipeConnectionComponentBase* CloneConn : CloneConnectors)
    {
        if (!CloneConn || CloneConn->IsConnected()) continue;

        // Find the connector whose position (relative to junction center) best faces toward source
        FVector ConnPos = CloneConn->GetComponentLocation();
        FVector DirFromCenter = (ConnPos - CloneLocation).GetSafeNormal();
        float Alignment = FVector::DotProduct(DirFromCenter, DirectionToSource);

        if (Alignment > BestAlignment)
        {
            BestAlignment = Alignment;
            BestCloneConnector = CloneConn;
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connector selection - BestAlignment=%.2f"),
        BestAlignment);

    if (BestCloneConnector && !PipeToClone->IsConnected())
    {
        PipeToClone->SetConnection(BestCloneConnector);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → clone %s.%s (alignment=%.2f, verified: PipeConnected=%d, CloneConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToClone->GetName(),
            *CloneJunction->GetName(), *BestCloneConnector->GetName(), BestAlignment,
            PipeToClone->IsConnected(), BestCloneConnector->IsConnected());
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connection failed (NoCloneConnector=%d, PipeConnected=%d)"),
            BestCloneConnector == nullptr, PipeToClone->IsConnected());
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: ✅ Wired manifold pipe %s between source and clone junctions"),
        *BuiltPipe->GetName());

    // Merge pipe networks to ensure fluid can flow
    // Cast to UFGPipeConnectionComponent to access GetPipeNetworkID()
    UFGPipeConnectionComponent* NetworkConn0 = Cast<UFGPipeConnectionComponent>(PipeConn0);
    UFGPipeConnectionComponent* NetworkConn1 = Cast<UFGPipeConnectionComponent>(PipeConn1);
    if (NetworkConn0 && NetworkConn1)
    {
        UWorld* World = BuiltPipe->GetWorld();
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
        {
            int32 Network0 = NetworkConn0->GetPipeNetworkID();
            int32 Network1 = NetworkConn1->GetPipeNetworkID();
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Network IDs - Conn0=%d, Conn1=%d"),
                Network0, Network1);

            if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
                AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net0 && Net1)
                {
                    Net0->MergeNetworks(Net1);
                    Net0->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Merged networks %d and %d, marked for rebuild"),
                        Network0, Network1);
                }
            }
            else if (Network0 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network0);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network0);
                }
            }
            else if (Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network1);
                }
            }
        }
    }
}

void USFExtendWiringService::WireManifoldBelt(AFGBuildableConveyorBelt* BuiltBelt, UFGFactoryConnectionComponent* SourceConnector, int32 CloneChainId)
{
    if (!BuiltBelt || !SourceConnector)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Invalid parameters"));
        return;
    }

    // Get the belt's two connectors
    UFGFactoryConnectionComponent* BeltConn0 = BuiltBelt->GetConnection0();
    UFGFactoryConnectionComponent* BeltConn1 = BuiltBelt->GetConnection1();

    if (!BeltConn0 || !BeltConn1)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Belt %s missing connectors"), *BuiltBelt->GetName());
        return;
    }

    // Determine flow direction based on SourceConnector direction
    EFactoryConnectionDirection SourceDir = SourceConnector->GetDirection();
    bool bSourceIsOutput = (SourceDir == EFactoryConnectionDirection::FCD_OUTPUT);

    // For SPLITTERS (INPUT chain): Source OUTPUT → Belt → Clone INPUT
    //   - SourceConnector is OUTPUT
    //   - Belt Conn0 (INPUT) connects to Source OUTPUT
    //   - Belt Conn1 (OUTPUT) connects to Clone INPUT
    //
    // For MERGERS (OUTPUT chain): Clone OUTPUT → Belt → Source INPUT
    //   - SourceConnector is INPUT (on source merger, receives from belt)
    //   - Belt Conn0 (INPUT) connects to Clone OUTPUT
    //   - Belt Conn1 (OUTPUT) connects to Source INPUT

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: SourceConnector=%s, Dir=%d, bSourceIsOutput=%d"),
        *SourceConnector->GetName(), (int32)SourceDir, bSourceIsOutput);

    // Find clone distributor from ExtendService->BuiltDistributorsByChain
    AFGBuildable** CloneDistributorPtr = ExtendService->BuiltDistributorsByChain.Find(CloneChainId);
    if (!CloneDistributorPtr || !*CloneDistributorPtr)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Clone distributor not found for chain %d"), CloneChainId);
        return;
    }

    AFGBuildable* CloneDistributor = *CloneDistributorPtr;

    // Get clone distributor's connectors
    TArray<UFGFactoryConnectionComponent*> CloneConnectors;
    CloneDistributor->GetComponents<UFGFactoryConnectionComponent>(CloneConnectors);

    // Assign belt connectors based on flow direction
    UFGFactoryConnectionComponent* BeltToSource = nullptr;
    UFGFactoryConnectionComponent* BeltToClone = nullptr;

    if (bSourceIsOutput)
    {
        // SPLITTER: Source OUTPUT → Belt Conn0 → Belt Conn1 → Clone INPUT
        BeltToSource = BeltConn0;  // Belt receives from source
        BeltToClone = BeltConn1;   // Belt sends to clone
    }
    else
    {
        // MERGER: Clone OUTPUT → Belt Conn0 → Belt Conn1 → Source INPUT
        BeltToSource = BeltConn1;  // Belt sends to source
        BeltToClone = BeltConn0;   // Belt receives from clone
    }

    // Wire belt to source distributor
    if (!BeltToSource->IsConnected() && !SourceConnector->IsConnected())
    {
        if (BeltToSource->CanConnectTo(SourceConnector))
        {
            BeltToSource->SetConnection(SourceConnector);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Connected %s.%s → source %s"),
                *BuiltBelt->GetName(), *BeltToSource->GetName(), *SourceConnector->GetName());
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: CanConnectTo failed for source (BeltDir=%d, SourceDir=%d)"),
                (int32)BeltToSource->GetDirection(), (int32)SourceConnector->GetDirection());
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Source connection failed (BeltConnected=%d, SourceConnected=%d)"),
            BeltToSource->IsConnected(), SourceConnector->IsConnected());
    }

    // For clone connector direction:
    // - SPLITTER: Clone needs INPUT (to receive from belt)
    // - MERGER: Clone needs OUTPUT (to send to belt)
    EFactoryConnectionDirection NeededDir = bSourceIsOutput
        ? EFactoryConnectionDirection::FCD_INPUT   // Splitter: clone receives
        : EFactoryConnectionDirection::FCD_OUTPUT; // Merger: clone sends

    // Find the closest available connector on the clone distributor
    FVector BeltCloneEndPos = BeltToClone->GetComponentLocation();

    UFGFactoryConnectionComponent* BestCloneConnector = nullptr;
    float BestDistance = FLT_MAX;

    for (UFGFactoryConnectionComponent* CloneConn : CloneConnectors)
    {
        if (!CloneConn || CloneConn->IsConnected()) continue;

        // Check direction compatibility
        EFactoryConnectionDirection CloneDir = CloneConn->GetDirection();
        if (CloneDir != NeededDir && CloneDir != EFactoryConnectionDirection::FCD_ANY)
        {
            continue;
        }

        // Find closest matching connector to belt's clone end
        float Distance = FVector::Dist(CloneConn->GetComponentLocation(), BeltCloneEndPos);

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestCloneConnector = CloneConn;
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Clone connector selection - NeededDir=%d, BestDistance=%.2f"),
        (int32)NeededDir, BestDistance);

    if (BestCloneConnector && !BeltToClone->IsConnected())
    {
        if (BeltToClone->CanConnectTo(BestCloneConnector))
        {
            BeltToClone->SetConnection(BestCloneConnector);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Connected %s.%s → clone %s.%s (distance=%.2f)"),
                *BuiltBelt->GetName(), *BeltToClone->GetName(),
                *CloneDistributor->GetName(), *BestCloneConnector->GetName(), BestDistance);
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: CanConnectTo failed for clone (SourceDir=%d, NeededDir=%d, CloneDir=%d)"),
                (int32)SourceDir, (int32)NeededDir, (int32)BestCloneConnector->GetDirection());
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Clone connection failed (NoCloneConnector=%d, BeltConnected=%d, NeededDir=%d)"),
            BestCloneConnector == nullptr, BeltToClone->IsConnected(), (int32)NeededDir);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: ✅ Wired manifold belt %s between source and clone distributors"),
        *BuiltBelt->GetName());

    // ============================================================
    // DIAGNOSTIC: Log chain state of manifold belt and connected belts
    // ============================================================
    {
        AFGConveyorChainActor* ManifoldChain = BuiltBelt->GetConveyorChainActor();
        int32 ManifoldBucketID = BuiltBelt->GetConveyorBucketID();
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Manifold belt %s - ChainActor=%s, BucketID=%d"),
            *BuiltBelt->GetName(),
            ManifoldChain ? *ManifoldChain->GetName() : TEXT("NULL"),
            ManifoldBucketID);

        // Log source distributor connections
        AFGBuildable* SrcDist = SourceConnector->GetOuterBuildable();
        if (SrcDist)
        {
            TArray<UFGFactoryConnectionComponent*> SrcConns;
            SrcDist->GetComponents<UFGFactoryConnectionComponent>(SrcConns);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Source distributor %s has %d connectors:"), *SrcDist->GetName(), SrcConns.Num());
            for (UFGFactoryConnectionComponent* Conn : SrcConns)
            {
                if (!Conn) continue;
                FString ConnectedTo = TEXT("(not connected)");
                FString ChainInfo = TEXT("");
                if (Conn->IsConnected())
                {
                    UFGFactoryConnectionComponent* Other = Conn->GetConnection();
                    AActor* OtherOwner = Other ? Other->GetOwner() : nullptr;
                    ConnectedTo = OtherOwner ? OtherOwner->GetName() : TEXT("(unknown)");

                    AFGBuildableConveyorBase* OtherBelt = Cast<AFGBuildableConveyorBase>(OtherOwner);
                    if (OtherBelt)
                    {
                        AFGConveyorChainActor* OtherChain = OtherBelt->GetConveyorChainActor();
                        int32 OtherBucket = OtherBelt->GetConveyorBucketID();
                        ChainInfo = FString::Printf(TEXT(" [Chain=%s, Bucket=%d]"),
                            OtherChain ? *OtherChain->GetName() : TEXT("NULL"), OtherBucket);
                    }
                }
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Diag:   %s [Dir=%d] → %s%s"),
                    *Conn->GetName(), (int32)Conn->GetDirection(), *ConnectedTo, *ChainInfo);
            }
        }

        // Log clone distributor connections
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Clone distributor %s has %d connectors:"), *CloneDistributor->GetName(), CloneConnectors.Num());
        for (UFGFactoryConnectionComponent* Conn : CloneConnectors)
        {
            if (!Conn) continue;
            FString ConnectedTo = TEXT("(not connected)");
            FString ChainInfo = TEXT("");
            if (Conn->IsConnected())
            {
                UFGFactoryConnectionComponent* Other = Conn->GetConnection();
                AActor* OtherOwner = Other ? Other->GetOwner() : nullptr;
                ConnectedTo = OtherOwner ? OtherOwner->GetName() : TEXT("(unknown)");

                AFGBuildableConveyorBase* OtherBelt = Cast<AFGBuildableConveyorBase>(OtherOwner);
                if (OtherBelt)
                {
                    AFGConveyorChainActor* OtherChain = OtherBelt->GetConveyorChainActor();
                    int32 OtherBucket = OtherBelt->GetConveyorBucketID();
                    ChainInfo = FString::Printf(TEXT(" [Chain=%s, Bucket=%d]"),
                        OtherChain ? *OtherChain->GetName() : TEXT("NULL"), OtherBucket);
                }
            }
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Diag:   %s [Dir=%d] → %s%s"),
                *Conn->GetName(), (int32)Conn->GetDirection(), *ConnectedTo, *ChainInfo);
        }
    }

    // ============================================================
    // CHAIN INTEGRATION NOTE
    // ============================================================
    // The manifold belt was built without connections, so it has no chain actor.
    // After wiring, it has BucketID (registered with subsystem) but ChainActor=NULL.
    //
    // We CANNOT destroy chain actors during the build process because:
    // 1. Immediate destruction crashes during parallel factory tick
    // 2. Deferred destruction still crashes because items flow before rebuild
    //
    // For now, we log the chain state and hope the game handles it gracefully.
    // TODO: Find a proper way to integrate the manifold belt into the chain system,
    // possibly by calling AFGBuildableSubsystem::MigrateConveyorGroupToChainActor
    // or by building the belt with connections already set.

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Manifold belt %s has ChainActor=%s, BucketID=%d - chain integration pending"),
        *BuiltBelt->GetName(),
        BuiltBelt->GetConveyorChainActor() ? *BuiltBelt->GetConveyorChainActor()->GetName() : TEXT("NULL"),
        BuiltBelt->GetConveyorBucketID());
}

// [#401/#414] Route a manifold lane through the ENGINE router and return WORLD-space spline data
// ready for the built actor (Respline / GetMutableSplinePointData - both consume the same space
// the legacy hand-rolled points used). The manifold lanes were the last routed lane path still
// hand-rolling a fixed-tangent 2-point spline on the BUILT actor (no routing-mode support,
// length-independent x100 bow, slope ramp on non-flat manifolds). A transient routing hologram -
// the same classes every auto-connect preview uses, running the descriptor-correct engine path -
// computes the real route, is harvested, and destroyed. Normals are the connectors' OUTWARD
// facings (the router negates the end side internally, like every other lane path).
static bool RouteManifoldSpanViaEngine(UWorld* World, UClass* ConduitBuildClass, bool bIsPipe,
	const FVector& StartPos, const FVector& StartNormal, const FVector& EndPos, const FVector& EndNormal,
	TArray<FSplinePointData>& OutWorldSplineData)
{
	if (!World || !ConduitBuildClass)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	OutWorldSplineData.Reset();
	AFGHologram* ToDestroy = nullptr;
	const TArray<FSplinePointData>* LocalData = nullptr;
	FTransform SplineXf = FTransform::Identity;

	if (bIsPipe)
	{
		ASFPipelineHologram* Pipe = World->SpawnActor<ASFPipelineHologram>(ASFPipelineHologram::StaticClass(), StartPos, FRotator::ZeroRotator, Params);
		if (!Pipe)
		{
			return false;
		}
		Pipe->SetReplicates(false);
		Pipe->SetActorHiddenInGame(true);
		Pipe->SetActorEnableCollision(false);
		Pipe->SetBuildClass(ConduitBuildClass);
		Pipe->RoutePipeLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);
		LocalData = &Pipe->GetSplineData();
		SplineXf = Pipe->GetSplineComponent() ? Pipe->GetSplineComponent()->GetComponentTransform() : Pipe->GetActorTransform();
		ToDestroy = Pipe;
	}
	else
	{
		ASFConveyorBeltHologram* Belt = World->SpawnActor<ASFConveyorBeltHologram>(ASFConveyorBeltHologram::StaticClass(), StartPos, FRotator::ZeroRotator, Params);
		if (!Belt)
		{
			return false;
		}
		Belt->SetReplicates(false);
		Belt->SetActorHiddenInGame(true);
		Belt->SetActorEnableCollision(false);
		Belt->SetBuildClass(ConduitBuildClass);
		Belt->RouteLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);
		LocalData = &Belt->GetSplineData();
		SplineXf = Belt->GetSplineComponent() ? Belt->GetSplineComponent()->GetComponentTransform() : Belt->GetActorTransform();
		ToDestroy = Belt;
	}

	if (LocalData)
	{
		for (const FSplinePointData& Local : *LocalData)
		{
			FSplinePointData WorldPoint;
			WorldPoint.Location = SplineXf.TransformPosition(Local.Location);
			WorldPoint.ArriveTangent = SplineXf.TransformVectorNoScale(Local.ArriveTangent);
			WorldPoint.LeaveTangent = SplineXf.TransformVectorNoScale(Local.LeaveTangent);
			OutWorldSplineData.Add(WorldPoint);
		}
	}
	if (ToDestroy)
	{
		ToDestroy->Destroy();
	}

	if (OutWorldSplineData.Num() < 2)
	{
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
			TEXT("RouteManifoldSpanViaEngine: engine route produced %d points - caller falls back to the legacy 2-point spline"),
			OutWorldSplineData.Num());
		return false;
	}
	return true;
}

bool USFExtendWiringService::CreateManifoldBelt(UFGFactoryConnectionComponent* FromConnector, UFGFactoryConnectionComponent* ToConnector)
{
    if (!FromConnector || !ToConnector)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Get belt tier from auto-connect settings (highest unlocked)
    int32 BeltTier = 5;  // Default to Mk5
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(World))
    {
        AFGPlayerController* PC = World->GetFirstPlayerController<AFGPlayerController>();
        BeltTier = SmartSubsystem->GetHighestUnlockedBeltTier(PC);
    }

    // Load belt class
    FString BeltPath = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
        BeltTier, BeltTier, BeltTier);
    UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);
    if (!BeltClass)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to load belt class: %s"), *BeltPath);
        return false;
    }

    // Calculate spline points for the belt
    FVector StartPos = FromConnector->GetComponentLocation();
    FVector EndPos = ToConnector->GetComponentLocation();
    FVector StartForward = FromConnector->GetForwardVector();
    FVector EndForward = -ToConnector->GetForwardVector();  // Negate for facing inward

    // [#401/#414] Engine-route the manifold lane (honors the player's belt routing mode,
    // length-aware bows, and chord pitch on sloped manifolds). The router takes OUTWARD
    // connector normals. Falls back to the legacy fixed-tangent 2-point spline on failure.
    TArray<FSplinePointData> SplineData;
    if (!RouteManifoldSpanViaEngine(World, BeltClass, /*bIsPipe=*/false,
        StartPos, StartForward, EndPos, ToConnector->GetForwardVector(), SplineData))
    {
        SplineData.Reset();
        FSplinePointData StartPoint;
        StartPoint.Location = StartPos;
        StartPoint.ArriveTangent = StartForward * 100.0f;
        StartPoint.LeaveTangent = StartForward * 100.0f;
        SplineData.Add(StartPoint);

        FSplinePointData EndPoint;
        EndPoint.Location = EndPos;
        EndPoint.ArriveTangent = EndForward * 100.0f;
        EndPoint.LeaveTangent = EndForward * 100.0f;
        SplineData.Add(EndPoint);
    }

    // Spawn belt
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(BeltClass, StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Belt)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to spawn manifold belt"));
        return false;
    }

    // Set spline data
    TArray<FSplinePointData>* MutableSpline = Belt->GetMutableSplinePointData();
    if (MutableSpline)
    {
        *MutableSpline = SplineData;
    }

    // Respline the belt to apply the spline data
    AFGBuildableConveyorBelt* ResplinedBelt = AFGBuildableConveyorBelt::Respline(Belt, SplineData);
    if (ResplinedBelt)
    {
        Belt = ResplinedBelt;
    }

    Belt->OnBuildEffectFinished();

    // Connect the belt FIRST
    UFGFactoryConnectionComponent* BeltConn0 = Belt->GetConnection0();
    UFGFactoryConnectionComponent* BeltConn1 = Belt->GetConnection1();

    if (BeltConn0 && BeltConn1)
    {
        BeltConn0->SetConnection(FromConnector);
        BeltConn1->SetConnection(ToConnector);
    }

    // CRITICAL: Register belt with BuildableSubsystem AFTER connections are set
    // AddConveyor uses connections to determine which chain the belt joins.
    // Calling it before connections causes crashes in the parallel factory tick.
    AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
    if (BuildableSubsystem)
    {
        BuildableSubsystem->AddConveyor(Belt);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("CreateManifoldBelt: Registered belt with subsystem (ChainActor=%s)"),
            Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("pending"));
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("CreateManifoldBelt: No BuildableSubsystem - belt will have no chain actor!"));
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔧 Created manifold belt Mk%d between distributors"), BeltTier);
    return true;
}

bool USFExtendWiringService::CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector)
{
    if (!FromConnector || !ToConnector)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Get pipe tier from auto-connect settings (use Mk2 by default for now)
    int32 PipeTier = 2;  // Default to Mk2

    // Load pipe class
    FString PipePath = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk%d/Build_PipelineMK%d.Build_PipelineMK%d_C"),
        PipeTier, PipeTier, PipeTier);
    UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);
    if (!PipeClass)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to load pipe class: %s"), *PipePath);
        return false;
    }

    // Calculate spline points for the pipe
    FVector StartPos = FromConnector->GetComponentLocation();
    FVector EndPos = ToConnector->GetComponentLocation();
    FVector StartForward = FromConnector->GetForwardVector();
    FVector EndForward = -ToConnector->GetForwardVector();

    // [#401/#414] Engine-route the manifold lane (honors the player's pipe routing mode,
    // length-aware bows, and chord pitch on sloped manifolds). The router takes OUTWARD
    // connector normals. Falls back to the legacy fixed-tangent 2-point spline on failure.
    TArray<FSplinePointData> SplineData;
    if (!RouteManifoldSpanViaEngine(World, PipeClass, /*bIsPipe=*/true,
        StartPos, StartForward, EndPos, ToConnector->GetForwardVector(), SplineData))
    {
        SplineData.Reset();
        FSplinePointData StartPoint;
        StartPoint.Location = StartPos;
        StartPoint.ArriveTangent = StartForward * 100.0f;
        StartPoint.LeaveTangent = StartForward * 100.0f;
        SplineData.Add(StartPoint);

        FSplinePointData EndPoint;
        EndPoint.Location = EndPos;
        EndPoint.ArriveTangent = EndForward * 100.0f;
        EndPoint.LeaveTangent = EndForward * 100.0f;
        SplineData.Add(EndPoint);
    }

    // Spawn pipe with DEFERRED construction (same technique as BuildExtendPipeAndReturn)
    FActorSpawnParameters SpawnParams;
    SpawnParams.bDeferConstruction = true;

    AFGBuildablePipeline* Pipe = World->SpawnActor<AFGBuildablePipeline>(PipeClass, StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Pipe)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to spawn manifold pipe"));
        return false;
    }

    // Apply spline data BEFORE FinishSpawning
    TArray<FSplinePointData>* MutableSpline = Pipe->GetMutableSplinePointData();
    if (!MutableSpline)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Cannot get mutable spline data for manifold pipe"));
        Pipe->Destroy();
        return false;
    }
    *MutableSpline = SplineData;

    // Finish spawning and signal build complete
    Pipe->FinishSpawning(FTransform(FRotator::ZeroRotator, StartPos));
    Pipe->OnBuildEffectFinished();

    // Connect the pipe
    UFGPipeConnectionComponent* PipeConn0 = Pipe->GetPipeConnection0();
    UFGPipeConnectionComponent* PipeConn1 = Pipe->GetPipeConnection1();

    if (PipeConn0 && PipeConn1)
    {
        PipeConn0->SetConnection(Cast<UFGPipeConnectionComponent>(FromConnector));
        PipeConn1->SetConnection(Cast<UFGPipeConnectionComponent>(ToConnector));

        // Merge pipe networks
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
        {
            int32 Network0 = PipeConn0->GetPipeNetworkID();
            int32 Network1 = PipeConn1->GetPipeNetworkID();
            if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
                AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net0 && Net1)
                {
                    Net0->MergeNetworks(Net1);
                    Net0->MarkForFullRebuild();
                }
            }
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔧 Created manifold pipe Mk%d between junctions"), PipeTier);
    return true;
}

AFGBuildableFactory* USFExtendWiringService::GetSourceFactory() const
{
    if (ExtendService->GetCurrentTopology().SourceBuilding.IsValid())
    {
        return Cast<AFGBuildableFactory>(ExtendService->GetCurrentTopology().SourceBuilding.Get());
    }
    return nullptr;
}

// ==================== Diagnostic Capture (delegates to ExtendService->DiagnosticsService) ====================

