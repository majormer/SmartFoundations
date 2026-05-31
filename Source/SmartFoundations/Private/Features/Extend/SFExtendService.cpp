// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendService Implementation
 *
 * See SFExtendService.h for architecture overview and documentation.
 */

#include "Features/Extend/SFExtendService.h"
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

    // Get building size from registry
    FVector BuildingSize = FVector(800.0f, 800.0f, 400.0f); // Default fallback
    if (USFBuildableSizeRegistry::HasProfile(SourceBuilding->GetClass()))
    {
        FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(SourceBuilding->GetClass());
        BuildingSize = Profile.DefaultSize;
    }

    // Calculate target position for this direction
    FVector LocalOffset = FVector::ZeroVector;
    switch (Direction)
    {
    case ESFExtendDirection::Right:
        LocalOffset = FVector(BuildingSize.X, 0.0f, 0.0f);
        break;
    case ESFExtendDirection::Left:
        LocalOffset = FVector(-BuildingSize.X, 0.0f, 0.0f);
        break;
    }

    FRotator BuildingRotation = SourceBuilding->GetActorRotation();
    FVector WorldOffset = BuildingRotation.RotateVector(LocalOffset);
    FVector TargetCenter = SourceBuilding->GetActorLocation() + WorldOffset;

    // Use a box overlap to check if there's a building at the target position
    // Use slightly smaller box to avoid false positives from adjacent buildings
    FVector HalfExtent = BuildingSize * 0.4f; // 80% of building size, centered

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(SourceBuilding);

    // Check for factory buildings at target position
    bool bHasOverlap = World->OverlapMultiByChannel(
        Overlaps,
        TargetCenter,
        FQuat::Identity,
        ECC_WorldStatic, // Use static channel to find buildings
        FCollisionShape::MakeBox(HalfExtent),
        QueryParams
    );

    if (bHasOverlap)
    {
        // Check if any overlap is a factory building (not just terrain/foundations)
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AActor* HitActor = Overlap.GetActor())
            {
                if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(HitActor))
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT(" EXTEND: Direction %s blocked by %s"),
                        Direction == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"),
                        *Factory->GetName());
                    return false;
                }
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
            if (bExtendCommitted)
            {
                // Committed — keep Extend alive, just maintain preview
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
            return false;
        }

        UClass* HologramBuildClass = SourceHologram->GetBuildClass();
        if (!HologramBuildClass || !HitBuilding->IsA(HologramBuildClass))
        {
            return false;
        }

        if (!IsValidExtendTarget(HitBuilding))
        {
            return false;
        }
    }

    // Issue #274: When Scaled Extend is committed (locked for inspection), suppress re-triggering
    // on a different building of the same type. The player is walking around to inspect the layout
    // and accidentally facing a similar building shouldn't tear down the entire chain.
    if (bExtendCommitted && CurrentExtendTarget.IsValid() && CurrentExtendTarget.Get() != HitBuilding)
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

    if (!WalkTopology(HitBuilding))
    {
        // Debug: Log topology walk failure
        static double LastTopoLog = 0;
        double Now = FPlatformTime::Seconds();
        if (Now - LastTopoLog > 2.0)
        {
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND: No valid topology found for %s (no belts/distributors connected?)"),
                *HitBuilding->GetName());
            LastTopoLog = Now;
        }
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

    AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(Hologram->GetWorld());
    const TArray<FItemAmount> TotalCost = Hologram->GetCost(/*includeChildren=*/true);
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

    // CRITICAL: Ensure hologram stays locked
    if (!ActiveHologram->IsHologramLocked())
    {
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
            SyncExtendPreviewMaterialState(SpawnedPair.Value);
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

ASFFactoryHologram* USFExtendService::SwapToSmartFactoryHologram(AFGHologram* VanillaHologram)
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

    // Track the swap (both locally and in HologramService for future migration)
    SwappedHologram = CustomHologram;
    bHasSwappedHologram = true;

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔄 EXTEND SWAP: ✅ Successfully swapped to ASFFactoryHologram"));

    return CustomHologram;
}

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
    return BuiltChainElements.Num() > 0 ||
        BuiltConveyorsByChain.Num() > 0 ||
        BuiltDistributorsByChain.Num() > 0 ||
        BuiltJunctionsByChain.Num() > 0 ||
        BuiltPipesByChain.Num() > 0 ||
        JsonBuiltActors.Num() > 0 ||
        JsonSpawnedHolograms.Num() > 0 ||
        PowerPoleWiringData.Num() > 0 ||
        ScaledExtendClones.Num() > 0 ||
        (StoredCloneTopology.IsValid() && StoredCloneTopology->ChildHolograms.Num() > 0);
}

