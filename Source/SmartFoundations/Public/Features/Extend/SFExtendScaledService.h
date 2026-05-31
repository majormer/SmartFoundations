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
#include "SFExtendScaledService.generated.h"

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
    TMap<FString, AFGHologram*> SpawnedHolograms;  // Clone ID -> hologram for this clone
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

private:
    /** Calculate world offsets for all clones based on current grid state */
    void CalculateScaledExtendPositions();

    /** Spawn preview holograms for all scaled extend clones */
    void SpawnScaledExtendPreviews();

    /** Validate belt/pipe constraints between consecutive clones */
    bool ValidateScaledExtendConstraints();

    /** Owning extend service (source of all shared scaled/wiring state; friended) */
    UPROPERTY()
    TObjectPtr<USFExtendService> Owner = nullptr;
};
