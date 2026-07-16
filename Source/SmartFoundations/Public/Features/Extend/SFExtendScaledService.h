// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendScaledService - Scaled Extend (Issue #265)
 *
 * Owns the Scaled Extend planning + preview path: when the player scales the Extend grid
 * (X clones / Y rows / spacing / steps / rotation), this service computes per-clone world
 * offsets, spawns the preview child holograms for every clone set, and validates belt/pipe
 * and power-pole constraints between consecutive clones.
 *
 * Extracted from SFExtendService (T1 slice E1, 2026-05-30) as a careful move: the scaled
 * logic reads and writes state still owned by USFExtendService (ScaledExtendClones,
 * StoredCloneTopology, the Json* maps, HologramService, the active target/hologram), so this
 * service holds a friended back-pointer (Owner) and operates on that shared state in place
 * rather than migrating it. USFExtendService keeps thin forwarders (OnScaledExtendStateChanged
 * / ClearScaledExtendClones / ValidatePowerCapacity) so the Subsystem + internal call sites are
 * unchanged. Behavior is identical to the pre-split implementation.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HUD/SFHUDTypes.h"   // FSFCounterState (#497: cached rebuild inputs for the count-only diff)
#include "SFExtendScaledService.generated.h"

class AFGBuildable;
class AFGHologram;
class ASFConveyorBeltHologram;
class ASFPipelineHologram;
class USFExtendService;
class USFSubsystem;
struct FSFCloneTopology;

/**
 * One clone set for Scaled Extend. Index 0 = first clone (adjacent to source), Index N = Nth
 * clone in chain. For 2D grids, includes auto-seed clones. (Relocated from a nested struct of
 * USFExtendService in slice E1 so both the scaled service and the post-build wiring path in
 * SFExtendService can name it.)
 */
struct FSFScaledExtendClone
{
    int32 GridX = 0;  // Grid position (0-based, 0 = first clone)
    int32 GridY = 0;  // Row index (0 = source row)
    bool bIsSeed = false;  // Auto-seed clone at (0, Y>0)
    FVector WorldOffset = FVector::ZeroVector;  // Offset from source building
    FRotator RotationOffset = FRotator::ZeroRotator;  // Rotation relative to source
    // GC-safe: weak ptrs never dangle. Raw AFGHologram* here are invisible to GC (plain struct,
    // non-UPROPERTY ScaledExtendClones), so engine/GC destruction of preview holograms would leave
    // dangling raws -> crash in ClearScaledExtendClones (EXCEPTION_ACCESS_VIOLATION). See #crash.
    TMap<FString, TWeakObjectPtr<AFGHologram>> SpawnedHolograms;  // Clone ID -> hologram for this clone
    TSharedPtr<FSFCloneTopology> CloneTopology;  // Clone topology for this set
};

/**
 * Service implementing Scaled Extend planning + preview. Holds a back-pointer to the owning
 * USFExtendService and operates on its shared scaled/wiring state.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendScaledService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendScaledService();

    /** Initialize with owning extend service reference */
    void Initialize(USFExtendService* InExtendService);

    /** Shutdown service */
    void Shutdown();

    // Forwarded from USFExtendService (called by Subsystem / internal stay-units):
    void OnScaledExtendStateChanged();
    void ClearScaledExtendClones();
    bool ValidatePowerCapacity();

    /** [EXTEND-MP] Server-commit entry: run the SAME spawn pipeline the SP preview uses against
     *  the already-installed session state (CurrentExtendTarget/CurrentExtendHologram/topology +
     *  ScaledExtendClones parameters). Used by ReconstructScaledCommitOnServer. */
    void SpawnCloneSetsForServerCommit() { SpawnScaledExtendPreviews(); }

private:
    /** Calculate world offsets for all clones based on current grid state */
    void CalculateScaledExtendPositions();

    /** Spawn preview holograms for all scaled extend clones */
    void SpawnScaledExtendPreviews();

    /** [#383/#384 perf] The actual heavy rebuild (clear + reposition + re-route every clone's
     *  belts/pipes). OnScaledExtendStateChanged debounces calls to this so a fast rotation/spacing
     *  scroll - which fired a full 22-clone re-derive + curved-pipe re-route on EVERY scroll tick
     *  (~15/sec) - coalesces: the leading change rebuilds immediately (slow deliberate adjustments
     *  stay instant), and a burst collapses to one trailing rebuild ~90ms after it settles. */
    void RebuildScaledExtendNow();

    /** Trailing-edge debounce timer for RebuildScaledExtendNow (see OnScaledExtendStateChanged). */
    FTimerHandle ScaledRebuildTimerHandle;

    /** Wall-clock of the last rebuild, for the leading-edge gate. */
    double LastScaledRebuildTime = 0.0;

    /** Validate belt/pipe constraints between consecutive clones */
    bool ValidateScaledExtendConstraints();

    /** #497 clone reuse: true when only the GridCounters MAGNITUDES changed since the last rebuild —
     *  same signs (direction), same spacing/steps/stagger/rotation. Existing clones' positions are
     *  index-keyed and unaffected by the count, so the rebuild can diff instead of destroy-all. */
    bool IsCountOnlyChange(const FSFCounterState& NowState) const;

    /** #497: destroy the spawned preview holograms of the given clones (mChildren unlink first, then
     *  weak-ptr destroy, then BeltPreviewHolograms removal) WITHOUT touching array membership.
     *  Shared by the full clear and the incremental shrink path. */
    void DestroyClonePreviewHolograms(TArray<FSFScaledExtendClone>& Clones);

    /** #497: rebuild Owner->StoredCloneTopology = fresh copy of the clone-1 base + every clone's
     *  topology appended. Non-destructive (the old Phase-6 merge mutated the shared base in place,
     *  which only worked because the full rebuild recreated it each time). */
    void RemergeScaledTopologyFromBase();

    /** #497: geometry inputs of the last completed rebuild, for the count-only diff. */
    FSFCounterState LastRebuildCounterState;
    bool bHasLastRebuildState = false;
    TWeakObjectPtr<AFGBuildable> LastRebuildTarget;

    /** Owning extend service (source of all shared scaled/wiring state; friended) */
    UPROPERTY()
    TObjectPtr<USFExtendService> Owner = nullptr;
};
