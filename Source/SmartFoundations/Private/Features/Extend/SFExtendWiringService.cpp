// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendWiringService Implementation
 *
 * Post-build connection wiring for EXTEND. Methods moved verbatim from SFExtendService
 * (slice E2); they operate on the owning service shared registry maps via ExtendService->
 * (friended). Subsystem references use this service own Subsystem back-ref. Behavior is
 * identical to the pre-split version. Impl split across SFExtendWiringService*.cpp by unit.
 */

#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendDiagnosticsService.h"
#include "Features/Extend/SFExtendRestoreReplayService.h"
#include "Features/Extend/SFExtendScaledService.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFWiringManifest.h"
#include "Features/Restore/SFRestoreService.h"
#include "Constants/SFAssetPaths.h"
#include "Services/SFRecipeManagementService.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Services/RadarPulse/SFRadarPulseService.h"
#include "SmartFoundations.h"  // For LogSmartExtend
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Logistics/SFConveyorAttachmentChildHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFPipelineJunctionChildHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Data/SFHologramDataRegistry.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGConveyorChainActor.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableMergerPriority.h"
#include "Buildables/FGBuildablePole.h"
#include "FGBuildablePolePipe.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePipelinePump.h"  // Issue #288: Phase 3.8b pump power wiring
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "FGBuildableSubsystem.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"  // AFGPlayerController (was transitively included via the size-registry files removed in T3)
#include "FGConstructDisqualifier.h"
#include "FGInventoryComponent.h"
#include "FGCentralStorageSubsystem.h"  // Extend affordability: Dimensional Depot stock
#include "Resources/FGItemDescriptor.h"  // Extend affordability: item names for diagnostics
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"  // For TActorIterator
#include "Components/BoxComponent.h"  // For clearance disabling on child factory holograms

USFExtendWiringService::USFExtendWiringService()
{
}

void USFExtendWiringService::Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService)
{
    Subsystem = InSubsystem;
    ExtendService = InExtendService;
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendWiringService initialized"));
}

void USFExtendWiringService::Shutdown()
{
    ExtendService = nullptr;
    Subsystem.Reset();
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendWiringService shutdown"));
}

// ==================== E-chain wiring (slice E2a) ====================

void USFExtendWiringService::ClearConnectionWiringMaps()
{
    ExtendService->SourceToHologramMap.Empty();
    ExtendService->PipeChainHologramMap.Empty();
    ExtendService->PipeChainJunctionMap.Empty();
    ExtendService->BeltChainHologramMap.Empty();
    ExtendService->LiftChainHologramMap.Empty();
    ExtendService->BeltChainDistributorMap.Empty();
    ExtendService->ManifoldBeltHolograms.Empty();

    // Also clear built tracking maps (used by WireBuiltChildConnections)
    ExtendService->BuiltConveyorsByChain.Empty();
    ExtendService->BuiltDistributorsByChain.Empty();
    ExtendService->BuiltJunctionsByChain.Empty();
    ExtendService->BuiltPipesByChain.Empty();
    ExtendService->BuiltChainIsInputMap.Empty();
    ExtendService->BuiltPipeChainIsInputMap.Empty();

    // Clear power pole wiring data (Issue #229)
    ExtendService->PowerPoleWiringData.Empty();

    // Clear source distributor/junction maps (used by WireManifoldConnections)
    ExtendService->SourceDistributorsByChain.Empty();
    ExtendService->SourceJunctionsByChain.Empty();

    // Clear distributor connector name map (used by Construct() to find correct output)
    ExtendService->DistributorConnectorNameByChain.Empty();
}

