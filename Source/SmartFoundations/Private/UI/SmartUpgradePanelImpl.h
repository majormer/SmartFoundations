// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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
#include "Core/Net/SFRCO.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGunBuild.h"
#include "EngineUtils.h"

#include "Features/Upgrade/SFUpgradeAuditService.h"

// Family helpers shared by SmartUpgradePanel.cpp and SmartUpgradePanel_Detail.cpp.
// Defined here (inline) so both split TUs see them under UE 5.6 non-unity builds.
inline bool IsConveyorUpgradeFamily(ESFUpgradeFamily Family)
{
	return Family == ESFUpgradeFamily::Belt || Family == ESFUpgradeFamily::Lift;
}

inline FString GetPanelFamilyDisplayName(ESFUpgradeFamily Family)
{
	return IsConveyorUpgradeFamily(Family)
		? FString(TEXT("Conveyors"))
		: USFUpgradeAuditService::GetFamilyDisplayName(Family);
}
