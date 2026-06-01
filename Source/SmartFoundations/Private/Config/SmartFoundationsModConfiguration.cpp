// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Config/SmartFoundationsModConfiguration.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

USmartFoundationsModConfiguration::USmartFoundationsModConfiguration()
{
	ConfigId.ModReference = TEXT("SmartFoundations");
	DisplayName = LOCTEXT("Config.DisplayName", "Smart!");
	Description = LOCTEXT("Config.Description", "Smart! Foundations settings.");

	RootSection = CreateSection(TEXT("Root"),
		LOCTEXT("Config.Root.Name", "Smart!"),
		LOCTEXT("Config.Root.Tooltip", "Smart! Foundations settings."));

	// ── Belt Auto-Connect ──
	UConfigPropertySection* Belt = CreateSection(TEXT("BeltAutoConnect"), LOCTEXT("Sec.Belt", "Belt Auto-Connect"), FText::GetEmpty());
	Belt->SectionProperties.Add(TEXT("bAutoConnectEnabled"),      CreateBoolProperty(TEXT("bAutoConnectEnabled"),      LOCTEXT("P.bAutoConnectEnabled", "Belt Auto-Connect"), true));
	Belt->SectionProperties.Add(TEXT("bAutoConnectDistributors"), CreateBoolProperty(TEXT("bAutoConnectDistributors"), LOCTEXT("P.bAutoConnectDistributors", "Use Manifold Lane Belts"), true));
	Belt->SectionProperties.Add(TEXT("BeltLevelMain"),            CreateIntegerProperty(TEXT("BeltLevelMain"),         LOCTEXT("P.BeltLevelMain", "Manifold Lane Belt"), 0));
	Belt->SectionProperties.Add(TEXT("BeltLevelToBuilding"),      CreateIntegerProperty(TEXT("BeltLevelToBuilding"),   LOCTEXT("P.BeltLevelToBuilding", "Factory Belt"), 0));
	Belt->SectionProperties.Add(TEXT("BeltRoutingMode"),         CreateIntegerProperty(TEXT("BeltRoutingMode"),        LOCTEXT("P.BeltRoutingMode", "Belt Routing Mode"), 0));
	Belt->SectionProperties.Add(TEXT("bStackableBeltEnabled"),   CreateBoolProperty(TEXT("bStackableBeltEnabled"),     LOCTEXT("P.bStackableBeltEnabled", "Stackable Belt Auto-Connect"), false));
	RootSection->SectionProperties.Add(TEXT("BeltAutoConnect"), Belt);

	// ── Pipe Auto-Connect ──
	UConfigPropertySection* Pipe = CreateSection(TEXT("PipeAutoConnect"), LOCTEXT("Sec.Pipe", "Pipe Auto-Connect"), FText::GetEmpty());
	Pipe->SectionProperties.Add(TEXT("bPipeAutoConnectEnabled"), CreateBoolProperty(TEXT("bPipeAutoConnectEnabled"),  LOCTEXT("P.bPipeAutoConnectEnabled", "Pipe Auto-Connect"), true));
	Pipe->SectionProperties.Add(TEXT("PipeLevelMain"),          CreateIntegerProperty(TEXT("PipeLevelMain"),          LOCTEXT("P.PipeLevelMain", "Main Tier"), 0));
	Pipe->SectionProperties.Add(TEXT("PipeLevelToBuilding"),    CreateIntegerProperty(TEXT("PipeLevelToBuilding"),    LOCTEXT("P.PipeLevelToBuilding", "To Building"), 0));
	Pipe->SectionProperties.Add(TEXT("PipeRoutingMode"),        CreateIntegerProperty(TEXT("PipeRoutingMode"),        LOCTEXT("P.PipeRoutingMode", "Pipe Routing Mode"), 0));
	Pipe->SectionProperties.Add(TEXT("PipeIndicator"),          CreateBoolProperty(TEXT("PipeIndicator"),             LOCTEXT("P.PipeIndicator", "Flow Indicator"), true));
	RootSection->SectionProperties.Add(TEXT("PipeAutoConnect"), Pipe);

	// ── Power Auto-Connect ──
	UConfigPropertySection* Power = CreateSection(TEXT("PowerAutoConnect"), LOCTEXT("Sec.Power", "Power Auto-Connect"), FText::GetEmpty());
	Power->SectionProperties.Add(TEXT("bPowerAutoConnectEnabled"), CreateBoolProperty(TEXT("bPowerAutoConnectEnabled"), LOCTEXT("P.bPowerAutoConnectEnabled", "Power Auto-Connect"), true));
	Power->SectionProperties.Add(TEXT("PowerConnectMode"),        CreateIntegerProperty(TEXT("PowerConnectMode"),       LOCTEXT("P.PowerConnectMode", "Grid Axis"), 0));
	Power->SectionProperties.Add(TEXT("PowerConnectRange"),       CreateIntegerProperty(TEXT("PowerConnectRange"),      LOCTEXT("P.PowerConnectRange", "Connection Range"), 50));
	Power->SectionProperties.Add(TEXT("PowerConnectReserved"),    CreateIntegerProperty(TEXT("PowerConnectReserved"),   LOCTEXT("P.PowerConnectReserved", "Connections to Keep Free"), 2));
	RootSection->SectionProperties.Add(TEXT("PowerAutoConnect"), Power);

	// ── Building Behavior ──
	UConfigPropertySection* Building = CreateSection(TEXT("BuildingBehavior"), LOCTEXT("Sec.Building", "Building Behavior"), FText::GetEmpty());
	Building->SectionProperties.Add(TEXT("bExtendEnabled"),        CreateBoolProperty(TEXT("bExtendEnabled"),        LOCTEXT("P.bExtendEnabled", "Extend Enabled"), true));
	Building->SectionProperties.Add(TEXT("bExtendPowerEnabled"),   CreateBoolProperty(TEXT("bExtendPowerEnabled"),   LOCTEXT("P.bExtendPowerEnabled", "Extend Power Poles"), false));
	Building->SectionProperties.Add(TEXT("bAutoHoldOnGridChange"), CreateBoolProperty(TEXT("bAutoHoldOnGridChange"), LOCTEXT("P.bAutoHoldOnGridChange", "Auto-Hold on Grid Change"), false));
	Building->SectionProperties.Add(TEXT("bApplyImmediately"),     CreateBoolProperty(TEXT("bApplyImmediately"),     LOCTEXT("P.bApplyImmediately", "Apply Immediately"), false));
	RootSection->SectionProperties.Add(TEXT("BuildingBehavior"), Building);

	// ── HUD ──
	UConfigPropertySection* Hud = CreateSection(TEXT("HUD"), LOCTEXT("Sec.HUD", "HUD"), FText::GetEmpty());
	Hud->SectionProperties.Add(TEXT("bShowHUD"),      CreateBoolProperty(TEXT("bShowHUD"),      LOCTEXT("P.bShowHUD", "Show HUD Overlay"), true));
	Hud->SectionProperties.Add(TEXT("HUDScale"),      CreateFloatProperty(TEXT("HUDScale"),     LOCTEXT("P.HUDScale", "HUD Scale"), 1.0f));
	Hud->SectionProperties.Add(TEXT("HUDPositionX"),  CreateFloatProperty(TEXT("HUDPositionX"), LOCTEXT("P.HUDPositionX", "HUD Position X"), 0.02f));
	Hud->SectionProperties.Add(TEXT("HUDPositionY"),  CreateFloatProperty(TEXT("HUDPositionY"), LOCTEXT("P.HUDPositionY", "HUD Position Y"), 0.25f));
	Hud->SectionProperties.Add(TEXT("HUDTheme"),      CreateIntegerProperty(TEXT("HUDTheme"),   LOCTEXT("P.HUDTheme", "HUD Theme"), 0));
	RootSection->SectionProperties.Add(TEXT("HUD"), Hud);

	// ── Arrows ──
	UConfigPropertySection* Arrows = CreateSection(TEXT("Arrows"), LOCTEXT("Sec.Arrows", "Arrows"), FText::GetEmpty());
	Arrows->SectionProperties.Add(TEXT("bShowArrows"),      CreateBoolProperty(TEXT("bShowArrows"),      LOCTEXT("P.bShowArrows", "Show Direction Arrows"), true));
	Arrows->SectionProperties.Add(TEXT("bShowArrowOrbit"),  CreateBoolProperty(TEXT("bShowArrowOrbit"),  LOCTEXT("P.bShowArrowOrbit", "Arrow Orbit Animation"), true));
	Arrows->SectionProperties.Add(TEXT("bShowArrowLabels"), CreateBoolProperty(TEXT("bShowArrowLabels"), LOCTEXT("P.bShowArrowLabels", "Arrow Axis Labels"), true));
	RootSection->SectionProperties.Add(TEXT("Arrows"), Arrows);
}

