// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Features/Upgrade/SFConveyorUpgradeOwner.h"

class AFGHologram;
class USFConstructionOperationCoordinator;

/** Exact native companion selection, retained relationships, full-item refunds
 * and returned outputs. No proximity discovery or synthetic child generation. */
namespace SFConveyorUpgradeCompanions
{
    bool Capture(AFGBuildableConveyorBase* Old, AFGHologram* Holo,
        FSFConveyorUpgradeOwner::FOperation& Operation, USFConstructionOperationCoordinator& Owner, FString& Reason);
    bool MatchesPrepared(AFGBuildableConveyorBase* Old, AFGHologram* Holo,
        const FSFConveyorUpgradeOwner::FOperation& Operation);
    bool Adopt(AFGBuildableConveyorBase* Old, const TArray<AActor*>& Children,
        FSFConveyorUpgradeOwner::FOperation& Operation, USFConstructionOperationCoordinator& Owner, bool bPreserveGroups, FString& Reason);
    bool PrepareRetirement(AFGBuildableConveyorBase* Old, FSFConveyorUpgradeOwner::FOperation& Operation,
        USFConstructionOperationCoordinator& Owner, FString& Reason);
    bool Retire(const FObjectKey& Old, FSFConveyorUpgradeOwner::FOperation& Operation,
        USFConstructionOperationCoordinator& Owner, FString& Reason);
    bool HasPendingLifecycle(const FSFConveyorUpgradeOwner::FOperation& Operation);
    bool Verify(const FSFConveyorUpgradeOwner::FOperation& Operation);
}
