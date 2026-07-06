// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Shared include set for the USFHologramHelperService implementation, split across SFHologramHelperService.cpp + _Part2 (each <2k).
 * Each split TU includes ONLY this header instead of duplicating the include block.
 * No behavior change.
 */

#pragma once

#include "Subsystem/SFHologramHelperService.h"
#include "SFSubsystem.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Hologram/FGBlueprintHologram.h"  // [#168] blueprint composite grid children
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Holograms/Core/SFSmartChildHologram.h"
#include "Holograms/Core/SFSmartFactoryChildHologram.h"
#include "Holograms/Core/SFSmartLogisticsChildHologram.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "Holograms/Core/SFBuildableChildHologram.h"
#include "Holograms/Core/SFFloodlightChildHologram.h"
#include "Holograms/Core/SFStandaloneSignChildHologram.h"
#include "Holograms/Logistics/SFConveyorPoleChildHologram.h"   // #354: standard conveyor pole grid child
#include "Hologram/FGStandaloneSignHologram.h"
#include "Hologram/FGSignPoleHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFHologramDataService.h"
#include "Logging/LogMacros.h"
#include "Kismet/GameplayStatics.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"

// Module dependencies (Phase 2)
#include "SFValidationService.h"
#include "SFHologramPerformanceProfiler.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"

// Hologram adapters
#include "Holograms/Adapters/ISFHologramAdapter.h"
#include "Holograms/Adapters/SFGenericAdapter.h"
#include "Holograms/Adapters/SFWallAdapter.h"
#include "Holograms/Adapters/SFPillarAdapter.h"
#include "Holograms/Adapters/SFWaterExtractorAdapter.h"

// Smart hologram base classes
#include "Holograms/Core/SFSmartHologram.h"
#include "Holograms/Adapters/SFResourceExtractorAdapter.h"
#include "Holograms/Adapters/SFFactoryAdapter.h"
#include "Holograms/Adapters/SFElevatorAdapter.h"
#include "Holograms/Adapters/SFRampAdapter.h"
#include "Holograms/Adapters/SFJumpPadAdapter.h"
#include "Holograms/Adapters/SFUnsupportedAdapter.h"

// Satisfactory hologram types for adapter factory
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGWallHologram.h"
#include "Hologram/FGPillarHologram.h"
#include "Hologram/FGStackableStorageHologram.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Hologram/FGResourceExtractorHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGPipeHyperAttachmentHologram.h"
#include "Hologram/FGCeilingLightHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Hologram/FGWallAttachmentHologram.h"
#include "Hologram/FGElevatorHologram.h"
#include "Hologram/FGStairHologram.h"
#include "Hologram/FGJumpPadHologram.h"
#include "Hologram/FGFactoryBuildingHologram.h"  // Issue #160: Zoop detection
#include "Hologram/FGWheeledVehicleHologram.h"
#include "Hologram/FGSpaceElevatorHologram.h"
