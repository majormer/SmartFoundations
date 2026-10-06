// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class UFGCircuitConnectionComponent;
class ASFWireHologram;
class AFGBuildableWire;
struct FSFCloneTopology;
struct FSFCloneHologram;

namespace SFExtendPowerConnections
{
    // Empty names are legacy captures: a single port is unambiguous; two are not.
    UFGCircuitConnectionComponent* Resolve(AActor* Actor, const FString& ComponentName);
    float MaxWireLength(const AFGBuildableWire* Wire,
        UFGCircuitConnectionComponent* From, UFGCircuitConnectionComponent* To);
    bool Connect(UWorld* World, UClass* WireClass,
        UFGCircuitConnectionComponent* From, UFGCircuitConnectionComponent* To);
    const FSFCloneHologram* Find(const FSFCloneTopology* Topology, const FString& Id);
    void RefreshPreview(ASFWireHologram* Wire, const FSFCloneHologram& Plan);
    bool ValidateCapacity(const FSFCloneTopology& Topology, FString& OutReason);
    // Per-socket slot bookkeeping shared across the cell topologies of one Extend plan.
    struct FSocketBudget
    {
        TMap<FString, int32> Used;
        TMap<FString, int32> Limits;
    };
    // Reserves every cable of an already-accepted topology (e.g. retained or base cells).
    void ReserveAll(const FSFCloneTopology& Topology, FSocketBudget& Budget);
    // Factory daisy links are optional: reserve this topology's other cables first, then keep
    // each daisy link (in plan order) only while both of its sockets still have a free slot.
    // Returns the number of daisy links removed from the topology.
    int32 PruneDaisyOverCapacity(FSFCloneTopology& Topology, FSocketBudget& Budget);
    void RemapCellTargets(FSFCloneHologram& Wire, const FString& CellPrefix, const FString& PreviousPrefix);
    void AddRestoredChainWires(FSFCloneTopology& Topology);
    // Upgrade saved cable intent before expansion/quoting; never invent an unpriced edge.
    bool PromoteLegacyRestoreWires(FSFCloneTopology& Topology);
}
