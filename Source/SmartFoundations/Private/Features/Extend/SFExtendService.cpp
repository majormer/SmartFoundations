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
#include "Features/Extend/SFManifoldJSON.h"
#include "Features/Extend/SFWiringManifest.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
// NOTE: SFRecipeCostInjector.h removed - child holograms automatically aggregate costs via GetCost()
#include "Services/RadarPulse/SFRadarPulseService.h"
#include "SmartFoundations.h"  // For LogSmartFoundations
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
    
    UE_LOG(LogSmartFoundations, Log, TEXT("Smart!: SFExtendService initialized (with DetectionService, TopologyService, HologramService, and WiringService)"));
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
            UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND: Unlocked CurrentExtendHologram"));
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
            UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND: Restoring pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
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
        UE_LOG(LogSmartFoundations, Log, TEXT("Smart!: EXTEND state cleared"));
    }
}

void USFExtendService::Shutdown()
{
    ClearExtendState();
    
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
    UE_LOG(LogSmartFoundations, Log, TEXT("Smart!: SFExtendService shutdown"));
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
        UE_LOG(LogSmartFoundations, Warning, TEXT(" EXTEND: CycleDirection called but no valid target or no detection service"));
        return;
    }

    // Get valid directions - only cycle if there's more than one option
    TArray<ESFExtendDirection> ValidDirs = GetValidDirections();
    ESFExtendDirection CurrentDir = DetectionService->GetExtendDirection();
    
    if (ValidDirs.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT(" EXTEND: No valid directions available - both sides blocked"));
        return;
    }
    
    if (ValidDirs.Num() == 1)
    {
        // Only one valid direction - can't cycle, but ensure we're using it
        if (CurrentDir != ValidDirs[0])
        {
            DetectionService->SetExtendDirection(ValidDirs[0]);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" EXTEND: Only one valid direction, using %s"), 
                ValidDirs[0] == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
        }
        else
        {
            UE_LOG(LogSmartFoundations, Log, TEXT(" EXTEND: Cannot cycle - only one valid direction (%s)"),
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
        UE_LOG(LogSmartFoundations, Log, TEXT(" EXTEND: Cannot cycle to %s - direction blocked"),
            NewDirection == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
        return;
    }
    
    DetectionService->SetExtendDirection(NewDirection);

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" EXTEND: Direction changed to %s"), 
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
                    UE_LOG(LogSmartFoundations, Log, TEXT(" EXTEND: Direction %s blocked by %s"),
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
        return TopologyService->WalkTopology(SourceBuilding);
    }
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("Smart!: WalkTopology called but TopologyService not initialized"));
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: TryExtend called - Building=%s, Hologram=%s, bHasValidTarget=%d"),
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
                UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND: Deactivating (not committed, looked away)"));
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Hologram changed (build completed?) - resetting EXTEND state"));
            
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Already extending from %s, refreshing"), *HitBuilding->GetName());
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
        UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND: User looked away from last built building - cooldown cleared"));
        LastBuiltFromBuilding.Reset();
    }

    // New target - walk topology
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Checks passed! Walking topology for %s"), *HitBuilding->GetName());
    
    // NOTE: Orphaned pending builds will be cleared at the start of CreateBeltPreviews().
    // No need to clear here - the flow is: WalkTopology → CreateBeltPreviews (clears all → spawns new).
    
    if (!WalkTopology(HitBuilding))
    {
        // Debug: Log topology walk failure
        static double LastTopoLog = 0;
        double Now = FPlatformTime::Seconds();
        if (Now - LastTopoLog > 2.0)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: No valid topology found for %s (no belts/distributors connected?)"), 
                *HitBuilding->GetName());
            LastTopoLog = Now;
        }
        return false;
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
        UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND: Snapshot pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
            PreExtendCounterSnapshot.GridCounters.X, PreExtendCounterSnapshot.GridCounters.Y,
            PreExtendCounterSnapshot.SpacingX);
    }

    // Pick a valid direction - check if current direction is blocked
    TArray<ESFExtendDirection> ValidDirs = GetValidDirections();
    if (ValidDirs.Num() == 0)
    {
        // No valid directions - both sides blocked, can't extend
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND: Cannot activate - both directions blocked by existing buildings"));
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Auto-selected direction %s (other side blocked)"),
            CurrentDir == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
    }

    UE_LOG(LogSmartFoundations, Display, TEXT("🔄 EXTEND: ✅ ACTIVATED - pointing at %s (direction: %s, valid dirs: %d)"), 
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
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND: Hologram swap failed, using vanilla hologram"));
    }
    
    // Track which hologram we've set up for EXTEND
    CurrentExtendHologram = ActiveHologram;

    // Lock the hologram
    ActiveHologram->LockHologramPosition(true);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Locked hologram %s"), *ActiveHologram->GetClass()->GetName());

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

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Positioned hologram at offset (%.1f, %.1f, %.1f) - LOCKED"),
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
    if (!bScaledExtendValid)
    {
        // Lane segment validation failed — keep hologram red and block building
        ActiveHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
    }
    else
    {
        ActiveHologram->ResetConstructDisqualifiers();
        ActiveHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    }
    
    // CRITICAL: Force child holograms back to their intended positions every frame
    // The engine's parent hologram tick resets children to origin via SetHologramLocationAndRotation
    // We counteract this by forcing them back to where we want them
    
    // Delegate basic position/rotation refresh to HologramService
    if (HologramService)
    {
        HologramService->RefreshChildPositions();
    }
    
    // Additional material handling for Smart! hologram types
    for (AFGHologram* Child : BeltPreviewHolograms)
    {
        if (IsValid(Child))
        {
            // CRITICAL: Call ForceApplyHologramMaterial to actually update mesh materials
            // SetPlacementMaterialState only sets the state, this applies it to mesh components
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
            
            // Disable tick to prevent engine's CheckValidPlacement from overriding our material state
            Child->SetActorTickEnabled(false);
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
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Pos=(%.0f,%.0f,%.0f) Scale=%.2f Visible=%d Hidden=%d Mat=%s"),
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
                    // Force material state back to OK and re-apply to spline meshes
                    BeltChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 BELT REFRESH: %s has %d SplineMeshComponents"), 
                            *BeltChild->GetName(), SplineMeshComps.Num());
                        for (int32 SMIdx = 0; SMIdx < SplineMeshComps.Num() && SMIdx < 2; SMIdx++)
                        {
                            USplineMeshComponent* SMC = SplineMeshComps[SMIdx];
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 BELT REFRESH:   SMC[%d]: Mesh=%s, Visible=%d, WorldPos=%s"),
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
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND Child[%d]: Pos=%s Hidden=%d MatState=%d"),
                    i, *Child->GetActorLocation().ToString(),
                    Child->IsHidden() ? 1 : 0,
                    (int32)Child->GetHologramMaterialState());
            }
            else
            {
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND Child[%d]: INVALID/DESTROYED"), i);
            }
        }
        
        LastRefreshPosLog = Now;
    }
}

void USFExtendService::CleanupExtension(AFGHologram* SourceHologram)
{
    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: CleanupExtension called for %s"), 
        SourceHologram ? *SourceHologram->GetName() : TEXT("nullptr"));
    
    // Restore pre-Extend counter snapshot so normal scaling isn't polluted
    if (bHasCounterSnapshot && Subsystem.IsValid())
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Restoring pre-Extend counters (X=%d, Y=%d, Spacing=%d)"),
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
        UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Set cooldown on %s to prevent immediate re-activation"), 
            *CurrentExtendTarget->GetName());
    }
    
    CurrentExtendTarget.Reset();
    CurrentExtendHologram.Reset();
    ClearTopology();
    
    // CRITICAL: Reset state flags so next build doesn't try to extend from same location
    bHasValidTarget = false;
    bExtendCommitted = false;
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: State fully cleared (bHasValidTarget=false, pending belts preserved for deferred build)"));
}

void USFExtendService::CheckAndPerformFinalCleanup()
{
    if (!bNeedsFinalCleanup)
    {
        return;
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Performing final cleanup (build gun left build mode)"));
    
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
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔧 EXTEND: Final cleanup complete"));
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
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND SWAP: Invalid vanilla hologram"));
        return nullptr;
    }
    
    // Only swap factory holograms
    AFGFactoryHologram* FactoryHolo = Cast<AFGFactoryHologram>(VanillaHologram);
    if (!FactoryHolo)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND SWAP: Not a factory hologram - %s"), 
            *VanillaHologram->GetClass()->GetName());
        return nullptr;
    }
    
    // Get the build gun and its build state
    AFGBuildGun* BuildGun = GetPlayerBuildGun();
    if (!BuildGun)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND SWAP: Could not get build gun"));
        return nullptr;
    }
    
    UFGBuildGunStateBuild* BuildState = GetBuildGunBuildState(BuildGun);
    if (!BuildState)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND SWAP: Could not get build state"));
        return nullptr;
    }
    
    // Get world for spawning
    UWorld* World = VanillaHologram->GetWorld();
    if (!World)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔄 EXTEND SWAP: No world"));
        return nullptr;
    }
    
    // Verify the vanilla hologram has a build class
    if (!VanillaHologram->GetBuildClass())
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("🔄 EXTEND SWAP: Vanilla hologram has no BuildClass"));
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
        UE_LOG(LogSmartFoundations, Error, TEXT("🔄 EXTEND SWAP: Failed to spawn deferred custom hologram"));
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
                UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND SWAP: ✅ Set mHologram via reflection"));
            }
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("🔄 EXTEND SWAP: Could not find mHologram property"));
        CustomHologram->Destroy();
        return nullptr;
    }
    
    // Destroy the vanilla hologram
    VanillaHologram->Destroy();
    
    // Track the swap (both locally and in HologramService for future migration)
    SwappedHologram = CustomHologram;
    bHasSwappedHologram = true;
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND SWAP: ✅ Successfully swapped to ASFFactoryHologram"));
    
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
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔄 EXTEND SWAP: Hologram swap state cleared"));
}

// ==================== Phase 2: Infrastructure Cloning (delegates to HologramService) ====================

void USFExtendService::CreateBeltPreviews(AFGHologram* ParentHologram)
{
    if (!ParentHologram || !GetCurrentTopology().bIsValid || !GetCurrentTopology().SourceBuilding.IsValid())
    {
        return;
    }

    // DIAGNOSTIC: Capture preview snapshot ONCE when EXTEND previews are first created
    if (!bHasPreviewSnapshot)
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
                    
                    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND: Stored power pole wiring data for %s (source=%s, free=%d)"),
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
    SourceToHologramMap.Empty();
    PipeChainHologramMap.Empty();
    PipeChainJunctionMap.Empty();
    BeltChainHologramMap.Empty();
    LiftChainHologramMap.Empty();
    BeltChainDistributorMap.Empty();
    ManifoldBeltHolograms.Empty();
    
    // Also clear built tracking maps (used by WireBuiltChildConnections)
    BuiltConveyorsByChain.Empty();
    BuiltDistributorsByChain.Empty();
    BuiltJunctionsByChain.Empty();
    BuiltPipesByChain.Empty();
    BuiltChainIsInputMap.Empty();
    BuiltPipeChainIsInputMap.Empty();
    
    // Clear power pole wiring data (Issue #229)
    PowerPoleWiringData.Empty();
    
    // Clear source distributor/junction maps (used by WireManifoldConnections)
    SourceDistributorsByChain.Empty();
    SourceJunctionsByChain.Empty();
    
    // Clear distributor connector name map (used by Construct() to find correct output)
    DistributorConnectorNameByChain.Empty();
}

UFGPipeConnectionComponentBase* USFExtendService::FindPipeConnectionByIndex(AFGHologram* Hologram, int32 Index) const
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
    
    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: FindPipeConnectionByIndex(%s, %d) - found %d valid connections"),
        *Hologram->GetName(), Index, ValidConnections.Num());
    
    if (Index < ValidConnections.Num())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire:   Returning %s"), *ValidConnections[Index]->GetName());
        return ValidConnections[Index];
    }
    
    return nullptr;
}

void USFExtendService::WirePipeChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain)
{
    // Get the pipe holograms for this chain (in order: 0=closest to factory, N-1=closest to junction)
    TArray<ASFPipelineHologram*>* PipeHolograms = PipeChainHologramMap.Find(ChainId);
    if (!PipeHolograms || PipeHolograms->Num() == 0)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Wire: No pipe holograms found for chain %d"), ChainId);
        return;
    }
    
    // Get the junction hologram for this chain
    ASFPipelineJunctionChildHologram* JunctionHologram = nullptr;
    if (ASFPipelineJunctionChildHologram** JunctionPtr = PipeChainJunctionMap.Find(ChainId))
    {
        JunctionHologram = *JunctionPtr;
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Wire: Wiring chain %d (%s) with %d pipes, Junction=%s"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Factory %s (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
                else
                {
                    // OUTPUT: Pipe[0].Conn0 → Factory.Output (items enter pipe from factory)
                    Conn0Target = FactoryConn;
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 → Factory %s (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Factory connection (no pipe connections found on parent!)"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 ← Pipe[%d].Conn1 (%s) [INPUT]"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Pipe[%d].Conn0 (%s) [INPUT]"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 ← Pipe[%d].Conn1 (%s) [OUTPUT]"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Pipe[%d].Conn0 (%s) [OUTPUT]"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn0 → Junction %s (%s) [INPUT]"),
                        i, *PipeHolo->GetName(), *JunctionHologram->GetName(), JunctionConn ? *JunctionConn->GetName() : TEXT("nullptr"));
                }
                else
                {
                    // OUTPUT: Pipe[N].Conn1 → Junction.Input (items exit pipe into junction)
                    Conn1Target = JunctionConn;
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Conn1 → Junction %s (%s) [OUTPUT]"),
                        i, *PipeHolo->GetName(), *JunctionHologram->GetName(), JunctionConn ? *JunctionConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Pipe[%d] %s - Junction connection (no junction hologram!)"),
                    i, *PipeHolo->GetName());
            }
        }
        
        // Apply the snapped connections
        // CRITICAL: Both connections must be non-null to prevent vanilla from spawning child poles
        // If either is null, vanilla thinks that end is dangling and spawns a pole
        if (Conn0Target || Conn1Target)
        {
            PipeHolo->SetSnappedConnections(Conn0Target, Conn1Target);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Wire: ✅ Set snapped connections for %s - Conn0=%s, Conn1=%s"),
                *PipeHolo->GetName(),
                Conn0Target ? *Conn0Target->GetName() : TEXT("nullptr"),
                Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Wire: ⚠️ Both connections null for %s - this will cause pole spawning!"),
                *PipeHolo->GetName());
        }
    }
}

UFGFactoryConnectionComponent* USFExtendService::FindFactoryConnectionByIndex(AFGHologram* Hologram, int32 Index) const
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
    
    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: FindFactoryConnectionByIndex(%s, %d) - found %d valid connections"),
        *Hologram->GetName(), Index, ValidConnections.Num());
    
    if (Index < ValidConnections.Num())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire:   Returning %s"), *ValidConnections[Index]->GetName());
        return ValidConnections[Index];
    }
    
    return nullptr;
}