UFGPipeConnectionComponentBase* USFExtendWiringService::FindPipeConnectionByIndex(AFGHologram* Hologram, int32 Index) const
{
    if (!Hologram || (Index != 0 && Index != 1))
    {
        return nullptr;
    }

    // Get all pipe connection components on the hologram
    TArray<UFGPipeConnectionComponentBase*> PipeConnections;
    Hologram->GetComponents<UFGPipeConnectionComponentBase>(PipeConnections);

    // Filter to pipe connections - accept both naming conventions:
    // - Pipes: "PipelineConnection0", "PipelineConnection1"
    // - Junctions: "Connection0", "Connection1", "Connection2", "Connection3"
    TArray<UFGPipeConnectionComponentBase*> ValidConnections;
    for (UFGPipeConnectionComponentBase* Conn : PipeConnections)
    {
        if (Conn)
        {
            FString ConnName = Conn->GetFName().ToString();
            // Accept "PipelineConnection" (pipes) or "Connection" (junctions)
            if (ConnName.Contains(TEXT("PipelineConnection")) || ConnName.StartsWith(TEXT("Connection")))
            {
                ValidConnections.Add(Conn);
            }
        }
    }

    // Sort by name to ensure consistent ordering
    ValidConnections.Sort([](const UFGPipeConnectionComponentBase& A, const UFGPipeConnectionComponentBase& B)
    {
        return A.GetFName().ToString() < B.GetFName().ToString();
    });

    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: FindPipeConnectionByIndex(%s, %d) - found %d valid connections"),
        *Hologram->GetName(), Index, ValidConnections.Num());

    if (Index < ValidConnections.Num())
    {
        UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire:   Returning %s"), *ValidConnections[Index]->GetName());
        return ValidConnections[Index];
    }

    return nullptr;
}

