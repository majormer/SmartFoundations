// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendService Implementation
 *
 * See SFExtendService.h for architecture overview and documentation.
 */

// SP/MP DIVERGENCE MAP — Extend   (entry point for an Extend multiplayer-only bug; see CodeOrganization.md §5)
//  1. Topology walk     — [MP-CLIENT] a client cannot walk the connection graph locally
//                         (mConnectedComponent is null on clients); [MP-SEAM] it requests the
//                         walk over Server_RequestExtendTopology (SFRCO) and the server pushes the
//                         authoritative topology back for preset capture.
//  2. Commit reconstruct— [MP-AUTH] the server reconstructs the FULL commit from its own graph walk
//                         (ReconstructCommitOnServer / ReconstructScaledCommitOnServer); topology is
//                         NEVER shipped from the client (client GetConnection() is null → poisoned wiring).
//  3. Cost charge       — [MP-AUTH] the childless server parent would charge the bare factory only, so
//                         GetCost is overridden with the client-captured preview cost. (Net seam: Hook A,
//                         Core/Net/SFGameInstanceModule_SpecHooks.cpp)
//  4. Designer context  — [MP-REPL] mBlueprintDesigner does not cross the construct message; re-derived
//                         authoritatively from containment at the construct seam. (Net seam: Hook B)
//  5. Clone-id wiring   — [MP-AUTH] the SP swapped-parent (ASFFactoryHologram) does not exist server-side,
//                         so the server position-matches clone-id children to built actors post-construct
//                         and runs the wiring pass synchronously. (Net seam: Hook B)

#include "Features/Extend/SFExtendService.h"
#include "Engine/OverlapResult.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendWiringService.h"
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
#include "Core/Net/SFNetworkHelper.h"   // [EXTEND-MP] IsClient (server-walk topology branch)
#include "Core/Net/SFRCO.h"                          // [EXTEND-MP] Server_RequestExtendTopology
#include "FGPlayerController.h"
// NOTE: SFRecipeCostInjector.h removed - child holograms automatically aggregate costs via GetCost()
#include "Services/RadarPulse/SFRadarPulseService.h"
#include "SmartFoundations.h"  // For LogSmartExtend
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Logistics/SFConveyorAttachmentChildHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFPipelineJunctionChildHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
// Poles removed from EXTEND - they don't affect connections
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

USFExtendService::USFExtendService()
{
}

void USFExtendService::Initialize(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    // Create and initialize detection service
    DetectionService = NewObject<USFExtendDetectionService>(this);
    DetectionService->Initialize(InSubsystem);

    // Create and initialize topology service
    TopologyService = NewObject<USFExtendTopologyService>(this);
    TopologyService->Initialize(InSubsystem, DetectionService);

    // Create and initialize hologram service
    HologramService = NewObject<USFExtendHologramService>(this);
    HologramService->Initialize(InSubsystem, this);

    // Create and initialize wiring service
    WiringService = NewObject<USFExtendWiringService>(this);
    WiringService->Initialize(InSubsystem, this);

    // Create and initialize diagnostics service
    DiagnosticsService = NewObject<USFExtendDiagnosticsService>(this);
    DiagnosticsService->Initialize(InSubsystem, this);

    // Create and initialize restore-replay service (Smart Restore Extend replay)
    RestoreReplayService = NewObject<USFExtendRestoreReplayService>(this);
    RestoreReplayService->Initialize(this);

    // Create and initialize scaled-extend service (Issue #265 planning/preview/validate)
    ScaledService = NewObject<USFExtendScaledService>(this);
    ScaledService->Initialize(this);

    UE_LOG(LogSmartExtend, Log, TEXT("Smart!: SFExtendService initialized (with DetectionService, TopologyService, HologramService, WiringService, DiagnosticsService, RestoreReplayService, and ScaledService)"));
}

// ==================== Mode Management ====================
// EXTEND mode is AUTOMATIC - no toggle needed!
// Activates when pointing at a compatible building of the same type

void USFExtendService::ClearExtendState()
{
    if (bHasValidTarget)
    {
        // [EXTEND-MP] Stage an explicit commit CLEAR (client-only no-op otherwise): a pre-staged
        // commit must never survive the session it belonged to.
        StageCommitClearForMP();

        // CRITICAL: Unlock the extend hologram BEFORE any cleanup.
        // This is the hologram that was locked at activation — must be unlocked
        // so the build gun can reposition it when the player aims elsewhere.
        if (CurrentExtendHologram.IsValid())
        {
            CurrentExtendHologram->LockHologramPosition(false);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND: Unlocked CurrentExtendHologram"));
        }

        // Restore the original hologram (cleans up swapped hologram tracking)
        RestoreOriginalHologram();

        // Also unlock whatever the subsystem considers the active hologram (belt & suspenders)
        if (Subsystem.IsValid())
        {
            if (AFGHologram* Hologram = Subsystem->GetActiveHologram())
            {
                Hologram->LockHologramPosition(false);
            }
        }

        // CRITICAL: Clear extend flags BEFORE restoring counters.
        // UpdateCounterState triggers OnScaledExtendStateChanged when IsExtendModeActive() is true,
        // which calls RefreshExtension → repositions and re-locks the hologram.
        // By clearing flags first, the counter restore won't trigger extend state changes.
        bHasValidTarget = false;
        bExtendCommitted = false;
        bExtendManualHold = false;  // #342: clear manual pin on teardown

        // Now safe to restore pre-Extend counter snapshot
        if (bHasCounterSnapshot && Subsystem.IsValid())
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND: Restoring pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
                PreExtendCounterSnapshot.GridCounters.X, PreExtendCounterSnapshot.GridCounters.Y,
                PreExtendCounterSnapshot.SpacingX);
            Subsystem->UpdateCounterState(PreExtendCounterSnapshot);
            bHasCounterSnapshot = false;
        }

        ClearScaledExtendClones();  // Issue #265: Clean up scaled extend clones first
        ClearBeltPreviews();  // CRITICAL: Clean up child holograms before state reset
        ClearTopology();
        CurrentExtendTarget.Reset();
        CurrentExtendHologram.Reset();
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("Smart!: EXTEND state cleared"));
    }
}

void USFExtendService::Shutdown()
{
    ClearExtendState();

    // Shutdown scaled-extend service
    if (ScaledService)
    {
        ScaledService->Shutdown();
        ScaledService = nullptr;
    }

    // Shutdown restore-replay service
    if (RestoreReplayService)
    {
        RestoreReplayService->Shutdown();
        RestoreReplayService = nullptr;
    }

    // Shutdown diagnostics service
    if (DiagnosticsService)
    {
        DiagnosticsService->Shutdown();
        DiagnosticsService = nullptr;
    }

    // Shutdown wiring service
    if (WiringService)
    {
        WiringService->Shutdown();
        WiringService = nullptr;
    }

    // Shutdown hologram service
    if (HologramService)
    {
        HologramService->Shutdown();
        HologramService = nullptr;
    }

    // Shutdown topology service
    if (TopologyService)
    {
        TopologyService->Shutdown();
        TopologyService = nullptr;
    }

    // Shutdown detection service
    if (DetectionService)
    {
        DetectionService->Shutdown();
        DetectionService = nullptr;
    }

    Subsystem.Reset();
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("Smart!: SFExtendService shutdown"));
}

// ==================== Direction Cycling ====================

ESFExtendDirection USFExtendService::GetExtendDirection() const
{
    if (DetectionService)
    {
        return DetectionService->GetExtendDirection();
    }
    return ESFExtendDirection::Right; // Fallback
}

void USFExtendService::CycleExtendDirection(int32 Delta)
{
    // Only cycle if we have a valid target (automatic mode)
    if (!bHasValidTarget || !DetectionService)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT(" EXTEND: CycleDirection called but no valid target or no detection service"));
        return;
    }

    // Get valid directions - only cycle if there's more than one option
    TArray<ESFExtendDirection> ValidDirs = GetValidDirections();
    ESFExtendDirection CurrentDir = DetectionService->GetExtendDirection();

    if (ValidDirs.Num() == 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT(" EXTEND: No valid directions available - both sides blocked"));
        return;
    }

    if (ValidDirs.Num() == 1)
    {
        // Only one valid direction - can't cycle, but ensure we're using it
        if (CurrentDir != ValidDirs[0])
        {
            DetectionService->SetExtendDirection(ValidDirs[0]);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT(" EXTEND: Only one valid direction, using %s"),
                ValidDirs[0] == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT(" EXTEND: Cannot cycle - only one valid direction (%s)"),
                CurrentDir == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
        }
        return;
    }

    // Toggle between Right and Left (only two valid directions for manifold alignment)
    // Forward/Backward would block input/output connectors
    ESFExtendDirection NewDirection = (CurrentDir == ESFExtendDirection::Right)
        ? ESFExtendDirection::Left
        : ESFExtendDirection::Right;

    // Verify the new direction is valid
    if (!IsDirectionValid(NewDirection))
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT(" EXTEND: Cannot cycle to %s - direction blocked"),
            NewDirection == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
        return;
    }

    DetectionService->SetExtendDirection(NewDirection);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT(" EXTEND: Direction changed to %s"),
        NewDirection == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));

    // Immediately refresh hologram position with new direction
    if (CurrentExtendHologram.IsValid())
    {
        RefreshExtension(CurrentExtendHologram.Get());

        // CRITICAL: Recreate belt previews at new offset after direction change
        // CreateBeltPreviews clears ALL pending builds and costs at the start,
        // ensuring a fresh calculation from the base recipe.
        CreateBeltPreviews(CurrentExtendHologram.Get());
    }
}

FVector USFExtendService::GetFurthestScaledCloneWorldPosition(const FVector& FallbackLocation) const
{
    // [#373] The clones are anchored to CurrentExtendTarget (the source building); each clone's
    // WorldOffset already bakes in the direction sign, spacing, steps, arc and row layout that
    // SFExtendScaledService computed. The furthest clone (largest offset from the source) is the head
    // of the run - what the Smart Camera should frame. No clones (between rebuilds) or a gone source
    // -> fall back to the caller's location (the parent hologram).
    const AFGBuildable* Source = CurrentExtendTarget.Get();
    if (!Source || ScaledExtendClones.Num() == 0)
    {
        return FallbackLocation;
    }

    const FVector SourceLocation = Source->GetActorLocation();
    const FVector* FurthestOffset = nullptr;
    float BestDistSq = -1.0f;
    for (const FSFScaledExtendClone& Clone : ScaledExtendClones)
    {
        const float DistSq = Clone.WorldOffset.SizeSquared();
        if (DistSq > BestDistSq)
        {
            BestDistSq = DistSq;
            FurthestOffset = &Clone.WorldOffset;
        }
    }

    return FurthestOffset ? (SourceLocation + *FurthestOffset) : FallbackLocation;
}

FVector USFExtendService::GetFurthestRestoredCloneWorldPosition(const FVector& FallbackLocation) const
{
    // [#373] Smart Restore replays a captured Extend pattern via a different path than live scaled extend
    // (no live target, so IsScaledExtendActive() is false) - the camera therefore can't read the live
    // ScaledExtendClones store, and the generic grid focus computes the MIRROR of the restore's actual
    // direction. Compute the furthest cell with the SAME function the restored clones use, relative to the
    // parent they were placed against, so the focus matches the run exactly (including the bidirectional
    // scroll sign). Falls back to the caller's location when no restore is active.
    if (!bRestoredCloneTopologyActive || !RestoredCloneTopologyTemplate.IsValid() || !Subsystem.IsValid())
    {
        return FallbackLocation;
    }
    AFGHologram* Parent = RestoredCloneParentHologram.Get();
    if (!Parent)
    {
        return FallbackLocation;
    }

    const FSFCounterState& State = Subsystem->GetCounterState();
    const int32 FurthestX = FMath::Max(1, FMath::Abs(State.GridCounters.X)) - 1;
    const int32 FurthestY = FMath::Max(1, FMath::Abs(State.GridCounters.Y)) - 1;
    const FRestoredScaledClonePlacement Placement = CalculateRestoredScaledClonePlacement(
        Parent, RestoredCloneTopologyTemplate.Get(), State, FurthestX, FurthestY);
    return Parent->GetActorLocation() + Placement.WorldOffset;
}

FVector USFExtendService::GetDirectionOffset(const FVector& BuildingSize, const FRotator& BuildingRotation) const
{
    // Calculate offset perpendicular to belt direction (Left/Right only)
    // Buildings have input/output on Y axis, so sides are on X axis
    // This maintains manifold alignment - Y offset would block connectors
    FVector LocalOffset = FVector::ZeroVector;

    ESFExtendDirection CurrentDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;

    switch (CurrentDir)
    {
    case ESFExtendDirection::Right:
        // Offset in +X direction (right side of building when facing input)
        LocalOffset = FVector(BuildingSize.X, 0.0f, 0.0f);
        break;
    case ESFExtendDirection::Left:
        // Offset in -X direction (left side of building when facing input)
        LocalOffset = FVector(-BuildingSize.X, 0.0f, 0.0f);
        break;
    }

    // Rotate offset by building rotation to get world-space offset
    return BuildingRotation.RotateVector(LocalOffset);
}

