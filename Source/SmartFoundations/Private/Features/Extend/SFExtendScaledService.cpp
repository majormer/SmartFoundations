// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendScaledService Implementation
 *
 * See header for overview. Moved verbatim from SFExtendService (T1 slice E1); the only
 * changes are accessing the owning service shared state through Owner-> and renaming
 * the enclosing class. Logic is identical to the pre-split version.
 */

#include "Features/Extend/SFExtendScaledService.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendDiagnosticsService.h"
#include "Features/Extend/SFExtendRestoreReplayService.h"
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
#include "Buildables/FGBuildableBlueprintDesigner.h"  // [#366] IsLocationInsideDesigner clamp
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

USFExtendScaledService::USFExtendScaledService()
{
}

void USFExtendScaledService::Initialize(USFExtendService* InExtendService)
{
    Owner = InExtendService;
}

void USFExtendScaledService::Shutdown()
{
    Owner = nullptr;
}

void USFExtendScaledService::OnScaledExtendStateChanged()
{
    if (!Owner->bHasValidTarget || !Owner->CurrentExtendTarget.IsValid() || !Owner->CurrentExtendHologram.IsValid())
    {
        return;
    }

    // First scale action commits to Extend (enables sticky behavior)
    if (!Owner->bExtendCommitted)
    {
        Owner->bExtendCommitted = true;
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Committed (first scale action)"));
    }

    int32 CloneCount = Owner->GetExtendCloneCount();
    int32 RowCount = Owner->GetExtendRowCount();

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: State changed - X=%d (clones=%d), Y=%d (rows=%d)"),
        CloneCount + 1, CloneCount, RowCount, RowCount);

    // Clear all existing previews (both primary clone infrastructure and scaled extend clones)
    Owner->ClearBeltPreviews();
    ClearScaledExtendClones();

    // CRITICAL: Refresh hologram position BEFORE creating belt previews.
    // Owner->RefreshExtension applies spacing/steps to clone 1's position.
    // Owner->CreateBeltPreviews derives infrastructure positions from the parent hologram's
    // current location (CloneOffset = HologramPos - SourcePos), so the hologram
    // must be at the correct position first.
    // Force refresh even if inspection lock is active - grid changes should always apply
    Owner->RefreshExtension(Owner->CurrentExtendHologram.Get(), true);

    // Now recreate the primary clone's infrastructure (clone 1 = parent hologram position)
    // This is the existing single-clone Extend behavior
    Owner->CreateBeltPreviews(Owner->CurrentExtendHologram.Get());

    // Apply rigid body rotation to clone 1's infrastructure when rotation is active.
    // Owner->CreateBeltPreviews positions infrastructure based on source topology + translation,
    // but doesn't rotate. The factory building was rotated in Owner->RefreshExtension(),
    // so infrastructure must rotate to maintain rigid topology.
    //
    // We use the same pattern as SpawnScaledExtendPreviews (Step 2.25 + IntendedPositions):
    // 1. Rotate topology data (positions, spline points, normals)
    // 2. Reposition spawned holograms from rotated topology
    // 3. Regenerate spline meshes at correct positions
    if (Owner->Subsystem.IsValid() && Owner->StoredCloneTopology.IsValid())
    {
        const FSFCounterState& State = Owner->Subsystem->GetCounterState();
        if (!FMath::IsNearlyZero(State.RotationZ))
        {
            ESFExtendDirection CurDir = Owner->DetectionService ? Owner->DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
            float DirSignRot = (CurDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;
            FRotator Clone1RotOffset(0.0f, State.RotationZ * DirSignRot, 0.0f);
            FVector FactoryCenter = Owner->CurrentExtendHologram->GetActorLocation();

            // Step 1: Rotate topology data (same as Step 2.25 for additional clones)
            // WorldOffset from source to clone 1 (used to determine clone-side of lane segments)
            FVector Clone1WorldOffset = FactoryCenter - Owner->CurrentExtendTarget->GetActorLocation();

            for (FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
            {
                if (Holo.bIsLaneSegment)
                {
                    // Lane segments are ADAPTIVE — only rotate the clone-side endpoint.
                    // Source-side stays fixed. (Same logic as Step 2.25)
                    if (Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
                    {
                        FVector StartWorld = Holo.SplineData.Points[0].World.ToFVector();
                        FVector EndWorld = Holo.SplineData.Points.Last().World.ToFVector();
                        FVector LaneDir = EndWorld - StartWorld;
                        bool bCloneAtEnd = (FVector::DotProduct(LaneDir, Clone1WorldOffset) > 0.0f);

                        if (bCloneAtEnd)
                        {
                            FVector RelEnd = EndWorld - FactoryCenter;
                            FVector RotatedEnd = FactoryCenter + Clone1RotOffset.RotateVector(RelEnd);
                            Holo.SplineData.Points.Last().World = FSFVec3(RotatedEnd);
                            Holo.LaneEndNormal = FSFVec3(Clone1RotOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                        }
                        else
                        {
                            FVector RelStart = StartWorld - FactoryCenter;
                            FVector RotatedStart = FactoryCenter + Clone1RotOffset.RotateVector(RelStart);
                            Holo.SplineData.Points[0].World = FSFVec3(RotatedStart);
                            Holo.LaneStartNormal = FSFVec3(Clone1RotOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                        }

                        // Recalculate transform from updated endpoints
                        FVector NewStart = Holo.SplineData.Points[0].World.ToFVector();
                        FVector NewEnd = Holo.SplineData.Points.Last().World.ToFVector();
                        Holo.SplineData.Length = FVector::Dist(NewStart, NewEnd);
                        Holo.Transform = FSFTransform(NewStart, (NewEnd - NewStart).Rotation());
                    }
                    continue;
                }

                // Rotate position around factory center
                FVector HoloPos = Holo.Transform.Location.ToFVector();
                FVector RelPos = HoloPos - FactoryCenter;
                FVector RotatedPos = FactoryCenter + Clone1RotOffset.RotateVector(RelPos);
                FRotator RotatedRot = Holo.Transform.Rotation.ToFRotator() + Clone1RotOffset;
                Holo.Transform = FSFTransform(RotatedPos, RotatedRot);

                // Rotate spline world positions
                if (Holo.bHasSplineData)
                {
                    for (FSFSplinePoint& Point : Holo.SplineData.Points)
                    {
                        FVector WorldPt = Point.World.ToFVector();
                        FVector RelPt = WorldPt - FactoryCenter;
                        Point.World = FSFVec3(FactoryCenter + Clone1RotOffset.RotateVector(RelPt));
                    }
                }

                // Rotate lift data
                if (Holo.bHasLiftData)
                {
                    FVector BotPos = Holo.LiftData.BottomTransform.Location.ToFVector();
                    FVector BotRel = BotPos - FactoryCenter;
                    Holo.LiftData.BottomTransform.Location = FSFVec3(FactoryCenter + Clone1RotOffset.RotateVector(BotRel));
                    Holo.LiftData.BottomTransform.Rotation = FSFRot3(Holo.LiftData.BottomTransform.Rotation.ToFRotator() + Clone1RotOffset);
                }
            }

            // Step 2: Reposition spawned holograms from rotated topology + regenerate meshes
            // Build IntendedPositions maps from rotated topology (same pattern as SpawnScaledExtendPreviews)
            TMap<FString, FVector> IntendedPositions;
            TMap<FString, FRotator> IntendedRotations;
            TMap<FString, const FSFCloneHologram*> HologramDataMap;
            for (const FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
            {
                IntendedPositions.Add(Holo.HologramId, Holo.Transform.Location.ToFVector());
                IntendedRotations.Add(Holo.HologramId, Holo.Transform.Rotation.ToFRotator());
                HologramDataMap.Add(Holo.HologramId, &Holo);
            }

            // Reposition each spawned hologram
            for (const auto& Pair : Owner->JsonSpawnedHolograms)
            {
                AFGHologram* SpawnedHolo = Pair.Value;
                if (!IsValid(SpawnedHolo)) continue;

                FVector IntendedPos = SpawnedHolo->GetActorLocation();
                FRotator IntendedRot = SpawnedHolo->GetActorRotation();
                if (FVector* FoundPos = IntendedPositions.Find(Pair.Key))
                    IntendedPos = *FoundPos;
                if (FRotator* FoundRot = IntendedRotations.Find(Pair.Key))
                    IntendedRot = *FoundRot;

                // Reposition actor
                SpawnedHolo->SetActorLocation(IntendedPos);
                SpawnedHolo->SetActorRotation(IntendedRot);
                if (USceneComponent* Root = SpawnedHolo->GetRootComponent())
                {
                    Root->SetWorldLocation(IntendedPos);
                    Root->SetWorldRotation(IntendedRot);
                }

                // Force component transforms to update before spline regeneration.
                // Without Owner, the spline component may use stale transforms when
                // converting local → world during mesh generation.
                SpawnedHolo->UpdateComponentTransforms();

                // Regenerate spline meshes at correct position
                // (same pattern as SpawnScaledExtendPreviews lines 5922-5980)
                if (const FSFCloneHologram** HoloDataPtr = HologramDataMap.Find(Pair.Key))
                {
                    const FSFCloneHologram& HoloData = **HoloDataPtr;

                    if (ASFConveyorBeltHologram* Belt = Cast<ASFConveyorBeltHologram>(SpawnedHolo))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            // Lane segments: use AutoRouteSplineWithNormals
                            FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            FVector EndNormal = HoloData.LaneEndNormal.ToFVector();
                            Belt->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
                            Belt->TriggerMeshGeneration();
                            Belt->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
                            // Regular belt: inline convert FSFSplineData → TArray<FSplinePointData>
                            TArray<FSplinePointData> SplinePoints;
                            for (const FSFSplinePoint& Point : HoloData.SplineData.Points)
                            {
                                FSplinePointData PointData;
                                PointData.Location = Point.Local.ToFVector();
                                PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
                                PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
                                SplinePoints.Add(PointData);
                            }
                            Belt->SetSplineDataAndUpdate(SplinePoints);
                            Belt->TriggerMeshGeneration();
                            Belt->ForceApplyHologramMaterial();
                        }
                    }
                    else if (ASFPipelineHologram* Pipe = Cast<ASFPipelineHologram>(SpawnedHolo))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            // Pipe lane segments: use TryUseBuildModeRouting with rotated world positions
                            FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            FVector EndNormal = HoloData.LaneEndNormal.ToFVector();

                            Pipe->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal);
                            Pipe->TriggerMeshGeneration();
                            Pipe->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
                            // Regular pipe segments: use raw spline data
                            TArray<FSplinePointData> SplinePoints;
                            for (const FSFSplinePoint& Point : HoloData.SplineData.Points)
                            {
                                FSplinePointData PointData;
                                PointData.Location = Point.Local.ToFVector();
                                PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
                                PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
                                SplinePoints.Add(PointData);
                            }
                            Pipe->SetSplineDataAndUpdate(SplinePoints);
                            Pipe->TriggerMeshGeneration();
                            Pipe->ForceApplyHologramMaterial();
                        }
                    }
                }

                // Update tracking
                if (Owner->HologramService)
                {
                    Owner->HologramService->TrackChildHologram(SpawnedHolo, IntendedPos, IntendedRot);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Applied rigid rotation (%.1f°) to clone 1 topology + repositioned %d holograms"),
                State.RotationZ, Owner->JsonSpawnedHolograms.Num());

            // CRITICAL: Disable clearance detection on the parent hologram when rotation is active.
            // Vanilla's CheckValidPlacement checks the parent's clearance box against nearby
            // buildings. With any rotation, the parent shifts enough to overlap with the source
            // building's clearance, causing "Encroaching another object's clearance!".
            if (Owner->CurrentExtendHologram.IsValid())
            {
                TArray<UBoxComponent*> ParentBoxes;
                Owner->CurrentExtendHologram->GetComponents<UBoxComponent>(ParentBoxes);
                for (UBoxComponent* Box : ParentBoxes)
                {
                    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Box->SetGenerateOverlapEvents(false);
                }
            }
        }
    }

    // If we have additional clones beyond the first, calculate and spawn them
    if (CloneCount > 1 || RowCount > 1)
    {
        // Calculate positions for additional clones (everything beyond clone 1 in row 0)
        CalculateScaledExtendPositions();

        // Validate constraints (belt/pipe lengths, angles, and Issue #288 pole capacity)
        Owner->bScaledExtendValid = ValidateScaledExtendConstraints() && ValidatePowerCapacity();

        if (Owner->bScaledExtendValid)
        {
            // Spawn factory holograms + infrastructure for additional clones
            SpawnScaledExtendPreviews();

            // Phase 6: Merge all scaled extend clone topologies into Owner->StoredCloneTopology.
            // The existing wiring system uses Owner->StoredCloneTopology to generate wiring manifests.
            // Without Owner merge, only clone 1's topology is available for post-build wiring.
            if (Owner->StoredCloneTopology.IsValid())
            {
                int32 MergedCount = 0;
                for (const FSFScaledExtendClone& Clone : Owner->ScaledExtendClones)
                {
                    if (Clone.CloneTopology.IsValid())
                    {
                        for (const FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
                        {
                            Owner->StoredCloneTopology->ChildHolograms.Add(Holo);
                            MergedCount++;
                        }
                    }
                }
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Phase 6: Merged %d holograms from %d clones into StoredCloneTopology (total: %d)"),
                    MergedCount, Owner->ScaledExtendClones.Num(), Owner->StoredCloneTopology->ChildHolograms.Num());
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND: Configuration invalid - %s"), *Owner->ScaledExtendInvalidReason);

            // Phase 7: Invalidate the grid - set hologram material to red/error
            if (Owner->CurrentExtendHologram.IsValid())
            {
                Owner->CurrentExtendHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Hologram set to ERROR state - %s"), *Owner->ScaledExtendInvalidReason);
            }
        }
    }

}

void USFExtendScaledService::CalculateScaledExtendPositions()
{
    Owner->ScaledExtendClones.Empty();

    if (!Owner->CurrentExtendTarget.IsValid() || !Owner->Subsystem.IsValid())
    {
        return;
    }

    AFGBuildable* SourceBuilding = Owner->CurrentExtendTarget.Get();
    const FSFCounterState& State = Owner->Subsystem->GetCounterState();

    // Get building size from registry
    USFBuildableSizeRegistry::Initialize();
    FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(SourceBuilding->GetClass());
    FVector BuildingSize = Profile.DefaultSize;

    FRotator SourceRotation = SourceBuilding->GetActorRotation();

    // Calculate effective Y row height from topology extent (prevents row overlap).
    // Infrastructure (distributors, belts, pipes) may extend beyond the factory's Y footprint.
    float EffectiveRowHeight = BuildingSize.Y;
    if (Owner->StoredCloneTopology.IsValid() && Owner->StoredCloneTopology->ChildHolograms.Num() > 0)
    {
        FVector CloneOffset = Owner->StoredCloneTopology->WorldOffset.ToFVector();
        FVector CloneFactoryCenter = SourceBuilding->GetActorLocation() + CloneOffset;
        FRotator InvRot = SourceRotation.GetInverse();

        float MinLocalY = 0.0f, MaxLocalY = 0.0f;
        for (const FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
        {
            if (Holo.bIsLaneSegment) continue;  // Lane segments span between clones, not relevant
            FVector WorldPos = Holo.Transform.Location.ToFVector();
            FVector LocalPos = InvRot.RotateVector(WorldPos - CloneFactoryCenter);

            // Expand bounds by half the building's Y width to use edges instead of centers.
            // Distributors (splitters/mergers/junctions) are the outermost infrastructure
            // and are ~200cm wide (half = 100cm). Belt/pipe segments are thin and don't
            // contribute meaningfully. Without Owner, ~4m shortfall from center-to-center.
            float HalfY = 0.0f;
            if (Holo.Role == TEXT("distributor"))
            {
                HalfY = 200.0f;  // Splitters/mergers are ~400cm wide, half = 200cm
            }

            MinLocalY = FMath::Min(MinLocalY, LocalPos.Y - HalfY);
            MaxLocalY = FMath::Max(MaxLocalY, LocalPos.Y + HalfY);
        }
        float TopologyYExtent = MaxLocalY - MinLocalY;
        EffectiveRowHeight = FMath::Max(BuildingSize.Y, TopologyYExtent);

        if (TopologyYExtent > BuildingSize.Y)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Topology Y extent (%.0f) > BuildingSize.Y (%.0f) — using topology extent for row spacing"),
                TopologyYExtent, BuildingSize.Y);
        }
    }

    int32 CloneCount = Owner->GetExtendCloneCount();
    int32 RowCount = Owner->GetExtendRowCount();

    // Get extend direction offset (perpendicular to belt flow)
    ESFExtendDirection CurrentDir = Owner->DetectionService ? Owner->DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
    float XDirectionSign = (CurrentDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;

    // Determine Y direction sign (negative Y = opposite perpendicular)
    int32 YDir = (Owner->Subsystem->GetCounterState().GridCounters.Y >= 0) ? 1 : -1;

    // [#366] Designer-bounds clamp. When extending a designer-resident building, a clone whose
    // position falls outside the Blueprint Designer volume can never construct (vanilla refuses
    // out-of-bounds buildables), yet it would still render a preview AND be charged in the cost
    // quote - the reported "Hologram cannot be placed in Blueprint Designer!" + "Missing materials"
    // while the in-bounds portion builds fine. This array is the single source of truth every
    // downstream spawner and the cost aggregation key from, so dropping out-of-bounds clones HERE
    // keeps preview and quote honest. No-op outside a designer (the common open-world case).
    AFGBuildableBlueprintDesigner* BoundsDesigner = Owner->CurrentExtendHologram.IsValid()
        ? Owner->CurrentExtendHologram->GetBlueprintDesigner() : nullptr;
    const FVector SourceWorldLocation = SourceBuilding->GetActorLocation();
    auto IsCloneInDesignerBounds = [&](const FSFScaledExtendClone& Clone) -> bool
    {
        if (!BoundsDesigner) { return true; }  // not designer-resident: never clamp
        return BoundsDesigner->IsLocationInsideDesigner(SourceWorldLocation + Clone.WorldOffset);
    };

    // For each row and clone position, calculate the world offset
    for (int32 Row = 0; Row < RowCount; Row++)
    {
        // For rows > 0, we need an auto-seed clone at position (0, Row)
        if (Row > 0)
        {
            FSFScaledExtendClone SeedClone;
            SeedClone.GridX = 0;
            SeedClone.GridY = Row;
            SeedClone.bIsSeed = true;

            // Seed position: offset in Y direction (perpendicular to extend direction)
            // Y direction is perpendicular to X (extend) direction
            FVector YOffset = FVector(0.0f, EffectiveRowHeight * Row * YDir, 0.0f);
            // Add Y spacing
            YOffset.Y += State.SpacingY * Row * YDir;
            // Add Y steps (vertical offset per row for terraced look)
            float YSteps = State.StepsY * Row;

            SeedClone.WorldOffset = SourceRotation.RotateVector(YOffset);
            SeedClone.WorldOffset.Z += YSteps;
            SeedClone.RotationOffset = FRotator::ZeroRotator;

            if (IsCloneInDesignerBounds(SeedClone))  // [#366] skip clones outside the designer volume
            {
                Owner->ScaledExtendClones.Add(SeedClone);
            }
        }

        // For each clone in Owner row
        // Row 0: Skip clone 1 (CloneIndex=1) — that's the parent hologram handled by existing flow
        // Row 1+: Include all clones (1+) since they all need factory holograms
        int32 StartClone = (Row == 0) ? 1 : 0;  // Row 0 starts at clone 2, other rows start at clone 1
        for (int32 Clone = StartClone; Clone < CloneCount; Clone++)
        {
            int32 CloneIndex = Clone + 1;  // 1-based (clone 1 is adjacent to source/seed)

            FSFScaledExtendClone ExtendClone;
            ExtendClone.GridX = CloneIndex;
            ExtendClone.GridY = Row;
            ExtendClone.bIsSeed = false;

            // Check if rotation is active
            bool bRotationActive = !FMath::IsNearlyZero(State.RotationZ);

            FVector CloneOffset;
            FRotator CloneRotation = FRotator::ZeroRotator;

            if (bRotationActive)
            {
                // Arc/radial placement - cumulative rotation per clone
                // Each clone steps along an arc, rotating by RotationZ degrees per step
                float ArcLength = BuildingSize.X + static_cast<float>(State.SpacingX);
                float StepRadians = FMath::Abs(FMath::DegreesToRadians(State.RotationZ));
                float Radius = (StepRadians > KINDA_SMALL_NUMBER) ? ArcLength / StepRadians : 0.0f;

                float AngleDeg = static_cast<float>(CloneIndex) * State.RotationZ;
                float AngleRad = FMath::DegreesToRadians(AngleDeg);
                float SignRotation = (State.RotationZ >= 0.0f) ? 1.0f : -1.0f;

                // Arc position in local space — matches CalculateRotationOffset pattern:
                //   X = SignX * R * Sin(|θ|)              (direction determines forward/backward)
                //   Y = SignRotation * (R - R*Cos(|θ|))   (NO direction sign — canonical)
                //   Rotation = AngleDeg * XDirectionSign  (sign baked into angle)
                float AbsAngleRad = FMath::Abs(AngleRad);
                float SignX = XDirectionSign;

                CloneOffset.X = SignX * Radius * FMath::Sin(AbsAngleRad);
                CloneOffset.Y = SignRotation * (Radius - Radius * FMath::Cos(AbsAngleRad));
                CloneOffset.Z = 0.0f;

                // Apply steps (vertical offset per clone)
                CloneOffset.Z += State.StepsX * CloneIndex;

                CloneRotation = FRotator(0.0f, AngleDeg * XDirectionSign, 0.0f);
            }
            else
            {
                // Linear placement - simple offset along extend direction
                CloneOffset.X = XDirectionSign * (BuildingSize.X * CloneIndex + State.SpacingX * CloneIndex);
                CloneOffset.Y = 0.0f;
                CloneOffset.Z = State.StepsX * CloneIndex;  // Steps = vertical offset per clone
            }

            // Add Y row offset
            if (Row > 0)
            {
                CloneOffset.Y += EffectiveRowHeight * Row * YDir + State.SpacingY * Row * YDir;
                CloneOffset.Z += State.StepsY * Row;
            }

            // Rotate offset by source building rotation to get world space
            ExtendClone.WorldOffset = SourceRotation.RotateVector(CloneOffset);
            // Preserve Z offset (steps) without rotation
            ExtendClone.WorldOffset.Z = CloneOffset.Z;

            ExtendClone.RotationOffset = CloneRotation;

            if (IsCloneInDesignerBounds(ExtendClone))  // [#366] skip clones outside the designer volume
            {
                Owner->ScaledExtendClones.Add(ExtendClone);
            }
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Calculated %d clone positions (Clones=%d, Rows=%d)"),
        Owner->ScaledExtendClones.Num(), CloneCount, RowCount);
}

void USFExtendScaledService::SpawnScaledExtendPreviews()
{
    if (!Owner->CurrentExtendTarget.IsValid() || !Owner->CurrentExtendHologram.IsValid() || !Owner->TopologyService)
    {
        return;
    }

    if (Owner->ScaledExtendClones.Num() == 0)
    {
        return;
    }

    AFGBuildable* SourceBuilding = Owner->CurrentExtendTarget.Get();
    AFGHologram* ParentHologram = Owner->CurrentExtendHologram.Get();
    FVector SourceLocation = SourceBuilding->GetActorLocation();
    FRotator SourceRotation = SourceBuilding->GetActorRotation();

    // Get source topology (already walked)
    const FSFExtendTopology& Topology = Owner->TopologyService->GetCurrentTopology();
    if (!Topology.bIsValid)
    {
        return;
    }

    // Capture source topology once for cloning
    FSFSourceTopology SourceTopo = FSFSourceTopology::CaptureFromTopology(Topology);

    int32 TotalHologramsSpawned = 0;

    for (int32 i = 0; i < Owner->ScaledExtendClones.Num(); i++)
    {
        FSFScaledExtendClone& Clone = Owner->ScaledExtendClones[i];

        // Calculate world position for Owner clone's factory building
        FVector CloneWorldPos = SourceLocation + Clone.WorldOffset;
        FRotator CloneWorldRot = SourceRotation + Clone.RotationOffset;

        // === Step 1: Spawn factory building hologram for Owner clone ===
        // Use the same mechanism as normal grid scaling (SpawnChildHologramFromRecipe)
        static int32 ScaledExtendChildCounter = 0;
        FName ChildName = *FString::Printf(TEXT("SE_Factory_%d_%d_%d"), Clone.GridX, Clone.GridY, ScaledExtendChildCounter++);

        AFGHologram* FactoryHologram = AFGHologram::SpawnChildHologramFromRecipe(
            ParentHologram,
            ChildName,
            ParentHologram->GetRecipe(),
            ParentHologram->GetOwner() ? ParentHologram->GetOwner() : ParentHologram,
            CloneWorldPos,
            nullptr
        );

        if (FactoryHologram)
        {
            // Position and rotate the factory hologram
            FactoryHologram->SetActorLocation(CloneWorldPos);
            FactoryHologram->SetActorRotation(CloneWorldRot);

            // Mark as child and disable validation (same as normal grid scaling)
            USFHologramDataService::DisableValidation(FactoryHologram);
            USFHologramDataService::MarkAsChild(FactoryHologram, ParentHologram, ESFChildHologramType::ScalingGrid);

            // [#365] Carry the designer context so designer-resident scaled-extend factories
            // register with the designer (vanilla copies hologram->buildable at construct).
            if (AFGBuildableBlueprintDesigner* Designer = ParentHologram->GetBlueprintDesigner())
            {
                FactoryHologram->SetInsideBlueprintDesigner(Designer);
            }

            // Copy stored recipe to Owner clone
            if (Owner->Subsystem.IsValid() && Owner->Subsystem->bHasStoredProductionRecipe)
            {
                USFHologramDataService::StoreRecipe(FactoryHologram, Owner->Subsystem->StoredProductionRecipe);
            }

            // Force valid appearance
            FactoryHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
            FactoryHologram->SetActorTickEnabled(false);

            // CRITICAL: Disable clearance detection on child factory holograms.
            // Vanilla CheckValidPlacement on the parent iterates mChildren and calls
            // CheckValidPlacement on each. These vanilla FGFactoryHologram children have
            // full clearance boxes that overlap with nearby buildings when rotated,
            // causing "Encroaching another object's clearance!" on the parent.
            // Disabling collision on their clearance detector prevents overlap detection.
            TArray<UBoxComponent*> BoxComponents;
            FactoryHologram->GetComponents<UBoxComponent>(BoxComponents);
            for (UBoxComponent* Box : BoxComponents)
            {
                Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Box->SetGenerateOverlapEvents(false);
            }
            // Box collision disabling above is sufficient to prevent clearance overlap detection

            // Tag as Smart! child so Owner->HologramService's mChildren filter catches it
            FactoryHologram->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));

            // Set JsonCloneId so the factory registers in Owner->JsonBuiltActors during Construct()
            // This is needed for the wiring system to resolve "sc{i}_factory" references
            FString FactoryCloneId = FString::Printf(TEXT("sc%d_factory"), i);
            FSFHologramData* FactoryHoloData = USFHologramDataRegistry::GetData(FactoryHologram);
            if (!FactoryHoloData)
            {
                FactoryHoloData = USFHologramDataRegistry::AttachData(FactoryHologram);
            }
            if (FactoryHoloData)
            {
                FactoryHoloData->JsonCloneId = FactoryCloneId;
            }

            // Track in preview list
            Owner->BeltPreviewHolograms.Add(FactoryHologram);
            Clone.SpawnedHolograms.Add(TEXT("factory"), FactoryHologram);

            // Track in hologram service for position refresh
            if (Owner->HologramService)
            {
                Owner->HologramService->TrackChildHologram(FactoryHologram, CloneWorldPos, CloneWorldRot);
            }

            TotalHologramsSpawned++;

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Spawned factory hologram for Clone[%d] (%d,%d) at (%+.0f, %+.0f, %+.0f)%s"),
                i, Clone.GridX, Clone.GridY, CloneWorldPos.X, CloneWorldPos.Y, CloneWorldPos.Z,
                Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("⚡ SCALED EXTEND: Failed to spawn factory hologram for Clone[%d]"), i);
            continue;
        }

        // === Step 2: Spawn infrastructure (belts, distributors, pipes, power) around Owner clone ===
        Clone.CloneTopology = MakeShared<FSFCloneTopology>(FSFCloneTopology::FromSource(SourceTopo, Clone.WorldOffset));

        // === Step 2.25: RIGID BODY ROTATION ===
        // Clone topology is a rigid body relative to the factory building.
        // FromSource generates positions with translation only (no rotation).
        // If the factory has a RotationOffset, rotate ALL infrastructure positions
        // around the factory center to maintain the rigid spatial relationship.
        // Lane segments are adaptive — only their clone-side endpoint rotates.
        if (!Clone.RotationOffset.IsNearlyZero())
        {
            // Factory center = source factory position + Owner clone's world offset
            FVector FactoryCenter = SourceTopo.Factory.Transform.Location.ToFVector() + Clone.WorldOffset;

            for (FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
            {
                if (Holo.bIsLaneSegment || Holo.bIsSourceToCloneWire)
                {
                    // Issue #345: source->clone power cables are adaptive like lane segments.
                    // Lane segments are ADAPTIVE — only rotate the clone-side endpoint.
                    // The source-side endpoint stays fixed (chain post-processing shifts it later).
                    if (Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
                    {
                        FVector StartWorld = Holo.SplineData.Points[0].World.ToFVector();
                        FVector EndWorld = Holo.SplineData.Points.Last().World.ToFVector();
                        FVector LaneDir = EndWorld - StartWorld;
                        bool bCloneAtEnd = (FVector::DotProduct(LaneDir, Clone.WorldOffset) > 0.0f);

                        if (bCloneAtEnd)
                        {
                            // End is at clone — rotate end around factory center
                            FVector RelEnd = EndWorld - FactoryCenter;
                            FVector RotatedEnd = FactoryCenter + Clone.RotationOffset.RotateVector(RelEnd);
                            Holo.SplineData.Points.Last().World = FSFVec3(RotatedEnd);
                            // Rotate clone-side normal
                            Holo.LaneEndNormal = FSFVec3(Clone.RotationOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                        }
                        else
                        {
                            // Start is at clone — rotate start around factory center
                            FVector RelStart = StartWorld - FactoryCenter;
                            FVector RotatedStart = FactoryCenter + Clone.RotationOffset.RotateVector(RelStart);
                            Holo.SplineData.Points[0].World = FSFVec3(RotatedStart);
                            // Rotate clone-side normal
                            Holo.LaneStartNormal = FSFVec3(Clone.RotationOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                        }

                        // Recalculate transform from updated endpoints
                        FVector NewStart = Holo.SplineData.Points[0].World.ToFVector();
                        FVector NewEnd = Holo.SplineData.Points.Last().World.ToFVector();
                        Holo.SplineData.Length = FVector::Dist(NewStart, NewEnd);
                        Holo.Transform = FSFTransform(NewStart, (NewEnd - NewStart).Rotation());
                    }
                }
                else
                {
                    // RIGID infrastructure — rotate position around factory center + add rotation
                    FVector HoloPos = Holo.Transform.Location.ToFVector();
                    FVector RelPos = HoloPos - FactoryCenter;
                    FVector RotatedPos = FactoryCenter + Clone.RotationOffset.RotateVector(RelPos);
                    FRotator RotatedRot = Holo.Transform.Rotation.ToFRotator() + Clone.RotationOffset;
                    Holo.Transform = FSFTransform(RotatedPos, RotatedRot);

                    // Rotate spline world positions for belt/pipe segments
                    if (Holo.bHasSplineData)
                    {
                        for (FSFSplinePoint& Point : Holo.SplineData.Points)
                        {
                            FVector WorldPt = Point.World.ToFVector();
                            FVector RelPt = WorldPt - FactoryCenter;
                            Point.World = FSFVec3(FactoryCenter + Clone.RotationOffset.RotateVector(RelPt));
                        }
                    }

                    // Rotate lift data transforms
                    if (Holo.bHasLiftData)
                    {
                        FVector BottomPos = Holo.LiftData.BottomTransform.Location.ToFVector();
                        FVector RelBottom = BottomPos - FactoryCenter;
                        Holo.LiftData.BottomTransform.Location = FSFVec3(FactoryCenter + Clone.RotationOffset.RotateVector(RelBottom));
                        FRotator BottomRot = Holo.LiftData.BottomTransform.Rotation.ToFRotator() + Clone.RotationOffset;
                        Holo.LiftData.BottomTransform.Rotation = FSFRot3(BottomRot);
                    }
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ RIGID ROTATION: Clone[%d] rotated %d holograms by (%.0f,%.0f,%.0f) around factory center"),
                i, Clone.CloneTopology->ChildHolograms.Num(), Clone.RotationOffset.Pitch, Clone.RotationOffset.Yaw, Clone.RotationOffset.Roll);
        }

        // === Step 2.5: CHAIN TOPOLOGY - Modify lane segments to chain from previous clone ===
        // FromSource creates lane segments from Source(0,0,0) → ThisClone(WorldOffset).
        // For scaled extend, lanes must chain: PrevClone → ThisClone.
        // We shift each lane segment's start position by PrevCloneOffset.
        {
            // Seed clones should NOT have lane segments connecting back to the source.
            // The source building already has its own manifold. The next clone's lane
            // will connect TO the seed, not the other way around.
            if (Clone.bIsSeed)
            {
                Clone.CloneTopology->ChildHolograms.RemoveAll([](const FSFCloneHologram& H) { return H.bIsLaneSegment; });
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ CHAIN: Seed clone - removed lane segments (source already has manifold)"));
            }
            else
            {
            // Determine previous clone's world offset and rotation
            FVector PrevCloneOffset;
            FRotator PrevCloneRotation = FRotator::ZeroRotator;
            if (Clone.GridY == 0 && (i == 0 || Owner->ScaledExtendClones[i-1].GridY != Clone.GridY))
            {
                // First additional clone in row 0 → previous is clone 1 (parent hologram)
                PrevCloneOffset = Owner->CurrentExtendHologram->GetActorLocation() - Owner->CurrentExtendTarget->GetActorLocation();
                // Parent hologram IS rotated by RotationZ * DirSign when rotation is active
                // Must include DirSign to match clone 1's actual rotation from Owner->RefreshExtension
                if (Owner->Subsystem.IsValid())
                {
                    const FSFCounterState& CounterState = Owner->Subsystem->GetCounterState();
                    if (!FMath::IsNearlyZero(CounterState.RotationZ))
                    {
                        ESFExtendDirection PrevDir = Owner->DetectionService ? Owner->DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
                        float PrevDirSign = (PrevDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;
                        PrevCloneRotation = FRotator(0.0f, CounterState.RotationZ * PrevDirSign, 0.0f);
                    }
                }
            }
            else
            {
                // Previous clone in same row
                PrevCloneOffset = Owner->ScaledExtendClones[i-1].WorldOffset;
                PrevCloneRotation = Owner->ScaledExtendClones[i-1].RotationOffset;
            }

            // Source factory center (needed for rotating source-side around prev clone's factory center)
            FVector SourceFactoryPos = SourceTopo.Factory.Transform.Location.ToFVector();

            // Modify lane segments: move the "source" end to the previous clone's
            // ROTATED distributor connector position.
            // FromSource generates lanes between Source ↔ ThisClone.
            // The "source" end can be at START or END depending on flow direction.
            // Detect which end is the source using dot product with clone direction.
            for (FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
            {
                // Issue #345: source->clone power cables chain to the previous clone like lane segments.
                if (!Holo.bIsLaneSegment && !Holo.bIsSourceToCloneWire) continue;

                if (Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
                {
                    FVector OldStartWorld = Holo.SplineData.Points[0].World.ToFVector();
                    FVector OldEndWorld = Holo.SplineData.Points.Last().World.ToFVector();

                    // Determine which end is the "source" end:
                    // If lane direction (start→end) aligns with source→clone direction,
                    // then start is at source. If anti-aligned, end is at source.
                    FVector LaneDir = OldEndWorld - OldStartWorld;
                    bool bSourceAtStart = (FVector::DotProduct(LaneDir, Clone.WorldOffset) > 0.0f);

                    // Helper: compute the previous clone's ROTATED connector position.
                    // The source-side endpoint is at the SOURCE distributor connector.
                    // The previous clone's matching connector = same relative position
                    // to its factory center, but rotated by PrevCloneRotation.
                    // PrevFactoryCenter = SourceFactoryPos + PrevCloneOffset
                    // NewPos = PrevFactoryCenter + Rotate(OldPos - SourceFactoryPos, PrevCloneRotation)
                    auto RotateToPreClone = [&](const FVector& SourceEndpoint) -> FVector
                    {
                        FVector PrevFactoryCenter = SourceFactoryPos + PrevCloneOffset;
                        FVector RelPos = SourceEndpoint - SourceFactoryPos;
                        return PrevFactoryCenter + PrevCloneRotation.RotateVector(RelPos);
                    };

                    FVector NewStartWorld, NewEndWorld;
                    if (bSourceAtStart)
                    {
                        // Source is at START → move start to previous clone's rotated connector
                        NewStartWorld = RotateToPreClone(OldStartWorld);
                        NewEndWorld = OldEndWorld; // End stays at Owner clone
                        // Rotate source-side normal to match previous clone's distributor orientation
                        if (!PrevCloneRotation.IsNearlyZero())
                        {
                            Holo.LaneStartNormal = FSFVec3(PrevCloneRotation.RotateVector(Holo.LaneStartNormal.ToFVector()));
                        }
                    }
                    else
                    {
                        // Source is at END → move end to previous clone's rotated connector
                        NewStartWorld = OldStartWorld; // Start stays at Owner clone
                        NewEndWorld = RotateToPreClone(OldEndWorld);
                        // Rotate source-side normal to match previous clone's distributor orientation
                        if (!PrevCloneRotation.IsNearlyZero())
                        {
                            Holo.LaneEndNormal = FSFVec3(PrevCloneRotation.RotateVector(Holo.LaneEndNormal.ToFVector()));
                        }
                    }

                    // Recalculate spline
                    float NewLength = FVector::Dist(NewStartWorld, NewEndWorld);
                    FRotator NewRotation = (NewEndWorld - NewStartWorld).Rotation();

                    // Update transform (position = start, rotation = direction)
                    Holo.Transform = FSFTransform(NewStartWorld, NewRotation);

                    // Update spline data
                    Holo.SplineData.Length = NewLength;
                    Holo.SplineData.Points[0].World = FSFVec3(NewStartWorld);
                    Holo.SplineData.Points[0].Local = FSFVec3(FVector::ZeroVector);
                    Holo.SplineData.Points.Last().World = FSFVec3(NewEndWorld);
                    Holo.SplineData.Points.Last().Local = FSFVec3(FVector(NewLength, 0, 0));

                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ CHAIN: Clone[%d] lane %s: %s shifted by PrevOffset(%.0f,%.0f,%.0f), length %.0f→%.0f"),
                        i, *Holo.HologramId, bSourceAtStart ? TEXT("START") : TEXT("END"),
                        PrevCloneOffset.X, PrevCloneOffset.Y, PrevCloneOffset.Z,
                        FVector::Dist(OldStartWorld, OldEndWorld), NewLength);
                }
                else if (Holo.bHasLiftData)
                {
                    // Shift lift bottom transform by PrevCloneOffset
                    FVector OldBottom = Holo.LiftData.BottomTransform.Location.ToFVector();
                    Holo.LiftData.BottomTransform.Location = FSFVec3(OldBottom + PrevCloneOffset);

                    // Update transform too
                    FVector OldTransformPos = Holo.Transform.Location.ToFVector();
                    Holo.Transform.Location = FSFVec3(OldTransformPos + PrevCloneOffset);
                }
            }
            } // end else (non-seed lane post-processing)
        }

        // Prefix all hologram IDs and update connection targets for uniqueness
        FString ClonePrefix = FString::Printf(TEXT("sc%d_"), i);
        FString FactoryCloneId = FString::Printf(TEXT("sc%d_factory"), i);

        // Determine previous clone's prefix for lane segment source-side target updates
        FString PrevClonePrefix;
        if (Clone.GridY == 0 && (i == 0 || Owner->ScaledExtendClones[i-1].GridY != Clone.GridY))
        {
            PrevClonePrefix = TEXT("");  // Parent hologram's infrastructure has no prefix
        }
        else if (i > 0)
        {
            PrevClonePrefix = FString::Printf(TEXT("sc%d_"), i - 1);
        }

        for (FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
        {
            // Save original targets BEFORE modifications (needed for lane segment cross-references)
            FString OrigConn0Target = Holo.CloneConnections.ConveyorAny0.Target;
            FString OrigConn1Target = Holo.CloneConnections.ConveyorAny1.Target;

            // Prefix hologram ID
            Holo.HologramId = ClonePrefix + Holo.HologramId;

            // === Conn0 target resolution ===
            if (OrigConn0Target == TEXT("parent"))
            {
                Holo.CloneConnections.ConveyorAny0.Target = FactoryCloneId;
            }
            else if (Holo.bIsLaneSegment && OrigConn0Target.StartsWith(TEXT("source:")))
            {
                // Lane segment source-side → previous clone's distributor.
                // Use ORIGINAL Conn1 (clone-side hologram ID) as base.
                Holo.CloneConnections.ConveyorAny0.Target = PrevClonePrefix + OrigConn1Target;
            }
            else if (!OrigConn0Target.IsEmpty() && OrigConn0Target != TEXT("external"))
            {
                // Internal clone reference — prefix it
                Holo.CloneConnections.ConveyorAny0.Target = ClonePrefix + OrigConn0Target;
            }

            // === Conn1 target resolution ===
            if (OrigConn1Target == TEXT("parent"))
            {
                Holo.CloneConnections.ConveyorAny1.Target = FactoryCloneId;
            }
            else if (Holo.bIsLaneSegment && OrigConn1Target.StartsWith(TEXT("source:")))
            {
                // Lane segment source-side → previous clone's distributor.
                // Use ORIGINAL Conn0 (clone-side hologram ID) as base.
                Holo.CloneConnections.ConveyorAny1.Target = PrevClonePrefix + OrigConn0Target;
            }
            else if (!OrigConn1Target.IsEmpty() && OrigConn1Target != TEXT("external"))
            {
                // Internal clone reference — prefix it
                Holo.CloneConnections.ConveyorAny1.Target = ClonePrefix + OrigConn1Target;
            }
        }

        // Spawn infrastructure child holograms
        TMap<FString, AFGHologram*> InfraHolograms;
        int32 InfraSpawned = Clone.CloneTopology->SpawnChildHolograms(
            ParentHologram, Owner, InfraHolograms);

        TotalHologramsSpawned += InfraSpawned;

        // Build maps of hologram ID → intended transforms and spline data from clone topology.
        // We need Owner because AddChild() inside SpawnChildHolograms repositions actors
        // to the parent hologram's location, AND generates spline meshes at the wrong
        // world position. We must reposition and regenerate meshes afterward.
        TMap<FString, FVector> IntendedPositions;
        TMap<FString, FRotator> IntendedRotations;
        TMap<FString, const FSFCloneHologram*> HologramDataMap;
        for (const FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
        {
            IntendedPositions.Add(Holo.HologramId, Holo.Transform.Location.ToFVector());
            IntendedRotations.Add(Holo.HologramId, Holo.Transform.Rotation.ToFRotator());
            HologramDataMap.Add(Holo.HologramId, &Holo);
        }

        // Track all infrastructure holograms
        for (auto& Pair : InfraHolograms)
        {
            if (Pair.Value)
            {
                Owner->BeltPreviewHolograms.Add(Pair.Value);
                Clone.SpawnedHolograms.Add(Pair.Key, Pair.Value);

                // Get intended position from topology (NOT from GetActorLocation which
                // may have been reset by AddChild to parent hologram's position)
                FVector IntendedPos = Pair.Value->GetActorLocation();
                FRotator IntendedRot = Pair.Value->GetActorRotation();
                if (FVector* FoundPos = IntendedPositions.Find(Pair.Key))
                {
                    IntendedPos = *FoundPos;
                }
                if (FRotator* FoundRot = IntendedRotations.Find(Pair.Key))
                {
                    IntendedRot = *FoundRot;
                }

                // NOTE: RotationOffset is already applied in Step 2.25 (rigid body rotation
                // in the topology). Do NOT apply it again here — would double-rotate.

                // Force actor to intended position BEFORE regenerating meshes
                Pair.Value->SetActorLocation(IntendedPos);
                Pair.Value->SetActorRotation(IntendedRot);
                if (USceneComponent* Root = Pair.Value->GetRootComponent())
                {
                    Root->SetWorldLocation(IntendedPos);
                    Root->SetWorldRotation(IntendedRot);
                }

                // CRITICAL: Re-apply spline data and regenerate meshes AFTER repositioning.
                // SpawnChildHolograms generates meshes while the actor is at the parent's
                // position (due to AddChild). The spline mesh components are placed in world
                // space during generation, so they don't follow when we move the actor.
                // We must regenerate at the correct position.
                if (const FSFCloneHologram** HoloDataPtr = HologramDataMap.Find(Pair.Key))
                {
                    const FSFCloneHologram& HoloData = **HoloDataPtr;

                    if (ASFConveyorBeltHologram* Belt = Cast<ASFConveyorBeltHologram>(Pair.Value))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            // Lane segments: use AutoRouteSplineWithNormals for proper curved routing
                            FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            FVector EndNormal = HoloData.LaneEndNormal.ToFVector();

                            Belt->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
                            Belt->TriggerMeshGeneration();
                            Belt->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
                            // Regular belt segments: use raw spline data
                            TArray<FSplinePointData> SplinePoints;
                            for (const FSFSplinePoint& Point : HoloData.SplineData.Points)
                            {
                                FSplinePointData PointData;
                                PointData.Location = Point.Local.ToFVector();
                                PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
                                PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
                                SplinePoints.Add(PointData);
                            }
                            Belt->SetSplineDataAndUpdate(SplinePoints);
                            Belt->TriggerMeshGeneration();
                            Belt->ForceApplyHologramMaterial();
                        }
                    }
                    else if (ASFPipelineHologram* Pipe = Cast<ASFPipelineHologram>(Pair.Value))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            // Pipe lane segments: use TryUseBuildModeRouting with world positions
                            FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            FVector EndNormal = HoloData.LaneEndNormal.ToFVector();

                            Pipe->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal);
                            Pipe->TriggerMeshGeneration();
                            Pipe->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
                            // Regular pipe segments: use raw spline data
                            TArray<FSplinePointData> SplinePoints;
                            for (const FSFSplinePoint& Point : HoloData.SplineData.Points)
                            {
                                FSplinePointData PointData;
                                PointData.Location = Point.Local.ToFVector();
                                PointData.ArriveTangent = Point.ArriveTangent.ToFVector();
                                PointData.LeaveTangent = Point.LeaveTangent.ToFVector();
                                SplinePoints.Add(PointData);
                            }
                            Pipe->SetSplineDataAndUpdate(SplinePoints);
                            Pipe->TriggerMeshGeneration();
                            Pipe->ForceApplyHologramMaterial();
                        }
                    }
                }

                // Track in Owner->HologramService for position refresh using
                // the TOPOLOGY-derived position, not GetActorLocation().
                if (Owner->HologramService)
                {
                    Owner->HologramService->TrackChildHologram(
                        Pair.Value, IntendedPos, IntendedRot);
                }
            }
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Clone[%d] (%d,%d) - factory + %d infrastructure holograms%s"),
            i, Clone.GridX, Clone.GridY, InfraSpawned, Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("⚡ SCALED EXTEND: Total %d holograms spawned across %d additional clone sets"),
        TotalHologramsSpawned, Owner->ScaledExtendClones.Num());

    // CRITICAL: Scrub nulls from parent's mChildren array.
    // SpawnChildHologramFromRecipe may add entries to mChildren before the spawn completes.
    // If spawning fails, null entries are left in mChildren which crash
    // AFGHologram::ResetConstructDisqualifiers() during tick.
    if (Owner->CurrentExtendHologram.IsValid())
    {
        AFGHologram* Parent = Owner->CurrentExtendHologram.Get();
        if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
        {
            TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
            if (ChildrenArray)
            {
                int32 NullsRemoved = 0;
                for (int32 i = ChildrenArray->Num() - 1; i >= 0; --i)
                {
                    if (!(*ChildrenArray)[i] || !IsValid((*ChildrenArray)[i]))
                    {
                        ChildrenArray->RemoveAt(i);
                        NullsRemoved++;
                    }
                }
                if (NullsRemoved > 0)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND: Scrubbed %d null/invalid entries from parent mChildren"), NullsRemoved);
                }
            }
        }
    }
}

void USFExtendScaledService::ClearScaledExtendClones()
{
    if (Owner->ScaledExtendClones.Num() == 0)
    {
        return;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Clearing %d clone sets"), Owner->ScaledExtendClones.Num());

    // CRITICAL: Remove all scaled extend children from parent hologram's mChildren array
    // BEFORE destroying them. Without Owner, destroyed holograms leave dangling pointers
    // in mChildren, causing crash in AFGHologram::ResetConstructDisqualifiers during tick.
    if (Owner->CurrentExtendHologram.IsValid())
    {
        AFGHologram* Parent = Owner->CurrentExtendHologram.Get();
        if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
        {
            TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
            if (ChildrenArray)
            {
                // Collect all hologram pointers we're about to destroy
                TSet<AFGHologram*> ToRemove;
                for (const FSFScaledExtendClone& Clone : Owner->ScaledExtendClones)
                {
                    for (const auto& Pair : Clone.SpawnedHolograms)
                    {
                        if (Pair.Value.IsValid())
                        {
                            ToRemove.Add(Pair.Value.Get());
                        }
                    }
                }

                // Remove them from mChildren (iterate backwards for safe removal)
                int32 RemovedCount = 0;
                for (int32 i = ChildrenArray->Num() - 1; i >= 0; --i)
                {
                    if (ToRemove.Contains((*ChildrenArray)[i]))
                    {
                        ChildrenArray->RemoveAt(i);
                        RemovedCount++;
                    }
                }

                if (RemovedCount > 0)
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND: Removed %d children from parent mChildren"), RemovedCount);
                }
            }
        }
    }

    // Now safe to destroy the holograms
    // CRITICAL: Use TWeakObjectPtr for the destroy phase. Raw pointers in SpawnedHolograms
    // can become dangling if the engine already destroyed the holograms (e.g., build gun
    // cleanup when player aims away). IsValid(rawPtr) on freed memory is undefined behavior.
    // TWeakObjectPtr::IsValid() uses the UObject index system and is safe against GC'd objects.
    TArray<TWeakObjectPtr<AFGHologram>> WeakHologramsToDestroy;
    for (FSFScaledExtendClone& Clone : Owner->ScaledExtendClones)
    {
        for (auto& Pair : Clone.SpawnedHolograms)
        {
            if (Pair.Value.IsValid())
            {
                WeakHologramsToDestroy.Add(Pair.Value);
            }
        }
        Clone.SpawnedHolograms.Empty();
        Clone.CloneTopology.Reset();
    }
    Owner->ScaledExtendClones.Empty();

    // Destroy using weak pointers (safe against already-destroyed/GC'd objects)
    for (TWeakObjectPtr<AFGHologram>& WeakHolo : WeakHologramsToDestroy)
    {
        if (WeakHolo.IsValid())
        {
            AFGHologram* Holo = WeakHolo.Get();
            Owner->BeltPreviewHolograms.Remove(Holo);
            Holo->SetActorHiddenInGame(true);
            Holo->Destroy();
        }
    }

    Owner->bScaledExtendValid = true;
    Owner->ScaledExtendInvalidReason.Empty();
}

bool USFExtendScaledService::ValidateScaledExtendConstraints()
{
    if (Owner->ScaledExtendClones.Num() == 0)
    {
        return true;  // No clones = no constraints to check
    }

    // ========================================================================
    // Lane Segment Validation (Phase 8)
    // ========================================================================
    // Lane segments are the belt/pipe connections between adjacent clones.
    // They are the ONLY connections affected by spacing/steps/rotation transforms.
    // Intra-clone infrastructure uses rigid body rotation and is valid by construction.
    //
    // Since the inter-clone geometry is uniform (same spacing/steps/rotation between
    // all adjacent pairs), we only need to validate Owner->StoredCloneTopology's lane segments
    // (Source → Clone1). If those pass, all subsequent clone pairs will too.
    // ========================================================================

    // Belt constraints (matching CreateOrUpdateBeltPreview in SFAutoConnectService)
    constexpr float MinBeltLength = 50.0f;       // 0.5m minimum
    constexpr float MaxBeltLength = 5600.0f;     // 56m maximum
    constexpr float MaxBeltAngleDeg = 30.0f;     // 30° max at each connector

    // Pipe constraints (matching ProcessPipeJunctions in SFPipeAutoConnectManager)
    constexpr float MinPipeLength = 50.0f;       // 0.5m minimum (straight)
    constexpr float MaxPipeLength = 2500.0f;     // 25m maximum
    constexpr float MaxPipeAngleDeg = 30.0f;     // 30° max at each connector

    if (Owner->StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
        {
            if (!Holo.bIsLaneSegment) continue;
            if (!Holo.bHasSplineData || Holo.SplineData.Points.Num() < 2) continue;

            FVector StartPos = Holo.SplineData.Points[0].World.ToFVector();
            FVector EndPos = Holo.SplineData.Points.Last().World.ToFVector();
            FVector StartNormal = Holo.LaneStartNormal.ToFVector();
            FVector EndNormal = Holo.LaneEndNormal.ToFVector();

            float SegmentLength = FVector::Dist(StartPos, EndPos);
            bool bIsPipe = (Holo.LaneSegmentType == TEXT("pipe"));

            float MinLength = bIsPipe ? MinPipeLength : MinBeltLength;
            float MaxLength = bIsPipe ? MaxPipeLength : MaxBeltLength;
            float MaxAngle = bIsPipe ? MaxPipeAngleDeg : MaxBeltAngleDeg;
            const TCHAR* TypeName = bIsPipe ? TEXT("Pipe") : TEXT("Belt");

            // Distance validation
            if (SegmentLength < MinLength)
            {
                Owner->ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane too short (%.1fm < %.1fm minimum)"),
                    TypeName, SegmentLength / 100.0f, MinLength / 100.0f);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *Owner->ScaledExtendInvalidReason);
                return false;
            }

            if (SegmentLength > MaxLength)
            {
                Owner->ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane too long (%.1fm > %.0fm maximum)"),
                    TypeName, SegmentLength / 100.0f, MaxLength / 100.0f);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *Owner->ScaledExtendInvalidReason);
                return false;
            }

            // Angle validation — check departure angle at start and arrival angle at end
            // Same dual-angle check as CreateOrUpdateBeltPreview
            FVector SegmentDir = (EndPos - StartPos).GetSafeNormal();

            // Start connector: angle between connector normal and segment direction
            float DotStart = FVector::DotProduct(StartNormal, SegmentDir);
            float AngleStart = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotStart, -1.0f, 1.0f)));

            // End connector: angle between connector normal and reverse segment direction
            float DotEnd = FVector::DotProduct(EndNormal, -SegmentDir);
            float AngleEnd = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotEnd, -1.0f, 1.0f)));

            if (AngleStart > MaxAngle || AngleEnd > MaxAngle)
            {
                Owner->ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane angle too steep (%.0f°/%.0f° > %.0f° max)"),
                    TypeName, AngleStart, AngleEnd, MaxAngle);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *Owner->ScaledExtendInvalidReason);
                return false;
            }

            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ VALIDATE: %s lane OK — length %.1fm, angles %.0f°/%.0f°"),
                TypeName, SegmentLength / 100.0f, AngleStart, AngleEnd);
        }
    }

    Owner->ScaledExtendInvalidReason.Empty();
    return true;
}