void USFExtendService::WireBeltChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain)
{
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING CHAIN %d (%s) ============================"),
        ChainId, bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));
    
    // Get the unified conveyor chain (belts + lifts) for this chain
    TMap<int32, AFGHologram*>* UnifiedChainPtr = UnifiedConveyorChainMap.Find(ChainId);
    if (!UnifiedChainPtr || UnifiedChainPtr->Num() == 0)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING: No conveyor holograms found for chain %d"), ChainId);
        return;
    }
    
    TMap<int32, AFGHologram*>& UnifiedChain = *UnifiedChainPtr;
    
    // Get the distributor hologram for this chain (if any)
    AFGHologram** DistributorPtr = BeltChainDistributorMap.Find(ChainId);
    AFGHologram* DistributorHologram = DistributorPtr ? *DistributorPtr : nullptr;
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING: Chain has %d elements, Distributor=%s"),
        UnifiedChain.Num(), DistributorHologram ? *DistributorHologram->GetName() : TEXT("NONE"));
    
    // Get factory connections from parent factory hologram
    TArray<UFGFactoryConnectionComponent*> ParentFactoryConnections;
    if (ParentHologram)
    {
        ParentHologram->GetComponents<UFGFactoryConnectionComponent>(ParentFactoryConnections);
        UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Parent %s has %d factory connections"),
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Wire: Skipping non-belt hologram %s"), *Hologram->GetName());
            continue;
        }
        
        // Get hologram data to determine chain position
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(BeltHolo);
        if (!HoloData)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Wire: Belt %s has no hologram data!"), *BeltHolo->GetName());
            continue;
        }
        
        int32 ChainIndex = HoloData->ExtendChainIndex;
        int32 ChainLength = HoloData->ExtendChainLength;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Wire: Wiring belt %s at index %d/%d (%s)"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Distributor %s (%s) [INPUT SOURCE]"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Factory %s (%s) [OUTPUT SOURCE]"),
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
                UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn0 ← Prev[%d].Conn1 %s (%s)"),
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
                UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Next[%d].Conn0 %s (%s)"),
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
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Factory %s (%s) [INPUT DEST]"),
                        ChainIndex, *BeltHolo->GetName(), *ParentHologram->GetName(), FactoryConn ? *FactoryConn->GetName() : TEXT("nullptr"));
                }
            }
            else
            {
                // OUTPUT chain destination = Distributor (merger)
                if (DistributorHologram)
                {
                    Conn1Target = FindFactoryConnectionByIndex(DistributorHologram, 0);
                    UE_LOG(LogSmartFoundations, Verbose, TEXT("🔧 EXTEND Wire: Belt[%d] %s - Conn1 → Distributor %s (%s) [OUTPUT DEST]"),
                        ChainIndex, *BeltHolo->GetName(), *DistributorHologram->GetName(), Conn1Target ? *Conn1Target->GetName() : TEXT("nullptr"));
                }
            }
        }
        
        // Apply snapped connections
        BeltHolo->SetSnappedConnections(Conn0Target, Conn1Target);
        
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING [%d/%d] %s:"),
            ChainIndex, ChainLength, *BeltHolo->GetName());
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌   Conn0 ← %s"),
            Conn0Target ? *FString::Printf(TEXT("%s on %s"), *Conn0Target->GetName(), 
                Conn0Target->GetOwner() ? *Conn0Target->GetOwner()->GetName() : TEXT("null")) : TEXT("NONE"));
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌   Conn1 → %s"),
            Conn1Target ? *FString::Printf(TEXT("%s on %s"), *Conn1Target->GetName(), 
                Conn1Target->GetOwner() ? *Conn1Target->GetOwner()->GetName() : TEXT("null")) : TEXT("NONE"));
        
        if (!Conn0Target && !Conn1Target)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 ⚠️ BOTH CONNECTIONS NULL for %s - will get isolated chain!"),
                *BeltHolo->GetName());
        }
    }
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 WIRING CHAIN %d COMPLETE ============================"), ChainId);
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("EXTEND: Provided synthetic floor hit to hologram %s at %s"), 
        *Hologram->GetName(), *Location.ToString());
}

void USFExtendService::ConnectAllChainElements(AFGBuildableFactory* NewFactory)
{
    if (!NewFactory)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Phase 3.7: ConnectAllChainElements called with null factory"));
        BuiltChainElements.Empty();
        ChainIsInputMap.Empty();
        return;
    }
    
    if (BuiltChainElements.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: No chain elements to connect"));
        return;
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Connecting chain elements for %s (%d chains)"),
        *NewFactory->GetName(), BuiltChainElements.Num());
    
    int32 TotalConnections = 0;
    int32 FailedConnections = 0;
    
    // Process each chain
    for (auto& ChainPair : BuiltChainElements)
    {
        int32 ChainId = ChainPair.Key;
        TMap<int32, AFGBuildableConveyorBase*>& ElementsByIndex = ChainPair.Value;
        bool bIsInputChain = ChainIsInputMap.FindRef(ChainId);
        
        // Sort chain indices
        TArray<int32> SortedIndices;
        ElementsByIndex.GetKeys(SortedIndices);
        SortedIndices.Sort();
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Chain %d (%s) has %d elements at indices: %s"),
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
                UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Null element at index %d or %d"), CurrentIndex, NextIndex);
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
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s (idx %d) to %s (idx %d)"),
                    *Current->GetName(), CurrentIndex, *Next->GetName(), NextIndex);
            }
            else
            {
                FailedConnections++;
                UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Could not connect %s (idx %d) to %s (idx %d) - Curr0=%d, Curr1=%d, Next0=%d, Next1=%d"),
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s to factory %s"),
                            *FirstElement->GetName(), *FactoryConn->GetName());
                    }
                }
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.7: Chain connections complete - %d succeeded, %d failed"),
        TotalConnections, FailedConnections);
    
    // Clear the temporary storage
    BuiltChainElements.Empty();
    ChainIsInputMap.Empty();
}

// ==================== Phase 3.8: Wire Built Child Connections ====================

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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Registered built conveyor %s in chain %d at index %d (now %d conveyors, isInput=%d)"),
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Registered built distributor %s for chain %d"),
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
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Stored distributor connector name '%s' for chain %d"),
        *ConnectorName.ToString(), ChainId);
}

void USFExtendService::RegisterBuiltJunction(int32 ChainId, AFGBuildable* BuiltJunction)
{
    if (!BuiltJunction) return;
    
    BuiltJunctionsByChain.Add(ChainId, BuiltJunction);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Registered built junction %s for pipe chain %d"),
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Registered built pipe %s for pipe chain %d, index %d (%s)"),
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

                UE_LOG(LogSmartFoundations, Log,
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

                    UE_LOG(LogSmartFoundations, Log,
                        TEXT("🔧 EXTEND Config Copy: Copied %d input priority value(s) from Priority Merger %s → %s (%s)"),
                        SourcePriorities.Num(), *SourcePriority->GetName(), *ClonePriority->GetName(), *Context);
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning,
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
                UE_LOG(LogSmartFoundations, Verbose,
                    TEXT("🔧 EXTEND Config Copy: No clone actor registered for HologramId=%s; skipping"),
                    *ChildHolo.HologramId);
                SkippedUnresolved++;
                continue;
            }

            AFGBuildable* SourceDistributor = GetSourceBuildableByName(ChildHolo.SourceId);
            if (!IsValid(SourceDistributor))
            {
                UE_LOG(LogSmartFoundations, Verbose,
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
        UE_LOG(LogSmartFoundations, Log,
            TEXT("🔧 EXTEND Config Copy: %d Smart Splitter + %d Priority Merger clone(s) received source configuration (%d non-configurable, %d unresolved)"),
            CopiedSmartCount, CopiedPriorityCount, SkippedNonConfigurable, SkippedUnresolved);
    }

    return TotalCopied;
}

void USFExtendService::WireBuiltChildConnections(AFGBuildableFactory* NewFactory)
{
    if (!NewFactory)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called with null factory"));
        return;
    }
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Phase 3.8: WireBuiltChildConnections called for %s (StoredCloneTopology=%s, JsonBuiltActors=%d)"),
        *NewFactory->GetName(),
        (StoredCloneTopology.IsValid() && StoredCloneTopology->ChildHolograms.Num() > 0) ? TEXT("VALID") : TEXT("INVALID"),
        JsonBuiltActors.Num());

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
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔧 Pipe attachment wired: %s.%s ↔ %s.%s (dist=%.1f cm)"),
                        *AttachmentActor->GetName(), *AttConn->GetName(),
                        *BestPipeConn->GetOwner()->GetName(), *BestPipeConn->GetName(),
                        FMath::Sqrt(BestDistSq));
                }
            }
        }
        UE_LOG(LogSmartFoundations, Display, TEXT("🔧 EXTEND Phase 3.8a (#288): wired %d pipe-attachment endpoint(s) to neighbouring cloned pipes (JsonBuiltActors=%d)"),
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
        UClass* PumpWireClass = LoadClass<AFGBuildableWire>(nullptr,
            TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
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
                UE_LOG(LogSmartFoundations, Display,
                    TEXT("⚡ EXTEND Phase 3.8b (#288) inventory: %s class=%s PowerPoleClone=%s"),
                    *Holo.HologramId, *Holo.BuildClass,
                    Holo.ConnectedPowerPoleHologramId.IsEmpty() ? TEXT("<none>") : *Holo.ConnectedPowerPoleHologramId);
            }
        }
        UE_LOG(LogSmartFoundations, Display,
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
                UE_LOG(LogSmartFoundations, Warning,
                    TEXT("⚡ EXTEND Phase 3.8b (#288): clone pole %s reached capacity (%d/%d) — skipping pump %s"),
                    *ClonePole->GetName(), PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections(),
                    *ClonePump->GetName());
                continue;
            }

            if (!PumpWireClass)
            {
                ++PumpsSkipped;
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Phase 3.8b (#288): Build_PowerLine_C class not loadable — skipping pump %s"), *ClonePump->GetName());
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
                UE_LOG(LogSmartFoundations, Display, TEXT("⚡ EXTEND Phase 3.8b (#288): wired pump %s → pole %s (pole now at %d/%d)"),
                    *ClonePump->GetName(), *ClonePole->GetName(),
                    PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections());
            }
            else
            {
                ++PumpsSkipped;
                NewWire->Destroy();
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Phase 3.8b (#288): Wire->Connect() failed for pump %s → pole %s"),
                    *ClonePump->GetName(), *ClonePole->GetName());
            }
        }

        UE_LOG(LogSmartFoundations, Display, TEXT("⚡ EXTEND Phase 3.8b (#288): pump power wiring complete — wired %d, skipped %d"),
            PumpsWired, PumpsSkipped);
    }
    else
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("⚡ EXTEND Phase 3.8b (#288): StoredCloneTopology invalid at pre-wire checkpoint — skipping pump power wiring"));
    }

    // ==================== PHASE 5/6: JSON-Based Wiring ====================
    // Try JSON-based wiring first (for JSON-spawned holograms)
    // This runs regardless of whether chain-based wiring has data
    // NOTE: GenerateAndExecuteWiring resets StoredCloneTopology and JsonBuiltActors
    // at its end — phases 3.8a/3.8b above must run before this call.
    int32 JsonWiredCount = GenerateAndExecuteWiring(NewFactory);
    if (JsonWiredCount > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 5/6: JSON-based wiring completed - %d connections"), JsonWiredCount);
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: No legacy built chains — continuing to JSON-based attachment phases (JSON wiring: %d)"), JsonWiredCount);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring built child connections for %s (Conveyor chains: %d, Pipe chains: %d)"),
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
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring ENDPOINT connections only (conveyor↔conveyor handled by snapped connections)"));
    
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
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Processing conveyor chain %d with %d conveyors (%s), indices: %s"),
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
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s.Conn1 → %s.Conn0"),
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
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s.%s → Factory.%s (distance=%.1f cm)"),
                        *FactoryConveyor->GetName(), bIsInputChain ? TEXT("Conn1") : TEXT("Conn0"),
                        *BestFactoryConn->GetName(), BestDistance);
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No factory %s connector found within 300cm of %s.%s"),
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s.%s → Distributor.%s (distance=%.1f cm)"),
                            *DistributorConveyor->GetName(), bIsInputChain ? TEXT("Conn0") : TEXT("Conn1"),
                            *BestDistConn->GetName(), BestDistance);
                    }
                    else
                    {
                        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No distributor %s connector found within 300cm of %s.%s"),
                            bIsInputChain ? TEXT("OUTPUT") : TEXT("INPUT"),
                            *DistributorConveyor->GetName(), bIsInputChain ? TEXT("Conn0") : TEXT("Conn1"));
                    }
                }
            }
            else
            {
                UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No built distributor found for chain %d"), ChainId);
            }
        }
    }
    
    // (Phases 3.8a and 3.8b formerly lived here — moved to before GenerateAndExecuteWiring above
    //  so that StoredCloneTopology and JsonBuiltActors are still populated when they run.)

    // ==================== PIPE CHAIN WIRING ====================
    // Process pipe chains similarly to belt chains
    
    if (TotalPipeChains > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring %d pipe chains"), TotalPipeChains);
        
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
            
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Processing pipe chain %d with %d pipes (%s), indices: %s"),
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
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s.Conn1 → %s.Conn0"),
                        *CurrentPipe->GetName(), *NextPipe->GetName());
                }
                else
                {
                    FailedConnections++;
                    UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Could not connect %s.Conn1 → %s.Conn0 (From.Connected=%d, To.Connected=%d)"),
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
                
                UE_LOG(LogSmartFoundations, Log, TEXT("   🔍 Factory wiring: %s chain %d, Pipe=%s, TargetConnector=%s"),
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
                    
                    UE_LOG(LogSmartFoundations, Log, TEXT("   🔍 Pipe Conn0 dist=%.1f cm, Conn1 dist=%.1f cm, using %s"),
                        Dist0, Dist1, (Dist0 < Dist1) ? TEXT("Conn0") : TEXT("Conn1"));
                }
                
                if (PipeConn && TargetFactoryConn && !PipeConn->IsConnected() && !TargetFactoryConn->IsConnected())
                {
                    float Distance = FVector::Dist(PipeConn->GetComponentLocation(), TargetFactoryConn->GetComponentLocation());
                    PipeConn->SetConnection(TargetFactoryConn);
                    TotalConnections++;
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s → Factory.%s (distance=%.1f cm)"),
                        *FactoryPipe->GetName(), *TargetFactoryConn->GetName(), Distance);
                }
                else if (!TargetFactoryConn)
                {
                    FailedConnections++;
                    UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Could not find factory connector '%s' on clone factory"),
                        *SourceFactoryConnectorName.ToString());
                }
                else if (PipeConn && PipeConn->IsConnected())
                {
                    UE_LOG(LogSmartFoundations, Log, TEXT("   ℹ️ Pipe connector already connected, skipping factory wiring"));
                }
                else if (TargetFactoryConn && TargetFactoryConn->IsConnected())
                {
                    UE_LOG(LogSmartFoundations, Log, TEXT("   ℹ️ Factory connector '%s' already connected, skipping"),
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
                    
                    UE_LOG(LogSmartFoundations, Log, TEXT("   🔍 Junction wiring: %s chain %d, Pipe=%s, Junction=%s (CLONE), TargetConnector=%s"),
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
                        
                        UE_LOG(LogSmartFoundations, Log, TEXT("   🔍 Target junction connector %s @ (%.1f, %.1f, %.1f)"),
                            *SourceJunctionConnectorName.ToString(), TargetLoc.X, TargetLoc.Y, TargetLoc.Z);
                        UE_LOG(LogSmartFoundations, Log, TEXT("   🔍 Pipe Conn0 dist=%.1f cm, Conn1 dist=%.1f cm, using %s"),
                            Dist0, Dist1, (Dist0 < Dist1) ? TEXT("Conn0") : TEXT("Conn1"));
                    }
                    
                    if (PipeConn && TargetJunctionConn && !PipeConn->IsConnected() && !TargetJunctionConn->IsConnected())
                    {
                        float Distance = FVector::Dist(PipeConn->GetComponentLocation(), TargetJunctionConn->GetComponentLocation());
                        PipeConn->SetConnection(TargetJunctionConn);
                        TotalConnections++;
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s → Junction.%s (distance=%.1f cm, verified: PipeConnected=%d, JunctionConnected=%d)"),
                            *JunctionPipe->GetName(), *TargetJunctionConn->GetName(), Distance,
                            PipeConn->IsConnected(), TargetJunctionConn->IsConnected());
                    }
                    else if (!TargetJunctionConn)
                    {
                        FailedConnections++;
                        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Could not find junction connector '%s' on clone junction"),
                            *SourceJunctionConnectorName.ToString());
                    }
                    else if (PipeConn && PipeConn->IsConnected())
                    {
                        UE_LOG(LogSmartFoundations, Log, TEXT("   ℹ️ Pipe connector already connected, skipping junction wiring"));
                    }
                    else if (TargetJunctionConn && TargetJunctionConn->IsConnected())
                    {
                        UE_LOG(LogSmartFoundations, Log, TEXT("   ℹ️ Junction connector '%s' already connected, skipping"),
                            *TargetJunctionConn->GetName());
                    }
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No built junction found for pipe chain %d"), PipeChainId);
                }
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Wiring complete - %d succeeded, %d failed"),
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
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Phase 3.8: Marked %d pipe networks for rebuild"),
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
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain Fix: Deleting fragmented belts and respawning from source topology..."));
    
    // Log topology info for debugging
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain Fix: Topology has %d input chains, %d output chains"),
        GetCurrentTopology().InputChains.Num(), GetCurrentTopology().OutputChains.Num());
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain Fix: BuiltConveyorsByChain has %d chains:"), BuiltConveyorsByChain.Num());
    for (auto& DebugPair : BuiltConveyorsByChain)
    {
        bool bDebugIsInput = BuiltChainIsInputMap.FindRef(DebugPair.Key);
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain Fix:   ChainId=%d, IsInput=%d, ConveyorCount=%d"),
            DebugPair.Key, bDebugIsInput ? 1 : 0, DebugPair.Value.Num());
    }
    
    // Calculate offset from source to clone factory
    FVector CloneOffset = FVector::ZeroVector;
    if (GetCurrentTopology().SourceBuilding.IsValid() && NewFactory)
    {
        CloneOffset = NewFactory->GetActorLocation() - GetCurrentTopology().SourceBuilding->GetActorLocation();
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Fix: Clone offset = %s"), *CloneOffset.ToString());
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain Fix: Cannot calculate offset - missing source or new factory!"));
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
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain %d: No source chain found in topology, skipping respawn"),
                ChainId);
            continue;
        }
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d (%s): Found %d source conveyors, %d built conveyors"),
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Got factory connector %s from clone %s (connected=%d)"),
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Got distributor connector %s from clone %s"),
                            ChainId, *DistConn->GetName(), *CloneDistributor->GetName());
                        break;
                    }
                }
            }
        }
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Endpoints - FactoryEnd=%s, DistributorEnd=%s"),
            ChainId,
            FactoryEndConn.IsValid() ? *FactoryEndConn->GetName() : TEXT("NULL"),
            DistributorEndConn.IsValid() ? *DistributorEndConn->GetName() : TEXT("NULL"));
        
        // Delete all fragmented conveyors (belts and lifts)
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d: Deleting %d fragmented conveyors..."),
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain %d (%s): Respawning %d conveyors (order: %s)..."),
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
                UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND   [%d] Source conveyor is null, skipping"), i);
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
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND   [%d] Spawning lift:"), i);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Source: %s at %s, Height=%.1f"),
                    *SourceLift->GetName(), *SourceLocation.ToString(), SourceLift->GetHeight());
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Clone location: %s"), *CloneLocation.ToString());
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Source TopTransform: %s (LOCAL)"), *SourceTopTransform.ToString());
                
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
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Set mTopTransform via reflection"));
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
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to previous Conn1"));
                            }
                            else if (bIsInput && DistributorEndConn.IsValid())
                            {
                                *Conn0Ptr = DistributorEndConn.Get();
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to distributor endpoint"));
                            }
                            else if (!bIsInput && FactoryEndConn.IsValid())
                            {
                                *Conn0Ptr = FactoryEndConn.Get();
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to factory endpoint"));
                            }
                        }
                    }
                    
                    UGameplayStatics::FinishSpawningActor(NewLift, CloneTransform);
                    NewLift->SetupConnections();
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND   [%d] Spawned lift %s at %s, Height=%.1f"),
                        i, *NewLift->GetName(), *NewLift->GetActorLocation().ToString(), NewLift->GetHeight());
                    
                    // Also set connections via SetConnection for immediate effect
                    if (PreviousConveyor)
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        UFGFactoryConnectionComponent* PrevConn1 = PreviousConveyor->GetConnection1();
                        if (OurConn0 && PrevConn1)
                        {
                            OurConn0->SetConnection(PrevConn1);
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to previous Conn1"));
                        }
                    }
                    else if (bIsInput && DistributorEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(DistributorEndConn.Get());
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to distributor endpoint %s"),
                                *DistributorEndConn->GetName());
                        }
                    }
                    else if (!bIsInput && FactoryEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewLift->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(FactoryEndConn.Get());
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to factory endpoint %s"),
                                *FactoryEndConn->GetName());
                        }
                    }
                    
                    NewConveyor = NewLift;
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND   [%d] SpawnActorDeferred FAILED"), i);
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
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND   [%d] Respawning belt:"), i);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Source: %s at %s"), 
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
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to previous Conn1"));
                            }
                            else if (bIsInput && DistributorEndConn.IsValid())
                            {
                                *Conn0Ptr = DistributorEndConn.Get();
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to distributor endpoint"));
                            }
                            else if (!bIsInput && FactoryEndConn.IsValid())
                            {
                                *Conn0Ptr = FactoryEndConn.Get();
                                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Pre-set snapped Conn0 to factory endpoint"));
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
                    
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND   [%d] Respawned belt %s -> %s at %s"),
                        i, *SourceBelt->GetName(), *NewBelt->GetName(), *NewBelt->GetActorLocation().ToString());
                    
                    // Also set connections via SetConnection for immediate effect
                    if (PreviousConveyor)
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        UFGFactoryConnectionComponent* PrevConn1 = PreviousConveyor->GetConnection1();
                        if (OurConn0 && PrevConn1)
                        {
                            OurConn0->SetConnection(PrevConn1);
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to previous Conn1"));
                        }
                    }
                    else if (bIsInput && DistributorEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(DistributorEndConn.Get());
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to distributor endpoint %s"),
                                *DistributorEndConn->GetName());
                        }
                    }
                    else if (!bIsInput && FactoryEndConn.IsValid())
                    {
                        UFGFactoryConnectionComponent* OurConn0 = NewBelt->GetConnection0();
                        if (OurConn0)
                        {
                            OurConn0->SetConnection(FactoryEndConn.Get());
                            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn0 to factory endpoint %s"),
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
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn1 to factory endpoint %s"),
                            *FactoryEndConn->GetName());
                    }
                    else if (!bIsInput && DistributorEndConn.IsValid() && OurConn1)
                    {
                        OurConn1->SetConnection(DistributorEndConn.Get());
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND     Connected Conn1 to distributor endpoint %s"),
                            *DistributorEndConn->GetName());
                    }
                }
                
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND   [%d] Chain=%s"),
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
        
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain %d: Respawn complete - %d conveyors now have %d chain actor(s)"),
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
    if (!SourceFactory || !CloneFactory)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold: Invalid factory pointers"));
        return;
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold: Wiring manifold connections between %s and %s"),
        *SourceFactory->GetName(), *CloneFactory->GetName());
    
    int32 BeltManifolds = 0;
    int32 PipeManifolds = 0;
    
    // Wire belt/lift manifold connections (distributor → distributor)
    for (auto& ChainPair : SourceDistributorsByChain)
    {
        int32 ChainId = ChainPair.Key;
        AFGBuildable* SourceDistributor = ChainPair.Value;
        AFGBuildable** CloneDistPtr = BuiltDistributorsByChain.Find(ChainId);
        
        if (!SourceDistributor || !CloneDistPtr || !*CloneDistPtr)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Missing distributor for chain %d"), ChainId);
            continue;
        }
        
        AFGBuildable* CloneDistributor = *CloneDistPtr;
        bool bIsInputChain = BuiltChainIsInputMap.FindRef(ChainId);
        
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
                UE_LOG(LogSmartFoundations, Log, TEXT("   ✅ Belt manifold: %s.%s → %s.%s (%.1f cm)"),
                    *FromDistributor->GetName(), *BestFrom->GetName(),
                    *ToDistributor->GetName(), *BestTo->GetName(), BestDistance);
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No available connectors for manifold on chain %d"), ChainId);
        }
    }
    
    // Wire pipe manifold connections (junction → junction)
    for (auto& ChainPair : SourceJunctionsByChain)
    {
        int32 ChainId = ChainPair.Key;
        AFGBuildable* SourceJunction = ChainPair.Value;
        AFGBuildable** CloneJunctionPtr = BuiltJunctionsByChain.Find(ChainId);
        
        if (!SourceJunction || !CloneJunctionPtr || !*CloneJunctionPtr)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Missing junction for pipe chain %d"), ChainId);
            continue;
        }
        
        AFGBuildable* CloneJunction = *CloneJunctionPtr;
        bool bIsInputChain = BuiltPipeChainIsInputMap.FindRef(ChainId);
        
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
                UE_LOG(LogSmartFoundations, Log, TEXT("   ✅ Pipe manifold: %s.%s → %s.%s (%.1f cm)"),
                    *FromJunction->GetName(), *BestFrom->GetName(),
                    *ToJunction->GetName(), *BestTo->GetName(), BestDistance);
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ No available connectors for pipe manifold on chain %d"), ChainId);
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold: Created %d belt manifolds, %d pipe manifolds"),
        BeltManifolds, PipeManifolds);
    
    // Clear source tracking maps after manifold wiring
    SourceDistributorsByChain.Empty();
    SourceJunctionsByChain.Empty();
}

