// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendRestoreReplayService Implementation
 *
 * See header for overview. Moved verbatim from SFExtendService (T1 split); the only
 * changes are accessing the owning service shared state through Owner-> and renaming
 * the enclosing class. Logic is identical to the pre-split version.
 */

#include "Features/Extend/SFExtendRestoreReplayService.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendControlFrame.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendDiagnosticsService.h"
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

namespace
{
    constexpr float SF_RESTORED_PARENT_TRANSFORM_TOLERANCE = 0.1f;

    // #422: Re-derive the lane endpoint normals from GEOMETRY (the lane's own world endpoints) instead
    // of trusting the captured normals. A preset captured on an MP client has poisoned lane normals: the
    // distributor connection data is server-only (IsConnected()/GetConnection() are null on a client), so
    // the capture's connector selection falls back to a distance tiebreak and can bake a REVERSED normal,
    // which makes the spline router loop the belt/pipe back on itself (the U-turn / wrap symptom). The
    // lane's two endpoints already sit ON the real connectors, so the chord direction IS the router-
    // convention facing: StartNormal points outward-along the lane, EndNormal outward-against it (both the
    // belt and pipe spline routers require dot(StartNormal, LaneDir) > 0 and dot(EndNormal, LaneDir) < 0).
    // Geometry-only — it never reads the poisoned connection data, so it is correct on a client, repairs
    // poisoned presets, and is a no-op on already-correct ones. Degenerate (zero-length) lanes untouched.
    void DeriveRestoredLaneNormals(FSFCloneHologram& Holo, const FVector& Start, const FVector& End)
    {
        if (!Holo.bIsLaneSegment || (Holo.LaneSegmentType != TEXT("belt") && Holo.LaneSegmentType != TEXT("pipe")))
        {
            return;
        }

        const FVector LaneDirection = (End - Start).GetSafeNormal();
        if (LaneDirection.IsNearlyZero())
        {
            return;
        }

        Holo.LaneStartNormal = FSFVec3(LaneDirection);
        Holo.LaneEndNormal = FSFVec3(-LaneDirection);
    }

    void KickRestoredPreviewParent(AFGHologram* ParentHologram)
    {
        if (!IsValid(ParentHologram))
        {
            return;
        }

        const FVector Location = ParentHologram->GetActorLocation();
        const FRotator Rotation = ParentHologram->GetActorRotation();
        const bool bWasLocked = ParentHologram->IsHologramLocked();

        FHitResult SyntheticHit;
        SyntheticHit.bBlockingHit = true;
        SyntheticHit.Location = Location;
        SyntheticHit.ImpactPoint = Location;
        SyntheticHit.ImpactNormal = FVector::UpVector;
        SyntheticHit.Normal = FVector::UpVector;
        SyntheticHit.TraceStart = Location + FVector(0.0f, 0.0f, 100.0f);
        SyntheticHit.TraceEnd = Location - FVector(0.0f, 0.0f, 100.0f);
        SyntheticHit.Distance = 100.0f;
        ParentHologram->SetHologramLocationAndRotation(SyntheticHit);

        ParentHologram->SetActorLocation(Location);
        ParentHologram->SetActorRotation(Rotation);
        if (USceneComponent* Root = ParentHologram->GetRootComponent())
        {
            Root->SetWorldLocation(Location);
            Root->SetWorldRotation(Rotation);
            Root->MarkRenderStateDirty();
        }

        ParentHologram->SetActorHiddenInGame(false);
        ParentHologram->UpdateComponentTransforms();

        ParentHologram->LockHologramPosition(!bWasLocked);
        ParentHologram->LockHologramPosition(bWasLocked);
    }

    void ScrubInvalidHologramChildren(AFGHologram* ParentHologram, const TCHAR* Context)
    {
        if (!IsValid(ParentHologram))
        {
            return;
        }

        FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
        if (!ChildrenProp)
        {
            return;
        }

        TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
        if (!ChildrenArray)
        {
            return;
        }

        int32 RemovedCount = 0;
        for (int32 Index = ChildrenArray->Num() - 1; Index >= 0; --Index)
        {
            if (!(*ChildrenArray)[Index] || !IsValid((*ChildrenArray)[Index]))
            {
                ChildrenArray->RemoveAt(Index);
                RemovedCount++;
            }
        }

        if (RemovedCount > 0)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                TEXT("[SmartRestore][Extend] Scrubbed %d null/invalid parent children: context=%s parent=%s"),
                RemovedCount,
                Context ? Context : TEXT("Unknown"),
                *GetNameSafe(ParentHologram));
        }
    }
}

// Shared with SFExtendService post-build wiring (declared in the header).
    FRestoredScaledClonePlacement CalculateRestoredScaledClonePlacement(
        const AFGHologram* ParentHologram,
        const FSFCloneTopology* TemplateTopology,
        const FSFCounterState& State,
        int32 GridX,
        int32 GridY)
    {
        FRestoredScaledClonePlacement Placement;
        if (!ParentHologram)
        {
            return Placement;
        }

        USFBuildableSizeRegistry::Initialize();
        FVector BuildingSize(800.0f, 800.0f, 400.0f);
        if (UClass* BuildClass = ParentHologram->GetBuildClass())
        {
            BuildingSize = USFBuildableSizeRegistry::GetProfile(BuildClass).DefaultSize;
        }

        const float EffectiveRowHeight = CalculateExtendEffectiveRowHeight(BuildingSize, TemplateTopology);
        const FSFExtendCellPlacement CellPlacement = CalculateExtendCellPlacement(
            ParentHologram->GetActorRotation(),
            BuildingSize,
            EffectiveRowHeight,
            State,
            GridX + 1,
            GridY,
            1,
            0);
        Placement.WorldOffset = CellPlacement.WorldOffset;
        Placement.RotationOffset = CellPlacement.RotationOffset;
        return Placement;
    }


USFExtendRestoreReplayService::USFExtendRestoreReplayService()
{
}