bool USFExtendService::IsDirectionValid(ESFExtendDirection Direction) const
{
    if (!CurrentExtendTarget.IsValid())
    {
        return false;
    }

    AFGBuildable* SourceBuilding = CurrentExtendTarget.Get();
    UWorld* World = SourceBuilding->GetWorld();
    if (!World)
    {
        return false;
    }

    const FRotator BuildingRotation = SourceBuilding->GetActorRotation();

    // World-space lateral axis the manifold extends along for this direction.
    // The manifold grows along the building's LOCAL X (see GetDirectionOffset):
    // local +X = Right, local -X = Left.
    const FVector DirAxis = BuildingRotation.RotateVector(
        Direction == ESFExtendDirection::Right ? FVector(1.f, 0.f, 0.f) : FVector(-1.f, 0.f, 0.f))
        .GetSafeNormal();

    // ========================================================================
    // (1) Connection-consumption disqualifier  (Issue #338 - primary check)
    // ------------------------------------------------------------------------
    // A direction is "occupied" when the manifold can no longer be wired that
    // way - i.e. the backbone connector on a chain's distributor/junction that
    // faces DirAxis is ALREADY connected to a neighbour. This is the authoritative
    // signal for "don't toggle back into the chain", and unlike a geometric probe
    // it never trips on open space, so it removes the false positives that forced
    // players to carve out large empty areas just to extend.
    //
    // Aggregation (per user decision): ANY consumed chain blocks the direction, so
    // a partially-wireable extend that would leave some lanes unconnected is refused.
    // ========================================================================
    const FSFExtendTopology& Topology = GetCurrentTopology();
    if (Topology.bIsValid)
    {
        // Walk a belt backbone connector outward; returns true only if it reaches another
        // conveyor attachment (splitter/merger) within a bounded number of hops - i.e. the
        // manifold chain genuinely continues that way. A connector that merely leads to an
        // external feed/sink belt (or a factory) returns false, so an end-of-manifold unit
        // can still be extended toward its feed side (avoids being MORE restrictive than the
        // old geometric check, which is the opposite of what we want).
        auto BeltLeadsToDistributor = [](UFGFactoryConnectionComponent* StartOnDistributor) -> bool
        {
            UFGFactoryConnectionComponent* Near = StartOnDistributor ? StartOnDistributor->GetConnection() : nullptr;
            for (int32 Hop = 0; Near && Hop < 64; ++Hop)
            {
                AActor* Owner = Near->GetOwner();
                if (Cast<AFGBuildableConveyorAttachment>(Owner))
                {
                    return true; // splitter/merger -> chain continues
                }
                AFGBuildableConveyorBase* Belt = Cast<AFGBuildableConveyorBase>(Owner);
                if (!Belt)
                {
                    return false; // factory / terminal -> not a chain distributor
                }
                UFGFactoryConnectionComponent* Far =
                    (Belt->GetConnection0() == Near) ? Belt->GetConnection1() : Belt->GetConnection0();
                Near = Far ? Far->GetConnection() : nullptr;
            }
            return false;
        };

        auto PipeLeadsToJunction = [](UFGPipeConnectionComponentBase* StartOnJunction) -> bool
        {
            UFGPipeConnectionComponentBase* Near = StartOnJunction ? StartOnJunction->GetConnection() : nullptr;
            for (int32 Hop = 0; Near && Hop < 64; ++Hop)
            {
                AActor* Owner = Near->GetOwner();
                if (Cast<AFGBuildablePipelineJunction>(Owner))
                {
                    return true; // junction -> chain continues
                }
                AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Owner);
                if (!Pipe)
                {
                    return false;
                }
                UFGPipeConnectionComponentBase* Far =
                    (Pipe->GetPipeConnection0() == Near) ? Pipe->GetPipeConnection1() : Pipe->GetPipeConnection0();
                Near = Far ? Far->GetConnection() : nullptr;
            }
            return false;
        };

        // True if `Distributor` has a backbone connector facing DirAxis whose chain genuinely
        // continues to another distributor/junction that way (not merely an external feed).
        auto BackboneConsumedTowardDir = [&](AFGBuildable* Distributor) -> bool
        {
            if (!IsValid(Distributor))
            {
                return false;
            }
            const FVector Center = Distributor->GetActorLocation();

            // Belt distributors (splitters / mergers)
            TArray<UFGFactoryConnectionComponent*> BeltConns;
            Distributor->GetComponents<UFGFactoryConnectionComponent>(BeltConns);
            for (UFGFactoryConnectionComponent* Conn : BeltConns)
            {
                if (!Conn || !Conn->IsConnected())
                {
                    continue;
                }
                const FVector Facing = (Conn->GetComponentLocation() - Center).GetSafeNormal();
                if (FVector::DotProduct(Facing, DirAxis) > 0.5f && BeltLeadsToDistributor(Conn))
                {
                    return true;
                }
            }

            // Pipe junctions
            TArray<UFGPipeConnectionComponentBase*> PipeConns;
            Distributor->GetComponents<UFGPipeConnectionComponentBase>(PipeConns);
            for (UFGPipeConnectionComponentBase* Conn : PipeConns)
            {
                if (!Conn || !Conn->IsConnected())
                {
                    continue;
                }
                const FVector Facing = (Conn->GetComponentLocation() - Center).GetSafeNormal();
                if (FVector::DotProduct(Facing, DirAxis) > 0.5f && PipeLeadsToJunction(Conn))
                {
                    return true;
                }
            }
            return false;
        };

        auto AnyBeltChainConsumed = [&](const TArray<FSFConnectionChainNode>& Chains) -> bool
        {
            for (const FSFConnectionChainNode& Chain : Chains)
            {
                if (BackboneConsumedTowardDir(Chain.Distributor.Get()))
                {
                    return true;
                }
            }
            return false;
        };
        auto AnyPipeChainConsumed = [&](const TArray<FSFPipeConnectionChainNode>& Chains) -> bool
        {
            for (const FSFPipeConnectionChainNode& Chain : Chains)
            {
                if (BackboneConsumedTowardDir(Chain.Junction.Get()))
                {
                    return true;
                }
            }
            return false;
        };

        if (AnyBeltChainConsumed(Topology.InputChains) ||
            AnyBeltChainConsumed(Topology.OutputChains) ||
            AnyPipeChainConsumed(Topology.PipeInputChains) ||
            AnyPipeChainConsumed(Topology.PipeOutputChains))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT(" EXTEND: Direction %s disqualified - manifold backbone already wired that way (chain consumed)"),
                Direction == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
            return false;
        }
    }

    // ========================================================================
    // (2) Foreign-collision backstop  (tight)
    // ------------------------------------------------------------------------
    // Extend disables vanilla clearance checks (ASFFactoryHologram::CheckValidPlacement
    // skips Super during Extend), so without this nothing stops a clone spawning inside
    // an unrelated building. Keep it deliberately tight: real building size, probing only
    // the actual placement cell, oriented to the building, and ignoring every actor that
    // belongs to the source manifold. Only a genuine machine (AFGBuildableFactory) overlap
    // blocks - foundations, belts, splitters and poles do not.
    // ========================================================================
    FVector BuildingSize;
    if (USFBuildableSizeRegistry::HasProfile(SourceBuilding->GetClass()))
    {
        BuildingSize = USFBuildableSizeRegistry::GetProfile(SourceBuilding->GetClass()).DefaultSize;
    }
    else
    {
        // Real bounds instead of an oversized fixed default (avoids over-blocking).
        FVector BoundsOrigin, BoxExtent;
        SourceBuilding->GetActorBounds(false, BoundsOrigin, BoxExtent);
        BuildingSize = BoxExtent * 2.0f;
    }

    const FVector TargetCenter = SourceBuilding->GetActorLocation() + DirAxis * BuildingSize.X;
    // [#385] Same-level wrong-side probe. The horizontal extent stays within the next cell so a
    // machine already occupying that cell beside us still blocks the direction. But the VERTICAL
    // extent is clamped to a thin slab: overlap is allowed (vanilla allows it), and a manifold
    // stacked a few walls above/below was never "the wrong side". Previously HalfExtent scaled
    // with full building height, so the box reached onto other floors and blocked a legitimate
    // stacked manifold (a 3-wall gap failed while a 4-wall gap worked — reporter @maxstudy).
    FVector HalfExtent = BuildingSize * 0.45f; // horizontal: stays within the next cell
    HalfExtent.Z = 50.0f;                       // vertical: same level only, ignore stacked neighbours

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(SourceBuilding);

    // Ignore the source manifold's own parts so the backstop only catches FOREIGN obstructions.
    if (Topology.bIsValid)
    {
        auto IgnoreActor = [&](AActor* A) { if (A) { QueryParams.AddIgnoredActor(A); } };
        auto IgnoreBeltChains = [&](const TArray<FSFConnectionChainNode>& Chains)
        {
            for (const FSFConnectionChainNode& C : Chains)
            {
                IgnoreActor(C.Distributor.Get());
                for (const auto& Conv : C.Conveyors)    { IgnoreActor(Conv.Get()); }
                for (const auto& Pole : C.SupportPoles) { IgnoreActor(Pole.Get()); }
                for (const auto& Pass : C.Passthroughs) { IgnoreActor(Pass.Get()); }
            }
        };
        auto IgnorePipeChains = [&](const TArray<FSFPipeConnectionChainNode>& Chains)
        {
            for (const FSFPipeConnectionChainNode& C : Chains)
            {
                IgnoreActor(C.Junction.Get());
                for (const auto& Pipe : C.Pipelines)     { IgnoreActor(Pipe.Get()); }
                for (const auto& Pole : C.SupportPoles)  { IgnoreActor(Pole.Get()); }
                for (const auto& Pass : C.Passthroughs)  { IgnoreActor(Pass.Get()); }
                for (const auto& Att : C.PipeAttachments){ IgnoreActor(Att.Get()); }
            }
        };
        IgnoreBeltChains(Topology.InputChains);
        IgnoreBeltChains(Topology.OutputChains);
        IgnorePipeChains(Topology.PipeInputChains);
        IgnorePipeChains(Topology.PipeOutputChains);
    }

    const bool bHasOverlap = World->OverlapMultiByChannel(
        Overlaps,
        TargetCenter,
        BuildingRotation.Quaternion(),
        ECC_WorldStatic,
        FCollisionShape::MakeBox(HalfExtent),
        QueryParams);

    if (bHasOverlap)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Overlap.GetActor()))
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT(" EXTEND: Direction %s blocked by foreign building %s (collision backstop)"),
                    Direction == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"),
                    *Factory->GetName());
                return false;
            }
        }
    }

    return true;
}

TArray<ESFExtendDirection> USFExtendService::GetValidDirections() const
{
    TArray<ESFExtendDirection> ValidDirections;

    if (IsDirectionValid(ESFExtendDirection::Right))
    {
        ValidDirections.Add(ESFExtendDirection::Right);
    }
    if (IsDirectionValid(ESFExtendDirection::Left))
    {
        ValidDirections.Add(ESFExtendDirection::Left);
    }

    return ValidDirections;
}

// ==================== Topology Walking (delegates to TopologyService) ====================