void USFExtendService::WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId)
{
    if (!BuiltPipe || !SourceConnector)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Invalid parameters"));
        return;
    }
    
    // Get the pipe's two connectors
    UFGPipeConnectionComponentBase* PipeConn0 = BuiltPipe->GetPipeConnection0();
    UFGPipeConnectionComponentBase* PipeConn1 = BuiltPipe->GetPipeConnection1();
    
    if (!PipeConn0 || !PipeConn1)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Pipe %s missing connectors"), *BuiltPipe->GetName());
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → source %s (verified: PipeConnected=%d, SourceConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToSource->GetName(), *SourceConnector->GetName(),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Source connection failed (PipeConnected=%d, SourceConnected=%d)"),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }
    
    // Find clone junction from BuiltJunctionsByChain
    AFGBuildable** CloneJunctionPtr = BuiltJunctionsByChain.Find(CloneChainId);
    if (!CloneJunctionPtr || !*CloneJunctionPtr)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone junction not found for chain %d"), CloneChainId);
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connector selection - BestAlignment=%.2f"),
        BestAlignment);
    
    if (BestCloneConnector && !PipeToClone->IsConnected())
    {
        PipeToClone->SetConnection(BestCloneConnector);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → clone %s.%s (alignment=%.2f, verified: PipeConnected=%d, CloneConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToClone->GetName(),
            *CloneJunction->GetName(), *BestCloneConnector->GetName(), BestAlignment,
            PipeToClone->IsConnected(), BestCloneConnector->IsConnected());
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connection failed (NoCloneConnector=%d, PipeConnected=%d)"),
            BestCloneConnector == nullptr, PipeToClone->IsConnected());
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: ✅ Wired manifold pipe %s between source and clone junctions"),
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Network IDs - Conn0=%d, Conn1=%d"),
                Network0, Network1);
            
            if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
                AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net0 && Net1)
                {
                    Net0->MergeNetworks(Net1);
                    Net0->MarkForFullRebuild();
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Merged networks %d and %d, marked for rebuild"),
                        Network0, Network1);
                }
            }
            else if (Network0 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network0);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network0);
                }
            }
            else if (Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network1);
                }
            }
        }
    }
}

void USFExtendService::WireManifoldBelt(AFGBuildableConveyorBelt* BuiltBelt, UFGFactoryConnectionComponent* SourceConnector, int32 CloneChainId)
{
    if (!BuiltBelt || !SourceConnector)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Invalid parameters"));
        return;
    }
    
    // Get the belt's two connectors
    UFGFactoryConnectionComponent* BeltConn0 = BuiltBelt->GetConnection0();
    UFGFactoryConnectionComponent* BeltConn1 = BuiltBelt->GetConnection1();
    
    if (!BeltConn0 || !BeltConn1)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Belt %s missing connectors"), *BuiltBelt->GetName());
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: SourceConnector=%s, Dir=%d, bSourceIsOutput=%d"),
        *SourceConnector->GetName(), (int32)SourceDir, bSourceIsOutput);
    
    // Find clone distributor from BuiltDistributorsByChain
    AFGBuildable** CloneDistributorPtr = BuiltDistributorsByChain.Find(CloneChainId);
    if (!CloneDistributorPtr || !*CloneDistributorPtr)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Clone distributor not found for chain %d"), CloneChainId);
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Connected %s.%s → source %s"),
                *BuiltBelt->GetName(), *BeltToSource->GetName(), *SourceConnector->GetName());
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: CanConnectTo failed for source (BeltDir=%d, SourceDir=%d)"),
                (int32)BeltToSource->GetDirection(), (int32)SourceConnector->GetDirection());
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Source connection failed (BeltConnected=%d, SourceConnected=%d)"),
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
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Clone connector selection - NeededDir=%d, BestDistance=%.2f"),
        (int32)NeededDir, BestDistance);
    
    if (BestCloneConnector && !BeltToClone->IsConnected())
    {
        if (BeltToClone->CanConnectTo(BestCloneConnector))
        {
            BeltToClone->SetConnection(BestCloneConnector);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: Connected %s.%s → clone %s.%s (distance=%.2f)"),
                *BuiltBelt->GetName(), *BeltToClone->GetName(),
                *CloneDistributor->GetName(), *BestCloneConnector->GetName(), BestDistance);
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: CanConnectTo failed for clone (SourceDir=%d, NeededDir=%d, CloneDir=%d)"),
                (int32)SourceDir, (int32)NeededDir, (int32)BestCloneConnector->GetDirection());
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Clone connection failed (NoCloneConnector=%d, BeltConnected=%d, NeededDir=%d)"),
            BestCloneConnector == nullptr, BeltToClone->IsConnected(), (int32)NeededDir);
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Manifold Belt Wire: ✅ Wired manifold belt %s between source and clone distributors"),
        *BuiltBelt->GetName());
    
    // ============================================================
    // DIAGNOSTIC: Log chain state of manifold belt and connected belts
    // ============================================================
    {
        AFGConveyorChainActor* ManifoldChain = BuiltBelt->GetConveyorChainActor();
        int32 ManifoldBucketID = BuiltBelt->GetConveyorBucketID();
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Manifold belt %s - ChainActor=%s, BucketID=%d"),
            *BuiltBelt->GetName(),
            ManifoldChain ? *ManifoldChain->GetName() : TEXT("NULL"),
            ManifoldBucketID);
        
        // Log source distributor connections
        AFGBuildable* SrcDist = SourceConnector->GetOuterBuildable();
        if (SrcDist)
        {
            TArray<UFGFactoryConnectionComponent*> SrcConns;
            SrcDist->GetComponents<UFGFactoryConnectionComponent>(SrcConns);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Source distributor %s has %d connectors:"), *SrcDist->GetName(), SrcConns.Num());
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
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Diag:   %s [Dir=%d] → %s%s"),
                    *Conn->GetName(), (int32)Conn->GetDirection(), *ConnectedTo, *ChainInfo);
            }
        }
        
        // Log clone distributor connections
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Diag: Clone distributor %s has %d connectors:"), *CloneDistributor->GetName(), CloneConnectors.Num());
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
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain Diag:   %s [Dir=%d] → %s%s"),
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
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Manifold Belt Wire: Manifold belt %s has ChainActor=%s, BucketID=%d - chain integration pending"),
        *BuiltBelt->GetName(),
        BuiltBelt->GetConveyorChainActor() ? *BuiltBelt->GetConveyorChainActor()->GetName() : TEXT("NULL"),
        BuiltBelt->GetConveyorBucketID());
}

bool USFExtendService::CreateManifoldBelt(UFGFactoryConnectionComponent* FromConnector, UFGFactoryConnectionComponent* ToConnector)
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
        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Failed to load belt class: %s"), *BeltPath);
        return false;
    }
    
    // Calculate spline points for the belt
    FVector StartPos = FromConnector->GetComponentLocation();
    FVector EndPos = ToConnector->GetComponentLocation();
    FVector StartForward = FromConnector->GetForwardVector();
    FVector EndForward = -ToConnector->GetForwardVector();  // Negate for facing inward
    
    // Create spline data (simple 2-point belt)
    TArray<FSplinePointData> SplineData;
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
    
    // Spawn belt
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(BeltClass, StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Belt)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Failed to spawn manifold belt"));
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CreateManifoldBelt: Registered belt with subsystem (ChainActor=%s)"),
            Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("pending"));
    }
    else
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("CreateManifoldBelt: No BuildableSubsystem - belt will have no chain actor!"));
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("   🔧 Created manifold belt Mk%d between distributors"), BeltTier);
    return true;
}

bool USFExtendService::CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector)
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
        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Failed to load pipe class: %s"), *PipePath);
        return false;
    }
    
    // Calculate spline points for the pipe
    FVector StartPos = FromConnector->GetComponentLocation();
    FVector EndPos = ToConnector->GetComponentLocation();
    FVector StartForward = FromConnector->GetForwardVector();
    FVector EndForward = -ToConnector->GetForwardVector();
    
    // Create spline data (simple 2-point pipe)
    TArray<FSplinePointData> SplineData;
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
    
    // Spawn pipe with DEFERRED construction (same technique as BuildExtendPipeAndReturn)
    FActorSpawnParameters SpawnParams;
    SpawnParams.bDeferConstruction = true;
    
    AFGBuildablePipeline* Pipe = World->SpawnActor<AFGBuildablePipeline>(PipeClass, StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Pipe)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Failed to spawn manifold pipe"));
        return false;
    }
    
    // Apply spline data BEFORE FinishSpawning
    TArray<FSplinePointData>* MutableSpline = Pipe->GetMutableSplinePointData();
    if (!MutableSpline)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Cannot get mutable spline data for manifold pipe"));
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
    
    UE_LOG(LogSmartFoundations, Log, TEXT("   🔧 Created manifold pipe Mk%d between junctions"), PipeTier);
    return true;
}