void USFExtendRestoreReplayService::Initialize(USFExtendService* InExtendService)
{
    Owner = InExtendService;
}

void USFExtendRestoreReplayService::Shutdown()
{
    Owner = nullptr;
}

bool USFExtendRestoreReplayService::IsHologramCompatibleWithRestoredCloneTopology(AFGHologram* ParentHologram) const
{
    if (!Owner->bRestoredCloneTopologyActive || !Owner->RestoredCloneTopologyTemplate.IsValid())
    {
        return true;
    }

    if (!IsValid(ParentHologram))
    {
        return false;
    }

    const FString& ExpectedBuildClass = Owner->RestoredCloneTopologyTemplate->ParentBuildClass;
    if (ExpectedBuildClass.IsEmpty())
    {
        return true;
    }

    UClass* ActiveBuildClass = ParentHologram->GetBuildClass();
    const FString ActiveBuildClassName = ActiveBuildClass ? ActiveBuildClass->GetName() : FString();
    const bool bCompatible = ActiveBuildClassName == ExpectedBuildClass;
    if (!bCompatible)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Restored topology build-class mismatch: expected=%s active=%s parent=%s"),
            *ExpectedBuildClass,
            ActiveBuildClassName.IsEmpty() ? TEXT("<none>") : *ActiveBuildClassName,
            *GetNameSafe(ParentHologram));
    }

    return bCompatible;
}

void USFExtendRestoreReplayService::ClearRestoredCloneTopologySession(const TCHAR* Reason)
{
    const int32 TemplateChildCount = Owner->RestoredCloneTopologyTemplate.IsValid()
        ? Owner->RestoredCloneTopologyTemplate->ChildHolograms.Num()
        : 0;

    // [EXTEND-MP] A pre-staged restore commit must never be consumed by a later plain fire
    // (client-only no-op elsewhere; the plain-fire hook also stages an explicit clear).
    Owner->StageCommitClearForMP();

    ClearRestoredCloneTopologyPreview();
    Owner->RestoredCloneParentHologram.Reset();
    Owner->RestoredCloneTopologyTemplate.Reset();
    Owner->RestoredCloneBaseTopology.Reset();
    Owner->LastCloneTopology.Reset();
    Owner->RestoredCloneLastParentLocation = FVector::ZeroVector;
    Owner->RestoredCloneLastParentRotation = FRotator::ZeroRotator;
    Owner->bRestoredCloneTopologyActive = false;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] Cleared restored topology session: reason=%s templateChildren=%d"),
        Reason ? Reason : TEXT("Unknown"),
        TemplateChildCount);
}

bool USFExtendRestoreReplayService::ReplayRestoreCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology)
{
    if (!ParentHologram)
    {
        return false;
    }

    if (!CloneTopology.ParentBuildClass.IsEmpty())
    {
        UClass* ActiveBuildClass = ParentHologram->GetBuildClass();
        const FString ActiveBuildClassName = ActiveBuildClass ? ActiveBuildClass->GetName() : FString();
        if (ActiveBuildClassName != CloneTopology.ParentBuildClass)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] Replay skipped for mismatched build class: expected=%s active=%s parent=%s"),
                *CloneTopology.ParentBuildClass,
                ActiveBuildClassName.IsEmpty() ? TEXT("<none>") : *ActiveBuildClassName,
                *GetNameSafe(ParentHologram));
            return false;
        }
    }

    ClearRestoredCloneTopologyPreview();
    if (Owner->ScaledExtendClones.Num() > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Clearing %d normal Scaled Extend clone record(s) before Restore replay owns the topology"),
            Owner->ScaledExtendClones.Num());
        Owner->ScaledExtendClones.Empty();
    }
    Owner->RestoredCloneParentHologram = ParentHologram;
    Owner->RestoredCloneTopologyTemplate = MakeShared<FSFCloneTopology>(CloneTopology);
    Owner->RestoredCloneBaseTopology = MakeShared<FSFCloneTopology>(CloneTopology);
    Owner->RestoredCloneLastParentLocation = ParentHologram->GetActorLocation();
    Owner->RestoredCloneLastParentRotation = ParentHologram->GetActorRotation();
    Owner->bRestoredCloneTopologyActive = true;
    if (Owner->Subsystem.IsValid())
    {
        Owner->Subsystem->ClearNormalGridChildrenForExtendSuppression(TEXT("ReplayRestoreCloneTopology"));
    }
    KickRestoredPreviewParent(ParentHologram);

    FSFCloneTopology ReplayTopology = BuildRestoredCloneTopologyForCurrentState(ParentHologram);
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] Replay restored scaled topology: parent=%s templateChildren=%d replayChildren=%d"),
        *GetNameSafe(ParentHologram),
        CloneTopology.ChildHolograms.Num(),
        ReplayTopology.ChildHolograms.Num());
    return SpawnRestoredCloneTopology(ParentHologram, ReplayTopology);
}