void USFExtendService::RegisterBuiltConveyor(int32 ChainId, int32 ChainIndex, AFGBuildableConveyorBase* BuiltConveyor, bool bIsInputChain)
{
    if (!BuiltConveyor) return;

    // Get or create the index map for this chain
    TMap<int32, AFGBuildableConveyorBase*>& ChainConveyors = BuiltConveyorsByChain.FindOrAdd(ChainId);
    ChainConveyors.Add(ChainIndex, BuiltConveyor);

    // Track chain direction
    if (!BuiltChainIsInputMap.Contains(ChainId))
    {
        BuiltChainIsInputMap.Add(ChainId, bIsInputChain);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built conveyor %s in chain %d at index %d (now %d conveyors, isInput=%d)"),
        *BuiltConveyor->GetName(), ChainId, ChainIndex, ChainConveyors.Num(), bIsInputChain);
}

AFGBuildableConveyorBase* USFExtendService::GetBuiltConveyor(int32 ChainId, int32 ChainIndex) const
{
    const TMap<int32, AFGBuildableConveyorBase*>* ChainConveyors = BuiltConveyorsByChain.Find(ChainId);
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

void USFExtendService::RegisterBuiltDistributor(int32 ChainId, AFGBuildable* BuiltDistributor)
{
    if (!BuiltDistributor) return;

    BuiltDistributorsByChain.Add(ChainId, BuiltDistributor);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built distributor %s for chain %d"),
        *BuiltDistributor->GetName(), ChainId);
}

AFGBuildable* USFExtendService::GetBuiltDistributor(int32 ChainId) const
{
    AFGBuildable* const* Distributor = BuiltDistributorsByChain.Find(ChainId);
    return Distributor ? *Distributor : nullptr;
}

FName USFExtendService::GetDistributorConnectorName(int32 ChainId) const
{
    const FName* ConnectorName = DistributorConnectorNameByChain.Find(ChainId);
    return ConnectorName ? *ConnectorName : NAME_None;
}

void USFExtendService::SetDistributorConnectorName(int32 ChainId, FName ConnectorName)
{
    DistributorConnectorNameByChain.Add(ChainId, ConnectorName);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Stored distributor connector name '%s' for chain %d"),
        *ConnectorName.ToString(), ChainId);
}

void USFExtendService::RegisterBuiltJunction(int32 ChainId, AFGBuildable* BuiltJunction)
{
    if (!BuiltJunction) return;

    BuiltJunctionsByChain.Add(ChainId, BuiltJunction);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built junction %s for pipe chain %d"),
        *BuiltJunction->GetName(), ChainId);
}

void USFExtendService::RegisterBuiltPipe(int32 ChainId, int32 ChainIndex, AFGBuildablePipeline* BuiltPipe, bool bIsInputChain)
{
    if (!BuiltPipe) return;

    // Create chain entry if it doesn't exist
    if (!BuiltPipesByChain.Contains(ChainId))
    {
        BuiltPipesByChain.Add(ChainId, TMap<int32, AFGBuildablePipeline*>());
    }

    // Add pipe to chain at index
    BuiltPipesByChain[ChainId].Add(ChainIndex, BuiltPipe);

    // Track chain direction
    if (!BuiltPipeChainIsInputMap.Contains(ChainId))
    {
        BuiltPipeChainIsInputMap.Add(ChainId, bIsInputChain);
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Registered built pipe %s for pipe chain %d, index %d (%s)"),
        *BuiltPipe->GetName(), ChainId, ChainIndex, bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));
}