AFGBuildableFactory* USFExtendService::GetSourceFactory() const
{
    if (GetCurrentTopology().SourceBuilding.IsValid())
    {
        return Cast<AFGBuildableFactory>(GetCurrentTopology().SourceBuilding.Get());
    }
    return nullptr;
}

// ==================== Diagnostic Capture ====================

FSFBuildableSnapshot USFExtendService::CaptureNearbyBuildables(float Radius)
{
    FSFBuildableSnapshot Snapshot;
    Snapshot.CaptureRadius = Radius;
    Snapshot.CaptureTime = FDateTime::Now();
    
    if (!Subsystem.IsValid())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📊 DIAG: Cannot capture - no subsystem"));
        return Snapshot;
    }
    
    // Get player location
    APlayerController* PC = Subsystem->GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("📊 DIAG: Cannot capture - no player"));
        return Snapshot;
    }
    
    FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
    
    // ==================== Build EXTEND Topology Lookup ====================
    // Maps buildable actor -> (Role, ChainId, ChainIndex)
    struct FExtendTopologyInfo {
        FString Role;
        int32 ChainId = -1;
        int32 ChainIndex = -1;
    };
    TMap<AActor*, FExtendTopologyInfo> TopologyLookup;
    
    // Add source factory
    if (CurrentExtendTarget.IsValid())
    {
        FExtendTopologyInfo Info;
        Info.Role = TEXT("SourceFactory");
        TopologyLookup.Add(CurrentExtendTarget.Get(), Info);
    }
    
    // Add input belt chain members (conveyors = belts + lifts, poles, and distributors)
    for (int32 ChainIdx = 0; ChainIdx < GetCurrentTopology().InputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = GetCurrentTopology().InputChains[ChainIdx];
        
        // Add distributor (splitter) at chain end
        if (Chain.Distributor.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Splitter");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Distributor.Get(), Info);
        }
        
        // Add all conveyors (belts and lifts) in chain
        for (int32 ConvIdx = 0; ConvIdx < Chain.Conveyors.Num(); ++ConvIdx)
        {
            if (Chain.Conveyors[ConvIdx].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[ConvIdx].Get();
                FExtendTopologyInfo Info;
                // Distinguish belt from lift
                Info.Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("InputLift") : TEXT("InputBelt");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = ConvIdx;
                TopologyLookup.Add(Conv, Info);
            }
        }
        
        // Add all support poles in chain (future cloning candidate)
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("Pole");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }
    
    // Add output belt chain members
    for (int32 ChainIdx = 0; ChainIdx < GetCurrentTopology().OutputChains.Num(); ++ChainIdx)
    {
        const FSFConnectionChainNode& Chain = GetCurrentTopology().OutputChains[ChainIdx];
        
        // Add distributor (merger) at chain end
        if (Chain.Distributor.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Merger");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Distributor.Get(), Info);
        }
        
        // Add all conveyors (belts and lifts) in chain
        for (int32 ConvIdx = 0; ConvIdx < Chain.Conveyors.Num(); ++ConvIdx)
        {
            if (Chain.Conveyors[ConvIdx].IsValid())
            {
                AFGBuildableConveyorBase* Conv = Chain.Conveyors[ConvIdx].Get();
                FExtendTopologyInfo Info;
                Info.Role = Cast<AFGBuildableConveyorLift>(Conv) ? TEXT("OutputLift") : TEXT("OutputBelt");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = ConvIdx;
                TopologyLookup.Add(Conv, Info);
            }
        }
        
        // Add all support poles in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("Pole");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }
    
    // Add input pipe chain members
    for (int32 ChainIdx = 0; ChainIdx < GetCurrentTopology().PipeInputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = GetCurrentTopology().PipeInputChains[ChainIdx];
        
        // Add junction at chain end
        if (Chain.Junction.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Junction");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Junction.Get(), Info);
        }
        
        // Add all pipes in chain
        for (int32 PipeIdx = 0; PipeIdx < Chain.Pipelines.Num(); ++PipeIdx)
        {
            if (Chain.Pipelines[PipeIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("InputPipe");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PipeIdx;
                TopologyLookup.Add(Chain.Pipelines[PipeIdx].Get(), Info);
            }
        }
        
        // Add all support poles (pipe supports) in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("PipeSupport");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }
    
    // Add output pipe chain members
    for (int32 ChainIdx = 0; ChainIdx < GetCurrentTopology().PipeOutputChains.Num(); ++ChainIdx)
    {
        const FSFPipeConnectionChainNode& Chain = GetCurrentTopology().PipeOutputChains[ChainIdx];
        
        // Add junction at chain end
        if (Chain.Junction.IsValid())
        {
            FExtendTopologyInfo Info;
            Info.Role = TEXT("Junction");
            Info.ChainId = ChainIdx;
            TopologyLookup.Add(Chain.Junction.Get(), Info);
        }
        
        // Add all pipes in chain
        for (int32 PipeIdx = 0; PipeIdx < Chain.Pipelines.Num(); ++PipeIdx)
        {
            if (Chain.Pipelines[PipeIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("OutputPipe");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PipeIdx;
                TopologyLookup.Add(Chain.Pipelines[PipeIdx].Get(), Info);
            }
        }
        
        // Add all support poles (pipe supports) in chain
        for (int32 PoleIdx = 0; PoleIdx < Chain.SupportPoles.Num(); ++PoleIdx)
        {
            if (Chain.SupportPoles[PoleIdx].IsValid())
            {
                FExtendTopologyInfo Info;
                Info.Role = TEXT("PipeSupport");
                Info.ChainId = ChainIdx;
                Info.ChainIndex = PoleIdx;
                TopologyLookup.Add(Chain.SupportPoles[PoleIdx].Get(), Info);
            }
        }
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DIAG: Built topology lookup with %d members"), TopologyLookup.Num());
    Snapshot.CaptureLocation = PlayerLocation;
    
    // Find all buildables in radius
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(Subsystem->GetWorld(), AFGBuildable::StaticClass(), FoundActors);
    
    for (AActor* Actor : FoundActors)
    {
        AFGBuildable* Buildable = Cast<AFGBuildable>(Actor);
        if (!Buildable) continue;
        
        float Distance = FVector::Dist(PlayerLocation, Buildable->GetActorLocation());
        if (Distance > Radius) continue;
        
        FSFCapturedBuildable Captured;
        
        // === Identity ===
        Captured.Name = Buildable->GetName();
        Captured.ClassName = Buildable->GetClass()->GetName();
        
        // === Transform ===
        Captured.Location = Buildable->GetActorLocation();
        Captured.Rotation = Buildable->GetActorRotation();
        Captured.Scale = Buildable->GetActorScale3D();
        
        // Get bounds
        FVector Origin, BoxExtent;
        Buildable->GetActorBounds(false, Origin, BoxExtent);
        Captured.BoundsMin = Origin - BoxExtent;
        Captured.BoundsMax = Origin + BoxExtent;
        
        // === State ===
        Captured.bIsHidden = Buildable->IsHidden();
        Captured.bIsPendingKill = !IsValid(Buildable);
        Captured.bHasBegunPlay = Buildable->HasActorBegunPlay();
        
        // === Capture Factory Connections ===
        TArray<UFGFactoryConnectionComponent*> FactoryConns;
        Buildable->GetComponents<UFGFactoryConnectionComponent>(FactoryConns);
        for (UFGFactoryConnectionComponent* Conn : FactoryConns)
        {
            if (!Conn) continue;
            
            FSFCapturedConnection CapturedConn;
            CapturedConn.ConnectorName = Conn->GetName();
            CapturedConn.ConnectorClass = Conn->GetClass()->GetName();
            CapturedConn.WorldLocation = Conn->GetComponentLocation();
            CapturedConn.WorldRotation = Conn->GetComponentRotation();
            CapturedConn.Direction = (int32)Conn->GetDirection();
            CapturedConn.bIsConnected = Conn->IsConnected();
            
            if (Conn->IsConnected())
            {
                UFGFactoryConnectionComponent* OtherConn = Conn->GetConnection();
                if (OtherConn && OtherConn->GetOwner())
                {
                    CapturedConn.ConnectedToActor = OtherConn->GetOwner()->GetName();
                    CapturedConn.ConnectedToConnector = OtherConn->GetName();
                }
            }
            
            Captured.FactoryConnections.Add(CapturedConn);
        }
        
        // === Capture Pipe Connections ===
        TArray<UFGPipeConnectionComponent*> PipeConns;
        Buildable->GetComponents<UFGPipeConnectionComponent>(PipeConns);
        for (UFGPipeConnectionComponent* Conn : PipeConns)
        {
            if (!Conn) continue;
            
            FSFCapturedConnection CapturedConn;
            CapturedConn.ConnectorName = Conn->GetName();
            CapturedConn.ConnectorClass = Conn->GetClass()->GetName();
            CapturedConn.WorldLocation = Conn->GetComponentLocation();
            CapturedConn.WorldRotation = Conn->GetComponentRotation();
            CapturedConn.Direction = (int32)Conn->GetPipeConnectionType();
            CapturedConn.bIsConnected = Conn->IsConnected();
            
            if (Conn->IsConnected())
            {
                UFGPipeConnectionComponentBase* OtherConnBase = Conn->GetConnection();
                if (OtherConnBase && OtherConnBase->GetOwner())
                {
                    CapturedConn.ConnectedToActor = OtherConnBase->GetOwner()->GetName();
                    CapturedConn.ConnectedToConnector = OtherConnBase->GetName();
                }
            }
            
            Captured.PipeConnections.Add(CapturedConn);
        }
        
        // === Categorize and gather type-specific data ===
        if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Buildable))
        {
            Captured.Category = TEXT("Belt");
            Captured.BeltSpeed = Belt->GetSpeed();
            
            // Get spline data
            if (USplineComponent* Spline = Belt->GetSplineComponent())
            {
                Captured.SplineLength = Spline->GetSplineLength();
                Captured.SplinePointCount = Spline->GetNumberOfSplinePoints();
                
                for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); ++i)
                {
                    FSFCapturedSplinePoint Point;
                    Point.Index = i;
                    Point.Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
                    Point.WorldLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.Rotation = Spline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Captured.SplinePoints.Add(Point);
                }
            }
        }
        else if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Buildable))
        {
            Captured.Category = TEXT("Lift");
            Captured.LiftHeight = Lift->GetHeight();
            Captured.bLiftIsReversed = Lift->GetIsReversed();
            
            // Get connection locations
            TArray<UFGFactoryConnectionComponent*> LiftConns;
            Lift->GetComponents<UFGFactoryConnectionComponent>(LiftConns);
            for (UFGFactoryConnectionComponent* Conn : LiftConns)
            {
                if (Conn->GetName().Contains(TEXT("0")))
                {
                    Captured.LiftBottomLocation = Conn->GetComponentLocation();
                }
                else
                {
                    Captured.LiftTopLocation = Conn->GetComponentLocation();
                }
            }
        }
        else if (Cast<AFGBuildableConveyorAttachment>(Buildable))
        {
            Captured.Category = TEXT("Distributor");
        }
        else if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable))
        {
            Captured.Category = TEXT("Pipe");
            
            // Get spline data
            if (USplineComponent* Spline = Pipe->GetSplineComponent())
            {
                Captured.SplineLength = Spline->GetSplineLength();
                Captured.SplinePointCount = Spline->GetNumberOfSplinePoints();
                
                for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); ++i)
                {
                    FSFCapturedSplinePoint Point;
                    Point.Index = i;
                    Point.Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
                    Point.WorldLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Point.Rotation = Spline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    Captured.SplinePoints.Add(Point);
                }
            }
        }
        else if (Cast<AFGBuildablePipelineJunction>(Buildable))
        {
            Captured.Category = TEXT("Junction");
        }
        else if (Cast<AFGBuildableFactory>(Buildable))
        {
            Captured.Category = TEXT("Factory");
        }
        else
        {
            Captured.Category = TEXT("Other");
        }
        
        // === Check if this buildable is part of EXTEND topology ===
        if (FExtendTopologyInfo* TopoInfo = TopologyLookup.Find(Buildable))
        {
            Captured.bIsExtendSource = true;
            Captured.ExtendRole = TopoInfo->Role;
            Captured.ExtendChainId = TopoInfo->ChainId;
            Captured.ExtendChainIndex = TopoInfo->ChainIndex;
        }
        
        Snapshot.Buildables.Add(Captured);
        
        // Update category count
        int32& Count = Snapshot.CountByCategory.FindOrAdd(Captured.Category);
        Count++;
    }
    
    return Snapshot;
}

void USFExtendService::CapturePreviewSnapshot()
{
    // DISABLED: RadarPulse generates too much log output (~1400 lines per Extend)
    // Re-enable for debugging by uncommenting the block below
    /*
    // Use RadarPulse service for enhanced capture (200m radius)
    USFRadarPulseService* RadarPulse = Subsystem.IsValid() ? Subsystem->GetRadarPulseService() : nullptr;
    if (RadarPulse)
    {
        // Capture with RadarPulse (200m = 20000cm)
        FSFRadarPulseSnapshot PulseSnapshot = RadarPulse->CaptureSnapshot(20000.0f, TEXT("EXTEND_PRE"));
        
        // Flag EXTEND source objects
        RadarPulse->FlagExtendSourceObjects(PulseSnapshot, GetCurrentTopology(), CurrentExtendTarget.Get());
        
        // Cache for later comparison
        RadarPulse->CacheSnapshot(TEXT("EXTEND_PRE"), PulseSnapshot);
        
        // Log the snapshot with EXTEND source details
        // RadarPulse->LogFlaggedObjects(PulseSnapshot, TEXT("ExtendSource"), true);  // DISABLED: Too verbose (~2000 lines)
        
        bHasPreviewSnapshot = true;
        UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: EXTEND preview snapshot captured (%d objects, %d EXTEND sources)"),
            PulseSnapshot.TotalObjects, PulseSnapshot.ExtendSourceCount);
        return;
    }
    */
    
    // Fallback to legacy capture if RadarPulse unavailable
    PreviewSnapshot = CaptureNearbyBuildables(15000.0f);  // 150m
    bHasPreviewSnapshot = true;
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DIAG: Captured PREVIEW snapshot - %d buildables within 150m"), 
        PreviewSnapshot.Buildables.Num());
    
    // Log summary by category
    for (const auto& Pair : PreviewSnapshot.CountByCategory)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DIAG:   %s: %d"), *Pair.Key, Pair.Value);
    }
    
    // === Log EXTEND Source Topology Summary ===
    int32 SourceCount = 0;
    TMap<FString, int32> SourceByRole;
    for (const FSFCapturedBuildable& B : PreviewSnapshot.Buildables)
    {
        if (B.bIsExtendSource)
        {
            SourceCount++;
            int32& Count = SourceByRole.FindOrAdd(B.ExtendRole);
            Count++;
        }
    }
    
    if (SourceCount > 0)
    {
        // DISABLED: Source topology box logging - generates ~30+ lines per Extend
        // Re-enable for debugging by uncommenting the block below
        /*
        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║           EXTEND SOURCE TOPOLOGY (%d members)                      ║"), SourceCount);
        UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
        
        for (const auto& Pair : SourceByRole)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("║   %-20s: %3d                                       ║"), *Pair.Key, Pair.Value);
        }
        
        UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
        */
        
        // DISABLED: Detailed source topology logging generates ~1500 lines per Extend
        // Re-enable for debugging by uncommenting the block below
        /*
        // Log full details of SOURCE topology members
        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("📊 EXTEND SOURCE DETAILS:"));
        
        for (const FSFCapturedBuildable& B : PreviewSnapshot.Buildables)
        {
            if (B.bIsExtendSource)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT(""));
                UE_LOG(LogSmartFoundations, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ★ %s [%s] - Chain %d, Index %d"), 
                    *B.ExtendRole, *B.Name, B.ExtendChainId, B.ExtendChainIndex);
                UE_LOG(LogSmartFoundations, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Category: %s | Class: %s"), *B.Category, *B.ClassName);
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Location: X=%.3f Y=%.3f Z=%.3f"), B.Location.X, B.Location.Y, B.Location.Z);
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Rotation: P=%.3f Y=%.3f R=%.3f"), B.Rotation.Pitch, B.Rotation.Yaw, B.Rotation.Roll);
                
                // Belt-specific data
                if (B.Category == TEXT("Belt"))
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ BELT DATA ═══"));
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ Speed: %.1f | SplineLength: %.1fcm | SplinePoints: %d"), 
                        B.BeltSpeed, B.SplineLength, B.SplinePointCount);
                    
                    for (const FSFCapturedSplinePoint& SP : B.SplinePoints)
                    {
                        UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] World=(%.1f,%.1f,%.1f)"),
                            SP.Index, SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                        UE_LOG(LogSmartFoundations, Display, TEXT("│             Arrive=(%.1f,%.1f,%.1f) Leave=(%.1f,%.1f,%.1f)"),
                            SP.ArriveTangent.X, SP.ArriveTangent.Y, SP.ArriveTangent.Z,
                            SP.LeaveTangent.X, SP.LeaveTangent.Y, SP.LeaveTangent.Z);
                    }
                }
                
                // Lift-specific data
                if (B.Category == TEXT("Lift"))
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ LIFT DATA ═══"));
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ Height: %.1fcm | Reversed: %d"), B.LiftHeight, B.bLiftIsReversed);
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ Bottom: (%.1f,%.1f,%.1f) Top: (%.1f,%.1f,%.1f)"),
                        B.LiftBottomLocation.X, B.LiftBottomLocation.Y, B.LiftBottomLocation.Z,
                        B.LiftTopLocation.X, B.LiftTopLocation.Y, B.LiftTopLocation.Z);
                }
                
                // Pipe-specific data
                if (B.Category == TEXT("Pipe"))
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE DATA ═══"));
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ SplineLength: %.1fcm | SplinePoints: %d"), B.SplineLength, B.SplinePointCount);
                    
                    for (const FSFCapturedSplinePoint& SP : B.SplinePoints)
                    {
                        UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] World=(%.1f,%.1f,%.1f)"),
                            SP.Index, SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                    }
                }
                
                // Factory connections
                if (B.FactoryConnections.Num() > 0)
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ FACTORY CONNECTIONS (%d) ═══"), B.FactoryConnections.Num());
                    for (const FSFCapturedConnection& Conn : B.FactoryConnections)
                    {
                        FString ConnStatus = Conn.bIsConnected 
                            ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                            : TEXT("(not connected)");
                        UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) %s"),
                            *Conn.ConnectorName, Conn.Direction,
                            Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                            *ConnStatus);
                    }
                }
                
                // Pipe connections
                if (B.PipeConnections.Num() > 0)
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE CONNECTIONS (%d) ═══"), B.PipeConnections.Num());
                    for (const FSFCapturedConnection& Conn : B.PipeConnections)
                    {
                        FString ConnStatus = Conn.bIsConnected 
                            ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                            : TEXT("(not connected)");
                        UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) %s"),
                            *Conn.ConnectorName, Conn.Direction,
                            Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                            *ConnStatus);
                    }
                }
                
                UE_LOG(LogSmartFoundations, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
            }
        }
        */
    }
}

