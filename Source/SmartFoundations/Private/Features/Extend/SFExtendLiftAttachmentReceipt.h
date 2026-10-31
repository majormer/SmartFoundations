// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Core/Construction/SFConstructionOperationTypes.h"
#include "Features/Extend/SFExtendCloneTopology.h"

class USFConstructionOperationCoordinator;
class AFGHologram;

/** Readback of constructed lift geometry and native floor-hole faces; no topology writes. */
struct FSFExtendLiftAttachmentReceipt
{
    /** Native recipe hologram replay, before physical construction/payment. */
    static bool ValidatePlacement(AFGHologram* Root, AFGHologram* Child, const FSFCloneHologram& Plan, FString& Reason);
    TSet<FObjectKey> Lifts;
    TSet<FObjectKey> Holes;

    bool Verify(const FSFCloneTopology& Plan, const TMap<FString, FObjectKey>& Actors,
        const TSet<FObjectKey>& ActualOutputs, USFConstructionOperationCoordinator& Owner,
        const FSFConstructionOperationKey& Key, FString& Reason);
};