int32 USFExtendService::CopyDistributorConfigurations()
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
    // registers them in JsonBuiltActors keyed by a symbolic HologramId (e.g. "distributor_0").
    // StoredCloneTopology->ChildHolograms links each clone HologramId to its source actor name
    // via SourceId. We walk ChildHolograms for every entry with Role=="distributor", resolve
    // source → clone via that mapping, and copy user state. The legacy BuiltDistributorsByChain
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
    if (StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& ChildHolo : StoredCloneTopology->ChildHolograms)
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
    // Older code paths that don't populate StoredCloneTopology may still populate
    // BuiltDistributorsByChain directly from ASFConveyorAttachmentChildHologram::Construct().
    // Walk it as a safety net. ChainId layout: [0..NumInputChains) indexes InputChains;
    // [NumInputChains..NumInputChains+NumOutputChains) indexes OutputChains. Mirrors the
    // ChainId convention used elsewhere in WireBuiltChildConnections for pipe/belt chains.
    if (!BuiltDistributorsByChain.IsEmpty())
    {
        const FSFExtendTopology& Topology = GetCurrentTopology();
        const int32 NumInputChains = Topology.InputChains.Num();
        const int32 NumOutputChains = Topology.OutputChains.Num();

        for (const TPair<int32, AFGBuildable*>& ClonePair : BuiltDistributorsByChain)
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

void USFExtendService::WireBuiltChildConnections(AFGBuildableFactory* NewFactory)
{
    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called with null factory"));
        return;
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called for %s (StoredCloneTopology=%s, JsonBuiltActors=%d)"),
        *NewFactory->GetName(),
        (StoredCloneTopology.IsValid() && StoredCloneTopology->ChildHolograms.Num() > 0) ? TEXT("VALID") : TEXT("INVALID"),
        JsonBuiltActors.Num());

    if (bRestoredCloneTopologyActive || bRestoredScaledWiringDeferred)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Post-build wiring start: factory=%s storedChildren=%d jsonBuilt=%d jsonSpawned=%d previewFactories=%d parentValid=%d retry=%d/%d deferred=%d"),
            *NewFactory->GetName(),
            StoredCloneTopology.IsValid() ? StoredCloneTopology->ChildHolograms.Num() : 0,
            JsonBuiltActors.Num(),
            JsonSpawnedHolograms.Num(),
            RestoredScaledFactoryPreviewLocations.Num(),
            RestoredCloneParentHologram.IsValid() ? 1 : 0,
            RestoredScaledWiringRetryAttempts,
            5,
            bRestoredScaledWiringDeferred ? 1 : 0);
    }

    // Issues #298, #299: Copy Smart/Programmable Splitter filter configuration from every
    // source distributor to its cloned counterpart before wiring brings belts online. Done
    // here (rather than at individual Construct() time) so the source → clone pairing has
    // the full topology available for lookup.
    CopyDistributorConfigurations();

    int32 TotalConveyorChains = BuiltConveyorsByChain.Num();
    int32 TotalPipeChains = BuiltPipesByChain.Num();

    // ==================== PIPE ATTACHMENT PRE-WIRING (Issue #288) ====================
    // Wire cloned valves/pumps to their neighbouring pipe connectors BEFORE the JSON
    // wiring pass in GenerateAndExecuteWiring consumes the topology. Each attachment's
    // two UFGPipeConnectionComponent endpoints coincide with the adjacent cloned pipes'
    // terminal connectors, so a small-radius proximity match reliably links them.
    // IMPORTANT: this must run BEFORE GenerateAndExecuteWiring — that function clears
    // StoredCloneTopology / JsonBuiltActors on completion, which would otherwise leave
    // both 3.8a and 3.8b with nothing to iterate.
    {
        constexpr float AttachmentConnectorProximitySqCm = 25.0f * 25.0f;  // 25 cm radius
        int32 AttachmentsWired = 0;
        for (const TPair<FString, AActor*>& Entry : JsonBuiltActors)
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

                for (const TPair<FString, AActor*>& PipeEntry : JsonBuiltActors)
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
            AttachmentsWired, JsonBuiltActors.Num());
    }

    // ==================== PUMP POWER WIRING (Issue #288, Phase 3.8b) ====================
    // Source-linked pump → pole wiring: for each pipe_attachment clone whose source
    // pump was directly connected to an in-manifold power pole, we spawn a power
    // line from the clone pump's PowerInput to the clone pole's power connector.
    // Runs BEFORE GenerateAndExecuteWiring because that function resets
    // StoredCloneTopology and empties JsonBuiltActors at its end.
    if (StoredCloneTopology.IsValid())
    {
        UClass* PumpWireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
        int32 PumpsWired = 0;
        int32 PumpsSkipped = 0;

        int32 AttachmentTotal = 0;
        int32 AttachmentLinked = 0;
        for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
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
            AttachmentTotal, AttachmentLinked, JsonBuiltActors.Num());

        for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
        {
            if (Holo.Role != TEXT("pipe_attachment")) continue;
            if (Holo.ConnectedPowerPoleHologramId.IsEmpty()) continue;  // valve or unpowered pump

            AActor* const* PumpActorPtr = JsonBuiltActors.Find(Holo.HologramId);
            AActor* const* PoleActorPtr = JsonBuiltActors.Find(Holo.ConnectedPowerPoleHologramId);
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
                ++PumpsWired;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ EXTEND Phase 3.8b (#288): wired pump %s → pole %s (pole now at %d/%d)"),
                    *ClonePump->GetName(), *ClonePole->GetName(),
                    PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections());
            }
            else
            {
                ++PumpsSkipped;
                NewWire->Destroy();
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
    // NOTE: GenerateAndExecuteWiring resets StoredCloneTopology and JsonBuiltActors
    // at its end — phases 3.8a/3.8b above must run before this call.
    int32 JsonWiredCount = GenerateAndExecuteWiring(NewFactory);
    if (bRestoredScaledWiringDeferred)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] JSON wiring deferred; leaving post-build tracking intact for retry (jsonBuilt=%d, jsonSpawned=%d, storedChildren=%d)"),
            JsonBuiltActors.Num(),
            JsonSpawnedHolograms.Num(),
            StoredCloneTopology.IsValid() ? StoredCloneTopology->ChildHolograms.Num() : 0);
        return;
    }
    if (JsonWiredCount > 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 5/6: JSON-based wiring completed - %d connections"), JsonWiredCount);
    }

    // NOTE (Issue #288): Previously this function early-returned here when
    // BuiltConveyorsByChain and BuiltPipesByChain were both empty. That gated
    // phases 3.8a (pipe-attachment pre-wiring) and 3.8b (pump→pole power wiring)
    // behind the legacy chain maps, even though both phases only read from
    // JsonBuiltActors / StoredCloneTopology. For JSON-only topologies (e.g. an
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
    //   (this creates unified chains)
    // - Endpoint connections (distributor↔first conveyor, last conveyor↔factory) are done
    //   post-build because snapped connections don't work for belt→distributor
    //
    // The snapped connections are set in SFConveyorBeltHologram::Construct() BEFORE
    // Super::Construct() is called, pointing to already-built conveyors.
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring ENDPOINT connections only (conveyor↔conveyor handled by snapped connections)"));

    for (auto& ChainPair : BuiltConveyorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ConveyorsByIndex = ChainPair.Value;
        bool bIsInputChain = BuiltChainIsInputMap.FindRef(ChainId);

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

            // Get the built distributor for this chain
            AFGBuildable** DistPtr = BuiltDistributorsByChain.Find(ChainId);
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
    //  so that StoredCloneTopology and JsonBuiltActors are still populated when they run.)

    // ==================== PIPE CHAIN WIRING ====================
    // Process pipe chains similarly to belt chains

    if (TotalPipeChains > 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring %d pipe chains"), TotalPipeChains);

        for (auto& PipeChainPair : BuiltPipesByChain)
        {
            int32 PipeChainId = PipeChainPair.Key;
            TMap<int32, AFGBuildablePipeline*>& PipesByIndex = PipeChainPair.Value;
            bool bIsInputChain = BuiltPipeChainIsInputMap.FindRef(PipeChainId);

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
                if (bIsInputChain && PipeChainId < GetCurrentTopology().PipeInputChains.Num())
                {
                    if (GetCurrentTopology().PipeInputChains[PipeChainId].SourceConnector.IsValid())
                    {
                        SourceFactoryConnectorName = GetCurrentTopology().PipeInputChains[PipeChainId].SourceConnector->GetFName();
                    }
                }
                else if (!bIsInputChain)
                {
                    int32 OutputChainIndex = PipeChainId - GetCurrentTopology().PipeInputChains.Num();
                    if (OutputChainIndex >= 0 && OutputChainIndex < GetCurrentTopology().PipeOutputChains.Num())
                    {
                        if (GetCurrentTopology().PipeOutputChains[OutputChainIndex].SourceConnector.IsValid())
                        {
                            SourceFactoryConnectorName = GetCurrentTopology().PipeOutputChains[OutputChainIndex].SourceConnector->GetFName();
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

                // Get the CLONE junction for this chain
                AFGBuildable** JunctionPtr = BuiltJunctionsByChain.Find(PipeChainId);
                if (JunctionPtr && *JunctionPtr)
                {
                    AFGBuildable* CloneJunction = *JunctionPtr;

                    // Get the source junction connector name from topology
                    FName SourceJunctionConnectorName = NAME_None;
                    if (bIsInputChain && PipeChainId < GetCurrentTopology().PipeInputChains.Num())
                    {
                        if (GetCurrentTopology().PipeInputChains[PipeChainId].JunctionConnector.IsValid())
                        {
                            SourceJunctionConnectorName = GetCurrentTopology().PipeInputChains[PipeChainId].JunctionConnector->GetFName();
                        }
                    }
                    else if (!bIsInputChain)
                    {
                        // For output chains, PipeChainId is offset by the number of input chains
                        int32 OutputChainIndex = PipeChainId - GetCurrentTopology().PipeInputChains.Num();
                        if (OutputChainIndex >= 0 && OutputChainIndex < GetCurrentTopology().PipeOutputChains.Num())
                        {
                            if (GetCurrentTopology().PipeOutputChains[OutputChainIndex].JunctionConnector.IsValid())
                            {
                                SourceJunctionConnectorName = GetCurrentTopology().PipeOutputChains[OutputChainIndex].JunctionConnector->GetFName();
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
            for (auto& PipeChainPair : BuiltPipesByChain)
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
        GetCurrentTopology().InputChains.Num(), GetCurrentTopology().OutputChains.Num());
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: BuiltConveyorsByChain has %d chains:"), BuiltConveyorsByChain.Num());
    for (auto& DebugPair : BuiltConveyorsByChain)
    {
        bool bDebugIsInput = BuiltChainIsInputMap.FindRef(DebugPair.Key);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix:   ChainId=%d, IsInput=%d, ConveyorCount=%d"),
            DebugPair.Key, bDebugIsInput ? 1 : 0, DebugPair.Value.Num());
    }

    // Calculate offset from source to clone factory
    FVector CloneOffset = FVector::ZeroVector;
    if (GetCurrentTopology().SourceBuilding.IsValid() && NewFactory)
    {
        CloneOffset = NewFactory->GetActorLocation() - GetCurrentTopology().SourceBuilding->GetActorLocation();
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Clone offset = %s"), *CloneOffset.ToString());
    }
    else
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Cannot calculate offset - missing source or new factory!"));
    }

    // Process each conveyor chain
    for (auto& ChainPair : BuiltConveyorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ConveyorMap = ChainPair.Value;
        bool bIsInput = BuiltChainIsInputMap.FindRef(ChainId);

        // Get the SOURCE chain from cached topology
        const FSFConnectionChainNode* SourceChain = nullptr;
        if (bIsInput && ChainId < GetCurrentTopology().InputChains.Num())
        {
            SourceChain = &GetCurrentTopology().InputChains[ChainId];
        }
        else if (!bIsInput)
        {
            // Output chains use ChainId offset by input chain count
            int32 OutputChainIndex = ChainId - GetCurrentTopology().InputChains.Num();
            if (OutputChainIndex >= 0 && OutputChainIndex < GetCurrentTopology().OutputChains.Num())
            {
                SourceChain = &GetCurrentTopology().OutputChains[OutputChainIndex];
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
            AFGBuildable** CloneDistributorPtr = BuiltDistributorsByChain.Find(ChainId);
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
    BuiltConveyorsByChain.Empty();
    BuiltDistributorsByChain.Empty();
    BuiltJunctionsByChain.Empty();
    BuiltPipesByChain.Empty();
    BuiltChainIsInputMap.Empty();
    BuiltPipeChainIsInputMap.Empty();

    // Also clear source tracking maps (used for pipe junction connections)
    SourceDistributorsByChain.Empty();
    SourceJunctionsByChain.Empty();
}

// ==================== Manifold Connections ====================

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
