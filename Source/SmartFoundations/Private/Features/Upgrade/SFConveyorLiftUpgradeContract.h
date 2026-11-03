// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Features/Upgrade/SFConveyorUpgradeOwner.h"

class AFGBuildableConveyorLift;
class AFGHologram;
class USFConstructionOperationCoordinator;

/** Native lift geometry and floor-hole bindings, separately from cargo/payment. */
namespace SFConveyorLiftUpgradeContract
{
    bool Prepare(AFGBuildableConveyorLift* Old, AFGHologram* Holo, FString& Reason);
    bool MatchesHologram(AFGBuildableConveyorLift* Old, AFGHologram* Holo);
    bool Capture(AFGBuildableConveyorLift* Lift, FSFConveyorUpgradeOwner::FOperation& Operation,
        USFConstructionOperationCoordinator& Owner, FString& Reason);
    bool Verify(const FSFConveyorUpgradeOwner::FOperation& Operation);
}