void USFExtendService::CapturePostBuildSnapshotAndLogDiff()
{
    if (!bHasPreviewSnapshot)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DIAG: No preview snapshot to compare against"));
        return;
    }
    
    // DISABLED: RadarPulse generates too much log output (~1400 lines per Extend)
    // Re-enable for debugging by uncommenting the block below
    /*
    // Use RadarPulse service for enhanced capture and diff
    USFRadarPulseService* RadarPulse = Subsystem.IsValid() ? Subsystem->GetRadarPulseService() : nullptr;
    if (RadarPulse)
    {
        // Capture POST snapshot (200m = 20000cm)
        FSFRadarPulseSnapshot PostSnapshot = RadarPulse->CaptureSnapshot(20000.0f, TEXT("EXTEND_POST"));
        
        // Get the PRE snapshot from cache
        FSFRadarPulseSnapshot PreSnapshot;
        if (RadarPulse->GetCachedSnapshot(TEXT("EXTEND_PRE"), PreSnapshot))
        {
            // Compare and log the diff
            FSFSnapshotDiff Diff = RadarPulse->CompareSnapshots(PreSnapshot, PostSnapshot);
            RadarPulse->LogDiff(Diff, true);  // Verbose output
            
            UE_LOG(LogSmartFoundations, Log, TEXT("📡 RadarPulse: EXTEND build diff - %d new, %d removed, %d modified"),
                Diff.NewObjects.Num(), Diff.RemovedObjects.Num(), Diff.ModifiedObjects.Num());
        }
        else
        {
            // No PRE snapshot cached, just log the POST snapshot
            RadarPulse->LogSnapshot(PostSnapshot, true);
        }
        
        // Clear cache
        RadarPulse->ClearCache(TEXT("EXTEND_PRE"));
        bHasPreviewSnapshot = false;
        return;
    }
    */
    
    // Fallback to legacy capture if RadarPulse unavailable
    FSFBuildableSnapshot PostBuildSnapshot = CaptureNearbyBuildables(15000.0f);  // 150m
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DIAG: Captured POST-BUILD snapshot - %d buildables within 150m"), 
        PostBuildSnapshot.Buildables.Num());
    
    LogSnapshotDiff(PreviewSnapshot, PostBuildSnapshot);
    
    // Clear the preview snapshot
    bHasPreviewSnapshot = false;
    PreviewSnapshot = FSFBuildableSnapshot();
}

void USFExtendService::LogSnapshotDiff(const FSFBuildableSnapshot& Before, const FSFBuildableSnapshot& After)
{
    // DISABLED: BUILD DIFF SUMMARY box logging - generates ~20+ lines per Extend
    // Re-enable for debugging by uncommenting the block below
    /*
    UE_LOG(LogSmartFoundations, Display, TEXT(""));
    UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║               EXTEND DIAGNOSTIC: BUILD DIFF SUMMARY               ║"));
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Capture Radius: 150m | Before: %4d buildables | After: %4d        ║"), 
        Before.Buildables.Num(), After.Buildables.Num());
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    
    // Build set of existing names for quick lookup
    TSet<FString> BeforeNames;
    for (const FSFCapturedBuildable& B : Before.Buildables)
    {
        BeforeNames.Add(B.Name);
    }
    
    TSet<FString> AfterNames;
    for (const FSFCapturedBuildable& A : After.Buildables)
    {
        AfterNames.Add(A.Name);
    }
    
    // Find new buildables (in After but not in Before)
    TArray<FSFCapturedBuildable> NewBuildables;
    for (const FSFCapturedBuildable& A : After.Buildables)
    {
        if (!BeforeNames.Contains(A.Name))
        {
            NewBuildables.Add(A);
        }
    }
    
    // Find removed buildables (in Before but not in After)
    TArray<FSFCapturedBuildable> RemovedBuildables;
    for (const FSFCapturedBuildable& B : Before.Buildables)
    {
        if (!AfterNames.Contains(B.Name))
        {
            RemovedBuildables.Add(B);
        }
    }
    
    // Count new by category
    TMap<FString, int32> NewByCategory;
    for (const FSFCapturedBuildable& N : NewBuildables)
    {
        int32& Count = NewByCategory.FindOrAdd(N.Category);
        Count++;
    }
    
    // Count removed by category
    TMap<FString, int32> RemovedByCategory;
    for (const FSFCapturedBuildable& R : RemovedBuildables)
    {
        int32& Count = RemovedByCategory.FindOrAdd(R.Category);
        Count++;
    }
    
    // Log category summary
    UE_LOG(LogSmartFoundations, Display, TEXT("║ Category        │ Before │ After  │ New    │ Removed │ Delta   ║"));
    UE_LOG(LogSmartFoundations, Display, TEXT("╟─────────────────┼────────┼────────┼────────┼─────────┼─────────╢"));
    
    // Collect all categories
    TSet<FString> AllCategories;
    for (const auto& Pair : Before.CountByCategory) AllCategories.Add(Pair.Key);
    for (const auto& Pair : After.CountByCategory) AllCategories.Add(Pair.Key);
    
    for (const FString& Category : AllCategories)
    {
        int32 BeforeCount = Before.CountByCategory.Contains(Category) ? Before.CountByCategory[Category] : 0;
        int32 AfterCount = After.CountByCategory.Contains(Category) ? After.CountByCategory[Category] : 0;
        int32 NewCount = NewByCategory.Contains(Category) ? NewByCategory[Category] : 0;
        int32 RemovedCount = RemovedByCategory.Contains(Category) ? RemovedByCategory[Category] : 0;
        int32 Delta = AfterCount - BeforeCount;
        
        FString DeltaStr = Delta > 0 ? FString::Printf(TEXT("+%d"), Delta) : FString::Printf(TEXT("%d"), Delta);
        
        UE_LOG(LogSmartFoundations, Display, TEXT("║ %-15s │ %6d │ %6d │ %6d │ %7d │ %7s ║"), 
            *Category, BeforeCount, AfterCount, NewCount, RemovedCount, *DeltaStr);
    }
    
    UE_LOG(LogSmartFoundations, Display, TEXT("╠═══════════════════════════════════════════════════════════════════╣"));
    UE_LOG(LogSmartFoundations, Display, TEXT("║ TOTAL           │ %6d │ %6d │ %6d │ %7d │ %+7d ║"), 
        Before.Buildables.Num(), After.Buildables.Num(), 
        NewBuildables.Num(), RemovedBuildables.Num(),
        After.Buildables.Num() - Before.Buildables.Num());
    UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
    */
    
    // DISABLED: Detailed buildable enumeration generates ~1000 lines per Extend
    // Re-enable for debugging by uncommenting the block below
    /*
    // Log FULL details of ALL new buildables
    if (NewBuildables.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT(""));
        UE_LOG(LogSmartFoundations, Display, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        UE_LOG(LogSmartFoundations, Display, TEXT("║            FULL ENUMERATION: NEW BUILDABLES (%d total)            ║"), NewBuildables.Num());
        UE_LOG(LogSmartFoundations, Display, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
        
        for (int32 i = 0; i < NewBuildables.Num(); ++i)
        {
            const FSFCapturedBuildable& N = NewBuildables[i];
            
            UE_LOG(LogSmartFoundations, Display, TEXT(""));
            UE_LOG(LogSmartFoundations, Display, TEXT("┌─────────────────────────────────────────────────────────────────────"));
            
            // Show EXTEND source badge prominently
            if (N.bIsExtendSource)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ [%d] ★ EXTEND SOURCE ★ %s"), i, *N.Name);
                UE_LOG(LogSmartFoundations, Display, TEXT("│     Role: %s | Chain: %d | Index: %d"), 
                    *N.ExtendRole, N.ExtendChainId, N.ExtendChainIndex);
            }
            else
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ [%d] %s"), i, *N.Name);
            }
            
            UE_LOG(LogSmartFoundations, Display, TEXT("├─────────────────────────────────────────────────────────────────────"));
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Category: %s | Class: %s"), *N.Category, *N.ClassName);
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Location: X=%.3f Y=%.3f Z=%.3f"), N.Location.X, N.Location.Y, N.Location.Z);
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Rotation: P=%.3f Y=%.3f R=%.3f"), N.Rotation.Pitch, N.Rotation.Yaw, N.Rotation.Roll);
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Scale: X=%.3f Y=%.3f Z=%.3f"), N.Scale.X, N.Scale.Y, N.Scale.Z);
            UE_LOG(LogSmartFoundations, Display, TEXT("│ Bounds: Min=(%.1f,%.1f,%.1f) Max=(%.1f,%.1f,%.1f)"), 
                N.BoundsMin.X, N.BoundsMin.Y, N.BoundsMin.Z, N.BoundsMax.X, N.BoundsMax.Y, N.BoundsMax.Z);
            UE_LOG(LogSmartFoundations, Display, TEXT("│ State: Hidden=%d PendingKill=%d BegunPlay=%d"), 
                N.bIsHidden, N.bIsPendingKill, N.bHasBegunPlay);
            
            // Belt-specific data
            if (N.Category == TEXT("Belt"))
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ BELT DATA ═══"));
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Speed: %.1f | SplineLength: %.1fcm | SplinePoints: %d"), 
                    N.BeltSpeed, N.SplineLength, N.SplinePointCount);
                
                for (const FSFCapturedSplinePoint& SP : N.SplinePoints)
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                        SP.Index, SP.Location.X, SP.Location.Y, SP.Location.Z, 
                        SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                    UE_LOG(LogSmartFoundations, Display, TEXT("│             Arrive=(%.1f,%.1f,%.1f) Leave=(%.1f,%.1f,%.1f)"),
                        SP.ArriveTangent.X, SP.ArriveTangent.Y, SP.ArriveTangent.Z,
                        SP.LeaveTangent.X, SP.LeaveTangent.Y, SP.LeaveTangent.Z);
                }
            }
            
            // Lift-specific data
            if (N.Category == TEXT("Lift"))
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ LIFT DATA ═══"));
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Height: %.1fcm | Reversed: %d"), N.LiftHeight, N.bLiftIsReversed);
                UE_LOG(LogSmartFoundations, Display, TEXT("│ Bottom: (%.1f,%.1f,%.1f) Top: (%.1f,%.1f,%.1f)"),
                    N.LiftBottomLocation.X, N.LiftBottomLocation.Y, N.LiftBottomLocation.Z,
                    N.LiftTopLocation.X, N.LiftTopLocation.Y, N.LiftTopLocation.Z);
            }
            
            // Pipe-specific data
            if (N.Category == TEXT("Pipe"))
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE DATA ═══"));
                UE_LOG(LogSmartFoundations, Display, TEXT("│ SplineLength: %.1fcm | SplinePoints: %d"), N.SplineLength, N.SplinePointCount);
                
                for (const FSFCapturedSplinePoint& SP : N.SplinePoints)
                {
                    UE_LOG(LogSmartFoundations, Display, TEXT("│   [Point %d] Local=(%.1f,%.1f,%.1f) World=(%.1f,%.1f,%.1f)"),
                        SP.Index, SP.Location.X, SP.Location.Y, SP.Location.Z, 
                        SP.WorldLocation.X, SP.WorldLocation.Y, SP.WorldLocation.Z);
                }
            }
            
            // Factory connections
            if (N.FactoryConnections.Num() > 0)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ FACTORY CONNECTIONS (%d) ═══"), N.FactoryConnections.Num());
                for (const FSFCapturedConnection& Conn : N.FactoryConnections)
                {
                    FString ConnStatus = Conn.bIsConnected 
                        ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                        : TEXT("(not connected)");
                    UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Dir=%d] @ (%.1f,%.1f,%.1f) %s"),
                        *Conn.ConnectorName, Conn.Direction,
                        Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                        *ConnStatus);
                }
            }
            
            // Pipe connections
            if (N.PipeConnections.Num() > 0)
            {
                UE_LOG(LogSmartFoundations, Display, TEXT("│ ═══ PIPE CONNECTIONS (%d) ═══"), N.PipeConnections.Num());
                for (const FSFCapturedConnection& Conn : N.PipeConnections)
                {
                    FString ConnStatus = Conn.bIsConnected 
                        ? FString::Printf(TEXT("-> %s.%s"), *Conn.ConnectedToActor, *Conn.ConnectedToConnector)
                        : TEXT("(not connected)");
                    UE_LOG(LogSmartFoundations, Display, TEXT("│   %s [Type=%d] @ (%.1f,%.1f,%.1f) %s"),
                        *Conn.ConnectorName, Conn.Direction,
                        Conn.WorldLocation.X, Conn.WorldLocation.Y, Conn.WorldLocation.Z,
                        *ConnStatus);
                }
            }
            
            UE_LOG(LogSmartFoundations, Display, TEXT("└─────────────────────────────────────────────────────────────────────"));
        }
    }
    
    // Log FULL details of removed buildables
    if (RemovedBuildables.Num() > 0)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT(""));
        UE_LOG(LogSmartFoundations, Warning, TEXT("╔═══════════════════════════════════════════════════════════════════╗"));
        UE_LOG(LogSmartFoundations, Warning, TEXT("║          FULL ENUMERATION: REMOVED BUILDABLES (%d total)          ║"), RemovedBuildables.Num());
        UE_LOG(LogSmartFoundations, Warning, TEXT("╚═══════════════════════════════════════════════════════════════════╝"));
        
        for (int32 i = 0; i < RemovedBuildables.Num(); ++i)
        {
            const FSFCapturedBuildable& R = RemovedBuildables[i];
            UE_LOG(LogSmartFoundations, Warning, TEXT("   [%d] [%s] %s @ (%.0f, %.0f, %.0f)"),
                i, *R.Category, *R.Name, R.Location.X, R.Location.Y, R.Location.Z);
        }
    }
    */
}

// ==================== Phase 5/6: JSON-Based Post-Build Wiring ====================

