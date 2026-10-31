// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "FGBuildableSubsystem.h"
#include "Features/Extend/SFExtendWiringPlan.h"
#include "Features/Extend/SFExtendScaledService.h"

/** Arbitration for one legacy wiring invocation, not evidence of native settlement.
 * Native IDs wrap and can be reused. The generated operation ID and world generation
 * distinguish invocations. Retain the incoming ID and the observed root ID separately:
 * native acknowledgement may populate fields before calling Construct.
 */
class FSFExtendWiringInvocation
{
public:
    FSFExtendWiringInvocation(const FGuid& InWorldGeneration, const FObjectKey& InRoot,
        const FNetConstructionID& InConstructionId, const FGuid& InOperationId = FGuid())
        : OperationId(InOperationId.IsValid() ? InOperationId : FGuid::NewGuid()),
          WorldGeneration(InWorldGeneration), Root(InRoot), RequestedConstructionId(InConstructionId) {}

    const FGuid& GetOperationId() const { return OperationId; }
    const FObjectKey& GetParentIdentity() const { return Parent; }
    const FNetConstructionID& GetRequestedConstructionId() const { return RequestedConstructionId; }

    bool BindParent(const FGuid& CurrentWorld, const FObjectKey& ConstructingRoot,
        const FNetConstructionID& NativeId, const FObjectKey& ReturnedParent)
    {
        if (!WorldGeneration.IsValid() || CurrentWorld != WorldGeneration || ConstructingRoot != Root
            || ReturnedParent == FObjectKey() || bInvalidated) return false;
        if (bParentBound && (Parent != ReturnedParent
            || NativeId.NetPlayerID != ObservedConstructionId.NetPlayerID
            || NativeId.Client_ID != ObservedConstructionId.Client_ID || NativeId.Server_ID != ObservedConstructionId.Server_ID))
        {
            bInvalidated = true; // contradictory root output must never select a different actor
            return false;
        }
        Parent = ReturnedParent;
        ObservedConstructionId = NativeId;
        bParentBound = true;
        return true;
    }

    bool TryClaim(const FGuid& CurrentOperation, const FGuid& CurrentWorld, const FObjectKey& ActualParent)
    {
        if (CurrentOperation != OperationId || CurrentWorld != WorldGeneration || !WorldGeneration.IsValid()
            || !bParentBound || bInvalidated || bInvoked || ActualParent != Parent) return false;
        bInvoked = true; // claim before calling either legacy writer, including reentrant callbacks
        return true;
    }

    // A void wiring return cannot establish correct graph, cargo, deferred work or payment.
    bool HasUnverifiedInvocation() const { return bInvoked; }

    // Mutable preview registries may be retired only by the invocation that
    // consumed that exact topology. A later preview has a different allocation,
    // even when it comes from the same saved module.
    bool CanRetirePreview(const TSharedPtr<FSFCloneTopology>& Current) const
    {
        return bInvoked && Current.IsValid() && PreviewTopology.Pin() == Current;
    }

    // Captured before native effects. The actor view is prepared from confirmed outputs,
    // never from a later preview or a world search after construction.
    TSharedPtr<FSFExtendWiringPlan> Plan;
    TWeakPtr<FSFCloneTopology> PreviewTopology;
    TArray<FSFScaledExtendClone> ScaledPlans;
    TMap<FString, FObjectKey> SourceBindings;
    TMap<FString, TWeakObjectPtr<UClass>> ExpectedClasses;
    TSet<FObjectKey> ActualOutputs;
    TMap<FString, TObjectPtr<AActor>> ActorView;
    bool bRequiresOwnedNativePhase = false;

private:
    const FGuid OperationId;
    const FGuid WorldGeneration;
    const FObjectKey Root;
    const FNetConstructionID RequestedConstructionId;
    FNetConstructionID ObservedConstructionId;
    FObjectKey Parent;
    bool bParentBound = false;
    bool bInvalidated = false;
    bool bInvoked = false;
};