bool USFExtendService::WalkTopology(AFGBuildable* SourceBuilding)
{
    // [MP-CLIENT][MP-SEAM] (Extend §1) A network client cannot walk the graph locally:
    // mConnectedComponent is UPROPERTY(SaveGame) - server-only, NOT replicated - so GetConnection()
    // is null on clients by design (live-diagnosed 2026-06-08: "2 factory connectors, 0 with resolved
    // GetConnection()"). Ask the SERVER to walk via the USFRCO topology request and consume the
    // cached reply here: activation calls WalkTopology every tick while aiming, so returning
    // false until the reply lands makes the flow async with zero restructuring. The reply's
    // actor/component references are replicated objects, so they resolve against local proxies.
    if (Subsystem.IsValid() && FSFNetworkHelper::IsClient(Subsystem->GetWorld()))
    {
        const double Now = FPlatformTime::Seconds();

        // Cached reply for THIS building, still fresh?
        if (CachedServerTopologyBuilding.Get() == SourceBuilding
            && Now - CachedServerTopologyTime < 3.0)
        {
            if (!CachedServerTopology.bIsValid)
            {
                return false; // negative result cached: nothing to extend here
            }
            if (TopologyService)
            {
                TopologyService->InjectTopology(CachedServerTopology);
            }
            LastExtendTopology = CachedServerTopology;
            return true;
        }

        // Not cached (or stale): fire a throttled server request and bail for this tick.
        if (PendingTopologyRequestBuilding.Get() != SourceBuilding
            || Now - LastTopologyRequestTime > 1.0)
        {
            bool bRequested = false;
            if (UWorld* World = Subsystem->GetWorld())
            {
                if (AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
                {
                    if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
                    {
                        RCO->Server_RequestExtendTopology(SourceBuilding);
                        bRequested = true;
                    }
                }
            }
            PendingTopologyRequestBuilding = SourceBuilding;
            LastTopologyRequestTime = Now;
            UE_LOG(LogSmartExtend, Verbose,
                TEXT("[EXTEND-MP] Client requested server topology walk for %s (%s)."),
                *GetNameSafe(SourceBuilding), bRequested ? TEXT("sent") : TEXT("RCO unavailable"));
        }
        return false;
    }

    if (TopologyService)
    {
        const bool bWalked = TopologyService->WalkTopology(SourceBuilding);
        if (bWalked)
        {
            LastExtendTopology = TopologyService->GetCurrentTopology();
        }
        return bWalked;
    }

    UE_LOG(LogSmartExtend, Verbose, TEXT("Smart!: WalkTopology called but TopologyService not initialized"));
    return false;
}

void USFExtendService::SetStoredCloneTopologyForServerCommit(const FSFCloneTopology& Clone)
{
    StoredCloneTopology = MakeShared<FSFCloneTopology>(Clone);
    UE_LOG(LogSmartExtend, Verbose,
        TEXT("[EXTEND-MP] Server installed staged clone topology: %d children (parent class %s)."),
        Clone.ChildHolograms.Num(), *Clone.ParentBuildClass);
}

void USFExtendService::SetServerCommitSourceBuilding(AFGBuildable* Source)
{
    LastBuiltFromBuilding = Source;
    CurrentExtendTarget = Source;
    UE_LOG(LogSmartExtend, Verbose,
        TEXT("[EXTEND-MP] Server installed commit source building: %s."), *GetNameSafe(Source));
}

void USFExtendService::GetScaledClonePlanForCommit(TArray<FSFExtendCommitScaledClone>& OutClones) const
{
    OutClones.Reset();
    for (const FSFScaledExtendClone& Clone : ScaledExtendClones)
    {
        FSFExtendCommitScaledClone Entry;
        Entry.WorldOffset = Clone.WorldOffset;
        Entry.RotationOffset = Clone.RotationOffset;
        Entry.GridX = Clone.GridX;
        Entry.GridY = Clone.GridY;
        Entry.bIsSeed = Clone.bIsSeed;
        OutClones.Add(Entry);
    }
}

bool USFExtendService::BuildCommitSpecForMP(AFGHologram* ParentHologram, FSFExtendCommitSpec& OutSpec) const
{
    OutSpec = FSFExtendCommitSpec();
    if (!ParentHologram)
    {
        return false;
    }

    // [EXTEND-MP] RESTORE commit: an active Smart Restore replay session owns the preview, so the
    // commit must be the restore one - there is no source building to walk (the topology comes
    // from the saved preset template, value-only and wire-safe). Ship the compact TEMPLATE plus
    // the panel state + production recipe the server-side replay pipeline reads; the server
    // re-expands at the validated parent's final position (fresher than any client re-emission).
    if (bRestoredCloneTopologyActive && RestoredCloneTopologyTemplate.IsValid())
    {
        const FSFCloneTopology& Template = *RestoredCloneTopologyTemplate;

        // Reliable-RPC ceiling guard (same rationale as the conduit-plan guard): a preset
        // template is normally one clone's infrastructure, but refuse anything that could
        // overflow the staging RPC before the previews are destroyed.
        int32 BytesEstimate = 0;
        for (const FSFCloneHologram& Holo : Template.ChildHolograms)
        {
            BytesEstimate += 400 + Holo.SplineData.Points.Num() * 80;
            // [#477] Captured customization rides the same reliable RPC: five class-path strings
            // (UTF-16 on the wire) + colors/rotation/flags. Model it so a richly customized
            // template can't pass this guard yet overflow the actual payload.
            if (Holo.Customization.bCaptured)
            {
                BytesEstimate += 64 + 2 * (Holo.Customization.SwatchClass.Len()
                    + Holo.Customization.PatternClass.Len()
                    + Holo.Customization.MaterialClass.Len()
                    + Holo.Customization.SkinClass.Len()
                    + Holo.Customization.PaintFinishClass.Len());
            }
        }
        if (BytesEstimate > 45000)
        {
            UE_LOG(LogSmartExtend, Warning,
                TEXT("[EXTEND-MP] Restore commit refused: preset template too large to stage reliably (%d children, ~%d bytes)."),
                Template.ChildHolograms.Num(), BytesEstimate);
            return false;
        }

        OutSpec.bIsRestore = true;
        OutSpec.RestoreTemplate = Template;
        OutSpec.Cost = ParentHologram->GetCost(true); // parent + restored preview children, exact
        OutSpec.BuildClass = ParentHologram->GetBuildClass();
        if (Subsystem.IsValid())
        {
            OutSpec.RestoreCounterState = Subsystem->GetCounterState();
            if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
            {
                if (RecipeSvc->HasStoredProductionRecipe())
                {
                    OutSpec.RestoreProductionRecipe = RecipeSvc->GetStoredProductionRecipe();
                }
            }
        }
        OutSpec.bValid = true;
        return true;
    }

    const FSFExtendTopology& Topo = GetLastExtendTopology();
    if (!Topo.bIsValid || !Topo.SourceBuilding.IsValid())
    {
        return false;
    }

    // PARAMETERS ONLY: the clone topology is derived SERVER-side (ReconstructCommitOnServer).
    // Capturing it here poisons every segment's connections - CaptureBeltChain/CapturePipeChain
    // read GetConnection(), null on clients - and the wiring manifest comes out empty (live root
    // cause 2026-06-10: every MP Extend built unwired).
    OutSpec.ParentOffset = ParentHologram->GetActorLocation() - Topo.SourceBuilding->GetActorLocation();
    // [#382] Ship the parent's yaw offset from the source so the server can rotate the parent's belts
    // (FromSource only positions them). Pure yaw - rotation is always around Z; buildings stay upright.
    const FRotator ParentDelta = ParentHologram->GetActorRotation() - Topo.SourceBuilding->GetActorRotation();
    OutSpec.ParentRotation = FRotator(0.0f, ParentDelta.Yaw, 0.0f);
    OutSpec.Cost = ParentHologram->GetCost(true); // parent + preview children, exact preview cost
    OutSpec.BuildClass = ParentHologram->GetBuildClass();
    OutSpec.SourceBuilding = Topo.SourceBuilding.Get();
    // [#380] Ship the client's belt routing mode; the server re-derives lanes with its own (default) settings.
    // [#382] Ship the counter state too - cross-clone lane math (e.g. PrevCloneRotation for the first
    // child's manifold lane to the parent distributor) reads CounterState.RotationZ directly server-side.
    if (Subsystem.IsValid())
    {
        OutSpec.BeltRoutingMode = Subsystem->GetAutoConnectRuntimeSettings().BeltRoutingMode;
        OutSpec.PipeRoutingMode = Subsystem->GetAutoConnectRuntimeSettings().PipeRoutingMode;
        OutSpec.BeltTierMain = Subsystem->GetAutoConnectRuntimeSettings().BeltTierMain;
        OutSpec.PipeTierMain = Subsystem->GetAutoConnectRuntimeSettings().PipeTierMain;
        OutSpec.bPipeIndicator = Subsystem->GetAutoConnectRuntimeSettings().bPipeIndicator;
        OutSpec.CounterState = Subsystem->GetCounterState();
    }
    GetScaledClonePlanForCommit(OutSpec.ScaledClones);
    OutSpec.bValid = true;
    return true;
}

// [MP-AUTH] (Extend §2) Server-authoritative commit reconstruction. Called from the construct seam
// (Net: Hook B). Derives topology from the server's own graph walk - NEVER from client-shipped data.
int32 USFExtendService::ReconstructCommitOnServer(AFGHologram* ParentHologram, const FSFExtendCommitSpec& Spec)
{
    if (!ParentHologram || !TopologyService)
    {
        return 0;
    }

    // [EXTEND-MP] RESTORE commit: no source building exists to walk - the preset TEMPLATE arrived
    // with the commit. Install the client's panel state + production recipe (the replay pipeline
    // and the restored-factory placement/wiring math read them from the subsystem), then run the
    // SAME replay pipeline the SP preview uses: expansion at this validated parent's position,
    // rr_X_Y factory spawn, infra spawn, pre-wiring, and the session state
    // (StoredCloneTopology / JsonSpawnedHolograms / RestoredScaledFactoryPreviewLocations) the
    // post-construct wiring pass consumes. Mirrors how the scaled commit reuses
    // SpawnScaledExtendPreviews via SpawnCloneSetsForServerCommit.
    if (Spec.bIsRestore)
    {
        if (Subsystem.IsValid())
        {
            Subsystem->UpdateCounterState(Spec.RestoreCounterState);
            if (Spec.RestoreProductionRecipe)
            {
                if (USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService())
                {
                    RecipeSvc->StoreProductionRecipeClass(Spec.RestoreProductionRecipe);
                }
            }
        }

        const bool bReplayed = ReplayRestoreCloneTopology(ParentHologram, Spec.RestoreTemplate);
        const int32 NumChildren = ParentHologram->GetHologramChildren().Num();
        UE_LOG(LogSmartFoundations, Verbose,
            TEXT("[EXTEND-MP] Server reconstructed RESTORE commit for %s: replayed=%d, parent now has %d children (template %d, grid %dx%d)."),
            *ParentHologram->GetName(), bReplayed ? 1 : 0, NumChildren,
            Spec.RestoreTemplate.ChildHolograms.Num(),
            Spec.RestoreCounterState.GridCounters.X, Spec.RestoreCounterState.GridCounters.Y);
        return bReplayed ? NumChildren : 0;
    }

    SetStoredCloneTopologyForServerCommit(FSFCloneTopology()); // replaced below; keep state coherent
    SetServerCommitSourceBuilding(Spec.SourceBuilding);

    // [#380] Apply the client's belt routing mode before re-deriving lanes (covers both the parent
    // path below and ReconstructScaledCommitOnServer). The server's own runtime settings default to
    // 0/Default, so without this MP lane belts ignore Curve/Straight even when the client set them.
    // [#382] Install the client's counter state too, so cross-clone lane math (PrevCloneRotation for
    // the first child's manifold lane to the parent distributor) reads the real RotationZ, not 0.
    if (Subsystem.IsValid())
    {
        Subsystem->SetAutoConnectBeltRoutingMode(Spec.BeltRoutingMode);
        Subsystem->SetAutoConnectPipeRoutingMode(Spec.PipeRoutingMode);
        Subsystem->SetAutoConnectBeltTierMain(Spec.BeltTierMain);
        Subsystem->SetAutoConnectPipeTierMain(Spec.PipeTierMain);
        Subsystem->SetAutoConnectPipeIndicator(Spec.bPipeIndicator);
        Subsystem->UpdateCounterState(Spec.CounterState);
    }

    AFGBuildable* Source = CurrentExtendTarget.Get();
    if (!Source)
    {
        UE_LOG(LogSmartExtend, Verbose,
            TEXT("[EXTEND-MP] ReconstructCommitOnServer: no source building in the commit - nothing reconstructed."));
        return 0;
    }

    // Authoritative graph walk: only the SERVER's GetConnection() values are real.
    if (!TopologyService->WalkTopology(Source))
    {
        UE_LOG(LogSmartExtend, Verbose,
            TEXT("[EXTEND-MP] ReconstructCommitOnServer: server topology walk failed for %s."),
            *GetNameSafe(Source));
        return 0;
    }

    const FSFSourceTopology Src = FSFSourceTopology::CaptureFromTopology(TopologyService->GetCurrentTopology());
    FSFCloneTopology Clone = FSFCloneTopology::FromSource(Src, Spec.ParentOffset);
    // [#382] FromSource only POSITIONS the parent's belts; the server's counter state is a default
    // mirror (RotationZ=0) so the SP preview's parent-rotation block never runs here. Apply the
    // parent yaw that arrived in the commit spec, using the SAME shared helper the preview uses, so
    // the parent's belts rotate on the build exactly as they did in the preview (children carry their
    // own per-clone rotation via SpawnScaledExtendPreviews and are unaffected).
    if (!FMath::IsNearlyZero(Spec.ParentRotation.Yaw))
    {
        Clone.ApplyRigidYawRotation(Spec.ParentRotation, ParentHologram->GetActorLocation(), Spec.ParentOffset);
    }
    SetStoredCloneTopologyForServerCommit(Clone);

    TMap<FString, AFGHologram*> SpawnedClones;
    const int32 NumSpawned = Clone.SpawnChildHolograms(ParentHologram, this, SpawnedClones);
    const int32 NumWired = Clone.WireChildHologramConnections(SpawnedClones, ParentHologram);
    UE_LOG(LogSmartFoundations, Verbose,
        TEXT("[EXTEND-MP] Server reconstructed %d/%d clone children (%d pre-wired) for %s; vanilla construct will build them."),
        NumSpawned, Clone.ChildHolograms.Num(), NumWired, *ParentHologram->GetName());

    // [EXTEND-MP] Push the authoritative clone topology to the BUILDING CLIENT so a later
    // Import-from-Last-Extend saves a preset with REAL segment connections - the client's own
    // capture has them all empty (GetConnection() null on clients) and such a preset restores
    // into unconnected belt segments (live finding 2026-06-10). Value-only struct, one reliable
    // RPC, bounded by the parent clone set's child count.
    if (APawn* InstigatorPawn = ParentHologram->GetConstructionInstigator())
    {
        if (AFGPlayerController* InstigatorPC = Cast<AFGPlayerController>(InstigatorPawn->GetController()))
        {
            if (!InstigatorPC->IsLocalController())
            {
                if (USFRCO* RCO = InstigatorPC->GetRemoteCallObjectOfClass<USFRCO>())
                {
                    RCO->Client_ReceiveServerCloneTopology(Clone);
                    UE_LOG(LogSmartExtend, Verbose,
                        TEXT("[EXTEND-MP] Pushed server-derived clone topology (%d children) to %s for preset capture."),
                        Clone.ChildHolograms.Num(), *GetNameSafe(InstigatorPC));
                }
            }
        }
    }

    // Scaled clone sets ride the same server-derived topology (SpawnScaledExtendPreviews
    // re-captures from the topology service's CurrentTopology, walked above).
    ReconstructScaledCommitOnServer(ParentHologram, Spec.ScaledClones);

    return NumSpawned;
}

// [MP-CLIENT][MP-SEAM] (Extend §2) Client-only: stage the commit to the server over USFRCO so the
// server can reconstruct it authoritatively (ReconstructCommitOnServer). No-op on authority.
void USFExtendService::MaybeStageCommitForMP(AFGHologram* ParentHologram)
{
    if (!Subsystem.IsValid() || !FSFNetworkHelper::IsClient(Subsystem->GetWorld()))
    {
        return;
    }
    const double Now = FPlatformTime::Seconds();
    if (Now - LastCommitStageTime < 0.25)
    {
        return;
    }
    LastCommitStageTime = Now;

    FSFExtendCommitSpec Spec;
    if (!BuildCommitSpecForMP(ParentHologram, Spec))
    {
        return;
    }
    if (UWorld* World = Subsystem->GetWorld())
    {
        if (AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
        {
            if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
            {
                RCO->Server_StageExtendCommit(Spec);
            }
        }
    }
}

void USFExtendService::StageCommitClearForMP()
{
    if (!Subsystem.IsValid() || !FSFNetworkHelper::IsClient(Subsystem->GetWorld()))
    {
        return;
    }
    if (UWorld* World = Subsystem->GetWorld())
    {
        if (AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController()))
        {
            if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
            {
                RCO->Server_StageExtendCommit(FSFExtendCommitSpec()); // bValid=false = clear
            }
        }
    }
}

int32 USFExtendService::ReconstructScaledCommitOnServer(AFGHologram* ParentHologram,
    const TArray<FSFExtendCommitScaledClone>& Clones)
{
    if (Clones.Num() == 0 || !ParentHologram || !TopologyService || !ScaledService)
    {
        return 0;
    }
    AFGBuildable* Source = CurrentExtendTarget.Get();
    if (!Source)
    {
        UE_LOG(LogSmartExtend, Verbose,
            TEXT("[EXTEND-MP] ReconstructScaledCommitOnServer: no source building installed - scaled clone sets skipped."));
        return 0;
    }

    // Authoritative graph walk (the server owns mConnectedComponent) - the spawn pipeline below
    // captures its source topology from TopologyService->GetCurrentTopology().
    if (!TopologyService->WalkTopology(Source))
    {
        UE_LOG(LogSmartExtend, Verbose,
            TEXT("[EXTEND-MP] ReconstructScaledCommitOnServer: server topology walk failed for %s - scaled clone sets skipped."),
            *GetNameSafe(Source));
        return 0;
    }

    // Install the session state SpawnScaledExtendPreviews reads, then run the SAME pipeline the
    // SP preview uses: per clone it spawns the factory child (JsonCloneId "sc{i}_factory"),
    // regenerates the infrastructure topology (FromSource + rigid-body rotation + lanes), and
    // fills ScaledExtendClones[i].SpawnedHolograms / CloneTopology - exactly the state the scaled
    // post-build wiring consumes after the vanilla child-construct loop builds everything.
    CurrentExtendHologram = ParentHologram;
    ScaledExtendClones.Empty();
    for (const FSFExtendCommitScaledClone& C : Clones)
    {
        FSFScaledExtendClone Entry;
        Entry.GridX = C.GridX;
        Entry.GridY = C.GridY;
        Entry.bIsSeed = C.bIsSeed;
        Entry.WorldOffset = C.WorldOffset;
        Entry.RotationOffset = C.RotationOffset;
        ScaledExtendClones.Add(Entry);
    }

    ScaledService->SpawnCloneSetsForServerCommit();

    UE_LOG(LogSmartExtend, Verbose,
        TEXT("[EXTEND-MP] Server reconstructed %d scaled clone set(s) for %s (parent now has %d children)."),
        ScaledExtendClones.Num(), *GetNameSafe(ParentHologram), ParentHologram->GetHologramChildren().Num());

    return ScaledExtendClones.Num();
}

void USFExtendService::ReceiveServerCloneTopology(const FSFCloneTopology& Topology)
{
    ServerDerivedCloneTopology = MakeShared<FSFCloneTopology>(Topology);
    UE_LOG(LogSmartExtend, Verbose,
        TEXT("[EXTEND-MP] Client received server-derived clone topology: %d children (preset-grade, full connections)."),
        Topology.ChildHolograms.Num());
}

void USFExtendService::ReceiveServerTopology(const FSFExtendTopology& Topology)
{
    CachedServerTopology = Topology;
    CachedServerTopologyBuilding = Topology.SourceBuilding;
    CachedServerTopologyTime = FPlatformTime::Seconds();

    UE_LOG(LogSmartExtend, Verbose,
        TEXT("[EXTEND-MP] Client received server topology for %s: valid=%d (beltIn=%d beltOut=%d pipeIn=%d pipeOut=%d power=%d)"),
        *GetNameSafe(Topology.SourceBuilding.Get()), Topology.bIsValid ? 1 : 0,
        Topology.InputChains.Num(), Topology.OutputChains.Num(),
        Topology.PipeInputChains.Num(), Topology.PipeOutputChains.Num(), Topology.PowerPoles.Num());
}

const FSFExtendTopology& USFExtendService::GetCurrentTopology() const
{
    if (TopologyService)
    {
        return TopologyService->GetCurrentTopology();
    }

    // Return empty topology if service not initialized
    static FSFExtendTopology EmptyTopology;
    return EmptyTopology;
}

const FSFExtendTopology& USFExtendService::GetLastExtendTopology() const
{
    const FSFExtendTopology& CurrentTopology = GetCurrentTopology();
    if (CurrentTopology.bIsValid && CurrentTopology.SourceBuilding.IsValid())
    {
        return CurrentTopology;
    }

    return LastExtendTopology;
}

TSharedPtr<FSFCloneTopology> USFExtendService::GetLastCloneTopology() const
{
    if (bRestoredCloneTopologyActive && RestoredCloneTopologyTemplate.IsValid())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] GetLastCloneTopology: returning restored template topology children=%d"),
            RestoredCloneTopologyTemplate->ChildHolograms.Num());
        return MakeShared<FSFCloneTopology>(*RestoredCloneTopologyTemplate);
    }

    // [EXTEND-MP] On a client, prefer the SERVER-derived copy (pushed after every commit
    // reconstruction): the client-captured topologies below have empty segment connections
    // (GetConnection() is null on clients) and poison any preset saved from them - restored
    // builds then place belts that never wire (live finding 2026-06-10).
    if (ServerDerivedCloneTopology.IsValid()
        && Subsystem.IsValid() && FSFNetworkHelper::IsClient(Subsystem->GetWorld()))
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] GetLastCloneTopology: returning SERVER-derived topology children=%d"),
            ServerDerivedCloneTopology->ChildHolograms.Num());
        return MakeShared<FSFCloneTopology>(*ServerDerivedCloneTopology);
    }

    if (StoredCloneTopology.IsValid())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] GetLastCloneTopology: returning StoredCloneTopology children=%d"),
            StoredCloneTopology->ChildHolograms.Num());
        return MakeShared<FSFCloneTopology>(*StoredCloneTopology);
    }

    if (LastCloneTopology.IsValid())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] GetLastCloneTopology: returning LastCloneTopology children=%d"),
            LastCloneTopology->ChildHolograms.Num());
        return MakeShared<FSFCloneTopology>(*LastCloneTopology);
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] GetLastCloneTopology: no clone topology cached"));
    return nullptr;
}

