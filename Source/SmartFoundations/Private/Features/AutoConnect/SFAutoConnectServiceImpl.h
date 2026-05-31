// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Shared include set for the USFAutoConnectService implementation, split across SFAutoConnectService.cpp + _Belt/_Stackable (each <2k).
 * Each split TU includes ONLY this header instead of duplicating the include block.
 * No behavior change.
 */

#pragma once

#include "Features/AutoConnect/SFAutoConnectService.h"
#include "SmartFoundations.h"
#include "SFLogMacros.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
// NOTE: SFDeferredCostService removed - child holograms automatically aggregate costs via GetCost()
#include "Subsystem/SFHologramDataService.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Logging/LogMacros.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildableStorage.h"
#include "FGPipeConnectionComponent.h"
#include "UObject/UnrealType.h"
#include "FGCentralStorageSubsystem.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGConstructDisqualifier.h"
#include "FGGameState.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Kismet/GameplayStatics.h"