int32 USFExtendService::GenerateAndExecuteWiring(AFGBuildableFactory* NewFactory)
{
    if (!NewFactory)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 EXTEND Phase 5/6: GenerateAndExecuteWiring called with null factory"));
        return 0;
    }
    
    // Check if we have stored clone topology from JSON spawning
    if (!StoredCloneTopology.IsValid() || StoredCloneTopology->ChildHolograms.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: No stored clone topology - skipping JSON wiring"));
        return 0;
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔌 EXTEND Phase 5/6: Generating wiring manifest from %d child holograms, %d registered built actors"),
        StoredCloneTopology->ChildHolograms.Num(), JsonBuiltActors.Num());
    
    // Build clone_id -> buildable mapping from JsonBuiltActors (populated during Construct())
    // Holograms are destroyed after Construct(), so we can't use JsonSpawnedHolograms here
    TMap<FString, AActor*> CloneIdToBuildable;
    
    // Add parent factory
    CloneIdToBuildable.Add(TEXT("parent"), NewFactory);
    
    // Copy all registered built actors
    for (const auto& Pair : JsonBuiltActors)
    {
        const FString& CloneId = Pair.Key;
        AActor* BuiltActor = Pair.Value;
        
        if (IsValid(BuiltActor))
        {
            CloneIdToBuildable.Add(CloneId, BuiltActor);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Using registered actor %s -> %s"),
                *CloneId, *BuiltActor->GetName());
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 EXTEND Phase 5/6: Registered actor for %s is no longer valid"), *CloneId);
        }
    }
    
    // Pre-resolve "source:" targets from topology into CloneIdToBuildable.
    // Clone 1's lane segments have "source:ActorName" targets pointing to the SOURCE
    // building's distributors (real world actors, not clones). Without this, Generate()
    // can't find them in CloneIdToBuildable and skips the connection.
    if (StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
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
                        UE_LOG(LogSmartFoundations, Log, TEXT("🔌 EXTEND Phase 5/6: Pre-resolved source target '%s' → %s"),
                            *Target, *SourceBuildable->GetName());
                    }
                }
            };
            
            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny0.Target);
            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny1.Target);
        }
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔌 EXTEND Phase 5/6: Mapped %d buildables (including parent + source targets)"), CloneIdToBuildable.Num());
    
    // Generate wiring manifest
    FSFWiringManifest WiringManifest = FSFWiringManifest::Generate(
        *StoredCloneTopology,
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
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Resolved source buildable '%s' -> %s"),
                        *SourceActorName, *SourceBuildable->GetName());
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("🔌 EXTEND Phase 5/6: Failed to resolve source buildable '%s'"),
                        *SourceActorName);
                }
            }
        }
    }
    
    // Save manifest for debugging
    FString LogDir = FPaths::ProjectLogDir();
    WiringManifest.SaveToFile(LogDir / TEXT("WiringManifest.json"));
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔌 EXTEND Phase 5/6: Generated manifest - %d belt connections, %d pipe connections"),
        WiringManifest.BeltConnections.Num(), WiringManifest.PipeConnections.Num());
    
    // Execute all wiring in single tick
    int32 WiredCount = WiringManifest.ExecuteWiring(GetWorld());
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔌 EXTEND Phase 5/6: Wiring complete - %d connections established"), WiredCount);
    
    // Create chain actors for wired belts (prevents crash in Factory_UpdateRadioactivity)
    // Pass JsonBuiltActors to include lane segments that were wired in ConfigureComponents
    int32 ChainsCreated = WiringManifest.CreateChainActors(GetWorld(), JsonBuiltActors);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Chain actors created - %d chains"), ChainsCreated);
    
    // Rebuild pipe networks to ensure fluid flow between source and clone manifolds
    // Pass JsonBuiltActors to include lane segment pipes that were wired in ConfigureComponents
    int32 NetworksRebuilt = WiringManifest.RebuildPipeNetworks(GetWorld(), JsonBuiltActors);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Pipe networks rebuilt - %d networks"), NetworksRebuilt);
    
    // ==================== Lift ↔ Passthrough Linking (Issue #260) ====================
    // Built lifts need mSnappedPassthroughs set so they render as half-height
    // when connected to floor holes. Uses world search (TActorIterator) to find
    // passthroughs near each lift — no dependency on JsonBuiltActors registration.
    {
        // Step 1: Collect ALL built lifts from JsonBuiltActors
        TArray<AFGBuildableConveyorLift*> BuiltLifts;
        for (const auto& Pair : JsonBuiltActors)
        {
            if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Pair.Value))
            {
                BuiltLifts.AddUnique(Lift);
            }
        }
        
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d built lifts in JsonBuiltActors (%d total entries)"),
            BuiltLifts.Num(), JsonBuiltActors.Num());
        
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
            
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d passthroughs within 100m of factory at %s"),
                NearbyPassthroughs.Num(), *BuildCenter.ToString());
            
            // Step 3: Get reflection property
            FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 PASSTHROUGH LINK: mSnappedPassthroughs property %s"),
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
                
                UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 PASSTHROUGH LINK: Lift %s bottom=(%s) top=(%s)"),
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
                            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → bottom=%s (dist=%.1f)"),
                                *BottomPT->GetName(), BestBottomDist);
                        }
                        if (TopPT)
                        {
                            (*PassthroughArray)[1] = TopPT;
                            TopPT->SetBottomSnappedConnection(Lift->GetConnection1());
                            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → top=%s (dist=%.1f)"),
                                *TopPT->GetName(), BestTopDist);
                        }
                        
                        // Trigger mesh rebuild via OnRep
                        UFunction* OnRepFunc = Lift->FindFunction(TEXT("OnRep_SnappedPassthroughs"));
                        if (OnRepFunc)
                        {
                            Lift->ProcessEvent(OnRepFunc, nullptr);
                            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → OnRep fired ✅"));
                        }
                        else
                        {
                            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → OnRep_SnappedPassthroughs NOT FOUND as UFunction"));
                        }
                        
                        LinkedCount++;
                    }
                    else
                    {
                        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → ContainerPtrToValuePtr returned null!"));
                    }
                }
                else
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("🔗   → no passthrough within %.0fcm"), SnapDistance);
                }
            }
            
            UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 PASSTHROUGH LINK: Linked %d/%d lifts to passthroughs"),
                LinkedCount, BuiltLifts.Num());
        }
    }
    
    // ==================== Pipe ↔ Passthrough Linking ====================
    // Built pipes through floor holes need SetTopSnappedConnection/SetBottomSnappedConnection
    // called on the passthrough with the pipe's connection. Same pattern as lift linking above.
    // AFGBuildablePassthrough does NOT own UFGPipeConnectionComponent components — it stores
    // references to connections from other actors that snap to it.
    {
        TArray<AFGBuildablePipeline*> BuiltPipes;
        for (const auto& Pair : JsonBuiltActors)
        {
            if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Pair.Value))
            {
                BuiltPipes.AddUnique(Pipe);
            }
        }
        
        if (BuiltPipes.Num() > 0)
        {
            // Collect nearby pipe passthroughs
            TArray<AFGBuildablePassthrough*> NearbyPipePassthroughs;
            FVector PipeBuildCenter = NewFactory ? NewFactory->GetActorLocation() : FVector::ZeroVector;
            
            for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (!IsValid(PT)) continue;
                
                // Only pipe floor holes (class name contains "Pipe")
                FString PTClassName = PT->GetClass()->GetFName().ToString();
                if (!PTClassName.Contains(TEXT("Pipe"))) continue;
                
                if (FVector::Dist(PT->GetActorLocation(), PipeBuildCenter) < 10000.0f)
                {
                    NearbyPipePassthroughs.Add(PT);
                }
            }
            
            int32 PipeLinkedCount = 0;
            const float PipeSnapDist = 100.0f; // 1m XY tolerance
            
            for (AFGBuildablePipeline* Pipe : BuiltPipes)
            {
                if (!IsValid(Pipe)) continue;
                
                // Check both endpoints of the pipe
                UFGPipeConnectionComponentBase* PipeConn0 = Pipe->GetPipeConnection0();
                UFGPipeConnectionComponentBase* PipeConn1 = Pipe->GetPipeConnection1();
                
                for (AFGBuildablePassthrough* PT : NearbyPipePassthroughs)
                {
                    FVector PTLoc = PT->GetActorLocation();
                    float PTZ = PTLoc.Z;
                    
                    // Check Conn0 against this passthrough
                    if (PipeConn0)
                    {
                        FVector Conn0Loc = PipeConn0->GetComponentLocation();
                        float DistXY = FVector::Dist2D(Conn0Loc, PTLoc);
                        if (DistXY < PipeSnapDist)
                        {
                            bool bIsTop = (Conn0Loc.Z >= PTZ);
                            UFGConnectionComponent* ConnBase = Cast<UFGConnectionComponent>(PipeConn0);
                            if (ConnBase)
                            {
                                if (bIsTop)
                                    PT->SetTopSnappedConnection(ConnBase);
                                else
                                    PT->SetBottomSnappedConnection(ConnBase);
                                PipeLinkedCount++;
                                UE_LOG(LogSmartFoundations, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: %s.Conn0 → %s Set%sSnappedConnection"),
                                    *Pipe->GetName(), *PT->GetName(), bIsTop ? TEXT("Top") : TEXT("Bottom"));
                            }
                        }
                    }
                    
                    // Check Conn1 against this passthrough
                    if (PipeConn1)
                    {
                        FVector Conn1Loc = PipeConn1->GetComponentLocation();
                        float DistXY = FVector::Dist2D(Conn1Loc, PTLoc);
                        if (DistXY < PipeSnapDist)
                        {
                            bool bIsTop = (Conn1Loc.Z >= PTZ);
                            UFGConnectionComponent* ConnBase = Cast<UFGConnectionComponent>(PipeConn1);
                            if (ConnBase)
                            {
                                if (bIsTop)
                                    PT->SetTopSnappedConnection(ConnBase);
                                else
                                    PT->SetBottomSnappedConnection(ConnBase);
                                PipeLinkedCount++;
                                UE_LOG(LogSmartFoundations, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: %s.Conn1 → %s Set%sSnappedConnection"),
                                    *Pipe->GetName(), *PT->GetName(), bIsTop ? TEXT("Top") : TEXT("Bottom"));
                            }
                        }
                    }
                }
            }
            
            if (PipeLinkedCount > 0)
            {
                UE_LOG(LogSmartFoundations, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: Linked %d pipe connections to %d pipe floor holes"),
                    PipeLinkedCount, NearbyPipePassthroughs.Num());
            }
        }
    }
    
    // VERIFICATION: Log chain actor status for all built conveyors
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: Checking %d built actors for chain actor status"), JsonBuiltActors.Num());
    int32 ValidChains = 0;
    int32 NullChains = 0;
    for (const auto& Pair : JsonBuiltActors)
    {
        if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Pair.Value))
        {
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            if (ChainActor)
            {
                ValidChains++;
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ VERIFY: ✅ %s -> Chain=%s (segments=%d)"),
                    *Conveyor->GetName(), *ChainActor->GetName(), ChainActor->GetNumChainSegments());
            }
            else
            {
                NullChains++;
                UE_LOG(LogSmartFoundations, Error, TEXT("⛓️ VERIFY: ❌ %s -> ChainActor=NULL (CRASH RISK!)"),
                    *Conveyor->GetName());
            }
        }
    }
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: %d valid, %d NULL (crash risk if NULL > 0)"), ValidChains, NullChains);
    
    // ==================== Power Pole Wiring (Issue #229) ====================
    // Wire built power poles: clone factory ↔ clone pole, and source pole ↔ clone pole
    int32 PowerWiredCount = 0;
    UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
    
    for (const auto& WiringPair : PowerPoleWiringData)
    {
        const FString& CloneId = WiringPair.Key;
        const FSFSourcePoleWiringData& SourceData = WiringPair.Value;
        
        // Find the built clone pole
        AActor* const* BuiltPoleActor = CloneIdToBuildable.Find(CloneId);
        if (!BuiltPoleActor || !IsValid(*BuiltPoleActor))
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Power Wire: Clone pole %s not found in built actors"), *CloneId);
            continue;
        }
        
        AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(*BuiltPoleActor);
        if (!ClonePole)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Power Wire: Built actor %s is not a power pole"), *CloneId);
            continue;
        }
        
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Processing %s -> %s"), *CloneId, *ClonePole->GetName());
        
        // --- Wire 1: Clone Factory ↔ Clone Pole ---
        if (WireClass && IsValid(NewFactory))
        {
            // Get circuit connections — factory power connections are UFGCircuitConnectionComponent subclass
            // (belt/pipe connectors use separate class hierarchies, so only power conns returned)
            TArray<UFGCircuitConnectionComponent*> FactoryCircuitConns;
            NewFactory->GetComponents<UFGCircuitConnectionComponent>(FactoryCircuitConns);
            
            TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);
            
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Factory %s has %d circuit conns, Pole %s has %d circuit conns"),
                *NewFactory->GetName(), FactoryCircuitConns.Num(), *ClonePole->GetName(), PoleCircuitConns.Num());
            
            if (FactoryCircuitConns.Num() > 0 && PoleCircuitConns.Num() > 0)
            {
                UFGCircuitConnectionComponent* FactoryConn = FactoryCircuitConns[0];
                UFGCircuitConnectionComponent* PoleConn = PoleCircuitConns[0];
                
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                
                AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                    WireClass, ClonePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
                
                if (NewWire)
                {
                    bool bConnected = NewWire->Connect(FactoryConn, PoleConn);
                    if (bConnected)
                    {
                        PowerWiredCount++;
                        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Connected clone factory %s ↔ clone pole %s"),
                            *NewFactory->GetName(), *ClonePole->GetName());
                    }
                    else
                    {
                        UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Power Wire: Wire Connect() failed for factory ↔ pole - destroying wire"));
                        NewWire->Destroy();
                    }
                }
            }
            else
            {
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Power Wire: Missing connections - factory %s has %d circuit conns, pole has %d circuit conns"),
                    *NewFactory->GetClass()->GetName(), FactoryCircuitConns.Num(), PoleCircuitConns.Num());
            }
        }
        
        // --- Wire 2: Source Pole ↔ Clone Pole (only if source has free connections) ---
        if (WireClass && SourceData.SourcePole.IsValid() && SourceData.bSourceHasFreeConnections)
        {
            AFGBuildablePowerPole* SourcePole = SourceData.SourcePole.Get();
            
            // Get circuit connections on both poles
            TArray<UFGCircuitConnectionComponent*> SourceCircuitConns;
            SourcePole->GetComponents<UFGCircuitConnectionComponent>(SourceCircuitConns);
            
            TArray<UFGCircuitConnectionComponent*> CloneCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(CloneCircuitConns);
            
            if (SourceCircuitConns.Num() > 0 && CloneCircuitConns.Num() > 0)
            {
                UFGCircuitConnectionComponent* SourceConn = SourceCircuitConns[0];
                UFGCircuitConnectionComponent* CloneConn = CloneCircuitConns[0];
                
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                
                AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                    WireClass, SourcePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
                
                if (NewWire)
                {
                    bool bConnected = NewWire->Connect(SourceConn, CloneConn);
                    if (bConnected)
                    {
                        PowerWiredCount++;
                        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Connected source pole %s ↔ clone pole %s"),
                            *SourcePole->GetName(), *ClonePole->GetName());
                    }
                    else
                    {
                        UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND Power Wire: Wire Connect() failed for source ↔ clone pole - destroying wire"));
                        NewWire->Destroy();
                    }
                }
            }
        }
        else if (SourceData.SourcePole.IsValid() && !SourceData.bSourceHasFreeConnections)
        {
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Source pole %s has no free connections - skipping source↔clone wire (subsequent extends will chain)"),
                *SourceData.SourcePole->GetName());
        }
    }
    
    if (PowerWiredCount > 0)
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EXTEND Power Wire: Complete - %d power connections established"), PowerWiredCount);
    }
    WiredCount += PowerWiredCount;
    
    // Capture built factory topology for comparison with source
    FSFSourceTopology BuiltTopology = FSFSourceTopology::CaptureFromBuiltFactory(NewFactory);
    BuiltTopology.SaveToFile(LogDir / TEXT("ManifoldBuilt.json"));
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📋 EXTEND: Saved ManifoldBuilt.json for comparison with ManifoldSource.json"));
    
    // ==================== Scaled Extend: Additional Clone Wiring (Issue #265) ====================
    // Process wiring for each additional clone set beyond clone 1
    int32 ScaledExtendWiredCount = 0;
    for (int32 CloneIdx = 0; CloneIdx < ScaledExtendClones.Num(); CloneIdx++)
    {
        FSFScaledExtendClone& Clone = ScaledExtendClones[CloneIdx];
        if (!Clone.CloneTopology.IsValid() || Clone.CloneTopology->ChildHolograms.Num() == 0)
        {
            continue;
        }
        
        // Find this clone's factory building from built actors
        // The factory hologram was registered as "factory" in the clone's SpawnedHolograms
        // But we need to find the BUILT factory, not the hologram
        AFGBuildableFactory* CloneFactory = nullptr;
        
        // Search JsonBuiltActors for any factory building near the clone's expected position
        // Clone.WorldOffset is relative to SOURCE building, not clone 1
        FVector SourcePos = CurrentExtendTarget.IsValid() ? CurrentExtendTarget->GetActorLocation() : (NewFactory->GetActorLocation() - ScaledExtendClones[0].WorldOffset);
        FVector ExpectedPos = SourcePos + Clone.WorldOffset;
        float BestDist = MAX_FLT;
        for (const auto& BuiltPair : JsonBuiltActors)
        {
            if (AFGBuildableFactory* BuiltFactory = Cast<AFGBuildableFactory>(BuiltPair.Value))
            {
                float Dist = FVector::Dist(BuiltFactory->GetActorLocation(), ExpectedPos);
                if (Dist < 500.0f && Dist < BestDist)  // Within 5m of expected position
                {
                    BestDist = Dist;
                    CloneFactory = BuiltFactory;
                }
            }
        }
        
        if (!CloneFactory)
        {
            // Try finding from OnActorSpawned-registered factories
            for (TActorIterator<AFGBuildableFactory> It(GetWorld()); It; ++It)
            {
                float Dist = FVector::Dist(It->GetActorLocation(), ExpectedPos);
                if (Dist < 500.0f && Dist < BestDist)
                {
                    BestDist = Dist;
                    CloneFactory = *It;
                }
            }
        }
        
        if (!CloneFactory)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND Wire: Could not find built factory for Clone[%d] at expected position"), CloneIdx);
            continue;
        }
        
        // Build clone_id -> buildable mapping for this clone set
        TMap<FString, AActor*> CloneBuiltActors;
        CloneBuiltActors.Add(TEXT("parent"), CloneFactory);
        
        // Find all built actors with this clone's prefix
        FString ClonePrefix = FString::Printf(TEXT("sc%d_"), CloneIdx);
        for (const auto& Pair : JsonBuiltActors)
        {
            if (Pair.Key.StartsWith(ClonePrefix) && IsValid(Pair.Value))
            {
                // Strip prefix for topology matching
                FString OriginalId = Pair.Key.Mid(ClonePrefix.Len());
                CloneBuiltActors.Add(OriginalId, Pair.Value);
            }
        }
        
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d built actors mapped (factory=%s)"),
            CloneIdx, CloneBuiltActors.Num(), *CloneFactory->GetName());
        
        // Generate and execute wiring for this clone
        // Use the clone's topology but with stripped IDs (matching original topology structure)
        FSFCloneTopology StrippedTopology = *Clone.CloneTopology;
        for (FSFCloneHologram& Holo : StrippedTopology.ChildHolograms)
        {
            if (Holo.HologramId.StartsWith(ClonePrefix))
            {
                Holo.HologramId = Holo.HologramId.Mid(ClonePrefix.Len());
            }
        }
        
        FSFWiringManifest CloneManifest = FSFWiringManifest::Generate(
            StrippedTopology, CloneBuiltActors, CloneFactory);
        
        int32 CloneWired = CloneManifest.ExecuteWiring(GetWorld());
        int32 CloneChains = CloneManifest.CreateChainActors(GetWorld(), CloneBuiltActors);
        int32 ClonePipes = CloneManifest.RebuildPipeNetworks(GetWorld(), CloneBuiltActors);
        
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
        
        // Pipe ↔ Passthrough linking for scaled clone
        {
            TArray<AFGBuildablePipeline*> ClonePipeList;
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(BuiltPair.Value))
                    ClonePipeList.AddUnique(Pipe);
            }
            if (ClonePipeList.Num() > 0)
            {
                TArray<AFGBuildablePassthrough*> NearbyPipePTs;
                FVector CenterLoc = CloneFactory->GetActorLocation();
                for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
                {
                    AFGBuildablePassthrough* PT = *It;
                    if (!IsValid(PT)) continue;
                    FString PTClass = PT->GetClass()->GetFName().ToString();
                    if (!PTClass.Contains(TEXT("Pipe"))) continue;
                    if (FVector::Dist(PT->GetActorLocation(), CenterLoc) < 10000.0f)
                        NearbyPipePTs.Add(PT);
                }
                for (AFGBuildablePipeline* Pipe : ClonePipeList)
                {
                    if (!IsValid(Pipe)) continue;
                    UFGPipeConnectionComponentBase* PC0 = Pipe->GetPipeConnection0();
                    UFGPipeConnectionComponentBase* PC1 = Pipe->GetPipeConnection1();
                    for (AFGBuildablePassthrough* PT : NearbyPipePTs)
                    {
                        FVector PTLoc = PT->GetActorLocation();
                        float PTZ = PTLoc.Z;
                        if (PC0)
                        {
                            FVector C0Loc = PC0->GetComponentLocation();
                            if (FVector::Dist2D(C0Loc, PTLoc) < 100.0f)
                            {
                                UFGConnectionComponent* CB = Cast<UFGConnectionComponent>(PC0);
                                if (CB)
                                {
                                    if (C0Loc.Z >= PTZ) PT->SetTopSnappedConnection(CB);
                                    else PT->SetBottomSnappedConnection(CB);
                                }
                            }
                        }
                        if (PC1)
                        {
                            FVector C1Loc = PC1->GetComponentLocation();
                            if (FVector::Dist2D(C1Loc, PTLoc) < 100.0f)
                            {
                                UFGConnectionComponent* CB = Cast<UFGConnectionComponent>(PC1);
                                if (CB)
                                {
                                    if (C1Loc.Z >= PTZ) PT->SetTopSnappedConnection(CB);
                                    else PT->SetBottomSnappedConnection(CB);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Power pole wiring for this clone — connect built power poles to clone factory
        int32 ClonePowerWired = 0;
        if (WireClass)
        {
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (!BuiltPair.Key.Contains(TEXT("power_pole"))) continue;
                
                AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(BuiltPair.Value);
                if (!ClonePole) continue;
                
                // Get circuit connections on both pole and factory
                // Factory power connections are UFGCircuitConnectionComponent subclass
                TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
                ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);
                
                TArray<UFGCircuitConnectionComponent*> FactoryCircuitConns;
                CloneFactory->GetComponents<UFGCircuitConnectionComponent>(FactoryCircuitConns);
                
                if (PoleCircuitConns.Num() > 0 && FactoryCircuitConns.Num() > 0)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    
                    AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                        WireClass, ClonePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
                    
                    if (NewWire)
                    {
                        bool bConnected = NewWire->Connect(FactoryCircuitConns[0], PoleCircuitConns[0]);
                        if (bConnected)
                        {
                            ClonePowerWired++;
                            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND Power: Connected %s ↔ %s"),
                                *CloneFactory->GetName(), *ClonePole->GetName());
                        }
                        else
                        {
                            NewWire->Destroy();
                        }
                    }
                }
            }
        }
        
        ScaledExtendWiredCount += CloneWired + ClonePowerWired;
        
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d belt/pipe, %d power, %d chains, %d pipe networks%s"),
            CloneIdx, CloneWired, ClonePowerWired, CloneChains, ClonePipes, Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
    }
    
    if (ScaledExtendWiredCount > 0)
    {
        UE_LOG(LogSmartFoundations, Display, TEXT("⚡ SCALED EXTEND Wire: Total %d additional connections across %d clones"),
            ScaledExtendWiredCount, ScaledExtendClones.Num());
        WiredCount += ScaledExtendWiredCount;
    }
    
    // ==================== Power Pole Chaining (Issue #229/#265) ====================
    // Chain power poles between consecutive clones: clone1_pole → clone2_pole → clone3_pole...
    // IDs: "power_pole_N" (clone 1), "sc0_power_pole_N" (clone 2), "sc1_power_pole_N" (clone 3), etc.
    if (WireClass)
    {
        // Discover all power pole indices from clone 1's topology
        TArray<int32> PoleIndices;
        for (const auto& Pair : CloneIdToBuildable)
        {
            FString Key = Pair.Key;
            if (Key.StartsWith(TEXT("power_pole_")))
            {
                FString IndexStr = Key.Mid(11); // after "power_pole_"
                int32 Idx = FCString::Atoi(*IndexStr);
                PoleIndices.AddUnique(Idx);
            }
        }
        
        int32 ChainWiredCount = 0;
        for (int32 PoleIdx : PoleIndices)
        {
            // Build ordered list: clone 1 pole, then each scaled clone's pole
            TArray<AFGBuildablePowerPole*> PoleChain;
            
            // Clone 1's pole
            FString Clone1PoleId = FString::Printf(TEXT("power_pole_%d"), PoleIdx);
            if (AActor* const* Actor = CloneIdToBuildable.Find(Clone1PoleId))
            {
                if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(*Actor))
                    PoleChain.Add(Pole);
            }
            
            // Each scaled clone's pole
            for (int32 CloneIdx = 0; CloneIdx < ScaledExtendClones.Num(); CloneIdx++)
            {
                FString ScPoleId = FString::Printf(TEXT("sc%d_power_pole_%d"), CloneIdx, PoleIdx);
                if (AActor* const* Actor = CloneIdToBuildable.Find(ScPoleId))
                {
                    if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(*Actor))
                        PoleChain.Add(Pole);
                }
            }
            
            // Wire consecutive poles in the chain
            for (int32 j = 0; j < PoleChain.Num() - 1; j++)
            {
                AFGBuildablePowerPole* PoleA = PoleChain[j];
                AFGBuildablePowerPole* PoleB = PoleChain[j + 1];
                
                TArray<UFGCircuitConnectionComponent*> ConnsA, ConnsB;
                PoleA->GetComponents<UFGCircuitConnectionComponent>(ConnsA);
                PoleB->GetComponents<UFGCircuitConnectionComponent>(ConnsB);
                
                if (ConnsA.Num() > 0 && ConnsB.Num() > 0)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    
                    AFGBuildableWire* ChainWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                        WireClass, PoleA->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
                    
                    if (ChainWire)
                    {
                        bool bConnected = ChainWire->Connect(ConnsA[0], ConnsB[0]);
                        if (bConnected)
                        {
                            ChainWiredCount++;
                            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ POWER CHAIN: Connected %s ↔ %s (pole_%d, link %d)"),
                                *PoleA->GetName(), *PoleB->GetName(), PoleIdx, j);
                        }
                        else
                        {
                            ChainWire->Destroy();
                        }
                    }
                }
            }
        }
        
        if (ChainWiredCount > 0)
        {
            UE_LOG(LogSmartFoundations, Display, TEXT("⚡ POWER CHAIN: %d pole-to-pole connections across %d pole indices"),
                ChainWiredCount, PoleIndices.Num());
            WiredCount += ChainWiredCount;
        }
    }
    
    // Clear stored topology and built actors after wiring
    StoredCloneTopology.Reset();
    JsonSpawnedHolograms.Empty();
    JsonBuiltActors.Empty();
    PowerPoleWiringData.Empty();
    
    // Clear scaled extend clones (their topologies were consumed by wiring)
    ScaledExtendClones.Empty();
    
    return WiredCount;
}

