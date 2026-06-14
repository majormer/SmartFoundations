// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Shared include set for the USFUpgradeExecutionService implementation, split across SFUpgradeExecutionService.cpp + _ConnectionRepair (each <2k).
 * Each split TU includes ONLY this header instead of duplicating the include block.
 * No behavior change.
 */

#pragma once

#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "SmartFoundations.h"
#include "SFLogMacros.h"
#include "Constants/SFAssetPaths.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFChainActorService.h"
#include "FGBuildableSubsystem.h"
#include "EngineUtils.h"  // For TActorIterator
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "FGCentralStorageSubsystem.h"
#include "FGGameState.h"
#include "FGPlayerState.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePole.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Hologram/FGHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGRecipe.h"
#include "FGFactoryConnectionComponent.h"
#include "FGSplineBuildableInterface.h"
#include "FGConveyorChainActor.h"
#include "Equipment/FGBuildGun.h"
#include "Features/Extend/SFExtendService.h"
#include "FGItemPickup_Spawnable.h"
#include "FGCrate.h"
#include "Core/Net/SFRCO.h"                     // [UPGRADE-MP] result echo to the requesting client's RCO
#include "Kismet/GameplayStatics.h"    // [UPGRADE-MP] RCO lookup (GetAllActorsOfClass)