void USFExtendRestoreReplayService::TickRestoredCloneTopology(float DeltaTime)
{
    if (!Owner->bRestoredCloneTopologyActive || !Owner->HologramService)
    {
        return;
    }

    if (!Owner->RestoredCloneParentHologram.IsValid())
    {
        const bool bHasPostBuildActorsOrRetry = Owner->JsonBuiltActors.Num() > 0
            || Owner->bRestoredScaledWiringDeferred
            || Owner->bRestoredScaledWiringRetryScheduled;
        const bool bCanStillFinishPostBuildWiring = bHasPostBuildActorsOrRetry
            && Owner->HasPendingPostBuildWiring()
            && (Owner->bRestoredScaledWiringRetryScheduled || Owner->RestoredScaledWiringRetryAttempts < 5);
        if (bCanStillFinishPostBuildWiring)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] Parent hologram invalid while post-build wiring is pending; retaining restored topology (storedChildren=%d, jsonBuilt=%d, jsonSpawned=%d, previewFactories=%d, retry=%d/%d)"),
                Owner->StoredCloneTopology.IsValid() ? Owner->StoredCloneTopology->ChildHolograms.Num() : 0,
                Owner->JsonBuiltActors.Num(),
                Owner->JsonSpawnedHolograms.Num(),
                Owner->RestoredScaledFactoryPreviewLocations.Num(),
                Owner->RestoredScaledWiringRetryAttempts,
                5);
            return;
        }

        ClearRestoredCloneTopologySession(TEXT("TickRestoredCloneTopology parent invalid"));
        if (Owner->Subsystem.IsValid())
        {
            if (USFRestoreService* RestoreSvc = Owner->Subsystem->GetRestoreService())
            {
                RestoreSvc->ClearActiveRestoreSession(TEXT("Restored Extend parent invalid"));
            }
        }
        return;
    }

    AFGHologram* ParentHologram = Owner->RestoredCloneParentHologram.Get();
    if (!IsHologramCompatibleWithRestoredCloneTopology(ParentHologram))
    {
        ClearRestoredCloneTopologySession(TEXT("TickRestoredCloneTopology build class mismatch"));
        if (Owner->Subsystem.IsValid())
        {
            if (USFRestoreService* RestoreSvc = Owner->Subsystem->GetRestoreService())
            {
                RestoreSvc->ClearActiveRestoreSession(TEXT("Restored Extend parent build class mismatch"));
            }
        }
        return;
    }

    const FVector ParentLocation = ParentHologram->GetActorLocation();
    const FRotator ParentRotation = ParentHologram->GetActorRotation();
    if (!ParentLocation.Equals(Owner->RestoredCloneLastParentLocation, SF_RESTORED_PARENT_TRANSFORM_TOLERANCE) || !ParentRotation.Equals(Owner->RestoredCloneLastParentRotation, SF_RESTORED_PARENT_TRANSFORM_TOLERANCE))
    {
        KickRestoredPreviewParent(ParentHologram);
        FSFCloneTopology ReplayTopology = BuildRestoredCloneTopologyForCurrentState(ParentHologram);
        TMap<FString, FVector> IntendedPositions;
        TMap<FString, FRotator> IntendedRotations;
        for (const FSFCloneHologram& Holo : ReplayTopology.ChildHolograms)
        {
            IntendedPositions.Add(Holo.HologramId, Holo.Transform.Location.ToFVector());
            IntendedRotations.Add(Holo.HologramId, Holo.Transform.Rotation.ToFRotator());
        }
        if (Owner->Subsystem.IsValid())
        {
            const FSFCounterState& State = Owner->Subsystem->GetCounterState();
            const int32 XCount = FMath::Max(1, FMath::Abs(State.GridCounters.X));
            const int32 YCount = FMath::Max(1, FMath::Abs(State.GridCounters.Y));
            if (XCount > 1 || YCount > 1)
            {
                const FSFCloneTopology* TemplateTopology = Owner->RestoredCloneTopologyTemplate.IsValid()
                    ? Owner->RestoredCloneTopologyTemplate.Get()
                    : nullptr;
                for (int32 Y = 0; Y < YCount; ++Y)
                {
                    for (int32 X = 0; X < XCount; ++X)
                    {
                        if (X == 0 && Y == 0)
                        {
                            continue;
                        }

                        const FRestoredScaledClonePlacement Placement = CalculateRestoredScaledClonePlacement(ParentHologram, TemplateTopology, State, X, Y);
                        const FString FactoryId = FString::Printf(TEXT("rr_%d_%d_factory"), X, Y);
                        IntendedPositions.Add(FactoryId, ParentLocation + Placement.WorldOffset);
                        IntendedRotations.Add(FactoryId, ParentRotation + Placement.RotationOffset);
                    }
                }
            }
        }

        for (const auto& Pair : Owner->JsonSpawnedHolograms)
        {
            AFGHologram* Child = Pair.Value;
            if (!IsValid(Child))
            {
                continue;
            }

            FVector IntendedPos = Child->GetActorLocation();
            FRotator IntendedRot = Child->GetActorRotation();
            if (FVector* FoundPos = IntendedPositions.Find(Pair.Key))
            {
                IntendedPos = *FoundPos;
            }
            if (FRotator* FoundRot = IntendedRotations.Find(Pair.Key))
            {
                IntendedRot = *FoundRot;
            }

            Child->SetActorLocation(IntendedPos);
            Child->SetActorRotation(IntendedRot);
            if (USceneComponent* Root = Child->GetRootComponent())
            {
                Root->SetWorldLocation(IntendedPos);
                Root->SetWorldRotation(IntendedRot);
                Root->MarkRenderStateDirty();
            }
            Owner->HologramService->TrackChildHologram(Child, IntendedPos, IntendedRot);
        }

        Owner->StoredCloneTopology = MakeShared<FSFCloneTopology>(ReplayTopology);
        Owner->RestoredCloneLastParentLocation = ParentLocation;
        Owner->RestoredCloneLastParentRotation = ParentRotation;
    }

    Owner->HologramService->RefreshChildPositions();

    // [EXTEND-MP] Pre-stage the RESTORE commit continuously while the replay session is live
    // (client-only + 250ms-throttled inside; BuildCommitSpecForMP builds the restore variant when
    // this session is active). Same rationale as the Extend pre-stage: the commit RPC is much
    // larger than the construct RPC and loses the cross-channel race when staged only at fire.
    Owner->MaybeStageCommitForMP(ParentHologram);
}

void USFExtendRestoreReplayService::OnRestoredCloneTopologyStateChanged()
{
    if (!Owner->bRestoredCloneTopologyActive || !Owner->RestoredCloneParentHologram.IsValid())
    {
        return;
    }

    AFGHologram* ParentHologram = Owner->RestoredCloneParentHologram.Get();
    FSFCloneTopology ReplayTopology = BuildRestoredCloneTopologyForCurrentState(ParentHologram);
    ClearRestoredCloneTopologyPreview();
    Owner->RestoredCloneParentHologram = ParentHologram;
    Owner->bRestoredCloneTopologyActive = true;
    if (Owner->Subsystem.IsValid())
    {
        Owner->Subsystem->ClearNormalGridChildrenForExtendSuppression(TEXT("OnRestoredCloneTopologyStateChanged"));
    }
    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] Restored scaled state changed: parent=%s replayChildren=%d"),
        *GetNameSafe(ParentHologram),
        ReplayTopology.ChildHolograms.Num());
    SpawnRestoredCloneTopology(ParentHologram, ReplayTopology);
}

