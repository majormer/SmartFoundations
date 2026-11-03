// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Core/Construction/SFConstructionOperationTypes.h"
#include "Core/Construction/SFConveyorCohort.h"

class UFGFactoryConnectionComponent;
struct FSFConveyorConnectionPair
{
    UFGFactoryConnectionComponent* A = nullptr;
    UFGFactoryConnectionComponent* B = nullptr;
};

/** Shared ordinary-cohort adapter. Caller owns effect claims and terminal observation. */
class FSFConveyorConnectionMutation
{
public:
    /** Links only already materialized actors. Native parent retains construction/payment ownership. */
    static ESFConstructionAdmission Queue(USFConstructionOperationCoordinator& Owner, APlayerController* Requester,
        uint64 Sequence, const FGuid& RequestId, const TArray<FSFConveyorConnectionPair>& Edges);
    static bool Apply(USFConstructionOperationCoordinator& Owner, const FSFConstructionOperationKey& Key,
        const TArray<TWeakObjectPtr<AFGBuildableConveyorBase>>& Seeds,
        const TArray<FSFConveyorConnectionPair>& Edges, FSFConveyorCohort& After,
        FSFConveyorCohort::FCargoSnapshot& AfterCargo, bool& bEffectsStarted, FString& Reason);
};
