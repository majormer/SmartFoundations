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
	UConfigPropertySection* Belt = CreateSection(TEXT("BeltAutoConnect"), LOCTEXT("Sec.Belt", "Belt Auto-Connect"),
		LOCTEXT("Sec.Belt.TT", "Automatically run conveyor belts between the buildings you place."));
	Belt->SectionProperties.Add(TEXT("bAutoConnectEnabled"),      CreateBoolProperty(TEXT("bAutoConnectEnabled"),      LOCTEXT("P.bAutoConnectEnabled", "Belt Auto-Connect"),
		LOCTEXT("P.bAutoConnectEnabled.TT", "Automatically connect conveyor belts between placed buildings and distributors."), true));
	Belt->SectionProperties.Add(TEXT("bAutoConnectDistributors"), CreateBoolProperty(TEXT("bAutoConnectDistributors"), LOCTEXT("P.bAutoConnectDistributors", "Use Manifold Lane Belts"),
		LOCTEXT("P.bAutoConnectDistributors.TT", "Chain distributors together with manifold lane belts, so a whole row is fed from one input."), true));
	Belt->SectionProperties.Add(TEXT("BeltLevelMain"),            CreateIntegerProperty(TEXT("BeltLevelMain"),         LOCTEXT("P.BeltLevelMain", "Manifold Lane Belt"),
		LOCTEXT("P.BeltLevelMain.TT", "Belt tier for distributor-to-distributor (manifold) connections. Auto uses the highest tier you've unlocked."), 0));
	Belt->SectionProperties.Add(TEXT("BeltLevelToBuilding"),      CreateIntegerProperty(TEXT("BeltLevelToBuilding"),   LOCTEXT("P.BeltLevelToBuilding", "Factory Belt"),
		LOCTEXT("P.BeltLevelToBuilding.TT", "Belt tier for distributor-to-building connections. Auto uses the highest tier you've unlocked."), 0));
	Belt->SectionProperties.Add(TEXT("BeltRoutingMode"),         CreateIntegerProperty(TEXT("BeltRoutingMode"),        LOCTEXT("P.BeltRoutingMode", "Belt Routing Mode"),
		LOCTEXT("P.BeltRoutingMode.TT", "How auto-connected belts are shaped: Default, Curve, or Straight."), 0));
	Belt->SectionProperties.Add(TEXT("bStackableBeltEnabled"),   CreateBoolProperty(TEXT("bStackableBeltEnabled"),     LOCTEXT("P.bStackableBeltEnabled", "Stackable Belt Auto-Connect"),
		LOCTEXT("P.bStackableBeltEnabled.TT", "Allow auto-connected belts to stack vertically where they overlap."), false));
	RootSection->SectionProperties.Add(TEXT("BeltAutoConnect"), Belt);

	// ── Pipe Auto-Connect ──
	UConfigPropertySection* Pipe = CreateSection(TEXT("PipeAutoConnect"), LOCTEXT("Sec.Pipe", "Pipe Auto-Connect"),
		LOCTEXT("Sec.Pipe.TT", "Automatically run pipes between the buildings you place."));
	Pipe->SectionProperties.Add(TEXT("bPipeAutoConnectEnabled"), CreateBoolProperty(TEXT("bPipeAutoConnectEnabled"),  LOCTEXT("P.bPipeAutoConnectEnabled", "Pipe Auto-Connect"),
		LOCTEXT("P.bPipeAutoConnectEnabled.TT", "Automatically connect pipes between placed buildings."), true));
	Pipe->SectionProperties.Add(TEXT("PipeLevelMain"),          CreateIntegerProperty(TEXT("PipeLevelMain"),          LOCTEXT("P.PipeLevelMain", "Main Tier"),
		LOCTEXT("P.PipeLevelMain.TT", "Pipe tier for the main connections. Auto uses the highest tier you've unlocked."), 0));
	Pipe->SectionProperties.Add(TEXT("PipeLevelToBuilding"),    CreateIntegerProperty(TEXT("PipeLevelToBuilding"),    LOCTEXT("P.PipeLevelToBuilding", "To Building"),
		LOCTEXT("P.PipeLevelToBuilding.TT", "Pipe tier for connections to buildings. Auto uses the highest tier you've unlocked."), 0));
	Pipe->SectionProperties.Add(TEXT("PipeRoutingMode"),        CreateIntegerProperty(TEXT("PipeRoutingMode"),        LOCTEXT("P.PipeRoutingMode", "Pipe Routing Mode"),
		LOCTEXT("P.PipeRoutingMode.TT", "How auto-connected pipes are shaped: Auto, 2D, Straight, Curve, Noodle, or Horizontal-to-Vertical."), 0));
	Pipe->SectionProperties.Add(TEXT("PipeIndicator"),          CreateBoolProperty(TEXT("PipeIndicator"),             LOCTEXT("P.PipeIndicator", "Flow Indicator"),
		LOCTEXT("P.PipeIndicator.TT", "Show a flow-direction indicator on auto-connected pipes."), true));
	RootSection->SectionProperties.Add(TEXT("PipeAutoConnect"), Pipe);

	// ── Power Auto-Connect ──
	UConfigPropertySection* Power = CreateSection(TEXT("PowerAutoConnect"), LOCTEXT("Sec.Power", "Power Auto-Connect"),
		LOCTEXT("Sec.Power.TT", "Automatically wire power between buildings and power poles as you build."));
	Power->SectionProperties.Add(TEXT("bPowerAutoConnectEnabled"), CreateBoolProperty(TEXT("bPowerAutoConnectEnabled"), LOCTEXT("P.bPowerAutoConnectEnabled", "Power Auto-Connect"),
		LOCTEXT("P.bPowerAutoConnectEnabled.TT", "Automatically wire power between placed buildings and nearby power poles."), true));
	Power->SectionProperties.Add(TEXT("PowerConnectMode"),        CreateIntegerProperty(TEXT("PowerConnectMode"),       LOCTEXT("P.PowerConnectMode", "Grid Axis"),
		LOCTEXT("P.PowerConnectMode.TT", "Which directions power poles connect: Auto, X only, Y only, or both X and Y."), 0));
	Power->SectionProperties.Add(TEXT("PowerConnectRange"),       CreateIntegerProperty(TEXT("PowerConnectRange"),      LOCTEXT("P.PowerConnectRange", "Connection Range"),
		LOCTEXT("P.PowerConnectRange.TT", "Maximum distance, in meters, to search for a power connection."), 50));
	Power->SectionProperties.Add(TEXT("PowerConnectReserved"),    CreateIntegerProperty(TEXT("PowerConnectReserved"),   LOCTEXT("P.PowerConnectReserved", "Connections to Keep Free"),
		LOCTEXT("P.PowerConnectReserved.TT", "Number of pole connection slots to leave free for your own manual wiring."), 2));
	RootSection->SectionProperties.Add(TEXT("PowerAutoConnect"), Power);

	// ── Building Behavior ──
	UConfigPropertySection* Building = CreateSection(TEXT("BuildingBehavior"), LOCTEXT("Sec.Building", "Building Behavior"),
		LOCTEXT("Sec.Building.TT", "How Extend, auto-hold, and Apply behave while building."));
	Building->SectionProperties.Add(TEXT("bExtendEnabled"),        CreateBoolProperty(TEXT("bExtendEnabled"),        LOCTEXT("P.bExtendEnabled", "Extend Enabled"),
		LOCTEXT("P.bExtendEnabled.TT", "Enable Extend - repeat a building or grid outward in one direction."), true));
	Building->SectionProperties.Add(TEXT("bExtendPowerEnabled"),   CreateBoolProperty(TEXT("bExtendPowerEnabled"),   LOCTEXT("P.bExtendPowerEnabled", "Extend Power Poles"),
		LOCTEXT("P.bExtendPowerEnabled.TT", "Include power poles when using Extend."), false));
	Building->SectionProperties.Add(TEXT("bExtendDaisyChainPower"), CreateBoolProperty(TEXT("bExtendDaisyChainPower"), LOCTEXT("P.bExtendDaisyChainPower", "Extend Daisy-Chain Power"),
		LOCTEXT("P.bExtendDaisyChainPower.TT", "Once Upgraded Power Connectors is unlocked, Extend wires power directly building-to-building along the lane instead of running a pole to each building."), true));
	Building->SectionProperties.Add(TEXT("bExtendDaisyChainPoleless"), CreateBoolProperty(TEXT("bExtendDaisyChainPoleless"), LOCTEXT("P.bExtendDaisyChainPoleless", "Daisy-Chain Pole-less Factories"),
		LOCTEXT("P.bExtendDaisyChainPoleless.TT", "When you Extend a factory that has no power pole and no existing daisy link (starting a fresh manifold), wire the new buildings together with daisy-chain power. Factories already daisy-chained are always continued regardless of this option."), true));
	Building->SectionProperties.Add(TEXT("bAutoHoldOnGridChange"), CreateBoolProperty(TEXT("bAutoHoldOnGridChange"), LOCTEXT("P.bAutoHoldOnGridChange", "Auto-Hold on Grid Change"),
		LOCTEXT("P.bAutoHoldOnGridChange.TT", "Automatically lock the hologram in place after any grid change. Press the Hold key to release it."), false));
	Building->SectionProperties.Add(TEXT("bApplyImmediately"),     CreateBoolProperty(TEXT("bApplyImmediately"),     LOCTEXT("P.bApplyImmediately", "Apply Immediately"),
		LOCTEXT("P.bApplyImmediately.TT", "Apply Smart Panel changes instantly instead of clicking the Apply button."), false));
	RootSection->SectionProperties.Add(TEXT("BuildingBehavior"), Building);

	// ── HUD ──
	UConfigPropertySection* Hud = CreateSection(TEXT("HUD"), LOCTEXT("Sec.HUD", "HUD"),
		LOCTEXT("Sec.HUD.TT", "The on-screen Smart! overlay shown while you build."));
	Hud->SectionProperties.Add(TEXT("bShowHUD"),      CreateBoolProperty(TEXT("bShowHUD"),      LOCTEXT("P.bShowHUD", "Show HUD Overlay"),
		LOCTEXT("P.bShowHUD.TT", "Show the Smart! overlay (grid, scale, and recipe info) while building."), true));
	Hud->SectionProperties.Add(TEXT("HUDScale"),      CreateFloatProperty(TEXT("HUDScale"),     LOCTEXT("P.HUDScale", "HUD Scale"),
		LOCTEXT("P.HUDScale.TT", "Size of the Smart! HUD overlay."), 1.0f));
	Hud->SectionProperties.Add(TEXT("HUDPositionX"),  CreateFloatProperty(TEXT("HUDPositionX"), LOCTEXT("P.HUDPositionX", "HUD Position X"),
		LOCTEXT("P.HUDPositionX.TT", "Horizontal position of the HUD on screen (0 = left edge, 1 = right edge)."), 0.02f));
	Hud->SectionProperties.Add(TEXT("HUDPositionY"),  CreateFloatProperty(TEXT("HUDPositionY"), LOCTEXT("P.HUDPositionY", "HUD Position Y"),
		LOCTEXT("P.HUDPositionY.TT", "Vertical position of the HUD on screen (0 = top, 1 = bottom)."), 0.25f));
	Hud->SectionProperties.Add(TEXT("HUDTheme"),      CreateIntegerProperty(TEXT("HUDTheme"),   LOCTEXT("P.HUDTheme", "HUD Theme"),
		LOCTEXT("P.HUDTheme.TT", "Color theme for the HUD: Default (FICSIT orange), Dark, Classic, High Contrast, Minimal, or Monochrome."), 0));
	RootSection->SectionProperties.Add(TEXT("HUD"), Hud);

	// ── Arrows ──
	UConfigPropertySection* Arrows = CreateSection(TEXT("Arrows"), LOCTEXT("Sec.Arrows", "Arrows"),
		LOCTEXT("Sec.Arrows.TT", "Directional arrows drawn on the building hologram."));
	Arrows->SectionProperties.Add(TEXT("bShowArrows"),      CreateBoolProperty(TEXT("bShowArrows"),      LOCTEXT("P.bShowArrows", "Show Direction Arrows"),
		LOCTEXT("P.bShowArrows.TT", "Show X/Y/Z direction arrows on the building hologram."), true));
	Arrows->SectionProperties.Add(TEXT("bShowArrowOrbit"),  CreateBoolProperty(TEXT("bShowArrowOrbit"),  LOCTEXT("P.bShowArrowOrbit", "Arrow Orbit Animation"),
		LOCTEXT("P.bShowArrowOrbit.TT", "Animate the arrows orbiting the hologram. Turn off for static arrows that still track you."), true));
	Arrows->SectionProperties.Add(TEXT("bShowArrowLabels"), CreateBoolProperty(TEXT("bShowArrowLabels"), LOCTEXT("P.bShowArrowLabels", "Arrow Axis Labels"),
		LOCTEXT("P.bShowArrowLabels.TT", "Show X/Y/Z axis labels on the direction arrows."), true));
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

