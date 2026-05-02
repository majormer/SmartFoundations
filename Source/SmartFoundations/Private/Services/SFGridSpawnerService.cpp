#include "Services/SFGridSpawnerService.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Subsystem/SFValidationService.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Hologram/FGCeilingLightHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Hologram/FGWallAttachmentHologram.h"
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "SmartFoundations.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"

namespace
{
    bool SuppressExtendOwnedGridOperation(USFSubsystem* Subsystem, const TCHAR* Context, bool bWarnOnSuppressedRegen)
    {
        if (!Subsystem || !Subsystem->ShouldSuppressNormalGridChildren())
        {
            return false;
        }

        const bool bRestoredExtendActive = Subsystem->IsRestoredExtendModeActive();
        if (bWarnOnSuppressedRegen && bRestoredExtendActive)
        {
            UE_LOG(LogSmartFoundations, Warning,
                TEXT("[SmartRestore][Extend] WARNING normal grid regeneration while restored topology active: context=%s parent=%s"),
                Context,
                *GetNameSafe(Subsystem->GetActiveHologram()));
        }
        else
        {
            UE_LOG(LogSmartFoundations, Log,
                TEXT("[SmartRestore][Extend] Suppressing normal Smart grid spawn/update: context=%s parent=%s liveExtend=%d restoredExtend=%d"),
                Context,
                *GetNameSafe(Subsystem->GetActiveHologram()),
                Subsystem->IsExtendModeActive() ? 1 : 0,
                bRestoredExtendActive ? 1 : 0);
        }

        Subsystem->ClearNormalGridChildrenForExtendSuppression(Context);
        return true;
    }
}

void USFGridSpawnerService::Initialize(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
}

void USFGridSpawnerService::RegenerateChildHologramGrid()
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS) return;

    if (SuppressExtendOwnedGridOperation(SS, TEXT("RegenerateChildHologramGrid"), true))
    {
        return;
    }

    if (AFGHologram* ParentHologram = SS->GetActiveHologram())
    {
        if (FSFHologramHelperService* HologramHelper = SS->GetHologramHelper())
        {
            auto UpdateCallback = [SS]()
            {
                if (USFGridSpawnerService* SpawnerLocal = SS->GetGridSpawnerService())
                {
                    SpawnerLocal->UpdateChildPositions();
                }
            };

            // NOTE: ValidationService/Adapter/Controller accessed via SS accessors or members
            FIntVector& GridRef = SS->AccessGridCountersRef();
            float& BaselineRef = SS->AccessBaselineHeightZRef();
            HologramHelper->RegenerateChildHologramGrid(
                ParentHologram,
                GridRef,                      // non-const ref (may be adjusted)
                SS->GetValidationService(),
                SS->GetCurrentAdapter(),
                SS->GetLastController(),
                BaselineRef,                  // non-const ref (may be adjusted)
                UpdateCallback
            );

            // Sync adjusted grid back to CounterState (authoritative)
            FSFCounterState NewState = SS->GetCounterState();
            NewState.GridCounters = SS->GetGridCounters();
            SS->UpdateCounterState(NewState);

            // CRITICAL: Update positions AFTER grid sync so CounterState has correct dimensions
            UpdateChildPositions();

            // NOTE: Orchestrator evaluation is now triggered in the progressive batch CompletionCallback
            // to ensure it runs strictly after all children are positioned.
        }
    }
}

void USFGridSpawnerService::QueueChildForDestroy(AFGHologram* Child)
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS || !Child) return;

    if (FSFHologramHelperService* Helper = SS->GetHologramHelper())
    {
        // Detach subsystem delegate if bound to prevent re-entrancy
        if (Child->OnDestroyed.IsAlreadyBound(SS, &USFSubsystem::OnChildHologramDestroyed))
        {
            Child->OnDestroyed.RemoveDynamic(SS, &USFSubsystem::OnChildHologramDestroyed);
        }
        Helper->QueueChildForDestroy(Child);
    }
}