bool USFExtendService::IsHologramCompatibleWithRestoredCloneTopology(AFGHologram* ParentHologram) const
{
    return !RestoreReplayService || RestoreReplayService->IsHologramCompatibleWithRestoredCloneTopology(ParentHologram);
}

void USFExtendService::ClearRestoredCloneTopologySession(const TCHAR* Reason)
{
    if (RestoreReplayService)
    {
        RestoreReplayService->ClearRestoredCloneTopologySession(Reason);
    }
}

bool USFExtendService::ReplayRestoreCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology)
{
    return RestoreReplayService && RestoreReplayService->ReplayRestoreCloneTopology(ParentHologram, CloneTopology);
}

void USFExtendService::TickRestoredCloneTopology(float DeltaTime)
{
    if (RestoreReplayService)
    {
        RestoreReplayService->TickRestoredCloneTopology(DeltaTime);
    }
}

void USFExtendService::OnRestoredCloneTopologyStateChanged()
{
    if (RestoreReplayService)
    {
        RestoreReplayService->OnRestoredCloneTopologyStateChanged();
    }
}

void USFExtendService::ClearTopology()
{
    if (TopologyService)
    {
        TopologyService->ClearTopology();
    }
}

bool USFExtendService::IsValidExtendTarget(AFGBuildable* Building) const
{
    // Delegate to detection service
    if (DetectionService)
    {
        return DetectionService->IsValidExtendTarget(Building);
    }

    // Fallback if detection service not initialized
    if (!IsValid(Building))
    {
        return false;
    }

    // Explicitly reject all resource extractors (miners, oil extractors, resource well extractors, water extractors)
    // NOTE: We may revisit re-enabling water extractors (AFGBuildableWaterPump) in the future.
    // EXCEPTION: Allow FICSMAS Gift Trees (Build_TreeGiftProducer_C) which are technically resource extractors but work with Extend
    if (Building->IsA(AFGBuildableResourceExtractorBase::StaticClass()))
    {
        // check if it's a gift tree
        if (Building->GetName().Contains(TEXT("TreeGiftProducer")))
        {
             return true;
        }

        return false;
    }

    // Allow power generators/plants
    if (Building->IsA(AFGBuildableGenerator::StaticClass()))
    {
        return true;
    }

    // Must be a production building.
    // NOTE: Intentionally excludes logistics (splitters/mergers/belts/pipes) as EXTEND targets.
    return (Cast<AFGBuildableManufacturer>(Building) != nullptr);
}

// FindPolesForConveyor and FindPolesForPipeline REMOVED
// Poles don't affect connections - they are just visual/structural support
// Can be revisited in the future as a "polish" feature

// ==================== Extension Execution ====================