UConfigPropertyBool* USmartFoundationsModConfiguration::CreateBoolProperty(const FName& Name, const FText& InDisplayName, const FText& Tooltip, bool Value)
{
	UConfigPropertyBool* Property = CreateDefaultSubobject<UConfigPropertyBool>(Name);
	Property->DisplayName = InDisplayName;
	Property->Tooltip = Tooltip;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

UConfigPropertyInteger* USmartFoundationsModConfiguration::CreateIntegerProperty(const FName& Name, const FText& InDisplayName, const FText& Tooltip, int32 Value)
{
	UConfigPropertyInteger* Property = CreateDefaultSubobject<UConfigPropertyInteger>(Name);
	Property->DisplayName = InDisplayName;
	Property->Tooltip = Tooltip;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

UConfigPropertyFloat* USmartFoundationsModConfiguration::CreateFloatProperty(const FName& Name, const FText& InDisplayName, const FText& Tooltip, float Value)
{
	UConfigPropertyFloat* Property = CreateDefaultSubobject<UConfigPropertyFloat>(Name);
	Property->DisplayName = InDisplayName;
	Property->Tooltip = Tooltip;
	Property->Value = Value;
	Property->bRequiresWorldReload = false;
	Property->bHidden = false;
	return Property;
}

#undef LOCTEXT_NAMESPACE