void USFGridSpawnerService::FlushPendingDestroy()
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS) return;

    if (FSFHologramHelperService* Helper = SS->GetHologramHelper())
    {
        Helper->FlushPendingDestroy();
    }
}

bool USFGridSpawnerService::CanSafelyDestroyChildren() const
{
    if (!Subsystem.IsValid()) return true;
    const USFSubsystem* SS = Subsystem.Get();
    if (!SS) return true;
    if (const FSFHologramHelperService* Helper = SS->GetHologramHelper())
    {
        return Helper->CanSafelyDestroyChildren(SS->GetActiveHologram());
    }
    return !SS->GetActiveHologram();
}

void USFGridSpawnerService::ForceDestroyPendingChildren()
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS) return;

    if (FSFHologramHelperService* Helper = SS->GetHologramHelper())
    {
        Helper->ForceDestroyPendingChildren();
    }
}

void USFGridSpawnerService::UpdateChildPositions()
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS) return;

    if (SuppressExtendOwnedGridOperation(SS, TEXT("UpdateChildPositions"), false))
    {
        return;
    }

    // Phase 2: Get children from HologramHelper (now owns SpawnedChildren tracking)
    TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren;
    if (FSFHologramHelperService* HologramHelper = SS->GetHologramHelper())
    {
        SpawnedChildren = HologramHelper->GetSpawnedChildren();
    }

    if (!SS->GetActiveHologram())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdateChildPositions: Early return - No active hologram"));
        return;
    }

    // CRITICAL: Even if there are no children, we must trigger the orchestrator to clean up orphaned previews
    if (SpawnedChildren.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdateChildPositions: No children - triggering orchestrator for cleanup"));

        if (AFGHologram* Parent = SS->GetActiveHologram())
        {
            if (USFAutoConnectOrchestrator* Orchestrator = SS->GetOrCreateOrchestrator(Parent))
            {
                // Phase 4: Type-safe grid cleanup (Belts vs Pipes vs Power vs Support Structures)
                if (USFAutoConnectService::IsDistributorHologram(Parent))
                {
                    Orchestrator->OnGridChanged();
                }
                if (USFAutoConnectService::IsPipelineJunctionHologram(Parent))
                {
                    Orchestrator->OnPipeGridChanged();
                }
                if (USFAutoConnectService::IsPowerPoleHologram(Parent))
                {
                    Orchestrator->OnPowerPolesMoved();  // Don't force recreate for cleanup
                }
                // Issue #220: Support structure auto-connect (stackable poles)
                if (USFAutoConnectService::IsBeltSupportHologram(Parent))
                {
                    Orchestrator->OnStackableConveyorPolesChanged();
                }
                if (USFAutoConnectService::IsStackablePipelineSupportHologram(Parent))
                {
                    Orchestrator->OnStackablePipelineSupportsChanged();
                }
                // Issue #187: Floor hole pipe auto-connect
                if (USFAutoConnectService::IsPassthroughPipeHologram(Parent))
                {
                    Orchestrator->OnFloorHolePipesChanged();
                }
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 Orchestrator: Triggered cleanup for valid types (no children)"));
            }
        }
        return;
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdateChildPositions: Called with %d children"), SpawnedChildren.Num());

    // Drop any invalid or disabled children before iterating
    int32 OriginalCount = SpawnedChildren.Num();
    int32 DisabledCount = 0;
    int32 InvalidCount = 0;

    if (FSFHologramHelperService* HologramHelper = SS->GetHologramHelper())
    {
        SpawnedChildren.RemoveAll([&DisabledCount, &InvalidCount](const TWeakObjectPtr<AFGHologram>& Child)
        {
            if (!Child.IsValid())
            {
                InvalidCount++;
                return true;  // Remove null/invalid children
            }

            if (Child->IsDisabled())
            {
                DisabledCount++;
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Filtering disabled child: %s"), *Child->GetName());
                return true;
            }

            return false;
        });
    }

    if (DisabledCount > 0 || InvalidCount > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Filtered SpawnedChildren: %d total → %d active (removed %d disabled, %d invalid)"),
            OriginalCount, SpawnedChildren.Num(), DisabledCount, InvalidCount);
    }

    if (SpawnedChildren.Num() == 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   No active children to position after filtering"));
        return;
    }

    AFGHologram* ParentHologram = SS->GetActiveHologram();
    FVector ParentLocation = ParentHologram->GetActorLocation();
    FRotator ParentRotation = ParentHologram->GetActorRotation();
    FVector ParentNudgeOffset = ParentHologram->GetHologramNudgeOffset();

    // DEBUG: Log parent state including nudge offset
    FVector ParentAnchorOffset = SS->GetCachedAnchorOffset();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔍 PARENT STATE: Location=%s, NudgeOffset=%s, AnchorOffset=%s"),
        *ParentLocation.ToString(), *ParentNudgeOffset.ToString(), *ParentAnchorOffset.ToString());

    // Use cached building size
    FVector ItemSize = SS->GetCachedBuildingSize();

    if (!ItemSize.Equals(SS->GetLastLoggedItemSize(), 0.5f))
    {
        SS->SetLastLoggedItemSize(ItemSize);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ITEM SIZE USED FOR SPACING: %s (CachedBuildingSize=%s)"),
            *ItemSize.ToString(), *SS->GetCachedBuildingSize().ToString());
    }

    // Calculate grid dimensions from authoritative CounterState
    const FIntVector& GridCounters = SS->GetCounterState().GridCounters;
    int32 XCount = FMath::Abs(GridCounters.X);
    int32 YCount = FMath::Abs(GridCounters.Y);
    int32 ZCount = FMath::Abs(GridCounters.Z);

    // Calculate grid directions
    int32 XDir = GridCounters.X >= 0 ? 1 : -1;
    int32 YDir = GridCounters.Y >= 0 ? 1 : -1;
    int32 ZDir = GridCounters.Z >= 0 ? 1 : -1;

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Grid dimensions: %dx%dx%d, directions: [%d,%d,%d]"),
        XCount, YCount, ZCount, XDir, YDir, ZDir);

    // Pre-compute grid indices for progressive batching
    TArray<FSFHologramHelperService::FGridIndex> GridIndices;
    GridIndices.Reserve(SpawnedChildren.Num());

    int32 ChildIndex = 0;
    for (int32 Z = 0; Z < ZCount; ++Z)
    {
        for (int32 X = 0; X < XCount; ++X)
        {
            for (int32 Y = 0; Y < YCount; ++Y)
            {
                if (X == 0 && Y == 0 && Z == 0)
                {
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Skipping parent position [0,0,0]"));
                    continue;
                }

                if (ChildIndex >= SpawnedChildren.Num())
                {
                    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Breaking: ChildIndex %d >= SpawnedChildren %d"),
                        ChildIndex, SpawnedChildren.Num());
                    break;
                }

                FSFHologramHelperService::FGridIndex GridIndex;
                GridIndex.X = X * XDir;
                GridIndex.Y = Y * YDir;
                GridIndex.Z = Z * ZDir;
                GridIndex.ChildArrayIndex = ChildIndex;
                GridIndices.Add(GridIndex);

                ChildIndex++;
            }
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Pre-computed %d grid indices for progressive batch"), GridIndices.Num());

    // Create update callback (called once per child during batch processing)
    auto UpdateCallback = [SS, SpawnedChildren, ParentHologram, ParentLocation, ParentRotation, ItemSize](int32 IndexInBatch) -> void
    {
        FSFHologramHelperService* HologramHelper = SS->GetHologramHelper();
        if (!HologramHelper) return;

        const FSFHologramHelperService::FGridIndex& GridIndex = HologramHelper->GetBatchGridIndex(IndexInBatch);
        const TWeakObjectPtr<AFGHologram>& Child = SpawnedChildren[GridIndex.ChildArrayIndex];

        if (!Child.IsValid())
        {
            return;
        }

        AFGHologram* ChildHologram = Child.Get();
        if (!IsValid(ChildHologram))
        {
            return;
        }

        if (ASFConveyorBeltHologram* BeltChild = Cast<ASFConveyorBeltHologram>(ChildHologram))
        {
            UE_LOG(LogSmartFoundations, Warning,
                TEXT("GRID POSITIONER touching belt child %s (parent=%s) grid=[%d,%d,%d] loc=%s"),
                *BeltChild->GetName(),
                *GetNameSafe(ParentHologram),
                GridIndex.X, GridIndex.Y, GridIndex.Z,
                *BeltChild->GetActorLocation().ToString());
        }

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Positioning child %d at grid [%d,%d,%d]"),
            GridIndex.ChildArrayIndex, GridIndex.X, GridIndex.Y, GridIndex.Z);

        // CRITICAL FIX: DO NOT pass AnchorOffset to CalculateChildPosition
        //
        // DISCOVERY: CalculateChildPosition adds AnchorOffset to the returned position,
        // but SetHologramLocationAndRotation() ALSO applies AnchorOffset compensation.
        // Passing AnchorOffset to both causes double-compensation.
        //
        // EXAMPLE: Splitter with AnchorOffset.Z = -100cm
        // - CalculateChildPosition with AnchorOffset: returns Z=8699.990 + (-100) = 8599.990
        // - SetHologramLocationAndRotation adds AnchorOffset: 8599.990 + (-100) = 8499.990
        // - Result: 200cm too low (double application)
        //
        // SOLUTION: Pass FVector::ZeroVector to CalculateChildPosition and let
        // SetHologramLocationAndRotation handle the AnchorOffset compensation.
        // We then counteract the API's behavior in the AdjustedPosition calculation above.

        // Calculate position using PositionCalculator (NO AnchorOffset - handled by hologram API)
        FVector ChildPosition = ParentLocation;
        if (FSFPositionCalculator* PosCalc = SS->GetPositionCalculator())
        {
            ChildPosition = PosCalc->CalculateChildPosition(
                GridIndex.X,
                GridIndex.Y,
                GridIndex.Z,
                ParentLocation,
                ParentRotation,
                ItemSize,
                SS->GetCounterState(),
                GridIndex.ChildArrayIndex,
                FVector::ZeroVector  // ZERO - Prevent double-compensation with SetHologramLocationAndRotation
            );
        }

        // === COMPREHENSIVE DIAGNOSTIC LOGGING ===
        FVector OldPosition = ChildHologram->GetActorLocation();
        const bool bParentWasLocked = ParentHologram->IsHologramLocked();
        const bool bChildWasLocked = ChildHologram->IsHologramLocked();

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ╔═══ CHILD %d POSITIONING START ═══"), GridIndex.ChildArrayIndex);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ OldPos: %s"), *OldPosition.ToString());
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ CalcPos: %s (from PositionCalculator)"), *ChildPosition.ToString());
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ParentLocked: %d  ChildLocked: %d"), bParentWasLocked ? 1 : 0, bChildWasLocked ? 1 : 0);

        // Get AnchorOffset for diagnostics
        FVector ChildAnchorOffset = FVector::ZeroVector;
        if (AFGBuildableHologram* BuildableChild = Cast<AFGBuildableHologram>(ChildHologram))
        {
            UClass* ChildBuildClass = BuildableChild->GetBuildClass();
            if (ChildBuildClass)
            {
                FSFBuildableSizeProfile ChildProfile = USFBuildableSizeRegistry::GetProfile(ChildBuildClass);
                ChildAnchorOffset = ChildProfile.AnchorOffset;
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ChildAnchorOffset: %s (from registry)"), *ChildAnchorOffset.ToString());
            }
        }

        // Delegate lock management to HologramHelperService
        HologramHelper->TemporarilyUnlockChild(ChildHologram, bParentWasLocked);
        const bool bChildUnlockedByHelper = !ChildHologram->IsHologramLocked() && bChildWasLocked;
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ChildUnlockedByHelper: %d"), bChildUnlockedByHelper ? 1 : 0);

        // Issue #171: Use SetActorLocation for ALL children instead of SetHologramLocationAndRotation.
        //
        // SetHologramLocationAndRotation does internal floor tracing/snapping which causes
        // children over foundations to snap to the foundation surface (different Z) while
        // children beyond the foundation edge stay at the calculated Z. This creates
        // inconsistent heights across the grid — some children 100cm higher than others.
        //
        // By using SetActorLocation directly, children are placed at the exact grid-calculated
        // position relative to the parent. No AnchorOffset compensation is needed since
        // SetActorLocation doesn't apply AnchorOffset (the old compensation was specifically
        // to counteract SetHologramLocationAndRotation's implicit AnchorOffset addition).
        ChildHologram->SetActorLocation(ChildPosition);

        // Calculate child rotation - includes arc rotation if radial transform is active
        FRotator ChildRotation = ParentRotation;
        const FSFCounterState& Counter = SS->GetCounterState();
        if (!FMath::IsNearlyZero(Counter.RotationZ))
        {
            // ROTATION MODE: Each child rotates progressively along the arc
            // Each building rotates to face tangent to the arc (same direction as arc curves)
            // The rotation matches the arc direction - positive RotationZ = clockwise arc = clockwise building rotation
            float ChildYawOffset = GridIndex.X * Counter.RotationZ;
            ChildRotation.Yaw += ChildYawOffset;

            UE_LOG(LogSmartFoundations, Verbose,
                TEXT("🔄 Spawn Child[%d] X=%d: ParentYaw=%.1f° + Offset=%.1f° = FinalYaw=%.1f°"),
                GridIndex.ChildArrayIndex, GridIndex.X, ParentRotation.Yaw, ChildYawOffset, ChildRotation.Yaw);
        }
        ChildHologram->SetActorRotation(ChildRotation);

        // Check position AFTER API call
        FVector NewPosition = ChildHologram->GetActorLocation();
        float DeltaZ = NewPosition.Z - OldPosition.Z;
        float OffsetFromCalc = NewPosition.Z - ChildPosition.Z;

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ NewPos: %s (after SetHologramLocationAndRotation)"), *NewPosition.ToString());
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ Delta Z: %.1f cm (NewPos - OldPos)"), DeltaZ);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ Offset from CalcPos: %.1f cm (NewPos - CalcPos)"), OffsetFromCalc);

        // Check if offset matches AnchorOffset
        if (FMath::Abs(OffsetFromCalc - ChildAnchorOffset.Z) < 1.0f)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ⚠️ MATCHES AnchorOffset.Z! Engine applied offset."));
        }
        else if (FMath::Abs(OffsetFromCalc) < 1.0f)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ✅ NO OFFSET - Position matches CalcPos exactly."));
        }
        else
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ║ ❓ UNEXPECTED OFFSET - Doesn't match AnchorOffset or zero."));
        }

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ╚═══════════════════════════════════"));

        // Delegate floor validation to ValidationService
        if (AFGBuildableHologram* BuildableChild = Cast<AFGBuildableHologram>(ChildHologram))
        {
            if (FSFValidationService* Validation = SS->GetValidationService())
            {
                const auto& CurrentCounter = SS->GetCounterState();
                const bool bStepsActive = (FMath::Abs(CurrentCounter.StepsX) > 0.1f || FMath::Abs(CurrentCounter.StepsY) > 0.1f);
                const bool bNeedsFloorValidation = Validation->ShouldEnableFloorValidation(
                    ParentHologram,
                    GridIndex.Z,
                    bStepsActive
                );

                BuildableChild->SetNeedsValidFloor(bNeedsFloorValidation);
            }
        }

        // Issue #200: Ceiling lights and wall floodlights have CheckValidPlacement() overrides
        // that check for ceiling/wall snapping. Children can't satisfy these checks since they're
        // positioned by Smart! via SetActorLocation. Disable tick to prevent CheckValidPlacement
        // from adding disqualifiers, and force valid material state.
        const bool bNeedsPlacementBypass = ParentHologram->IsA(AFGCeilingLightHologram::StaticClass())
            || ParentHologram->IsA(AFGFloodlightHologram::StaticClass())
            || ParentHologram->IsA(AFGWallAttachmentHologram::StaticClass());
        if (bNeedsPlacementBypass)
        {
            ChildHologram->SetActorTickEnabled(false);
            ChildHologram->ResetConstructDisqualifiers();
            ChildHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
        }

        // Ensure child visibility and material state match parent
        ChildHologram->SetActorHiddenInGame(false);
        // Issue #197: Water pump children run their own CheckValidPlacement() to validate
        // water position. Don't override their material state — let validation determine it.
        const bool bHasOwnValidation = ChildHologram->IsA(ASFWaterPumpChildHologram::StaticClass());
        if (!bNeedsPlacementBypass && !bHasOwnValidation)
        {
            ChildHologram->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
        }

        // Restore lock state via HologramHelperService
        HologramHelper->RestoreChildLock(ChildHologram, bParentWasLocked, SS->IsSuppressChildUpdates());

        // Special logging for water extractors
        if (ChildHologram->IsA(ASFWaterPumpChildHologram::StaticClass()))
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("  [WATER EXTRACTOR] Child %s moved from %s to %s (Grid[%d,%d,%d])"),
                *ChildHologram->GetName(), *OldPosition.ToString(), *ChildPosition.ToString(),
                GridIndex.X, GridIndex.Y, GridIndex.Z);
        }
    };

    // Completion callback
    auto CompletionCallback = [SS]() -> void
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Progressive batch reposition complete"));

        if (AFGHologram* Parent = SS->GetActiveHologram())
        {
            if (AFGPlayerController* PC = SS->GetLastController())
            {
                if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn()))
                {
                    if (UFGInventoryComponent* Inventory = Character->GetInventory())
                    {
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Validating parent after batch reposition"));
                        Parent->ValidatePlacementAndCost(Inventory);
                    }
                }
            }

            // Now that children are definitively positioned, trigger debounced evaluation via public API
            if (USFAutoConnectOrchestrator* Orchestrator = SS->GetOrCreateOrchestrator(Parent))
            {
                // Phase 4: Type-safe grid updates
                if (USFAutoConnectService::IsDistributorHologram(Parent))
                {
                    Orchestrator->OnGridChanged();
                }
                if (USFAutoConnectService::IsPipelineJunctionHologram(Parent))
                {
                    Orchestrator->OnPipeGridChanged();
                }
                if (USFAutoConnectService::IsPowerPoleHologram(Parent))
                {
                    Orchestrator->OnPowerPolesMoved();  // Don't force recreate for position updates
                }
                // Issue #220: Support structure auto-connect (stackable poles)
                if (USFAutoConnectService::IsBeltSupportHologram(Parent))
                {
                    Orchestrator->OnStackableConveyorPolesChanged();
                }
                if (USFAutoConnectService::IsStackablePipelineSupportHologram(Parent))
                {
                    Orchestrator->OnStackablePipelineSupportsChanged();
                }
                // Issue #187: Floor hole pipe auto-connect
                if (USFAutoConnectService::IsPassthroughPipeHologram(Parent))
                {
                    Orchestrator->OnFloorHolePipesChanged();
                }

                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 Orchestrator: Grid updates triggered in CompletionCallback"));
            }
        }