bool USFExtendScaledService::ValidatePowerCapacity()
{
    // Issue #288: Preview-time check that each cloned power pole can actually
    // host the connections we plan to make: factory (1) + inter-pole wire back
    // to source (1) + cloned pumps whose PowerInput was wired to Owner specific
    // source pole (N). Pumps connected to out-of-manifold poles are excluded
    // automatically because their ConnectedPowerPoleHologramId is empty.
    // Called from both regular-Extend and Scaled Extend preview paths so the
    // 1-clone case gets the same protection as 2+ clones.

    if (!Owner->StoredCloneTopology.IsValid() || Owner->StoredCloneTopology->ChildHolograms.Num() == 0)
    {
        return true;  // No topology → no poles → nothing to validate
    }

    // Tally pumps per clone pole HologramId in a single pass.
    TMap<FString, int32> PumpsPerClonePole;
    for (const FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
    {
        if (Holo.Role == TEXT("pipe_attachment") && !Holo.ConnectedPowerPoleHologramId.IsEmpty())
        {
            PumpsPerClonePole.FindOrAdd(Holo.ConnectedPowerPoleHologramId) += 1;
        }
    }

    // Walk poles; first over-capacity entry aborts with a descriptive reason.
    for (const FSFCloneHologram& Holo : Owner->StoredCloneTopology->ChildHolograms)
    {
        if (Holo.Role != TEXT("power_pole")) continue;
        if (Holo.PowerPoleMaxConnections <= 0) continue;  // Defensive: no tier data, skip

        const int32 PumpCount = PumpsPerClonePole.FindRef(Holo.HologramId);
        constexpr int32 FactoryConn = 1;   // clone factory ↔ clone pole
        constexpr int32 InterPoleConn = 1; // source pole ↔ clone pole (Power Extend)
        const int32 Projected = FactoryConn + InterPoleConn + PumpCount;

        if (Projected > Holo.PowerPoleMaxConnections)
        {
            // Human-readable tier label — we only know the class name, which is
            // reasonably greppable (Build_PowerPoleMk1_C, Build_PowerPoleWall_C,
            // etc). Strip the "Build_" prefix and "_C" suffix for the HUD line.
            FString Tier = Holo.SourceClass;
            Tier.RemoveFromStart(TEXT("Build_"));
            Tier.RemoveFromEnd(TEXT("_C"));

            Owner->ScaledExtendInvalidReason = FString::Printf(
                TEXT("Clone %s needs %d/%d connections (factory + inter-pole + %d pump%s) — upgrade the source pole, or move a pump to another pole"),
                *Tier, Projected, Holo.PowerPoleMaxConnections, PumpCount, (PumpCount == 1 ? TEXT("") : TEXT("s")));
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND POWER (#288): INVALID — %s"), *Owner->ScaledExtendInvalidReason);
            return false;
        }
    }

    return true;
}