UConfigPropertySection* USmartFoundationsModConfiguration::CreateSection(const FName& Name, const FText& InDisplayName, const FText& Tooltip)
{
	UConfigPropertySection* Property = CreateDefaultSubobject<UConfigPropertySection>(Name);
	Property->DisplayName = InDisplayName;
	Property->Tooltip = Tooltip;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

UConfigPropertyBool* USmartFoundationsModConfiguration::CreateBoolProperty(const FName& Name, const FText& InDisplayName, bool Value)
{
	UConfigPropertyBool* Property = CreateDefaultSubobject<UConfigPropertyBool>(Name);
	Property->DisplayName = InDisplayName;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

UConfigPropertyInteger* USmartFoundationsModConfiguration::CreateIntegerProperty(const FName& Name, const FText& InDisplayName, int32 Value)
{
	UConfigPropertyInteger* Property = CreateDefaultSubobject<UConfigPropertyInteger>(Name);
	Property->DisplayName = InDisplayName;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

UConfigPropertyFloat* USmartFoundationsModConfiguration::CreateFloatProperty(const FName& Name, const FText& InDisplayName, float Value)
{
	UConfigPropertyFloat* Property = CreateDefaultSubobject<UConfigPropertyFloat>(Name);
	Property->DisplayName = InDisplayName;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

#undef LOCTEXT_NAMESPACE