#if SMART_ARROWS_ENABLED
        if (SS->IsArrowsRuntimeVisible() && SS->GetCurrentAdapter() && SS->GetCurrentAdapter()->IsValid())
        {
            if (UWorld* World = SS->GetWorld())
            {
                if (AFGHologram* Parent = SS->GetActiveHologram())
                {
                    FTransform CurrentTransform = SS->GetCurrentAdapter()->GetBaseTransform();
                    if (FSFArrowModule_StaticMesh* ArrowModule = SS->GetArrowModule())
                    {
                        ArrowModule->UpdateArrows(World, CurrentTransform, SS->GetLastAxisInput(), true);
                        UE_LOG(LogSmartFoundations, Verbose, TEXT("   Arrows updated after child positioning"));
                    }
                }
            }
        }
#endif
    };

    if (FSFHologramHelperService* HologramHelper = SS->GetHologramHelper())
    {
        HologramHelper->BeginProgressiveBatchReposition(
            GridIndices,
            UpdateCallback,
            CompletionCallback,
            ParentHologram
        );
    }
    else
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("HologramHelper not available for progressive batch!"));
    }
}

void USFGridSpawnerService::UpdateChildrenForCurrentTransform()
{
    if (!Subsystem.IsValid()) return;
    USFSubsystem* SS = Subsystem.Get();
    if (!SS) return;

    if (SuppressExtendOwnedGridOperation(SS, TEXT("UpdateChildrenForCurrentTransform"), false))
    {
        return;
    }

    if (!SS->GetActiveHologram() || !SS->GetHologramHelper())
    {
        return;
    }

    const FTransform NewTransform = SS->GetActiveHologram()->GetActorTransform();

    auto UpdateCallback = [SS]()
    {
        if (USFGridSpawnerService* Spawner = SS->GetGridSpawnerService())
        {
            Spawner->UpdateChildPositions();
        }
    };

    auto ValidateCallback = [SS]()
    {
        if (AFGPlayerController* PC = SS->GetLastController())
        {
            if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn()))
            {
                if (UFGInventoryComponent* Inventory = Character->GetInventory())
                {
                    if (AFGHologram* Parent = SS->GetActiveHologram())
                    {
                        Parent->ValidatePlacementAndCost(Inventory);
                    }
                }
            }
        }
    };

    // Reuse progressive batch reposition pattern after transform change
    if (AFGHologram* Parent = SS->GetActiveHologram())
    {
        // Kick child positions update; validation is executed in completion callback
        UpdateCallback();
        ValidateCallback();

        // Schedule auto-connect evaluation on next tick to coalesce any rapid updates
        if (UWorld* World = SS->GetWorld())
        {
            TWeakObjectPtr<USFSubsystem> WeakSS = SS;
            FTimerDelegate D;
            D.BindLambda([WeakSS]()
            {
                if (!WeakSS.IsValid()) return;
                if (AFGHologram* ParentLater = WeakSS->GetActiveHologram())
                {
                    if (USFAutoConnectOrchestrator* Orchestrator = WeakSS->GetOrCreateOrchestrator(ParentLater))
                    {
                        // Phase 4: Type-safe movement updates
                        if (USFAutoConnectService::IsDistributorHologram(ParentLater))
                        {
                            Orchestrator->OnDistributorsMoved();
                        }
                        if (USFAutoConnectService::IsPipelineJunctionHologram(ParentLater))
                        {
                            Orchestrator->OnPipeJunctionsMoved();
                        }
                        if (USFAutoConnectService::IsPowerPoleHologram(ParentLater))
                        {
                            Orchestrator->OnPowerPolesMoved();
                        }
                        // Issue #220: Support structure auto-connect (stackable poles)
                        if (USFAutoConnectService::IsBeltSupportHologram(ParentLater))
                        {
                            Orchestrator->OnStackableConveyorPolesChanged();
                        }
                        if (USFAutoConnectService::IsStackablePipelineSupportHologram(ParentLater))
                        {
                            Orchestrator->OnStackablePipelineSupportsChanged();
                        }
                        // Issue #187: Floor hole pipe auto-connect
                        if (USFAutoConnectService::IsPassthroughPipeHologram(ParentLater))
                        {
                            Orchestrator->OnFloorHolePipesChanged();
                        }

                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 Orchestrator: Movement updates triggered (next tick)"));
                    }
                }
            });
            World->GetTimerManager().SetTimerForNextTick(D);
        }
    }
}