bool USFExtendService::TryExtendFromBuilding(AFGBuildable* HitBuilding, AFGHologram* SourceHologram)
{
    // Issue #257: Early exit if Extend is disabled (session double-tap or config)
    if (Subsystem.IsValid() && Subsystem->IsExtendDisabled())
    {
        return false;
    }

    // [EXTEND-MP] The DEDICATED SERVER must never run the Extend preview pipeline on its mirror
    // hologram. Extend previews are client-side cosmetics, and the staged commit
    // (Server_StageExtendCommit) carries everything the server needs at construct time. Live
    // crash 2026-06-10: the dedi's mirror activation spawned its OWN recipe-less clone preview
    // children; the client's construct message then deserialized into that hologram (clobbering
    // mChildren - the documented hazard) and ValidatePlacementAndCost made a virtual call into a
    // freed child (EXCEPTION_ACCESS_VIOLATION at 0xffffffff). SP and listen-host are unaffected
    // (their local player IS the previewing player).
    if (Subsystem.IsValid())
    {
        if (UWorld* World = Subsystem->GetWorld())
        {
            if (World->GetNetMode() == NM_DedicatedServer)
            {
                return false;
            }
        }
    }

    // Debug: Entry log
    static double LastEntryLog = 0;
    double EntryNow = FPlatformTime::Seconds();
    if (EntryNow - LastEntryLog > 1.0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: TryExtend called - Building=%s, Hologram=%s, bHasValidTarget=%d"),
            HitBuilding ? *HitBuilding->GetName() : TEXT("NULL"),
            SourceHologram ? *SourceHologram->GetName() : TEXT("NULL"),
            bHasValidTarget ? 1 : 0);
        LastEntryLog = EntryNow;
    }

    // STICKY EXTEND: Once committed (first scale action), Extend stays active when looking away.
    // Before committing, looking away deactivates Extend normally (allows middle-click sampling).
    // Only CleanupExtension (build gun clear, recipe change, build) tears down committed Extend.
    if (bHasValidTarget)
    {
        bool bStillValid = IsValid(HitBuilding) && IsValid(SourceHologram)
            && SourceHologram->GetBuildClass()
            && HitBuilding->IsA(SourceHologram->GetBuildClass())
            && IsValidExtendTarget(HitBuilding);

        if (!bStillValid)
        {
            if (bExtendCommitted || bExtendManualHold)
            {
                // Committed (scale action) or manually pinned (Hold key, #342) — keep Extend alive, maintain preview
                if (CurrentExtendHologram.IsValid())
                {
                    RefreshExtension(CurrentExtendHologram.Get());
                }
                return true;
            }
            else
            {
                // Not committed — deactivate Extend so user can sample/build elsewhere
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND: Deactivating (not committed, looked away)"));
                ClearExtendState();
                return false;
            }
        }
    }
    else
    {
        // Not yet activated — validate normally before first activation
        if (!IsValid(HitBuilding) || !IsValid(SourceHologram))
        {
            return false; // no target/hologram (handled/logged upstream)
        }

        // [#365] Extend WORKS inside the Blueprint Designer: every clone spawn site
        // propagates the designer context (SFPropagateDesignerToClone in the clone spawner),
        // so clones and logistics register with the designer and get captured by blueprint
        // saves. The earlier vanilla-only gate (added while auto-connect designer support was
        // unproven) is removed; #312's proxy-grouping guards remain - those are orthogonal.

        UClass* HologramBuildClass = SourceHologram->GetBuildClass();
        if (!HologramBuildClass || !HitBuilding->IsA(HologramBuildClass))
        {
            // [EXTEND-MP] TEMP: class mismatch is a common client failure (proxy class vs hologram build class).
            static double LastBail1 = 0; const double Now1 = FPlatformTime::Seconds();
            if (Now1 - LastBail1 > 1.0)
            {
                UE_LOG(LogSmartExtend, Verbose, TEXT("[EXTEND-MP] activation bail: IsA mismatch - building=%s holoBuildClass=%s"),
                    *HitBuilding->GetClass()->GetName(), HologramBuildClass ? *HologramBuildClass->GetName() : TEXT("NULL"));
                LastBail1 = Now1;
            }
            return false;
        }

        if (!IsValidExtendTarget(HitBuilding))
        {
            // [EXTEND-MP] TEMP: target rejected (resource extractor / not a manufacturer / detection-service check).
            static double LastBail2 = 0; const double Now2 = FPlatformTime::Seconds();
            if (Now2 - LastBail2 > 1.0)
            {
                UE_LOG(LogSmartExtend, Verbose, TEXT("[EXTEND-MP] activation bail: IsValidExtendTarget=false for %s"),
                    *HitBuilding->GetClass()->GetName());
                LastBail2 = Now2;
            }
            return false;
        }

        // [EXTEND-MP] TEMP: passed activation validation - Extend SHOULD proceed to build the preview.
        UE_LOG(LogSmartExtend, Verbose, TEXT("[EXTEND-MP] activation PASSED for %s - proceeding to preview"), *HitBuilding->GetName());
    }

    // Issue #274: When Scaled Extend is committed (locked for inspection), suppress re-triggering
    // on a different building of the same type. The player is walking around to inspect the layout
    // and accidentally facing a similar building shouldn't tear down the entire chain.
    // #342: a manual Hold-key pin gets the same protection.
    if ((bExtendCommitted || bExtendManualHold) && CurrentExtendTarget.IsValid() && CurrentExtendTarget.Get() != HitBuilding)
    {
        // Still committed to the original target — just maintain the current preview
        if (CurrentExtendHologram.IsValid())
        {
            RefreshExtension(CurrentExtendHologram.Get());
        }
        return true;
    }

    // Check if we're already extending from this building
    if (CurrentExtendTarget.IsValid() && CurrentExtendTarget.Get() == HitBuilding)
    {
        // Check if the hologram changed (build gun recreated it)
        bool bHologramChanged = !CurrentExtendHologram.IsValid() || CurrentExtendHologram.Get() != SourceHologram;

        if (bHologramChanged)
        {
            // Hologram changed (likely after a build) - clean up and reset EXTEND state
            // User must re-aim at target to reactivate EXTEND
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Hologram changed (build completed?) - resetting EXTEND state"));

            // CRITICAL: Remember which building we just built from to prevent immediate re-activation
            // User must look away first before extending from the same building again
            LastBuiltFromBuilding = HitBuilding;

            // CRITICAL: Destroy old children - they are NOT auto-destroyed with parent
            // because we don't add them via AddChild() (to prevent Construct crash)
            ClearBeltPreviews();

            // Reset EXTEND state - don't automatically recreate belt previews
            // This ensures cleanup after a build
            CurrentExtendTarget.Reset();
            CurrentExtendHologram.Reset();
            bHasValidTarget = false;

            // Don't return true here - let the code fall through to try re-activating
            // if the user is still pointing at a valid target on the NEXT frame
            return false;
        }

        // Already set up, just refresh position
        static double LastRefreshLog = 0;
        double RefreshNow = FPlatformTime::Seconds();
        if (RefreshNow - LastRefreshLog > 2.0)
        {
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Already extending from %s, refreshing"), *HitBuilding->GetName());
            LastRefreshLog = RefreshNow;
        }

        RefreshExtension(SourceHologram);
        return true;
    }

    // Check if this is the building we just built from - prevent immediate re-activation
    // User must look away first before extending from the same building again
    if (LastBuiltFromBuilding.IsValid() && LastBuiltFromBuilding.Get() == HitBuilding)
    {
        // Still pointing at the building we just built from - don't re-activate yet
        // This prevents stale preview holograms from appearing immediately after a build
        return false;
    }

    // User is pointing at a different building - clear the cooldown
    if (LastBuiltFromBuilding.IsValid() && LastBuiltFromBuilding.Get() != HitBuilding)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND: User looked away from last built building - cooldown cleared"));
        LastBuiltFromBuilding.Reset();
    }

    // New target - walk topology
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Checks passed! Walking topology for %s"), *HitBuilding->GetName());

    // NOTE: Orphaned pending builds will be cleared at the start of CreateBeltPreviews().
    // No need to clear here - the flow is: WalkTopology → CreateBeltPreviews (clears all → spawns new).

    const bool bWalked = WalkTopology(HitBuilding);
    {
        // [EXTEND-MP] TEMP: is WalkTopology the bail? (bare building vs client-specific failure)
        static double LastTopoLog = 0; const double Now = FPlatformTime::Seconds();
        if (Now - LastTopoLog > 1.0)
        {
            UE_LOG(LogSmartExtend, Verbose, TEXT("[EXTEND-MP] WalkTopology(%s) = %s"),
                *HitBuilding->GetName(), bWalked ? TEXT("TRUE - proceeding") : TEXT("FALSE - bail (no belts/distributors connected?)"));
            LastTopoLog = Now;
        }
    }
    if (!bWalked)
    {
        return false;
    }

    if (bRestoredCloneTopologyActive)
    {
        ClearRestoredCloneTopologySession(TEXT("Normal Extend activation"));
        if (Subsystem.IsValid())
        {
            if (USFRestoreService* RestoreSvc = Subsystem->GetRestoreService())
            {
                RestoreSvc->ClearActiveRestoreSession(TEXT("Normal Extend activation"));
            }
            Subsystem->ResetCounters();
        }
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Aborted restored topology so normal Extend can activate: target=%s parent=%s"),
            *GetNameSafe(HitBuilding),
            *GetNameSafe(SourceHologram));
    }

    CurrentExtendTarget = HitBuilding;
    bHasValidTarget = true;  // EXTEND is now active!
    bExtendCommitted = false;  // Not committed until first scale action (allows middle-click sampling)
    bExtendManualHold = false;  // #342: fresh Extend starts un-pinned; a deliberate Hold-key press toggles it on

    // Snapshot the current grid counters so we can restore them when Extend deactivates.
    // This prevents normal scaling from inheriting Extend's counter values (X=3, spacing=200, etc.)
    if (!bHasCounterSnapshot && Subsystem.IsValid())
    {
        PreExtendCounterSnapshot = Subsystem->GetCounterState();
        bHasCounterSnapshot = true;
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND: Snapshot pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
            PreExtendCounterSnapshot.GridCounters.X, PreExtendCounterSnapshot.GridCounters.Y,
            PreExtendCounterSnapshot.SpacingX);
    }

    // Pick a valid direction - check if current direction is blocked
    TArray<ESFExtendDirection> ValidDirs = GetValidDirections();
    if (ValidDirs.Num() == 0)
    {
        // No valid directions - both sides blocked, can't extend
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND: Cannot activate - both directions blocked by existing buildings"));
        CurrentExtendTarget.Reset();
        bHasValidTarget = false;
        return false;
    }

    // If current direction is not valid, switch to a valid one
    ESFExtendDirection CurrentDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
    if (!ValidDirs.Contains(CurrentDir))
    {
        CurrentDir = ValidDirs[0];
        if (DetectionService)
        {
            DetectionService->SetExtendDirection(CurrentDir);
        }
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Auto-selected direction %s (other side blocked)"),
            CurrentDir == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: ✅ ACTIVATED - pointing at %s (direction: %s, valid dirs: %d)"),
        *HitBuilding->GetName(),
        CurrentDir == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"),
        ValidDirs.Num());

    // Set flag so we know to cleanup when build gun leaves build mode
    bNeedsFinalCleanup = true;

    // Swap vanilla hologram for our ASFFactoryHologram which overrides CheckValidPlacement
    // to skip clearance checks during extend mode (prevents encroachment disqualifiers)
    AFGHologram* ActiveHologram = SwapToSmartFactoryHologram(SourceHologram);
    if (!ActiveHologram)
    {
        // Swap failed — fall back to vanilla hologram
        ActiveHologram = SourceHologram;
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND: Hologram swap failed, using vanilla hologram"));
    }

    // Track which hologram we've set up for EXTEND
    CurrentExtendHologram = ActiveHologram;

    // Lock the hologram
    ActiveHologram->LockHologramPosition(true);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Locked hologram %s"), *ActiveHologram->GetClass()->GetName());

    FVector SourceLocation = HitBuilding->GetActorLocation();
    FRotator SourceRotation = HitBuilding->GetActorRotation();

    // Get building size from registry (accurate dimensions)
    // NOTE: Do NOT add clearance padding here - scaling doesn't use it and buildings
    // should be flush when extended. The registry sizes are accurate for flush placement.
    USFBuildableSizeRegistry::Initialize();
    FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(HitBuilding->GetClass());
    FVector BuildingSize = Profile.DefaultSize;

    // Calculate offset based on direction
    FVector Offset = GetDirectionOffset(BuildingSize, SourceRotation);

    // Position the hologram at the offset location
    FVector NewLocation = SourceLocation + Offset;

    // Use SetActorLocation directly - our custom hologram blocks SetHologramLocationAndRotation
    // from the build gun when EXTEND is active, so we can position freely
    ActiveHologram->SetActorLocation(NewLocation);
    ActiveHologram->SetActorRotation(SourceRotation);

    // CRITICAL: Toggle lock to force validity recheck after repositioning
    // Without this, the hologram shows as invalid (red) because validity was cached at old position
    ActiveHologram->LockHologramPosition(false);
    ActiveHologram->LockHologramPosition(true);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Positioned hologram at offset (%.1f, %.1f, %.1f) - LOCKED"),
        Offset.X, Offset.Y, Offset.Z);

    // Phase 2: Create belt preview holograms for cloned infrastructure
    // MUST be after hologram is positioned so CloneOffset is correct
    CreateBeltPreviews(ActiveHologram);

    // Initialize grid X to 1 (parent clone only). User scrolls to add more.
    // GetExtendCloneCount maps X directly (X=1 → 1 clone = parent only).
    // MUST be after CurrentExtendHologram is set.
    if (Subsystem.IsValid())
    {
        FSFCounterState State = Subsystem->GetCounterState();
        if (FMath::Abs(State.GridCounters.X) < 1)
        {
            State.GridCounters.X = 1;
            Subsystem->UpdateCounterState(State);
        }
    }

    return true;
}

bool USFExtendService::CanAffordExtendCost(AFGHologram* Hologram, UFGInventoryComponent* Inventory) const
{
    if (!IsValid(Hologram) || !Inventory)
    {
        return true;  // Can't evaluate -> never block.
    }

    // #357: ALWAYS run the parent cost walk every frame, even under No Build Cost. Beyond
    // computing affordability, GetCost(includeChildren=true) walks the child holograms, and
    // that per-frame walk is what KEEPS THE EXTEND CHILD BELT PREVIEWS ALIVE. The internal
    // factory<->distributor belts (role "belt_segment", built via SetSplineDataAndUpdate) are
    // otherwise regenerated to ZERO spline meshes by vanilla's per-frame spline rebuild; the
    // between-distributor lanes (AutoRouteSplineWithNormals) survive regardless. Proven by
    // elimination: affordable-via-materials (walk runs -> belts visible, HMS_OK) and
    // affordable-via-No-Build-Cost (old early-return skipped the walk -> belts missing, also
    // HMS_OK) are BOTH HMS_OK, so the material state is NOT the trigger; running GetCost is.
    // The early-return added in 720e2bc (#324) skipped this walk under free build, which is the
    // sole behavioral difference between the visible and missing cases (the availability loop
    // below is pure inventory reads with no hologram side effect).
    AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(Hologram->GetWorld());
    const TArray<FItemAmount> TotalCost = Hologram->GetCost(/*includeChildren=*/true);

    // Free building (session No Build Cost cheat OR the per-player rule that Advanced Game
    // Settings / Creative Mode toggles) means materials are never required. GetNoBuildCost()
    // is the same method vanilla uses; without this, Creative Mode players got a material-cost
    // request and an unaffordable block on Extend even though the base game would build for free.
    // (We still ran GetCost above for its preview-keep-alive side effect.)
    if (Inventory->GetNoBuildCost())
    {
        return true;
    }

    for (const FItemAmount& Item : TotalCost)
    {
        if (!Item.ItemClass || Item.Amount <= 0)
        {
            continue;
        }
        int32 Available = Inventory->GetNumItems(Item.ItemClass);
        if (CentralStorage)
        {
            Available += CentralStorage->GetNumItemsFromCentralStorage(Item.ItemClass);
        }
        if (Available < Item.Amount)
        {
            return false;
        }
    }
    return true;
}

