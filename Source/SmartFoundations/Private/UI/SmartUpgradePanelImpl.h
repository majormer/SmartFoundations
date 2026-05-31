// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * Shared include set for the USmartUpgradePanel implementation, split across SmartUpgradePanel.cpp + _Detail (each <2k).
 * Each split TU includes ONLY this header instead of duplicating the include block.
 * No behavior change.
 */

#pragma once

#include "UI/SmartUpgradePanel.h"
#include "SmartFoundations.h"
#include "SFLogMacros.h"
#include "UI/SFFontLibrary.h"
#include "Constants/SFAssetPaths.h"
#include "FGPlayerController.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFHudService.h"
#include "Services/SFChainActorService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Styling/SlateTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Public/SFRCO.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGunBuild.h"
#include "EngineUtils.h"