FSFCloneTopology USFExtendRestoreReplayService::BuildRestoredCloneTopologyForCurrentState(AFGHologram* ParentHologram) const
{
    FSFCloneTopology ReplayTopology = Owner->RestoredCloneTopologyTemplate.IsValid()
        ? *Owner->RestoredCloneTopologyTemplate
        : FSFCloneTopology();
    const FVector OriginalParentLocation = ReplayTopology.ParentTransform.Location.ToFVector();
    const FRotator OriginalParentRotation = ReplayTopology.ParentTransform.Rotation.ToFRotator();
    const FVector NewParentLocation = ParentHologram->GetActorLocation();
    const FRotator NewParentRotation = ParentHologram->GetActorRotation();
    const FRotator RotationDelta = NewParentRotation - OriginalParentRotation;

    auto TransformLocation = [&](const FVector& Location) -> FVector
    {
        const FVector Relative = Location - OriginalParentLocation;
        return NewParentLocation + RotationDelta.RotateVector(Relative);
    };

    auto TransformRotation = [&](const FRotator& Rotation) -> FRotator
    {
        return Rotation + RotationDelta;
    };

    ReplayTopology.ParentTransform = FSFTransform(NewParentLocation, NewParentRotation);

    for (FSFCloneHologram& Holo : ReplayTopology.ChildHolograms)
    {
        Holo.Transform = FSFTransform(
            TransformLocation(Holo.Transform.Location.ToFVector()),
            TransformRotation(Holo.Transform.Rotation.ToFRotator()));

        if (Holo.bHasSplineData)
        {
            for (FSFSplinePoint& Point : Holo.SplineData.Points)
            {
                Point.World = FSFVec3(TransformLocation(Point.World.ToFVector()));
            }
        }

        if (Holo.bHasLiftData)
        {
            // TopTransform is actor-local; only the world-space bottom transform follows the parent.
            Holo.LiftData.BottomTransform = FSFTransform(
                TransformLocation(Holo.LiftData.BottomTransform.Location.ToFVector()),
                TransformRotation(Holo.LiftData.BottomTransform.Rotation.ToFRotator()));
        }

        if (Holo.bIsLaneSegment)
        {
            // #422: re-derive lane normals from the (already world-transformed) endpoints rather than the
            // poisoned captured normals; degenerate <2-point lanes keep the rotated fallback.
            if (Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
            {
                DeriveRestoredLaneNormals(Holo,
                    Holo.SplineData.Points[0].World.ToFVector(),
                    Holo.SplineData.Points.Last().World.ToFVector());
            }
            else
            {
                Holo.LaneStartNormal = FSFVec3(RotationDelta.RotateVector(Holo.LaneStartNormal.ToFVector()));
                Holo.LaneEndNormal = FSFVec3(RotationDelta.RotateVector(Holo.LaneEndNormal.ToFVector()));
            }
        }
    }

    const TArray<FSFCloneHologram> SourceChildHolograms = ReplayTopology.ChildHolograms;
    ReplayTopology.ChildHolograms.RemoveAll([](const FSFCloneHologram& Holo)
    {
        return Holo.bIsLaneSegment
            || Holo.HologramId.StartsWith(TEXT("wire_source_"))
            || Holo.CloneConnections.ConveyorAny0.Target.StartsWith(TEXT("source:"))
            || Holo.CloneConnections.ConveyorAny1.Target.StartsWith(TEXT("source:"));
    });

    if (Owner->Subsystem.IsValid() && SourceChildHolograms.Num() > 0)
    {
        const FSFCounterState& State = Owner->Subsystem->GetCounterState();
        const int32 XCount = FMath::Max(1, FMath::Abs(State.GridCounters.X));
        const int32 YCount = FMath::Max(1, FMath::Abs(State.GridCounters.Y));
        if (XCount > 1 || YCount > 1)
        {
            TArray<FSFCloneHologram> ExpandedChildHolograms = ReplayTopology.ChildHolograms;
            const FSFCloneTopology* TemplateTopology = Owner->RestoredCloneTopologyTemplate.IsValid()
                ? Owner->RestoredCloneTopologyTemplate.Get()
                : nullptr;
            FVector RestoredSourceFactoryLocation = ParentHologram->GetActorLocation();
            if (TemplateTopology)
            {
                const FRotator TemplateParentRotation = TemplateTopology->ParentTransform.Rotation.ToFRotator();
                const FRotator TemplateRotationDelta = ParentHologram->GetActorRotation() - TemplateParentRotation;
                RestoredSourceFactoryLocation -= TemplateRotationDelta.RotateVector(TemplateTopology->WorldOffset.ToFVector());
            }
            auto PrefixInternalTarget = [](const FString& InPrefix, const FString& Target) -> FString
            {
                if (!Target.IsEmpty()
                    && Target != TEXT("parent")
                    && Target != TEXT("external")
                    && !Target.StartsWith(TEXT("source:")))
                {
                    return InPrefix + Target;
                }
                return Target;
            };
            for (int32 Y = 0; Y < YCount; ++Y)
            {
                for (int32 X = 0; X < XCount; ++X)
                {
                    if (X == 0 && Y == 0)
                    {
                        continue;
                    }

                    const FString Prefix = FString::Printf(TEXT("rr_%d_%d_"), X, Y);
                    const FString FactoryId = Prefix + TEXT("factory");
                    const FString PreviousPrefix = ((X - 1) == 0 && Y == 0)
                        ? FString()
                        : FString::Printf(TEXT("rr_%d_%d_"), X - 1, Y);
                    auto ResolveTargetForCurrentClone = [&](const FString& Target) -> FString
                    {
                        if (Target == TEXT("parent"))
                        {
                            return FactoryId;
                        }
                        return PrefixInternalTarget(Prefix, Target);
                    };
                    const FRestoredScaledClonePlacement Placement = CalculateRestoredScaledClonePlacement(ParentHologram, TemplateTopology, State, X, Y);
                    const FRestoredScaledClonePlacement PreviousPlacement = (X - 1 == 0 && Y == 0)
                        ? FRestoredScaledClonePlacement()
                        : CalculateRestoredScaledClonePlacement(ParentHologram, TemplateTopology, State, X - 1, Y);
                    const FVector ParentLocation = ParentHologram->GetActorLocation();
                    const FVector CurrentFactoryCenter = ParentLocation + Placement.WorldOffset;
                    const FVector PreviousFactoryCenter = ParentLocation + PreviousPlacement.WorldOffset;

                    for (FSFCloneHologram Holo : SourceChildHolograms)
                    {
                        if (Holo.HologramId.StartsWith(TEXT("wire_source_")))
                        {
                            continue;
                        }
                        if (Holo.bIsLaneSegment && X == 0)
                        {
                            continue;
                        }

                        const FString OriginalC0Target = Holo.CloneConnections.ConveyorAny0.Target;
                        const FString OriginalC1Target = Holo.CloneConnections.ConveyorAny1.Target;
                        if (!Holo.bIsLaneSegment
                            && (OriginalC0Target.StartsWith(TEXT("source:")) || OriginalC1Target.StartsWith(TEXT("source:"))))
                        {
                            continue;
                        }
                        Holo.HologramId = Prefix + Holo.HologramId;

                        // [#460] Restore scaling is bidirectional on X (unlike Extend's abs-lock). Clone
                        // POSITIONS are sign-aware (CalculateRestoredScaledClonePlacement), but a source-
                        // chained lane's endpoint roles were assigned purely by cell index (source end ->
                        // previous cell, clone end -> current cell). Scaling OPPOSITE the captured chain
                        // direction mirrors the positions while the roles stay fixed, wiring the belt
                        // Input->Output backward. Detect the reversed case from the lane's captured flow
                        // vector (source end -> clone end) vs the current previous->current placement
                        // direction, then treat the physically-upstream clone (the one whose output feeds
                        // this lane) as such so flow stays Output->Input. Defaults preserve native behavior.
                        const bool bLaneSourceChained = Holo.bIsLaneSegment
                            && (OriginalC0Target.StartsWith(TEXT("source:")) || OriginalC1Target.StartsWith(TEXT("source:")));
                        const FString* UpstreamPrefix = &PreviousPrefix;
                        const FString* DownstreamPrefix = &Prefix;
                        FVector UpstreamCenter = PreviousFactoryCenter;
                        FVector DownstreamCenter = CurrentFactoryCenter;
                        const FRestoredScaledClonePlacement* UpstreamPlacement = &PreviousPlacement;
                        const FRestoredScaledClonePlacement* DownstreamPlacement = &Placement;
                        if (bLaneSourceChained && Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
                        {
                            const FVector CapStart = Holo.SplineData.Points[0].World.ToFVector();
                            const FVector CapEnd = Holo.SplineData.Points.Last().World.ToFVector();
                            const FVector CapturedDownstream = OriginalC0Target.StartsWith(TEXT("source:"))
                                ? (CapEnd - CapStart)   // source at start: captured flow runs start -> end
                                : (CapStart - CapEnd);  // source at end:   captured flow runs end -> start
                            const FVector CurrentChainDir = CurrentFactoryCenter - PreviousFactoryCenter;
                            if (FVector::DotProduct(CurrentChainDir, CapturedDownstream) < 0.0f)
                            {
                                Swap(UpstreamPrefix, DownstreamPrefix);
                                Swap(UpstreamCenter, DownstreamCenter);
                                Swap(UpstreamPlacement, DownstreamPlacement);
                            }
                        }

                        if (Holo.bIsLaneSegment && OriginalC0Target.StartsWith(TEXT("source:")))
                        {
                            // C0 = source/output end -> upstream clone; C1 = clone/input end -> downstream.
                            Holo.CloneConnections.ConveyorAny0.Target = PrefixInternalTarget(*UpstreamPrefix, OriginalC1Target);
                            Holo.CloneConnections.ConveyorAny1.Target = PrefixInternalTarget(*DownstreamPrefix, OriginalC1Target);
                            Holo.LaneFromDistributorId = Holo.CloneConnections.ConveyorAny0.Target;
                            Holo.LaneToDistributorId = Holo.CloneConnections.ConveyorAny1.Target;
                        }
                        else if (Holo.bIsLaneSegment && OriginalC1Target.StartsWith(TEXT("source:")))
                        {
                            // C1 = source/output end -> upstream clone; C0 = clone/input end -> downstream.
                            Holo.CloneConnections.ConveyorAny0.Target = PrefixInternalTarget(*DownstreamPrefix, OriginalC0Target);
                            Holo.CloneConnections.ConveyorAny1.Target = PrefixInternalTarget(*UpstreamPrefix, OriginalC0Target);
                            Holo.LaneFromDistributorId = Holo.CloneConnections.ConveyorAny0.Target;
                            Holo.LaneToDistributorId = Holo.CloneConnections.ConveyorAny1.Target;
                        }
                        else
                        {
                            Holo.CloneConnections.ConveyorAny0.Target = ResolveTargetForCurrentClone(OriginalC0Target);
                            Holo.CloneConnections.ConveyorAny1.Target = ResolveTargetForCurrentClone(OriginalC1Target);
                        }
                        Holo.ConnectedPowerPoleHologramId = PrefixInternalTarget(Prefix, Holo.ConnectedPowerPoleHologramId);
                        if (Holo.bHasLiftData)
                        {
                            for (FString& PassthroughCloneId : Holo.LiftData.PassthroughCloneIds)
                            {
                                PassthroughCloneId = PrefixInternalTarget(Prefix, PassthroughCloneId);
                            }
                        }
                        if (Holo.bIsLaneSegment && Holo.bHasSplineData && Holo.SplineData.Points.Num() >= 2)
                        {
                            const bool bSourceAtStart = OriginalC0Target.StartsWith(TEXT("source:"));
                            const bool bSourceAtEnd = OriginalC1Target.StartsWith(TEXT("source:"));
                            const FVector BaseStart = Holo.SplineData.Points[0].World.ToFVector();
                            const FVector BaseEnd = Holo.SplineData.Points.Last().World.ToFVector();

                            // [#460] Anchor the source (output) end at the upstream clone and the clone
                            // (input) end at the downstream clone - upstream/downstream already account
                            // for a reversed scale direction (see the chaining block above), so the belt
                            // spline runs Output->Input in both scale directions.
                            auto MoveSourceEndpoint = [&](const FVector& SourceEndpoint) -> FVector
                            {
                                const FVector RelativeToSourceFactory = SourceEndpoint - RestoredSourceFactoryLocation;
                                return UpstreamCenter + UpstreamPlacement->RotationOffset.RotateVector(RelativeToSourceFactory);
                            };
                            auto MoveCloneEndpoint = [&](const FVector& CloneEndpoint) -> FVector
                            {
                                const FVector RelativeToParentFactory = CloneEndpoint - ParentLocation;
                                return DownstreamCenter + DownstreamPlacement->RotationOffset.RotateVector(RelativeToParentFactory);
                            };

                            FVector NewStart = BaseStart;
                            FVector NewEnd = BaseEnd;
                            if (bSourceAtStart)
                            {
                                NewStart = MoveSourceEndpoint(BaseStart);
                                NewEnd = MoveCloneEndpoint(BaseEnd);
                                Holo.LaneStartNormal = FSFVec3(UpstreamPlacement->RotationOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                                Holo.LaneEndNormal = FSFVec3(DownstreamPlacement->RotationOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                            }
                            else if (bSourceAtEnd)
                            {
                                NewStart = MoveCloneEndpoint(BaseStart);
                                NewEnd = MoveSourceEndpoint(BaseEnd);
                                Holo.LaneStartNormal = FSFVec3(DownstreamPlacement->RotationOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                                Holo.LaneEndNormal = FSFVec3(UpstreamPlacement->RotationOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                            }
                            else
                            {
                                NewStart = MoveCloneEndpoint(BaseStart);
                                NewEnd = MoveCloneEndpoint(BaseEnd);
                                Holo.LaneStartNormal = FSFVec3(DownstreamPlacement->RotationOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                                Holo.LaneEndNormal = FSFVec3(DownstreamPlacement->RotationOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                            }

                            DeriveRestoredLaneNormals(Holo, NewStart, NewEnd);
                            const float NewLength = FVector::Dist(NewStart, NewEnd);
                            Holo.Transform = FSFTransform(NewStart, (NewEnd - NewStart).Rotation());
                            Holo.SplineData.Length = NewLength;
                            Holo.SplineData.Points[0].World = FSFVec3(NewStart);
                            Holo.SplineData.Points[0].Local = FSFVec3(FVector::ZeroVector);
                            Holo.SplineData.Points.Last().World = FSFVec3(NewEnd);
                            Holo.SplineData.Points.Last().Local = FSFVec3(FVector(NewLength, 0.0f, 0.0f));

                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                                TEXT("[SmartRestore][Extend] Restored lane chained: id=%s grid=(%d,%d) sourceSide=%s prev=%s current=%s length=%.0f"),
                                *Holo.HologramId,
                                X,
                                Y,
                                bSourceAtStart ? TEXT("start") : (bSourceAtEnd ? TEXT("end") : TEXT("unknown")),
                                *PreviousPrefix,
                                *Prefix,
                                NewLength);
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                                TEXT("[SmartRestore][Extend] Restored lane normals re-derived from endpoints (#422): id=%s type=%s grid=(%d,%d)"),
                                *Holo.HologramId,
                                *Holo.LaneSegmentType,
                                X,
                                Y);
                        }
                        else
                        {
                            const FVector RelativeHoloLocation = Holo.Transform.Location.ToFVector() - ParentLocation;
                            Holo.Transform.Location = FSFVec3(CurrentFactoryCenter + Placement.RotationOffset.RotateVector(RelativeHoloLocation));
                            Holo.Transform.Rotation = FSFRot3(Holo.Transform.Rotation.ToFRotator() + Placement.RotationOffset);
                        }
                        if (!Holo.bIsLaneSegment && Holo.bHasSplineData)
                        {
                            for (FSFSplinePoint& Point : Holo.SplineData.Points)
                            {
                                const FVector RelativePointLocation = Point.World.ToFVector() - ParentLocation;
                                Point.World = FSFVec3(CurrentFactoryCenter + Placement.RotationOffset.RotateVector(RelativePointLocation));
                            }
                        }
                        if (!Holo.bIsLaneSegment && Holo.bHasLiftData)
                        {
                            const FVector RelativeBottomLocation = Holo.LiftData.BottomTransform.Location.ToFVector() - ParentLocation;
                            Holo.LiftData.BottomTransform.Location = FSFVec3(CurrentFactoryCenter + Placement.RotationOffset.RotateVector(RelativeBottomLocation));
                            Holo.LiftData.BottomTransform.Rotation = FSFRot3(Holo.LiftData.BottomTransform.Rotation.ToFRotator() + Placement.RotationOffset);
                        }
                        if (Holo.bIsLaneSegment && (!Holo.bHasSplineData || Holo.SplineData.Points.Num() < 2))
                        {
                            Holo.LaneStartNormal = FSFVec3(Placement.RotationOffset.RotateVector(Holo.LaneStartNormal.ToFVector()));
                            Holo.LaneEndNormal = FSFVec3(Placement.RotationOffset.RotateVector(Holo.LaneEndNormal.ToFVector()));
                        }
                        ExpandedChildHolograms.Add(Holo);
                    }
                }
            }
            ReplayTopology.ChildHolograms = ExpandedChildHolograms;
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] Restored scaled topology expanded: grid=(%d,%d) children=%d baseChildren=%d"),
                State.GridCounters.X,
                State.GridCounters.Y,
                ReplayTopology.ChildHolograms.Num(),
                ExpandedChildHolograms.Num() / FMath::Max(1, XCount * YCount));
        }
    }

    return ReplayTopology;
}

void USFExtendRestoreReplayService::ClearRestoredCloneTopologyPreview()
{
    if (Owner->HologramService)
    {
        if (Owner->RestoredCloneParentHologram.IsValid())
        {
            Owner->HologramService->SetCurrentParentHologram(Owner->RestoredCloneParentHologram.Get());
        }
        Owner->HologramService->ClearBeltPreviews();
    }
    Owner->JsonSpawnedHolograms.Empty();
    Owner->RestoredScaledFactoryPreviewLocations.Empty();
    Owner->StoredCloneTopology.Reset();
    Owner->bRestoredCloneTopologyActive = false;
    Owner->bRestoredScaledWiringDeferred = false;
    Owner->bRestoredScaledWiringRetryScheduled = false;
    Owner->RestoredScaledWiringRetryAttempts = 0;
}

int32 USFExtendRestoreReplayService::SpawnRestoredScaledFactoryHolograms(AFGHologram* ParentHologram, TMap<FString, AFGHologram*>& OutSpawnedHolograms)
{
    if (!ParentHologram || !Owner->Subsystem.IsValid())
    {
        return 0;
    }

    if (FSFHologramData* ParentData = USFHologramDataService::GetOrCreateData(ParentHologram))
    {
        ParentData->JsonCloneId = TEXT("parent");
    }

    const FSFCounterState& State = Owner->Subsystem->GetCounterState();
    const int32 XCount = FMath::Max(1, FMath::Abs(State.GridCounters.X));
    const int32 YCount = FMath::Max(1, FMath::Abs(State.GridCounters.Y));
    Owner->RestoredScaledFactoryPreviewLocations.Empty();
    if (XCount <= 1 && YCount <= 1)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Restored scaled factories: base parent only parent=%s"),
            *GetNameSafe(ParentHologram));
        return 0;
    }

    const FVector ParentLocation = ParentHologram->GetActorLocation();
    const FRotator ParentRotation = ParentHologram->GetActorRotation();
    const FSFCloneTopology* TemplateTopology = Owner->RestoredCloneTopologyTemplate.IsValid()
        ? Owner->RestoredCloneTopologyTemplate.Get()
        : nullptr;
    int32 SpawnedFactories = 0;

    for (int32 Y = 0; Y < YCount; ++Y)
    {
        for (int32 X = 0; X < XCount; ++X)
        {
            if (X == 0 && Y == 0)
            {
                continue;
            }

            const FRestoredScaledClonePlacement Placement = CalculateRestoredScaledClonePlacement(ParentHologram, TemplateTopology, State, X, Y);
            const FVector FactoryLocation = ParentLocation + Placement.WorldOffset;
            const FString FactoryId = FString::Printf(TEXT("rr_%d_%d_factory"), X, Y);
            static int32 RestoredScaledFactoryCounter = 0;
            const FName ChildName(*FString::Printf(TEXT("RestoredFactory_%d_%d_%d"), X, Y, RestoredScaledFactoryCounter++));

            AFGHologram* FactoryHologram = AFGHologram::SpawnChildHologramFromRecipe(
                ParentHologram,
                ChildName,
                ParentHologram->GetRecipe(),
                ParentHologram->GetOwner() ? ParentHologram->GetOwner() : ParentHologram,
                FactoryLocation,
                nullptr);

            if (!FactoryHologram)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                    TEXT("[SmartRestore][Extend] Restored scaled factory spawn failed: id=%s grid=(%d,%d) parent=%s"),
                    *FactoryId,
                    X,
                    Y,
                    *GetNameSafe(ParentHologram));
                continue;
            }

            FactoryHologram->SetActorLocation(FactoryLocation);
            FactoryHologram->SetActorRotation(ParentRotation + Placement.RotationOffset);
            FactoryHologram->SetActorHiddenInGame(false);
            if (USceneComponent* Root = FactoryHologram->GetRootComponent())
            {
                Root->SetWorldLocation(FactoryLocation);
                Root->SetWorldRotation(ParentRotation + Placement.RotationOffset);
                Root->MarkRenderStateDirty();
            }
            FactoryHologram->UpdateComponentTransforms();
            FactoryHologram->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
            USFHologramDataService::DisableValidation(FactoryHologram);
            USFHologramDataService::MarkAsChild(FactoryHologram, ParentHologram, ESFChildHologramType::ExtendClone);

            if (USFRecipeManagementService* RecipeSvc = Owner->Subsystem->GetRecipeManagementService())
            {
                if (RecipeSvc->HasStoredProductionRecipe() && RecipeSvc->GetStoredProductionRecipe())
                {
                    USFHologramDataService::StoreRecipe(FactoryHologram, RecipeSvc->GetStoredProductionRecipe());
                }
            }

            if (FSFHologramData* FactoryData = USFHologramDataService::GetOrCreateData(FactoryHologram))
            {
                FactoryData->JsonCloneId = FactoryId;
            }

            TArray<UBoxComponent*> BoxComponents;
            FactoryHologram->GetComponents<UBoxComponent>(BoxComponents);
            for (UBoxComponent* Box : BoxComponents)
            {
                Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Box->SetGenerateOverlapEvents(false);
            }

            FactoryHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
            FactoryHologram->SetActorTickEnabled(false);
            OutSpawnedHolograms.Add(FactoryId, FactoryHologram);
            Owner->RestoredScaledFactoryPreviewLocations.Add(FactoryId, FactoryLocation);
            SpawnedFactories++;

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] Restored scaled factory spawned: id=%s grid=(%d,%d) loc=%s"),
                *FactoryId,
                X,
                Y,
                *FactoryLocation.ToString());
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] Restored scaled factories spawned: count=%d grid=(%d,%d) parent=%s"),
        SpawnedFactories,
        State.GridCounters.X,
        State.GridCounters.Y,
        *GetNameSafe(ParentHologram));
    return SpawnedFactories;
}