EHologramMaterialState USFExtendService::ResolveChildPreviewMaterialState(const UObject* WorldContext)
{
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(WorldContext))
    {
        if (SmartSubsystem->IsExtendModeActive())
        {
            if (USFExtendService* ExtendService = SmartSubsystem->GetExtendService())
            {
                return ExtendService->GetExtendChildMaterialState();
            }
        }
    }
    return EHologramMaterialState::HMS_OK;
}

void USFExtendService::RefreshExtension(AFGHologram* SourceHologram, bool bForceRefresh)
{
    if (!bHasValidTarget || !CurrentExtendTarget.IsValid())
    {
        return;
    }

    // Use our swapped hologram if available, otherwise fall back to passed hologram
    AFGHologram* ActiveHologram = nullptr;
    if (bHasSwappedHologram && CurrentExtendHologram.IsValid())
    {
        ActiveHologram = CurrentExtendHologram.Get();
    }
    else
    {
        ActiveHologram = SourceHologram;
    }

    // [EXTEND-MP] Pre-stage the commit while the preview is active (throttled, client-only no-op
    // otherwise): the commit RPC is large and lost the cross-channel race against the tiny
    // construct RPC when staged only at fire time (live 2026-06-10).
    MaybeStageCommitForMP(ActiveHologram);

    if (!IsValid(ActiveHologram))
    {
        return;
    }

    AFGBuildable* HitBuilding = CurrentExtendTarget.Get();

    FVector SourceLocation = HitBuilding->GetActorLocation();
    FRotator SourceRotation = HitBuilding->GetActorRotation();

    // Get building size from registry (accurate dimensions)
    // NOTE: Do NOT add clearance padding - buildings should be flush (same as scaling)
    USFBuildableSizeRegistry::Initialize();
    FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(HitBuilding->GetClass());
    FVector BuildingSize = Profile.DefaultSize;

    // Calculate offset based on current direction
    FVector Offset = GetDirectionOffset(BuildingSize, SourceRotation);

    // Issue #265: Apply spacing, steps, and rotation to clone 1 (parent hologram) when extend is active
    FRotator Clone1Rotation = SourceRotation;  // Default: same rotation as source
    if (Subsystem.IsValid())
    {
        const FSFCounterState& State = Subsystem->GetCounterState();
        ESFExtendDirection CurrentDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
        float DirSign = (CurrentDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;

        bool bRotationActive = !FMath::IsNearlyZero(State.RotationZ);

        if (bRotationActive)
        {
            // Arc/radial placement for clone 1 (CloneIndex=1)
            float ArcLength = BuildingSize.X + static_cast<float>(State.SpacingX);
            float StepRadians = FMath::Abs(FMath::DegreesToRadians(State.RotationZ));
            float Radius = (StepRadians > KINDA_SMALL_NUMBER) ? ArcLength / StepRadians : 0.0f;

            float AngleDeg = State.RotationZ;  // CloneIndex=1 → 1 * RotationZ
            float AngleRad = FMath::DegreesToRadians(AngleDeg);
            float SignRotation = (State.RotationZ >= 0.0f) ? 1.0f : -1.0f;

            // Arc position in local space — matches CalculateRotationOffset pattern:
            //   X = SignX * R * Sin(|θ|)              (direction determines forward/backward)
            //   Y = SignRotation * (R - R*Cos(|θ|))   (NO direction sign — canonical)
            //   Rotation = AngleDeg * DirSign          (sign baked into angle)
            FVector ArcLocal;
            ArcLocal.X = DirSign * Radius * FMath::Sin(FMath::Abs(AngleRad));
            ArcLocal.Y = SignRotation * (Radius - Radius * FMath::Cos(FMath::Abs(AngleRad)));
            ArcLocal.Z = static_cast<float>(State.StepsX);

            Offset = SourceRotation.RotateVector(ArcLocal);
            Clone1Rotation = SourceRotation + FRotator(0.0f, AngleDeg * DirSign, 0.0f);
        }
        else if (State.SpacingX != 0 || State.StepsX != 0)
        {
            // Linear placement with spacing/steps
            FVector SpacingLocal(DirSign * static_cast<float>(State.SpacingX), 0.0f, 0.0f);
            Offset += SourceRotation.RotateVector(SpacingLocal);
            Offset.Z += static_cast<float>(State.StepsX);
        }
    }

    // Update hologram position
    FVector NewLocation = SourceLocation + Offset;

    // CRITICAL: Do a line trace down to find the floor and create a valid hit result
    // Without this, the hologram's internal hit data is stale and CheckValidPlacement fails
    // with "Surface is too uneven" because it has no valid floor normal
    FHitResult FloorHit;
    FVector TraceStart = NewLocation + FVector(0, 0, 100);  // Start slightly above
    FVector TraceEnd = NewLocation - FVector(0, 0, 500);    // Trace down 5m
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(ActiveHologram);
    QueryParams.AddIgnoredActor(HitBuilding);

    if (GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        // Found floor - call SetHologramLocationAndRotation with valid hit result
        // This updates the hologram's internal mIsValidHitResult and floor normal
        ActiveHologram->SetHologramLocationAndRotation(FloorHit);
    }

    // CRITICAL: Ensure hologram stays locked — and (#342) detect a deliberate Hold press here.
    // Extend re-locks every frame, which masks the player's unlock from a once-per-frame subsystem
    // poll. So we catch it at the exact point we'd otherwise blindly re-lock: after activation the
    // hologram is always locked on entry, so finding it UNLOCKED means the player pressed Hold (H) →
    // TOGGLE the manual pin. We then always re-assert the lock so the next frame sees a clean locked
    // state (no repeated toggling), and the lock itself stays on as before. The pin only changes
    // stickiness (bExtendManualHold feeds the look-away keep-alive), never the scale-commit, so scaled
    // Extend / transforms are untouched.
    if (!ActiveHologram->IsHologramLocked())
    {
        bExtendManualHold = !bExtendManualHold;
        UE_LOG(LogSmartExtend, Verbose, TEXT("📌 EXTEND: manual hold %s — preview %s"),
            bExtendManualHold ? TEXT("ENGAGED") : TEXT("RELEASED"),
            bExtendManualHold ? TEXT("pinned (look around to check clearance)") : TEXT("tracking"));
        ActiveHologram->LockHologramPosition(true);
    }

    // For our custom hologram, SetHologramLocationAndRotation is blocked when EXTEND is active
    // So we position directly with SetActorLocation - the build gun can't override because our override blocks it
    ActiveHologram->SetActorLocation(NewLocation);
    ActiveHologram->SetActorRotation(Clone1Rotation);

    // Also update root component to ensure mesh moves
    if (USceneComponent* Root = ActiveHologram->GetRootComponent())
    {
        Root->SetWorldLocation(NewLocation);
        Root->SetWorldRotation(Clone1Rotation);
        Root->MarkRenderStateDirty();
    }

    // Force hologram to be visible
    ActiveHologram->SetActorHiddenInGame(false);

    // Force parent hologram to valid/invalid state based on scaled extend validation.
    // Our ASFFactoryHologram::CheckValidPlacement override skips vanilla's clearance
    // checks during extend mode, so no encroachment disqualifiers get added.
    EHologramMaterialState ExtendMaterialState = EHologramMaterialState::HMS_OK;
    if (!bScaledExtendValid)
    {
        // Lane segment validation failed — keep hologram red and block building
        ActiveHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
        ExtendMaterialState = EHologramMaterialState::HMS_ERROR;
    }
    else
    {
        ActiveHologram->ResetConstructDisqualifiers();

        UFGInventoryComponent* PlayerInventory = nullptr;
        if (Subsystem.IsValid())
        {
            if (AFGPlayerController* PlayerController = Subsystem->GetLastController())
            {
                if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PlayerController->GetPawn()))
                {
                    PlayerInventory = Character->GetInventory();
                }
            }
        }

        if (PlayerInventory)
        {
            ActiveHologram->ValidatePlacementAndCost(PlayerInventory);
        }
        else
        {
            ActiveHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
        }

        // Authoritative extend material state. Do NOT derive this from GetConstructDisqualifiers:
        // child previews mirror this state by adding their own FGCDUnaffordable (see the child
        // CheckValidPlacement overrides), and those child disqualifiers aggregate back onto the
        // parent. Deriving the parent state from the aggregated disqualifiers therefore forms a
        // feedback loop that latches red and never recovers once it has gone red, even after the
        // build becomes affordable again. In extend mode the only real gates are geometry/power
        // (bScaledExtendValid, handled in the !bScaledExtendValid branch above) and depot-aware
        // affordability — so derive purely from those.
        const bool bCanAffordExtend = CanAffordExtendCost(ActiveHologram, PlayerInventory);
        ExtendMaterialState = bCanAffordExtend
            ? EHologramMaterialState::HMS_OK
            : EHologramMaterialState::HMS_ERROR;
        ActiveHologram->SetPlacementMaterialState(ExtendMaterialState);

        // Defensive: if unaffordable, ensure the parent reflects ERROR (the SetPlacementMaterialState
        // above already applied ExtendMaterialState; this guards against a stale OK state).
        if (ExtendMaterialState == EHologramMaterialState::HMS_ERROR
            && ActiveHologram->GetHologramMaterialState() != EHologramMaterialState::HMS_ERROR)
        {
            ActiveHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
        }
    }

    // Publish the authoritative child-preview state. Child holograms read this via
    // USFExtendService::ResolveChildPreviewMaterialState() in their CheckValidPlacement,
    // so the build gun's per-frame validation cascade can no longer reset them to cyan.
    ExtendChildMaterialState = ExtendMaterialState;

    // CRITICAL: Force child holograms back to their intended positions every frame
    // The engine's parent hologram tick resets children to origin via SetHologramLocationAndRotation
    // We counteract this by forcing them back to where we want them

    // Delegate basic position/rotation refresh to HologramService
    if (HologramService)
    {
        HologramService->RefreshChildPositions();
    }

    TSet<AFGHologram*> SyncedMaterialChildren;
    auto SyncExtendPreviewMaterialState = [&](AFGHologram* Child)
    {
        if (!IsValid(Child) || SyncedMaterialChildren.Contains(Child))
        {
            return;
        }

        SyncedMaterialChildren.Add(Child);
        Child->SetActorHiddenInGame(false);
        Child->SetPlacementMaterialState(ExtendMaterialState);

        if (ASFConveyorLiftHologram* LiftChild = Cast<ASFConveyorLiftHologram>(Child))
        {
            LiftChild->ForceApplyHologramMaterial();
        }
        else if (ASFConveyorBeltHologram* BeltChild = Cast<ASFConveyorBeltHologram>(Child))
        {
            BeltChild->ForceApplyHologramMaterial();
        }
        else if (ASFPipelineHologram* PipeChild = Cast<ASFPipelineHologram>(Child))
        {
            PipeChild->ForceApplyHologramMaterial();
        }

        // Prevent child preview validation from immediately repainting spline previews.
        Child->SetActorTickEnabled(false);
    };

    for (FSFScaledExtendClone& Clone : ScaledExtendClones)
    {
        for (auto& SpawnedPair : Clone.SpawnedHolograms)
        {
            if (SpawnedPair.Value.IsValid())
            {
                SyncExtendPreviewMaterialState(SpawnedPair.Value.Get());
            }
        }
    }

    for (AFGHologram* Child : BeltPreviewHolograms)
    {
        SyncExtendPreviewMaterialState(Child);
    }

    for (const auto& SpawnedPair : JsonSpawnedHolograms)
    {
        SyncExtendPreviewMaterialState(SpawnedPair.Value);
    }

    if (HologramService)
    {
        for (AFGHologram* Child : HologramService->GetTrackedChildren())
        {
            SyncExtendPreviewMaterialState(Child);
        }
    }

    // Debug: Verify position (throttled)
    static double LastRefreshPosLog = 0;
    double Now = FPlatformTime::Seconds();
    if (Now - LastRefreshPosLog > 2.0)
    {
        FVector ActualPos = ActiveHologram->GetActorLocation();
        FVector ActorScale = ActiveHologram->GetActorScale3D();

        // Check mesh component positions and visibility
        TArray<UMeshComponent*> MeshComps;
        ActiveHologram->GetComponents(MeshComps);
        FVector MeshWorldPos = FVector::ZeroVector;
        FVector MeshScale = FVector::ZeroVector;
        bool bCompVisible = false;
        bool bHiddenInGame = true;
        int32 MatCount = 0;
        FString MatName = TEXT("none");

        if (MeshComps.Num() > 0 && MeshComps[0])
        {
            MeshWorldPos = MeshComps[0]->GetComponentLocation();
            MeshScale = MeshComps[0]->GetComponentScale();
            bCompVisible = MeshComps[0]->IsVisible();
            bHiddenInGame = MeshComps[0]->bHiddenInGame;
            MatCount = MeshComps[0]->GetNumMaterials();
            if (MatCount > 0 && MeshComps[0]->GetMaterial(0))
            {
                MatName = MeshComps[0]->GetMaterial(0)->GetName();
            }
        }

        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: Pos=(%.0f,%.0f,%.0f) Scale=%.2f Visible=%d Hidden=%d Mat=%s"),
            MeshWorldPos.X, MeshWorldPos.Y, MeshWorldPos.Z,
            MeshScale.X,
            bCompVisible ? 1 : 0,
            bHiddenInGame ? 1 : 0,
            *MatName);

        // Track child hologram positions and refresh belt materials
        for (int32 i = 0; i < BeltPreviewHolograms.Num(); ++i)
        {
            AFGHologram* Child = BeltPreviewHolograms[i];
            if (IsValid(Child))
            {
                // CRITICAL: Refresh material state for belt children
                // Vanilla's hologram system may reset material state, so we need to re-apply
                if (ASFConveyorBeltHologram* BeltChild = Cast<ASFConveyorBeltHologram>(Child))
                {
                    // Re-apply current parent state to spline meshes.
                    BeltChild->SetPlacementMaterialState(ExtendMaterialState);
                    BeltChild->ForceApplyHologramMaterial();

                    // Ensure visibility
                    BeltChild->SetActorHiddenInGame(false);
                    TArray<USplineMeshComponent*> SplineMeshComps;
                    BeltChild->GetComponents<USplineMeshComponent>(SplineMeshComps);

                    // Log spline mesh state for debugging (only first belt, every 2 seconds)
                    static double LastBeltMeshLog = 0;
                    if (i == 1 && Now - LastBeltMeshLog > 2.0)  // Child[1] is first belt
                    {
                        LastBeltMeshLog = Now;
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🎯 BELT REFRESH: %s has %d SplineMeshComponents"),
                            *BeltChild->GetName(), SplineMeshComps.Num());
                        for (int32 SMIdx = 0; SMIdx < SplineMeshComps.Num() && SMIdx < 2; SMIdx++)
                        {
                            USplineMeshComponent* SMC = SplineMeshComps[SMIdx];
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🎯 BELT REFRESH:   SMC[%d]: Mesh=%s, Visible=%d, WorldPos=%s"),
                                SMIdx,
                                SMC->GetStaticMesh() ? *SMC->GetStaticMesh()->GetName() : TEXT("NULL"),
                                SMC->IsVisible() ? 1 : 0,
                                *SMC->GetComponentLocation().ToString());
                        }
                    }

                    for (USplineMeshComponent* SMC : SplineMeshComps)
                    {
                        SMC->SetVisibility(true, true);
                        SMC->SetHiddenInGame(false);
                    }
                }

                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND Child[%d]: Pos=%s Hidden=%d MatState=%d"),
                    i, *Child->GetActorLocation().ToString(),
                    Child->IsHidden() ? 1 : 0,
                    (int32)Child->GetHologramMaterialState());
            }
            else
            {
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND Child[%d]: INVALID/DESTROYED"), i);
            }
        }

        LastRefreshPosLog = Now;
    }
}