void USFExtendWiringService::WirePipeChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain)
{
    // Get the pipe holograms for ExtendService chain (in order: 0=closest to factory, N-1=closest to junction)
    TArray<ASFPipelineHologram*>* PipeHolograms = ExtendService->PipeChainHologramMap.Find(ChainId);
    if (!PipeHolograms || PipeHolograms->Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Wire: No pipe holograms found for chain %d"), ChainId);
        return;
    }

    // Get the junction hologram for ExtendService chain
    ASFPipelineJunctionChildHologram* JunctionHologram = nullptr;
    if (ASFPipelineJunctionChildHologram** JunctionPtr = ExtendService->PipeChainJunctionMap.Find(ChainId))
    {
        JunctionHologram = *JunctionPtr;
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Wire: Wiring chain %d (%s) with %d pipes, Junction=%s"),
        ChainId,
        bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"),
        PipeHolograms->Num(),
        JunctionHologram ? *JunctionHologram->GetName() : TEXT("NONE"));

    // Get parent factory hologram's pipe connections (for the first pipe in the chain)
    // The parent hologram represents the cloned factory
    TArray<UFGPipeConnectionComponentBase*> ParentPipeConnections;
    if (ParentHologram)
    {
        ParentHologram->GetComponents<UFGPipeConnectionComponentBase>(ParentPipeConnections);
    }

    // For each pipe in the chain, set its snapped connections
    // Flow direction determines endpoint connections:
    // OUTPUT chain: Factory.Output → Pipe[0].Conn0 → ... → Pipe[N].Conn1 → Junction.Input
    // INPUT chain:  Junction.Output → Pipe[N].Conn0 → ... → Pipe[0].Conn1 → Factory.Input
    for (int32 i = 0; i < PipeHolograms->Num(); i++)
    {
        ASFPipelineHologram* PipeHolo = (*PipeHolograms)[i];
        if (!PipeHolo)
        {
            continue;
        }

        UFGPipeConnectionComponentBase* Conn0Target = nullptr;  // Connection0 target
        UFGPipeConnectionComponentBase* Conn1Target = nullptr;  // Connection1 target

        // === FACTORY CONNECTION (first pipe, index 0) ===
        if (i == 0)
        {
            if (ParentPipeConnections.Num() > 0)
            {
                UFGPipeConnectionComponentBase* FactoryConn = ParentPipeConnections[ChainId < ParentPipeConnections.Num() ? ChainId : 0];
                if (bIsInputChain)
                {
                    // INPUT: Pipe[0].Conn1 → Factory.Input (items exit pipe into factory)
                    Conn1Target = FactoryConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Factory %s (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
                else
                {
                    // OUTPUT: Pipe[0].Conn0 → Factory.Output (items enter pipe from factory)
                    Conn0Target = FactoryConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 → Factory %s (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Factory connection (no pipe connections found on parent!)"),
                    i, *PipeHolo->GetName());
            }
        }

        // === INTERNAL PIPE-TO-PIPE CONNECTIONS ===
        if (bIsInputChain)
        {
            // INPUT: items flow from higher indices toward lower indices
            if (i < PipeHolograms->Num() - 1)
            {
                // This pipe's Conn0 receives from next pipe's Conn1
                ASFPipelineHologram* NextPipe = (*PipeHolograms)[i + 1];
                if (NextPipe)
                {
                    Conn0Target = FindPipeConnectionByIndex(NextPipe, 1);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 ← Pipe[%d].Conn1 (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), i + 1, Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"));
                }
            }

            if (i > 0)
            {
                // This pipe's Conn1 sends to previous pipe's Conn0
                ASFPipelineHologram* PrevPipe = (*PipeHolograms)[i - 1];
                if (PrevPipe)
                {
                    Conn1Target = FindPipeConnectionByIndex(PrevPipe, 0);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Pipe[%d].Conn0 (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), i - 1, Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
                }
            }
        }
        else
        {
            // OUTPUT: items flow from lower indices toward higher indices
            if (i > 0)
            {
                // This pipe's Conn0 receives from previous pipe's Conn1
                ASFPipelineHologram* PrevPipe = (*PipeHolograms)[i - 1];
                if (PrevPipe)
                {
                    Conn0Target = FindPipeConnectionByIndex(PrevPipe, 1);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 ← Pipe[%d].Conn1 (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), i - 1, Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"));
                }
            }

            if (i < PipeHolograms->Num() - 1)
            {
                // This pipe's Conn1 sends to next pipe's Conn0
                ASFPipelineHologram* NextPipe = (*PipeHolograms)[i + 1];
                if (NextPipe)
                {
                    Conn1Target = FindPipeConnectionByIndex(NextPipe, 0);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Pipe[%d].Conn0 (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), i + 1, Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
                }
            }
        }

        // === JUNCTION CONNECTION (last pipe, index N-1) ===
        if (i == PipeHolograms->Num() - 1)
        {
            if (JunctionHologram)
            {
                UFGPipeConnectionComponentBase* JunctionConn = FindPipeConnectionByIndex(JunctionHologram, 0);
                if (bIsInputChain)
                {
                    // INPUT: Pipe[N].Conn0 → Junction.Output (items enter pipe from junction)
                    Conn0Target = JunctionConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 → Junction %s (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), *JunctionHologram->GetName(), JunctionConn ? *JunctionConn->GetName() : TEXT("nullptr"));
                }
                else
                {
                    // OUTPUT: Pipe[N].Conn1 → Junction.Input (items exit pipe into junction)
                    Conn1Target = JunctionConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Junction %s (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), *JunctionHologram->GetName(), JunctionConn ? *JunctionConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Junction connection (no junction hologram!)"),
                    i, *PipeHolo->GetName());
            }
        }

        // Apply the snapped connections
        // CRITICAL: Both connections must be non-null to prevent vanilla from spawning child poles
        // If either is null, vanilla thinks that end is dangling and spawns a pole
        if (Conn0Target || Conn1Target)
        {
            PipeHolo->SetSnappedConnections(Conn0Target, Conn1Target);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Wire: ✅ Set snapped connections for %s - Conn0=%s, Conn1=%s"),
                *PipeHolo->GetName(),
                Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"),
                Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Wire: ⚠️ Both connections null for %s - this will cause pole spawning!"),
                *PipeHolo->GetName());
        }
    }
}

UFGFactoryConnectionComponent* USFExtendWiringService::FindFactoryConnectionByIndex(AFGHologram* Hologram, int32 Index) const
{
    if (!Hologram || (Index != 0 && Index != 1))
    {
        return nullptr;
    }

    // Get all factory connection components on the hologram
    TArray<UFGFactoryConnectionComponent*> FactoryConnections;
    Hologram->GetComponents<UFGFactoryConnectionComponent>(FactoryConnections);

    // Filter to conveyor connections - accept "ConveyorAny" naming convention
    TArray<UFGFactoryConnectionComponent*> ValidConnections;
    for (UFGFactoryConnectionComponent* Conn : FactoryConnections)
    {
        if (Conn)
        {
            FString ConnName = Conn->GetFName().ToString();
            // Accept "ConveyorAny0", "ConveyorAny1" for belts
            // Also accept "Input0", "Output0" for distributors
            if (ConnName.Contains(TEXT("ConveyorAny")) ||
                ConnName.Contains(TEXT("Input")) ||
                ConnName.Contains(TEXT("Output")))
            {
                ValidConnections.Add(Conn);
            }
        }
    }

    // Sort by name to ensure consistent ordering
    ValidConnections.Sort([](const UFGFactoryConnectionComponent& A, const UFGFactoryConnectionComponent& B)
    {
        return A.GetFName().ToString() < B.GetFName().ToString();
    });

    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: FindFactoryConnectionByIndex(%s, %d) - found %d valid connections"),
        *Hologram->GetName(), Index, ValidConnections.Num());

    if (Index < ValidConnections.Num())
    {
        UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire:   Returning %s"), *ValidConnections[Index]->GetName());
        return ValidConnections[Index];
    }

    return nullptr;
}