bool USFExtendRestoreReplayService::SpawnRestoredCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology)
{
    TMap<FString, AFGHologram*> SpawnedHolograms;
    const int32 SpawnedFactoryCount = SpawnRestoredScaledFactoryHolograms(ParentHologram, SpawnedHolograms);
    const int32 SpawnedCount = CloneTopology.SpawnChildHolograms(ParentHologram, Owner, SpawnedHolograms);
    ScrubInvalidHologramChildren(ParentHologram, TEXT("SpawnRestoredCloneTopology"));
    if (SpawnedCount <= 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
            TEXT("[SmartRestore][Extend] ReplayRestoreCloneTopology failed: spawned=%d factories=%d inputChildren=%d parent=%s"),
            SpawnedCount,
            SpawnedFactoryCount,
            CloneTopology.ChildHolograms.Num(),
            *GetNameSafe(ParentHologram));
        return false;
    }

    CloneTopology.WireChildHologramConnections(SpawnedHolograms, ParentHologram);
    Owner->StoredCloneTopology = MakeShared<FSFCloneTopology>(CloneTopology);
    Owner->JsonSpawnedHolograms.Reset();
    for (const TPair<FString, AFGHologram*>& Pair : SpawnedHolograms)
    {
        Owner->JsonSpawnedHolograms.Add(Pair.Key, Pair.Value);
    }
    if (Owner->HologramService)
    {
        Owner->HologramService->SetCurrentParentHologram(ParentHologram);
        Owner->HologramService->StoreCloneTopology(Owner->StoredCloneTopology);
        Owner->HologramService->StoreJsonSpawnedHolograms(SpawnedHolograms);
        TMap<FString, FVector> IntendedPositions;
        TMap<FString, FRotator> IntendedRotations;
        TMap<FString, const FSFCloneHologram*> HologramDataMap;
        for (const FSFCloneHologram& Holo : CloneTopology.ChildHolograms)
        {
            IntendedPositions.Add(Holo.HologramId, Holo.Transform.Location.ToFVector());
            IntendedRotations.Add(Holo.HologramId, Holo.Transform.Rotation.ToFRotator());
            HologramDataMap.Add(Holo.HologramId, &Holo);
        }
        for (const auto& Pair : SpawnedHolograms)
        {
            if (AFGHologram* Child = Pair.Value)
            {
                FVector IntendedPos = Child->GetActorLocation();
                FRotator IntendedRot = Child->GetActorRotation();
                if (FVector* FoundPos = IntendedPositions.Find(Pair.Key))
                {
                    IntendedPos = *FoundPos;
                }
                if (FRotator* FoundRot = IntendedRotations.Find(Pair.Key))
                {
                    IntendedRot = *FoundRot;
                }
                Child->SetActorLocation(IntendedPos);
                Child->SetActorRotation(IntendedRot);
                if (USceneComponent* Root = Child->GetRootComponent())
                {
                    Root->SetWorldLocation(IntendedPos);
                    Root->SetWorldRotation(IntendedRot);
                }
                Child->UpdateComponentTransforms();
                if (const FSFCloneHologram** HoloDataPtr = HologramDataMap.Find(Pair.Key))
                {
                    const FSFCloneHologram& HoloData = **HoloDataPtr;
                    if (ASFConveyorBeltHologram* Belt = Cast<ASFConveyorBeltHologram>(Child))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            const FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            const FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            const FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            const FVector EndNormal = HoloData.LaneEndNormal.ToFVector();
                            // [#380] Honor the configured belt routing mode on the MP/server-commit replay
                            // path too - this previously called AutoRouteSplineWithNormals directly, so
                            // lane belts always routed "Default" on dedicated servers even with Curve set.
                            Belt->RouteLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);
                            Belt->TriggerMeshGeneration();
                            Belt->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
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
                    else if (ASFPipelineHologram* Pipe = Cast<ASFPipelineHologram>(Child))
                    {
                        if (HoloData.bIsLaneSegment && HoloData.bHasSplineData && HoloData.SplineData.Points.Num() >= 2)
                        {
                            const FVector StartPos = HoloData.SplineData.Points[0].World.ToFVector();
                            const FVector EndPos = HoloData.SplineData.Points.Last().World.ToFVector();
                            const FVector StartNormal = HoloData.LaneStartNormal.ToFVector();
                            const FVector EndNormal = HoloData.LaneEndNormal.ToFVector();
                            Pipe->RoutePipeLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);  // [#383] honor pipe routing mode
                            Pipe->TriggerMeshGeneration();
                            Pipe->ForceApplyHologramMaterial();
                        }
                        else if (HoloData.bHasSplineData)
                        {
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
                Owner->HologramService->TrackChildHologram(Child, IntendedPos, IntendedRot);
            }
        }
        KickRestoredPreviewParent(ParentHologram);
        Owner->HologramService->RefreshChildPositions();
    }

    Owner->RestoredCloneLastParentLocation = ParentHologram->GetActorLocation();
    Owner->RestoredCloneLastParentRotation = ParentHologram->GetActorRotation();

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][Extend] Replayed Extend topology: infra=%d factories=%d tracked=%d parent=%s"),
        SpawnedCount,
        SpawnedFactoryCount,
        Owner->HologramService ? Owner->HologramService->GetTrackedChildren().Num() : 0,
        *GetNameSafe(ParentHologram));
    return true;
}
