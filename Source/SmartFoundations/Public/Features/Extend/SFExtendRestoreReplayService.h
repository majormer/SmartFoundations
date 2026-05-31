// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendRestoreReplayService - Smart Restore Extend Replay
 *
 * Owns the "restored clone topology" replay path used by Smart Restore Enhanced:
 * re-spawning a saved Extend clone topology under a fresh parent hologram, tracking
 * the parent transform, and re-deriving scaled-grid clones each tick.
 *
 * Extracted from SFExtendService (T1 decomposition, 2026-05-30) as a careful move:
 * the restored-replay logic reads and writes state still owned by USFExtendService
 * (StoredCloneTopology, the Json* maps, ScaledExtendClones, HologramService), so this
 * service holds a friended back-pointer (Owner) and operates on that shared state in
 * place rather than migrating it. USFExtendService keeps thin forwarders so the
 * Subsystem/Restore call sites are unchanged. Behavior is identical to the pre-split
 * implementation.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFExtendRestoreReplayService.generated.h"

class AFGHologram;
class USFExtendService;
struct FSFCloneTopology;
struct FSFCounterState;

/** Result of placing one restored scaled clone relative to the parent hologram. */
struct FRestoredScaledClonePlacement
{
    FVector WorldOffset = FVector::ZeroVector;
    FRotator RotationOffset = FRotator::ZeroRotator;
};

/**
 * Compute the world offset + rotation for restored scaled clone (GridX, GridY) relative
 * to the parent hologram. Shared between the restore-replay path and the post-build
 * wiring path in SFExtendService (hence a free function, not a private helper).
 */
FRestoredScaledClonePlacement CalculateRestoredScaledClonePlacement(
    const AFGHologram* ParentHologram,
    const FSFCloneTopology* TemplateTopology,
    const FSFCounterState& State,
    int32 GridX,
    int32 GridY);

/**
 * Service implementing Smart Restore's Extend clone-topology replay. Holds a back-pointer
 * to the owning USFExtendService and operates on its shared replay/wiring state.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendRestoreReplayService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendRestoreReplayService();

    /** Initialize with owning extend service reference */
    void Initialize(USFExtendService* InExtendService);

    /** Shutdown service */
    void Shutdown();

    bool IsHologramCompatibleWithRestoredCloneTopology(AFGHologram* ParentHologram) const;
    void ClearRestoredCloneTopologySession(const TCHAR* Reason);
    bool ReplayRestoreCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology);
    void TickRestoredCloneTopology(float DeltaTime);
    void OnRestoredCloneTopologyStateChanged();

private:
    FSFCloneTopology BuildRestoredCloneTopologyForCurrentState(AFGHologram* ParentHologram) const;
    void ClearRestoredCloneTopologyPreview();
    int32 SpawnRestoredScaledFactoryHolograms(AFGHologram* ParentHologram, TMap<FString, AFGHologram*>& OutSpawnedHolograms);
    bool SpawnRestoredCloneTopology(AFGHologram* ParentHologram, const FSFCloneTopology& CloneTopology);

    /** Owning extend service (source of all shared replay/wiring state; friended) */
    UPROPERTY()
    TObjectPtr<USFExtendService> Owner = nullptr;
};