void USFExtendWiringService::WireBeltChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain)
{
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRING CHAIN %d (%s) ============================"),
        ChainId, bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));

    // Get the unified conveyor chain (belts + lifts) for ExtendService chain
    TMap<int32, AFGHologram*>* UnifiedChainPtr = ExtendService->UnifiedConveyorChainMap.Find(ChainId);
    if (!UnifiedChainPtr || UnifiedChainPtr->Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRING: No conveyor holograms found for chain %d"), ChainId);
        return;
    }

    TMap<int32, AFGHologram*>& UnifiedChain = *UnifiedChainPtr;

    // Get the distributor hologram for ExtendService chain (if any)
    AFGHologram** DistributorPtr = ExtendService->BeltChainDistributorMap.Find(ChainId);
    AFGHologram* DistributorHologram = DistributorPtr ? *DistributorPtr : nullptr;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRING: Chain has %d elements, Distributor=%s"),
        UnifiedChain.Num(), DistributorHologram ? *DistributorHologram->GetName() : TEXT("NONE"));

    // Get factory connections from parent factory hologram
    TArray<UFGFactoryConnectionComponent*> ParentFactoryConnections;
    if (ParentHologram)
    {
        ParentHologram->GetComponents<UFGFactoryConnectionComponent>(ParentFactoryConnections);
        UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Parent %s has %d factory connections"),
            *ParentHologram->GetName(), ParentFactoryConnections.Num());
    }

    // === UNIFIED CHAIN CONVENTION (after cloning fix) ===
    //
    // Both INPUT and OUTPUT chains now use the same index convention:
    //   Index 0 = SOURCE end (where items ENTER the chain)
    //   Index N-1 = DESTINATION end (where items EXIT the chain)
    //
    // For OUTPUT chains: Source = Factory, Destination = Distributor (merger)
    // For INPUT chains:  Source = Distributor (splitter), Destination = Factory
    //
    // Items always flow: Conn0 → Conn1 through each belt
    // Chain flow: [0].Conn1 → [1].Conn0 → [1].Conn1 → [2].Conn0 → ... → [N-1].Conn1
    //
    // So for ALL chains:
    //   - Index 0: Conn0 receives from source (factory or distributor)
    //   - Index i: Conn0 receives from [i-1].Conn1, Conn1 sends to [i+1].Conn0
    //   - Index N-1: Conn1 sends to destination (distributor or factory)
    //

    // Wire each belt in the chain (we only wire belts, lifts don't have snapped connections)
    for (auto& ChainPair : UnifiedChain)
    {
        AFGHologram* Hologram = ChainPair.Value;
        if (!Hologram)
        {
            continue;
        }

        // Only wire belt holograms (lifts don't have snapped connections)
        ASFConveyorBeltHologram* BeltHolo = Cast<ASFConveyorBeltHologram>(Hologram);
        if (!BeltHolo)
        {
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Wire: Skipping non-belt hologram %s"), *Hologram->GetName());
            continue;
        }

        // Get hologram data to determine chain position
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(BeltHolo);
        if (!HoloData)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Wire: Belt %s has no hologram data!"), *BeltHolo->GetName());
            continue;
        }

        int32 ChainIndex = HoloData->ExtendChainIndex;
        int32 ChainLength = HoloData->ExtendChainLength;

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Wire: Wiring belt %s at index %d/%d (%s)"),
            *BeltHolo->GetName(), ChainIndex, ChainLength, bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));

        UFGFactoryConnectionComponent* Conn0Target = nullptr;
        UFGFactoryConnectionComponent* Conn1Target = nullptr;

        // === SOURCE CONNECTION (first element, index 0) ===
        // Conn0 receives from the source of the chain
        if (ChainIndex == 0)
        {
            if (bIsInputChain)
            {
                // INPUT chain source = Distributor (splitter)
                if (DistributorHologram)
                {
                    Conn0Target = FindFactoryConnectionByIndex(DistributorHologram, 0);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Distributor %s (%s) [INPUT SOURCE]"),
                        ChainIndex, *BeltHolo->GetName(), *DistributorHologram->GetName(), Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                // OUTPUT chain source = Factory
                if (ParentFactoryConnections.Num() > 0)
                {
                    UFGFactoryConnectionComponent* FactoryConn = ParentFactoryConnections[ChainId < ParentFactoryConnections.Num() ? ChainId : 0];
                    Conn0Target = FactoryConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Factory %s (%s) [OUTPUT SOURCE]"),
                        ChainIndex, *BeltHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
            }
        }

        // === INTERNAL CONVEYOR-TO-CONVEYOR CONNECTIONS ===
        // Items flow forward through the chain: [i-1].Conn1 → [i].Conn0 → [i].Conn1 → [i+1].Conn0
        // This is the same for both INPUT and OUTPUT chains now!

        if (ChainIndex > 0)
        {
            // This belt's Conn0 receives from previous conveyor's Conn1
            AFGHologram* PrevHolo = UnifiedChain.FindRef(ChainIndex - 1);
            if (PrevHolo)
            {
                Conn0Target = FindFactoryConnectionByIndex(PrevHolo, 1);
                UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Prev[%d].Conn1 %s (%s)"),
                    ChainIndex, *BeltHolo->GetName(), ChainIndex - 1, *PrevHolo->GetName(), Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"));
            }
        }

        if (ChainIndex < ChainLength - 1)
        {
            // This belt's Conn1 sends to next conveyor's Conn0
            AFGHologram* NextHolo = UnifiedChain.FindRef(ChainIndex + 1);
            if (NextHolo)
            {
                Conn1Target = FindFactoryConnectionByIndex(NextHolo, 0);
                UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Next[%d].Conn0 %s (%s)"),
                    ChainIndex, *BeltHolo->GetName(), ChainIndex + 1, *NextHolo->GetName(), Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
            }
        }

        // === DESTINATION CONNECTION (last element, index N-1) ===
        // Conn1 sends to the destination of the chain
        if (ChainIndex == ChainLength - 1)
        {
            if (bIsInputChain)
            {
                // INPUT chain destination = Factory
                if (ParentFactoryConnections.Num() > 0)
                {
                    UFGFactoryConnectionComponent* FactoryConn = ParentFactoryConnections[ChainId < ParentFactoryConnections.Num() ? ChainId : 0];
                    Conn1Target = FactoryConn;
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Factory %s (%s) [INPUT DEST]"),
                        ChainIndex, *BeltHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                // OUTPUT chain destination = Distributor (merger)
                if (DistributorHologram)
                {
                    Conn1Target = FindFactoryConnectionByIndex(DistributorHologram, 0);
                    UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Distributor %s (%s) [OUTPUT DEST]"),
                        ChainIndex, *BeltHolo->GetName(), *DistributorHologram->GetName(), Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
                }
            }
        }

        // Apply snapped connections
        BeltHolo->SetSnappedConnections(Conn0Target, Conn1Target);

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRING [%d/%d] %s:"),
            ChainIndex, ChainLength, *BeltHolo->GetName());
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌   Conn0 ← %s"),
            Conn0Target ? *FString::Printf(TEXT("%s on %s"), *Conn0Target->GetName(),
                Conn0Target->GetOwner() ? *Conn0Target->GetOwner()->GetName() : TEXT("null")) : TEXT("NONE"));
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌   Conn1 → %s"),
            Conn1Target ? *FString::Printf(TEXT("%s on %s"), *Conn1Target->GetName(),
                Conn1Target->GetOwner() ? *Conn1Target->GetOwner()->GetName() : TEXT("null")) : TEXT("NONE"));

        if (!Conn0Target && !Conn1Target)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 ⚠️ BOTH CONNECTIONS NULL for %s - will get isolated chain!"),
                *BeltHolo->GetName());
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 WIRING CHAIN %d COMPLETE ============================"), ChainId);
}