void USFExtendService::RegisterJsonBuiltActor(const FString& CloneId, AActor* BuiltActor)
{
    if (!BuiltActor || CloneId.IsEmpty())
    {
        return;
    }
    
    JsonBuiltActors.Add(CloneId, BuiltActor);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔌 EXTEND: Registered built actor %s -> %s"),
        *CloneId, *BuiltActor->GetName());
}

AFGBuildable* USFExtendService::GetBuiltActorByCloneId(const FString& CloneId) const
{
    if (CloneId.IsEmpty())
    {
        return nullptr;
    }
    
    // Check JsonBuiltActors map
    if (AActor* const* FoundActor = JsonBuiltActors.Find(CloneId))
    {
        return Cast<AFGBuildable>(*FoundActor);
    }
    
    return nullptr;
}

AFGBuildable* USFExtendService::GetSourceBuildableByName(const FString& ActorName) const
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
    
    UE_LOG(LogSmartFoundations, Warning, TEXT("🛤️ LANE: Source buildable '%s' not found in world"), *ActorName);
    return nullptr;
}

// ==================== Scaled Extend (Issue #265) ====================

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
    if (!bHasValidTarget || !CurrentExtendTarget.IsValid() || !CurrentExtendHologram.IsValid())
    {
        return;
    }
    
    // First scale action commits to Extend (enables sticky behavior)
    if (!bExtendCommitted)
    {
        bExtendCommitted = true;
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Committed (first scale action)"));
    }
    
    int32 CloneCount = GetExtendCloneCount();
    int32 RowCount = GetExtendRowCount();
    
    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: State changed - X=%d (clones=%d), Y=%d (rows=%d)"),
        CloneCount + 1, CloneCount, RowCount, RowCount);
    
    // Clear all existing previews (both primary clone infrastructure and scaled extend clones)
    ClearBeltPreviews();
    ClearScaledExtendClones();
    
    // CRITICAL: Refresh hologram position BEFORE creating belt previews.
    // RefreshExtension applies spacing/steps to clone 1's position.
    // CreateBeltPreviews derives infrastructure positions from the parent hologram's
    // current location (CloneOffset = HologramPos - SourcePos), so the hologram
    // must be at the correct position first.
    // Force refresh even if inspection lock is active - grid changes should always apply
    RefreshExtension(CurrentExtendHologram.Get(), true);
    
    // Now recreate the primary clone's infrastructure (clone 1 = parent hologram position)
    // This is the existing single-clone Extend behavior
    CreateBeltPreviews(CurrentExtendHologram.Get());
    
    // Apply rigid body rotation to clone 1's infrastructure when rotation is active.
    // CreateBeltPreviews positions infrastructure based on source topology + translation,
    // but doesn't rotate. The factory building was rotated in RefreshExtension(),
    // so infrastructure must rotate to maintain rigid topology.
    //
    // We use the same pattern as SpawnScaledExtendPreviews (Step 2.25 + IntendedPositions):
    // 1. Rotate topology data (positions, spline points, normals)
    // 2. Reposition spawned holograms from rotated topology
    // 3. Regenerate spline meshes at correct positions
    if (Subsystem.IsValid() && StoredCloneTopology.IsValid())
    {
        const FSFCounterState& State = Subsystem->GetCounterState();
        if (!FMath::IsNearlyZero(State.RotationZ))
        {
            ESFExtendDirection CurDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
            float DirSignRot = (CurDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;
            FRotator Clone1RotOffset(0.0f, State.RotationZ * DirSignRot, 0.0f);
            FVector FactoryCenter = CurrentExtendHologram->GetActorLocation();
            
            // Step 1: Rotate topology data (same as Step 2.25 for additional clones)
            // WorldOffset from source to clone 1 (used to determine clone-side of lane segments)
            FVector Clone1WorldOffset = FactoryCenter - CurrentExtendTarget->GetActorLocation();
            
            for (FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
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
                    FVector TopPos = Holo.LiftData.TopTransform.Location.ToFVector();
                    FVector TopRel = TopPos - FactoryCenter;
                    Holo.LiftData.TopTransform.Location = FSFVec3(FactoryCenter + Clone1RotOffset.RotateVector(TopRel));
                    Holo.LiftData.TopTransform.Rotation = FSFRot3(Holo.LiftData.TopTransform.Rotation.ToFRotator() + Clone1RotOffset);
                    
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
            for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
            {
                IntendedPositions.Add(Holo.HologramId, Holo.Transform.Location.ToFVector());
                IntendedRotations.Add(Holo.HologramId, Holo.Transform.Rotation.ToFRotator());
                HologramDataMap.Add(Holo.HologramId, &Holo);
            }
            
            // Reposition each spawned hologram
            for (const auto& Pair : JsonSpawnedHolograms)
            {
                AFGHologram* SpawnedHolo = Pair.Value;
                if (!SpawnedHolo) continue;
                
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
                // Without this, the spline component may use stale transforms when
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
                if (HologramService)
                {
                    HologramService->TrackChildHologram(SpawnedHolo, IntendedPos, IntendedRot);
                }
            }
            
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Applied rigid rotation (%.1f°) to clone 1 topology + repositioned %d holograms"),
                State.RotationZ, JsonSpawnedHolograms.Num());
            
            // CRITICAL: Disable clearance detection on the parent hologram when rotation is active.
            // Vanilla's CheckValidPlacement checks the parent's clearance box against nearby
            // buildings. With any rotation, the parent shifts enough to overlap with the source
            // building's clearance, causing "Encroaching another object's clearance!".
            if (CurrentExtendHologram.IsValid())
            {
                TArray<UBoxComponent*> ParentBoxes;
                CurrentExtendHologram->GetComponents<UBoxComponent>(ParentBoxes);
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
        bScaledExtendValid = ValidateScaledExtendConstraints() && ValidatePowerCapacity();
        
        if (bScaledExtendValid)
        {
            // Spawn factory holograms + infrastructure for additional clones
            SpawnScaledExtendPreviews();
            
            // Phase 6: Merge all scaled extend clone topologies into StoredCloneTopology.
            // The existing wiring system uses StoredCloneTopology to generate wiring manifests.
            // Without this merge, only clone 1's topology is available for post-build wiring.
            if (StoredCloneTopology.IsValid())
            {
                int32 MergedCount = 0;
                for (const FSFScaledExtendClone& Clone : ScaledExtendClones)
                {
                    if (Clone.CloneTopology.IsValid())
                    {
                        for (const FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
                        {
                            StoredCloneTopology->ChildHolograms.Add(Holo);
                            MergedCount++;
                        }
                    }
                }
                UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND Phase 6: Merged %d holograms from %d clones into StoredCloneTopology (total: %d)"),
                    MergedCount, ScaledExtendClones.Num(), StoredCloneTopology->ChildHolograms.Num());
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND: Configuration invalid - %s"), *ScaledExtendInvalidReason);
            
            // Phase 7: Invalidate the grid - set hologram material to red/error
            if (CurrentExtendHologram.IsValid())
            {
                CurrentExtendHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
                UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Hologram set to ERROR state - %s"), *ScaledExtendInvalidReason);
            }
        }
    }
    
}

void USFExtendService::CalculateScaledExtendPositions()
{
    ScaledExtendClones.Empty();
    
    if (!CurrentExtendTarget.IsValid() || !Subsystem.IsValid())
    {
        return;
    }
    
    AFGBuildable* SourceBuilding = CurrentExtendTarget.Get();
    const FSFCounterState& State = Subsystem->GetCounterState();
    
    // Get building size from registry
    USFBuildableSizeRegistry::Initialize();
    FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(SourceBuilding->GetClass());
    FVector BuildingSize = Profile.DefaultSize;
    
    FRotator SourceRotation = SourceBuilding->GetActorRotation();
    
    // Calculate effective Y row height from topology extent (prevents row overlap).
    // Infrastructure (distributors, belts, pipes) may extend beyond the factory's Y footprint.
    float EffectiveRowHeight = BuildingSize.Y;
    if (StoredCloneTopology.IsValid() && StoredCloneTopology->ChildHolograms.Num() > 0)
    {
        FVector CloneOffset = StoredCloneTopology->WorldOffset.ToFVector();
        FVector CloneFactoryCenter = SourceBuilding->GetActorLocation() + CloneOffset;
        FRotator InvRot = SourceRotation.GetInverse();
        
        float MinLocalY = 0.0f, MaxLocalY = 0.0f;
        for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
        {
            if (Holo.bIsLaneSegment) continue;  // Lane segments span between clones, not relevant
            FVector WorldPos = Holo.Transform.Location.ToFVector();
            FVector LocalPos = InvRot.RotateVector(WorldPos - CloneFactoryCenter);
            
            // Expand bounds by half the building's Y width to use edges instead of centers.
            // Distributors (splitters/mergers/junctions) are the outermost infrastructure
            // and are ~200cm wide (half = 100cm). Belt/pipe segments are thin and don't
            // contribute meaningfully. Without this, ~4m shortfall from center-to-center.
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
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Topology Y extent (%.0f) > BuildingSize.Y (%.0f) — using topology extent for row spacing"),
                TopologyYExtent, BuildingSize.Y);
        }
    }
    
    int32 CloneCount = GetExtendCloneCount();
    int32 RowCount = GetExtendRowCount();
    
    // Get extend direction offset (perpendicular to belt flow)
    ESFExtendDirection CurrentDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
    float XDirectionSign = (CurrentDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;
    
    // Determine Y direction sign (negative Y = opposite perpendicular)
    int32 YDir = (Subsystem->GetCounterState().GridCounters.Y >= 0) ? 1 : -1;
    
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
            
            ScaledExtendClones.Add(SeedClone);
        }
        
        // For each clone in this row
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
            
            ScaledExtendClones.Add(ExtendClone);
        }
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Calculated %d clone positions (Clones=%d, Rows=%d)"),
        ScaledExtendClones.Num(), CloneCount, RowCount);
}