void USFExtendService::CleanupExtension(AFGHologram* SourceHologram)
{
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: CleanupExtension called for %s"),
        SourceHologram ? *SourceHologram->GetName() : TEXT("nullptr"));

    if (bRestoredCloneTopologyActive)
    {
        ClearScaledExtendClones();
        ClearBeltPreviews();
        CurrentExtendTarget.Reset();
        CurrentExtendHologram.Reset();
        bHasValidTarget = false;
        bExtendCommitted = false;
        bHasCounterSnapshot = false;

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] CleanupExtension preserved staged restored topology and counters"));
        return;
    }

    // Restore pre-Extend counter snapshot so normal scaling isn't polluted
    if (bHasCounterSnapshot && Subsystem.IsValid())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: Restoring pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
            PreExtendCounterSnapshot.GridCounters.X, PreExtendCounterSnapshot.GridCounters.Y,
            PreExtendCounterSnapshot.SpacingX);
        Subsystem->UpdateCounterState(PreExtendCounterSnapshot);
        bHasCounterSnapshot = false;
    }

    // Clear visual preview holograms (always)
    ClearScaledExtendClones();  // Issue #265: Clean up scaled extend clones
    ClearBeltPreviews();

    // NOTE: Do NOT clear pending belt builds here!
    // When user BUILDS, OnActorSpawned defers belt construction to next tick.
    // CleanupExtension is called BEFORE that deferred tick executes.
    // BuildPendingBelts() will clear them after building.
    // If user CANCELS (no factory spawned), the pending builds will be orphaned
    // but harmless - they'll be cleared on next EXTEND activation.

    // CRITICAL: Set LastBuiltFromBuilding BEFORE resetting CurrentExtendTarget
    // This prevents immediate re-activation if user is still pointing at the same building
    if (CurrentExtendTarget.IsValid())
    {
        LastBuiltFromBuilding = CurrentExtendTarget.Get();
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: Set cooldown on %s to prevent immediate re-activation"),
            *CurrentExtendTarget->GetName());
    }

    CurrentExtendTarget.Reset();
    CurrentExtendHologram.Reset();
    ClearTopology();

    // CRITICAL: Reset state flags so next build doesn't try to extend from same location
    bHasValidTarget = false;
    bExtendCommitted = false;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: State fully cleared (bHasValidTarget=false, pending belts preserved for deferred build)"));
}

void USFExtendService::CheckAndPerformFinalCleanup()
{
    if (!bNeedsFinalCleanup)
    {
        return;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: Performing final cleanup (build gun left build mode)"));

    // Force destroy any remaining preview holograms
    ClearScaledExtendClones();  // Issue #265: Clean up scaled extend clones
    ClearBeltPreviews();

    // Reset all state
    CurrentExtendTarget.Reset();
    CurrentExtendHologram.Reset();
    ClearTopology();
    bHasValidTarget = false;
    LastBuiltFromBuilding.Reset();

    // Clear the flag
    bNeedsFinalCleanup = false;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔧 EXTEND: Final cleanup complete"));
}

// ==================== Build Gun Hologram Swapping ====================

AFGBuildGun* USFExtendService::GetPlayerBuildGun() const
{
    if (!Subsystem.IsValid())
    {
        return nullptr;
    }

    UWorld* World = Subsystem->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Get the local player controller
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC)
    {
        return nullptr;
    }

    AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
    if (!Character)
    {
        return nullptr;
    }

    // Get the build gun from the character's equipment
    // The build gun is stored as the active equipment when in build mode
    return Character->GetBuildGun();
}

UFGBuildGunStateBuild* USFExtendService::GetBuildGunBuildState(AFGBuildGun* BuildGun) const
{
    if (!BuildGun)
    {
        return nullptr;
    }

    // Get the build state from the build gun
    return Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
}

ASFFactoryHologram* USFExtendService::SwapToSmartFactoryHologram(AFGHologram* VanillaHologram, bool bTrackAsExtendSwap)
{
    if (!VanillaHologram || !VanillaHologram->IsValidLowLevel())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Invalid vanilla hologram"));
        return nullptr;
    }

    // Only swap factory holograms
    AFGFactoryHologram* FactoryHolo = Cast<AFGFactoryHologram>(VanillaHologram);
    if (!FactoryHolo)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Not a factory hologram - %s"),
            *VanillaHologram->GetClass()->GetName());
        return nullptr;
    }

    // Get the build gun and its build state
    AFGBuildGun* BuildGun = GetPlayerBuildGun();
    if (!BuildGun)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Could not get build gun"));
        return nullptr;
    }

    UFGBuildGunStateBuild* BuildState = GetBuildGunBuildState(BuildGun);
    if (!BuildState)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Could not get build state"));
        return nullptr;
    }

    // Get world for spawning
    UWorld* World = VanillaHologram->GetWorld();
    if (!World)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: No world"));
        return nullptr;
    }

    // Verify the vanilla hologram has a build class
    if (!VanillaHologram->GetBuildClass())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Vanilla hologram has no BuildClass"));
        return nullptr;
    }

    // Use SpawnActorDeferred so we can initialize BEFORE BeginPlay is called
    ASFFactoryHologram* CustomHologram = World->SpawnActorDeferred<ASFFactoryHologram>(
        ASFFactoryHologram::StaticClass(),
        FTransform(VanillaHologram->GetActorRotation(), VanillaHologram->GetActorLocation()),
        BuildGun,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (!CustomHologram)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Failed to spawn deferred custom hologram"));
        return nullptr;
    }

    // CRITICAL: Initialize from the vanilla hologram BEFORE BeginPlay
    // This copies mBuildClass, mRecipe, etc. which are required for BeginPlay
    CustomHologram->InitializeFromHologram(VanillaHologram);

    // [#331] Carry the Blueprint Designer context across the swap (vanilla copies it onto
    // every constructed buildable; dropping it makes builds invisible to the designer).
    if (AFGBuildableBlueprintDesigner* Designer = VanillaHologram->GetBlueprintDesigner())
    {
        CustomHologram->SetInsideBlueprintDesigner(Designer);
    }

    // Copy mConstructionInstigator (private) via reflection — needed for build FX
    FProperty* InstigatorProp = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mConstructionInstigator"));
    if (InstigatorProp)
    {
        InstigatorProp->CopyCompleteValue(
            InstigatorProp->ContainerPtrToValuePtr<void>(CustomHologram),
            InstigatorProp->ContainerPtrToValuePtr<void>(VanillaHologram)
        );
    }

    // Now finish spawning - this will call BeginPlay with mBuildClass properly set
    CustomHologram->FinishSpawning(FTransform(VanillaHologram->GetActorRotation(), VanillaHologram->GetActorLocation()));

    // [#461] Carry the vanilla hologram's blueprint-placeability onto the swapped hologram.
    // mCanBePlacedInBlueprintDesigner is "initialized on spawn" by the build gun's hologram setup
    // (FGHologram.h) - which this deferred SpawnActorDeferred path bypasses - so without this the
    // swapped ASFFactoryHologram keeps the CDO default (not placeable) and vanilla adds
    // FGCDNotAllowedInBlueprint inside a Blueprint Designer. RestoreOriginalHologram doesn't swap
    // back, so the lingering swapped hologram carries the bad flag even after Extend deactivates
    // (repro: middle-click Sample an Extend-valid factory, then place into a designer). Copied AFTER
    // FinishSpawning so nothing in the spawn/BeginPlay path can clobber it.
    if (FProperty* BpPlaceableProp = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mCanBePlacedInBlueprintDesigner")))
    {
        BpPlaceableProp->CopyCompleteValue(
            BpPlaceableProp->ContainerPtrToValuePtr<void>(CustomHologram),
            BpPlaceableProp->ContainerPtrToValuePtr<void>(VanillaHologram)
        );
    }

    // The build state has mHologram as a UPROPERTY - use reflection to set it
    // This is the key: we replace the build gun's hologram pointer with our custom one
    FProperty* HologramProp = BuildState->GetClass()->FindPropertyByName(TEXT("mHologram"));
    if (HologramProp)
    {
        void* ValuePtr = HologramProp->ContainerPtrToValuePtr<void>(BuildState);
        if (ValuePtr)
        {
            // Cast to object property and set the value
            FObjectProperty* ObjProp = CastField<FObjectProperty>(HologramProp);
            if (ObjProp)
            {
                ObjProp->SetObjectPropertyValue(ValuePtr, CustomHologram);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND SWAP: ✅ Set mHologram via reflection"));
            }
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Could not find mHologram property"));
        CustomHologram->Destroy();
        return nullptr;
    }

    // Destroy the vanilla hologram
    VanillaHologram->Destroy();

    // Track the swap (both locally and in HologramService for future migration).
    // The MP scaling spec path reuses this swap but must NOT put Extend into "swapped" state
    // (no Extend target is active and RestoreOriginalHologram must stay a no-op).
    if (bTrackAsExtendSwap)
    {
        SwappedHologram = CustomHologram;
        bHasSwappedHologram = true;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND SWAP: ✅ Successfully swapped to ASFFactoryHologram"));

    return CustomHologram;
}

// (SwapToSmartFoundationHologram was removed: the MP spec-construction path is now class-agnostic
// via SML hooks - see USFGameInstanceModule::RegisterSpecConstructionHooks - so no scaling swap of
// any kind is needed. The factory swap above remains for EXTEND.)

void USFExtendService::RestoreOriginalHologram()
{
    if (!bHasSwappedHologram)
    {
        return;
    }

    // We don't actually need to restore - the build gun will spawn a new hologram
    // when the recipe is re-selected or the state changes.
    // Just clean up our tracking.

    if (SwappedHologram.IsValid())
    {
        // Unlock the hologram before cleanup
        SwappedHologram->LockHologramPosition(false);
    }

    SwappedHologram.Reset();
    bHasSwappedHologram = false;

    // Also restore in HologramService
    if (HologramService)
    {
        HologramService->RestoreOriginalHologram();
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND SWAP: Hologram swap state cleared"));
}

// ==================== Phase 2: Infrastructure Cloning (delegates to HologramService) ====================

void USFExtendService::CreateBeltPreviews(AFGHologram* ParentHologram)
{
    if (!ParentHologram || !GetCurrentTopology().bIsValid || !GetCurrentTopology().SourceBuilding.IsValid())
    {
        const FSFExtendTopology& Topology = GetCurrentTopology();
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] CreateBeltPreviews skipped: parent=%s topologyValid=%d sourceBuilding=%s"),
            *GetNameSafe(ParentHologram),
            Topology.bIsValid ? 1 : 0,
            *GetNameSafe(Topology.SourceBuilding.Get()));
        return;
    }

    // DIAGNOSTIC: Capture preview snapshot ONCE when EXTEND previews are first created
    if (!HasPreviewSnapshot())
    {
        CapturePreviewSnapshot();
    }

    // Delegate to HologramService
    if (HologramService)
    {
        HologramService->SetCurrentParentHologram(ParentHologram);
        HologramService->CreateBeltPreviews(ParentHologram);

        // Copy references for backwards compatibility with existing code
        StoredCloneTopology = HologramService->GetStoredCloneTopology();
        JsonSpawnedHolograms = HologramService->GetJsonSpawnedHolograms();
        if (StoredCloneTopology.IsValid())
        {
            LastCloneTopology = MakeShared<FSFCloneTopology>(*StoredCloneTopology);
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] CreateBeltPreviews cached clone topology: storedChildren=%d spawnedHolograms=%d trackedChildren=%d parent=%s"),
                StoredCloneTopology->ChildHolograms.Num(),
                JsonSpawnedHolograms.Num(),
                HologramService->GetTrackedChildren().Num(),
                *GetNameSafe(ParentHologram));
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                TEXT("[SmartRestore][Extend] CreateBeltPreviews did not receive StoredCloneTopology: spawnedHolograms=%d trackedChildren=%d parent=%s"),
                JsonSpawnedHolograms.Num(),
                HologramService->GetTrackedChildren().Num(),
                *GetNameSafe(ParentHologram));
        }

        // Issue #288: Validate cloned power pole capacity for pump wiring. Runs
        // for the single-clone Extend preview; the scaled-extend path re-runs
        // this check at line ~6308 AND'd with lane validation.
        bScaledExtendValid = ValidatePowerCapacity();
        if (!bScaledExtendValid && CurrentExtendHologram.IsValid())
        {
            CurrentExtendHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
        }

        // Issue #229: Populate power pole wiring data from cached topology
        PowerPoleWiringData.Empty();
        if (TopologyService && TopologyService->HasValidTopology())
        {
            const FSFExtendTopology& Topology = TopologyService->GetCurrentTopology();
            for (int32 i = 0; i < Topology.PowerPoles.Num(); i++)
            {
                const FSFPowerChainNode& PowerNode = Topology.PowerPoles[i];
                if (PowerNode.PowerPole.IsValid())
                {
                    FString CloneId = FString::Printf(TEXT("power_pole_%d"), i);
                    FSFSourcePoleWiringData WiringData;
                    WiringData.SourcePole = PowerNode.PowerPole;
                    WiringData.bSourceHasFreeConnections = PowerNode.bSourceHasFreeConnections;
                    WiringData.SourceFreeConnections = PowerNode.SourceFreeConnections;
                    WiringData.MaxConnections = PowerNode.MaxConnections;
                    PowerPoleWiringData.Add(CloneId, WiringData);

                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND: Stored power pole wiring data for %s (source=%s, free=%d)"),
                        *CloneId, *PowerNode.PowerPole->GetName(), PowerNode.SourceFreeConnections);
                }
            }
        }

        // Copy tracked children for RefreshExtension
        BeltPreviewHolograms.Empty();
        for (AFGHologram* Child : HologramService->GetTrackedChildren())
        {
            BeltPreviewHolograms.Add(Child);
            if (FVector* Pos = HologramService->GetIntendedPosition(Child))
            {
                ChildIntendedPositions.Add(Child, *Pos);
            }
            if (FRotator* Rot = HologramService->GetIntendedRotation(Child))
            {
                ChildIntendedRotations.Add(Child, *Rot);
            }
        }
    }
}


