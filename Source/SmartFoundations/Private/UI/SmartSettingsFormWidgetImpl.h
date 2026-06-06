// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * Shared include set for the USmartSettingsFormWidget implementation, split across SmartSettingsFormWidget.cpp + _Presets/_Controls (each <2k).
 * Each split TU includes ONLY this header instead of duplicating the include block.
 * No behavior change.
 */

#pragma once

#include "UI/SmartSettingsFormWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"
#include "InputCoreTypes.h"
#include "SmartFoundations.h"
#include "UI/SFFontLibrary.h"
#include "Subsystem/SFSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Services/SFRecipeManagementService.h"
#include "Features/Restore/SFRestoreService.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "Hologram/FGHologram.h"
#include "Services/SFHudService.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "FGPlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