void USFExtendService::SpawnScaledExtendPreviews()
{
    if (!CurrentExtendTarget.IsValid() || !CurrentExtendHologram.IsValid() || !TopologyService)
    {
        return;
    }
    
    if (ScaledExtendClones.Num() == 0)
    {
        return;
    }
    
    AFGBuildable* SourceBuilding = CurrentExtendTarget.Get();
    AFGHologram* ParentHologram = CurrentExtendHologram.Get();
    FVector SourceLocation = SourceBuilding->GetActorLocation();
    FRotator SourceRotation = SourceBuilding->GetActorRotation();
    
    // Get source topology (already walked)
    const FSFExtendTopology& Topology = TopologyService->GetCurrentTopology();
    if (!Topology.bIsValid)
    {
        return;
    }
    
    // Capture source topology once for cloning
    FSFSourceTopology SourceTopo = FSFSourceTopology::CaptureFromTopology(Topology);
    
    int32 TotalHologramsSpawned = 0;
    
    for (int32 i = 0; i < ScaledExtendClones.Num(); i++)
    {
        FSFScaledExtendClone& Clone = ScaledExtendClones[i];
        
        // Calculate world position for this clone's factory building
        FVector CloneWorldPos = SourceLocation + Clone.WorldOffset;
        FRotator CloneWorldRot = SourceRotation + Clone.RotationOffset;
        
        // === Step 1: Spawn factory building hologram for this clone ===
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
            
            // Copy stored recipe to this clone
            if (Subsystem.IsValid() && Subsystem->bHasStoredProductionRecipe)
            {
                USFHologramDataService::StoreRecipe(FactoryHologram, Subsystem->StoredProductionRecipe);
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
            
            // Tag as Smart! child so HologramService's mChildren filter catches it
            FactoryHologram->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
            
            // Set JsonCloneId so the factory registers in JsonBuiltActors during Construct()
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
            BeltPreviewHolograms.Add(FactoryHologram);
            Clone.SpawnedHolograms.Add(TEXT("factory"), FactoryHologram);
            
            // Track in hologram service for position refresh
            if (HologramService)
            {
                HologramService->TrackChildHologram(FactoryHologram, CloneWorldPos, CloneWorldRot);
            }
            
            TotalHologramsSpawned++;
            
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Spawned factory hologram for Clone[%d] (%d,%d) at (%+.0f, %+.0f, %+.0f)%s"),
                i, Clone.GridX, Clone.GridY, CloneWorldPos.X, CloneWorldPos.Y, CloneWorldPos.Z,
                Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
        }
        else
        {
            UE_LOG(LogSmartFoundations, Error, TEXT("⚡ SCALED EXTEND: Failed to spawn factory hologram for Clone[%d]"), i);
            continue;
        }
        
        // === Step 2: Spawn infrastructure (belts, distributors, pipes, power) around this clone ===
        Clone.CloneTopology = MakeShared<FSFCloneTopology>(FSFCloneTopology::FromSource(SourceTopo, Clone.WorldOffset));
        
        // === Step 2.25: RIGID BODY ROTATION ===
        // Clone topology is a rigid body relative to the factory building.
        // FromSource generates positions with translation only (no rotation).
        // If the factory has a RotationOffset, rotate ALL infrastructure positions
        // around the factory center to maintain the rigid spatial relationship.
        // Lane segments are adaptive — only their clone-side endpoint rotates.
        if (!Clone.RotationOffset.IsNearlyZero())
        {
            // Factory center = source factory position + this clone's world offset
            FVector FactoryCenter = SourceTopo.Factory.Transform.Location.ToFVector() + Clone.WorldOffset;
            
            for (FSFCloneHologram& Holo : Clone.CloneTopology->ChildHolograms)
            {
                if (Holo.bIsLaneSegment)
                {
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
            
            UE_LOG(LogSmartFoundations, Log, TEXT("⚡ RIGID ROTATION: Clone[%d] rotated %d holograms by (%.0f,%.0f,%.0f) around factory center"),
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
                UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CHAIN: Seed clone - removed lane segments (source already has manifold)"));
            }
            else
            {
            // Determine previous clone's world offset and rotation
            FVector PrevCloneOffset;
            FRotator PrevCloneRotation = FRotator::ZeroRotator;
            if (Clone.GridY == 0 && (i == 0 || ScaledExtendClones[i-1].GridY != Clone.GridY))
            {
                // First additional clone in row 0 → previous is clone 1 (parent hologram)
                PrevCloneOffset = CurrentExtendHologram->GetActorLocation() - CurrentExtendTarget->GetActorLocation();
                // Parent hologram IS rotated by RotationZ * DirSign when rotation is active
                // Must include DirSign to match clone 1's actual rotation from RefreshExtension
                if (Subsystem.IsValid())
                {
                    const FSFCounterState& CounterState = Subsystem->GetCounterState();
                    if (!FMath::IsNearlyZero(CounterState.RotationZ))
                    {
                        ESFExtendDirection PrevDir = DetectionService ? DetectionService->GetExtendDirection() : ESFExtendDirection::Right;
                        float PrevDirSign = (PrevDir == ESFExtendDirection::Right) ? 1.0f : -1.0f;
                        PrevCloneRotation = FRotator(0.0f, CounterState.RotationZ * PrevDirSign, 0.0f);
                    }
                }
            }
            else
            {
                // Previous clone in same row
                PrevCloneOffset = ScaledExtendClones[i-1].WorldOffset;
                PrevCloneRotation = ScaledExtendClones[i-1].RotationOffset;
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
                if (!Holo.bIsLaneSegment) continue;
                
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
                        NewEndWorld = OldEndWorld; // End stays at this clone
                        // Rotate source-side normal to match previous clone's distributor orientation
                        if (!PrevCloneRotation.IsNearlyZero())
                        {
                            Holo.LaneStartNormal = FSFVec3(PrevCloneRotation.RotateVector(Holo.LaneStartNormal.ToFVector()));
                        }
                    }
                    else
                    {
                        // Source is at END → move end to previous clone's rotated connector
                        NewStartWorld = OldStartWorld; // Start stays at this clone
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
                    
                    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CHAIN: Clone[%d] lane %s: %s shifted by PrevOffset(%.0f,%.0f,%.0f), length %.0f→%.0f"),
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
        if (Clone.GridY == 0 && (i == 0 || ScaledExtendClones[i-1].GridY != Clone.GridY))
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
            ParentHologram, this, InfraHolograms);
        
        TotalHologramsSpawned += InfraSpawned;
        
        // Build maps of hologram ID → intended transforms and spline data from clone topology.
        // We need this because AddChild() inside SpawnChildHolograms repositions actors
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
                BeltPreviewHolograms.Add(Pair.Value);
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
                
                // Track in HologramService for position refresh using
                // the TOPOLOGY-derived position, not GetActorLocation().
                if (HologramService)
                {
                    HologramService->TrackChildHologram(
                        Pair.Value, IntendedPos, IntendedRot);
                }
            }
        }
        
        UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Clone[%d] (%d,%d) - factory + %d infrastructure holograms%s"),
            i, Clone.GridX, Clone.GridY, InfraSpawned, Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
    }
    
    UE_LOG(LogSmartFoundations, Display, TEXT("⚡ SCALED EXTEND: Total %d holograms spawned across %d additional clone sets"),
        TotalHologramsSpawned, ScaledExtendClones.Num());
    
    // CRITICAL: Scrub nulls from parent's mChildren array.
    // SpawnChildHologramFromRecipe may add entries to mChildren before the spawn completes.
    // If spawning fails, null entries are left in mChildren which crash
    // AFGHologram::ResetConstructDisqualifiers() during tick.
    if (CurrentExtendHologram.IsValid())
    {
        AFGHologram* Parent = CurrentExtendHologram.Get();
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
                    UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND: Scrubbed %d null/invalid entries from parent mChildren"), NullsRemoved);
                }
            }
        }
    }
}

void USFExtendService::ClearScaledExtendClones()
{
    if (ScaledExtendClones.Num() == 0)
    {
        return;
    }
    
    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Clearing %d clone sets"), ScaledExtendClones.Num());
    
    // CRITICAL: Remove all scaled extend children from parent hologram's mChildren array
    // BEFORE destroying them. Without this, destroyed holograms leave dangling pointers
    // in mChildren, causing crash in AFGHologram::ResetConstructDisqualifiers during tick.
    if (CurrentExtendHologram.IsValid())
    {
        AFGHologram* Parent = CurrentExtendHologram.Get();
        if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
        {
            TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
            if (ChildrenArray)
            {
                // Collect all hologram pointers we're about to destroy
                TSet<AFGHologram*> ToRemove;
                for (const FSFScaledExtendClone& Clone : ScaledExtendClones)
                {
                    for (const auto& Pair : Clone.SpawnedHolograms)
                    {
                        if (Pair.Value)
                        {
                            ToRemove.Add(Pair.Value);
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
                    UE_LOG(LogSmartFoundations, Log, TEXT("⚡ SCALED EXTEND: Removed %d children from parent mChildren"), RemovedCount);
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
    for (FSFScaledExtendClone& Clone : ScaledExtendClones)
    {
        for (auto& Pair : Clone.SpawnedHolograms)
        {
            if (Pair.Value)
            {
                WeakHologramsToDestroy.Add(Pair.Value);
            }
        }
        Clone.SpawnedHolograms.Empty();
        Clone.CloneTopology.Reset();
    }
    ScaledExtendClones.Empty();
    
    // Destroy using weak pointers (safe against already-destroyed/GC'd objects)
    for (TWeakObjectPtr<AFGHologram>& WeakHolo : WeakHologramsToDestroy)
    {
        if (WeakHolo.IsValid())
        {
            AFGHologram* Holo = WeakHolo.Get();
            BeltPreviewHolograms.Remove(Holo);
            Holo->SetActorHiddenInGame(true);
            Holo->Destroy();
        }
    }
    
    bScaledExtendValid = true;
    ScaledExtendInvalidReason.Empty();
}

bool USFExtendService::ValidateScaledExtendConstraints()
{
    if (ScaledExtendClones.Num() == 0)
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
    // all adjacent pairs), we only need to validate StoredCloneTopology's lane segments
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
    
    if (StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
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
                ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane too short (%.1fm < %.1fm minimum)"),
                    TypeName, SegmentLength / 100.0f, MinLength / 100.0f);
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *ScaledExtendInvalidReason);
                return false;
            }
            
            if (SegmentLength > MaxLength)
            {
                ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane too long (%.1fm > %.0fm maximum)"),
                    TypeName, SegmentLength / 100.0f, MaxLength / 100.0f);
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *ScaledExtendInvalidReason);
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
                ScaledExtendInvalidReason = FString::Printf(
                    TEXT("%s lane angle too steep (%.0f°/%.0f° > %.0f° max)"),
                    TypeName, AngleStart, AngleEnd, MaxAngle);
                UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ SCALED EXTEND: INVALID - %s"), *ScaledExtendInvalidReason);
                return false;
            }
            
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ VALIDATE: %s lane OK — length %.1fm, angles %.0f°/%.0f°"),
                TypeName, SegmentLength / 100.0f, AngleStart, AngleEnd);
        }
    }
    
    ScaledExtendInvalidReason.Empty();
    return true;
}

bool USFExtendService::ValidatePowerCapacity()
{
    // Issue #288: Preview-time check that each cloned power pole can actually
    // host the connections we plan to make: factory (1) + inter-pole wire back
    // to source (1) + cloned pumps whose PowerInput was wired to this specific
    // source pole (N). Pumps connected to out-of-manifold poles are excluded
    // automatically because their ConnectedPowerPoleHologramId is empty.
    // Called from both regular-Extend and Scaled Extend preview paths so the
    // 1-clone case gets the same protection as 2+ clones.
    
    if (!StoredCloneTopology.IsValid() || StoredCloneTopology->ChildHolograms.Num() == 0)
    {
        return true;  // No topology → no poles → nothing to validate
    }
    
    // Tally pumps per clone pole HologramId in a single pass.
    TMap<FString, int32> PumpsPerClonePole;
    for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
    {
        if (Holo.Role == TEXT("pipe_attachment") && !Holo.ConnectedPowerPoleHologramId.IsEmpty())
        {
            PumpsPerClonePole.FindOrAdd(Holo.ConnectedPowerPoleHologramId) += 1;
        }
    }
    
    // Walk poles; first over-capacity entry aborts with a descriptive reason.
    for (const FSFCloneHologram& Holo : StoredCloneTopology->ChildHolograms)
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
            
            ScaledExtendInvalidReason = FString::Printf(
                TEXT("Clone %s needs %d/%d connections (factory + inter-pole + %d pump%s) — upgrade the source pole, or move a pump to another pole"),
                *Tier, Projected, Holo.PowerPoleMaxConnections, PumpCount, (PumpCount == 1 ? TEXT("") : TEXT("s")));
            UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ EXTEND POWER (#288): INVALID — %s"), *ScaledExtendInvalidReason);
            return false;
        }
    }
    
    return true;
}