void USFExtendService::ClearBeltPreviews()
{
    // Delegate to HologramService
    if (HologramService)
    {
        HologramService->SetCurrentParentHologram(CurrentExtendHologram.Get());
        HologramService->ClearBeltPreviews();
    }

    // Clear local tracking (for backwards compatibility)
    BeltPreviewHolograms.Empty();
    ChildIntendedPositions.Empty();
    ChildIntendedRotations.Empty();

    // Clear connection wiring maps
    ClearConnectionWiringMaps();
}

void USFExtendService::ClearConnectionWiringMaps()
{
    if (WiringService)
    {
        WiringService->ClearConnectionWiringMaps();
    }
}

void USFExtendService::ProvideFloorHitResult(AFGHologram* Hologram, const FVector& Location)
{
    // Create a synthetic hit result based on the parent hologram's valid floor
    // This is simpler than tracing - we just use the parent's floor position
    // The parent is already validated, so we know the floor is valid

    FHitResult SyntheticHit;
    SyntheticHit.bBlockingHit = true;
    SyntheticHit.Location = Location;
    SyntheticHit.ImpactPoint = Location;
    SyntheticHit.ImpactNormal = FVector::UpVector;  // Flat floor
    SyntheticHit.Normal = FVector::UpVector;
    SyntheticHit.TraceStart = Location + FVector(0, 0, 100.0f);
    SyntheticHit.TraceEnd = Location - FVector(0, 0, 100.0f);
    SyntheticHit.Distance = 100.0f;

    // Provide the synthetic hit result to the hologram
    Hologram->SetHologramLocationAndRotation(SyntheticHit);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("EXTEND: Provided synthetic floor hit to hologram %s at %s"),
        *Hologram->GetName(), *Location.ToString());
}

void USFExtendService::ConnectAllChainElements(AFGBuildableFactory* NewFactory)
{
    if (WiringService)
    {
        WiringService->ConnectAllChainElements(NewFactory);
    }
}

bool USFExtendService::HasPendingPostBuildWiring() const
{
    return WiringService ? WiringService->HasPendingPostBuildWiring() : false;
}

void USFExtendService::RegisterBuiltConveyor(int32 ChainId, int32 ChainIndex, AFGBuildableConveyorBase* BuiltConveyor, bool bIsInputChain)
{
    if (WiringService) { WiringService->RegisterBuiltConveyor(ChainId, ChainIndex, BuiltConveyor, bIsInputChain); }
}

AFGBuildableConveyorBase* USFExtendService::GetBuiltConveyor(int32 ChainId, int32 ChainIndex) const
{
    return WiringService ? WiringService->GetBuiltConveyor(ChainId, ChainIndex) : nullptr;
}

void USFExtendService::RegisterBuiltDistributor(int32 ChainId, AFGBuildable* BuiltDistributor)
{
    if (WiringService) { WiringService->RegisterBuiltDistributor(ChainId, BuiltDistributor); }
}

AFGBuildable* USFExtendService::GetBuiltDistributor(int32 ChainId) const
{
    return WiringService ? WiringService->GetBuiltDistributor(ChainId) : nullptr;
}

FName USFExtendService::GetDistributorConnectorName(int32 ChainId) const
{
    return WiringService ? WiringService->GetDistributorConnectorName(ChainId) : NAME_None;
}

void USFExtendService::SetDistributorConnectorName(int32 ChainId, FName ConnectorName)
{
    if (WiringService) { WiringService->SetDistributorConnectorName(ChainId, ConnectorName); }
}

void USFExtendService::RegisterBuiltJunction(int32 ChainId, AFGBuildable* BuiltJunction)
{
    if (WiringService) { WiringService->RegisterBuiltJunction(ChainId, BuiltJunction); }
}

void USFExtendService::RegisterBuiltPipe(int32 ChainId, int32 ChainIndex, AFGBuildablePipeline* BuiltPipe, bool bIsInputChain)
{
    if (WiringService) { WiringService->RegisterBuiltPipe(ChainId, ChainIndex, BuiltPipe, bIsInputChain); }
}

int32 USFExtendService::CopyDistributorConfigurations()
{
    return WiringService ? WiringService->CopyDistributorConfigurations() : 0;
}

void USFExtendService::WireBuiltChildConnections(AFGBuildableFactory* NewFactory)
{
    if (WiringService) { WiringService->WireBuiltChildConnections(NewFactory); }
}

void USFExtendService::WireManifoldConnections(AFGBuildableFactory* SourceFactory, AFGBuildableFactory* CloneFactory)
{
    if (WiringService) { WiringService->WireManifoldConnections(SourceFactory, CloneFactory); }
}

void USFExtendService::WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId)
{
    if (WiringService) { WiringService->WireManifoldPipe(BuiltPipe, SourceConnector, CloneChainId); }
}

void USFExtendService::WireManifoldBelt(AFGBuildableConveyorBelt* BuiltBelt, UFGFactoryConnectionComponent* SourceConnector, int32 CloneChainId)
{
    if (WiringService) { WiringService->WireManifoldBelt(BuiltBelt, SourceConnector, CloneChainId); }
}

bool USFExtendService::CreateManifoldBelt(UFGFactoryConnectionComponent* FromConnector, UFGFactoryConnectionComponent* ToConnector)
{
    return WiringService ? WiringService->CreateManifoldBelt(FromConnector, ToConnector) : false;
}

bool USFExtendService::CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector)
{
    return WiringService ? WiringService->CreateManifoldPipe(FromConnector, ToConnector) : false;
}

AFGBuildableFactory* USFExtendService::GetSourceFactory() const
{
    return WiringService ? WiringService->GetSourceFactory() : nullptr;
}

void USFExtendService::CapturePreviewSnapshot()
{
    if (DiagnosticsService)
    {
        DiagnosticsService->CapturePreviewSnapshot();
    }
}

void USFExtendService::CapturePostBuildSnapshotAndLogDiff()
{
    if (DiagnosticsService)
    {
        DiagnosticsService->CapturePostBuildSnapshotAndLogDiff();
    }
}

bool USFExtendService::HasPreviewSnapshot() const
{
    return DiagnosticsService && DiagnosticsService->HasPreviewSnapshot();
}

// ==================== Phase 5/6: JSON-Based Post-Build Wiring ====================

int32 USFExtendService::GenerateAndExecuteWiring(AFGBuildableFactory* NewFactory)
{
    return WiringService ? WiringService->GenerateAndExecuteWiring(NewFactory) : 0;
}

void USFExtendService::RegisterJsonBuiltActor(const FString& CloneId, AActor* BuiltActor)
{
    if (WiringService) { WiringService->RegisterJsonBuiltActor(CloneId, BuiltActor); }
}

AFGBuildable* USFExtendService::GetBuiltActorByCloneId(const FString& CloneId) const
{
    return WiringService ? WiringService->GetBuiltActorByCloneId(CloneId) : nullptr;
}

AFGBuildable* USFExtendService::GetSourceBuildableByName(const FString& ActorName) const
{
    return WiringService ? WiringService->GetSourceBuildableByName(ActorName) : nullptr;
}

int32 USFExtendService::GetExtendCloneCount() const
{
    if (!Subsystem.IsValid())
    {
        return 0;
    }

    // X counter = total clones (parent + additional). X=1 means source only (0 clones).
    // X=2 means 2 clones (parent + 1 additional), X=3 means 3, etc.
    const FSFCounterState& State = Subsystem->GetCounterState();
    int32 XCount = FMath::Abs(State.GridCounters.X);
    return FMath::Max(0, XCount);  // Direct: X=2 → 2 clones
}

int32 USFExtendService::GetExtendRowCount() const
{
    if (!Subsystem.IsValid())
    {
        return 1;
    }

    // Y counter controls number of rows. Y=1 = single row (current behavior).
    const FSFCounterState& State = Subsystem->GetCounterState();
    return FMath::Max(1, FMath::Abs(State.GridCounters.Y));
}

bool USFExtendService::IsScaledExtendActive() const
{
    return bHasValidTarget && (GetExtendCloneCount() > 1 || GetExtendRowCount() > 1);
}

void USFExtendService::OnScaledExtendStateChanged()
{
    if (ScaledService)
    {
        ScaledService->OnScaledExtendStateChanged();
    }
}

void USFExtendService::ClearScaledExtendClones()
{
    if (ScaledService)
    {
        ScaledService->ClearScaledExtendClones();
    }
}

bool USFExtendService::ValidatePowerCapacity()
{
    return ScaledService ? ScaledService->ValidatePowerCapacity() : true;
}