void USFExtendWiringService::ConnectAllChainElements(AFGBuildableFactory* NewFactory)
{
    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Phase 3.7: ConnectAllChainElements called with null factory"));
        ExtendService->BuiltChainElements.Empty();
        ExtendService->ChainIsInputMap.Empty();
        return;
    }

    if (ExtendService->BuiltChainElements.Num() == 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: No chain elements to connect"));
        return;
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Connecting chain elements for %s (%d chains)"),
        *NewFactory->GetName(), ExtendService->BuiltChainElements.Num());

    int32 TotalConnections = 0;
    int32 FailedConnections = 0;

    // Process each chain
    for (auto& ChainPair : ExtendService->BuiltChainElements)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ElementsByIndex = ChainPair.Value;
        bool bIsInputChain = ExtendService->ChainIsInputMap.FindRef(ChainId);

        // Sort chain indices
        TArray<int32> SortedIndices;
        ElementsByIndex.GetKeys(SortedIndices);
        SortedIndices.Sort();

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Chain %d (%s) has %d elements at indices: %s"),
            ChainId, bIsInputChain ? TEXT("input") : TEXT("output"), SortedIndices.Num(),
            *FString::JoinBy(SortedIndices, TEXT(", "), [](int32 i) { return FString::FromInt(i); }));

        // Connect consecutive elements
        for (int32 i = 0; i < SortedIndices.Num() - 1; i++)
        {
            int32 CurrentIndex = SortedIndices[i];
            int32 NextIndex = SortedIndices[i + 1];

            AFGBuildableConveyorBase* Current = ElementsByIndex[CurrentIndex];
            AFGBuildableConveyorBase* Next = ElementsByIndex[NextIndex];

            if (!Current || !Next)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Null element at index %d or %d"), CurrentIndex, NextIndex);
                FailedConnections++;
                continue;
            }

            // Both belts and lifts inherit from AFGBuildableConveyorBase and have GetConnection0/1
            // Connection0 = INPUT (items flow in), Connection1 = OUTPUT (items flow out)
            // For input chains: items flow Distributor → Element[N-1] → ... → Element[0] → Factory
            //   So Element[i].Connection0 ← Element[i+1].Connection1
            // For output chains: items flow Factory → Element[0] → ... → Element[N-1] → Distributor
            //   So Element[i].Connection1 → Element[i+1].Connection0

            UFGFactoryConnectionComponent* CurrentConn = nullptr;
            UFGFactoryConnectionComponent* NextConn = nullptr;

            if (bIsInputChain)
            {
                // Input chain: Current receives from Next
                CurrentConn = Current->GetConnection0();
                NextConn = Next->GetConnection1();
            }
            else
            {
                // Output chain: Current sends to Next
                CurrentConn = Current->GetConnection1();
                NextConn = Next->GetConnection0();
            }

            if (CurrentConn && NextConn && !CurrentConn->IsConnected() && !NextConn->IsConnected())
            {
                CurrentConn->SetConnection(NextConn);
                TotalConnections++;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s (idx %d) to %s (idx %d)"),
                    *Current->GetName(), CurrentIndex, *Next->GetName(), NextIndex);
            }
            else
            {
                FailedConnections++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Could not connect %s (idx %d) to %s (idx %d) - Curr0=%d, Curr1=%d, Next0=%d, Next1=%d"),
                    *Current->GetName(), CurrentIndex, *Next->GetName(), NextIndex,
                    CurrentConn ? CurrentConn->IsConnected() : -1,
                    Current->GetConnection1() ? Current->GetConnection1()->IsConnected() : -1,
                    Next->GetConnection0() ? Next->GetConnection0()->IsConnected() : -1,
                    NextConn ? NextConn->IsConnected() : -1);
            }
        }

        // Connect first element (closest to factory) to factory
        if (SortedIndices.Num() > 0)
        {
            int32 FirstIndex = SortedIndices[0];
            AFGBuildableConveyorBase* FirstElement = ElementsByIndex[FirstIndex];

            if (FirstElement)
            {
                // Find appropriate factory connector
                TArray<UFGFactoryConnectionComponent*> FactoryConnectors;
                NewFactory->GetComponents<UFGFactoryConnectionComponent>(FactoryConnectors);

                EFactoryConnectionDirection NeededDirection = bIsInputChain
                    ? EFactoryConnectionDirection::FCD_INPUT
                    : EFactoryConnectionDirection::FCD_OUTPUT;

                UFGFactoryConnectionComponent* FactoryConn = nullptr;
                for (UFGFactoryConnectionComponent* Conn : FactoryConnectors)
                {
                    if (!Conn || Conn->IsConnected()) continue;
                    if (Conn->GetDirection() != NeededDirection) continue;
                    FactoryConn = Conn;
                    break;
                }

                if (FactoryConn)
                {
                    // For input chains: FirstElement.Connection1 (output) → Factory.Input
                    // For output chains: Factory.Output → FirstElement.Connection0 (input)
                    UFGFactoryConnectionComponent* ElementConn = bIsInputChain
                        ? FirstElement->GetConnection1()
                        : FirstElement->GetConnection0();

                    if (ElementConn && !ElementConn->IsConnected())
                    {
                        ElementConn->SetConnection(FactoryConn);
                        TotalConnections++;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("   ✅ Connected %s to factory %s"),
                            *FirstElement->GetName(), *FactoryConn->GetName());
                    }
                }
            }
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Chain connections complete - %d succeeded, %d failed"),
        TotalConnections, FailedConnections);

    // Clear the temporary storage
    ExtendService->BuiltChainElements.Empty();
    ExtendService->ChainIsInputMap.Empty();
}

// ==================== Phase 3.8: Wire Built Child Connections ====================

