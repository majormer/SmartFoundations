// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once
#include "CoreMinimal.h"
#include "Configuration/ConfigManager.h"
#include "Engine/Engine.h"
#include "Smart_ConfigStruct.generated.h"

/*
 * Mod Configuration Asset '/SmartFoundations/SmartFoundations/Config/Smart_Config' is now organized
 * into named sections (Belt / Pipe / Power Auto-Connect, Building Behavior, HUD, Arrows). In SML a
 * named config section maps to a NESTED struct field, so the menu's section layout is mirrored by
 * the sub-structs below and FSmart_ConfigStruct_Sections.
 *
 * The public FSmart_ConfigStruct remains FLAT and unchanged so existing consuming code is untouched.
 * GetActiveConfig() fills the nested sections struct from the config system and copies the values
 * down into the flat struct it returns. Fields that are not menu settings (AutoConnectMode,
 * PowerPoleMk1-4MaxConnections) are not part of any section and keep their flat defaults.
 */

// ── Section mirror sub-structs (field names must match the config asset's leaf keys) ──

USTRUCT(BlueprintType)
struct FSmart_BeltConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bAutoConnectEnabled{};
    UPROPERTY(BlueprintReadWrite) bool bAutoConnectDistributors{};
    UPROPERTY(BlueprintReadWrite) int32 BeltLevelMain{};
    UPROPERTY(BlueprintReadWrite) int32 BeltLevelToBuilding{};
    UPROPERTY(BlueprintReadWrite) int32 BeltRoutingMode{};
    UPROPERTY(BlueprintReadWrite) bool bStackableBeltEnabled{};
};

USTRUCT(BlueprintType)
struct FSmart_PipeConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bPipeAutoConnectEnabled{};
    UPROPERTY(BlueprintReadWrite) int32 PipeLevelMain{};
    UPROPERTY(BlueprintReadWrite) int32 PipeLevelToBuilding{};
    UPROPERTY(BlueprintReadWrite) int32 PipeRoutingMode{};
    UPROPERTY(BlueprintReadWrite) bool PipeIndicator{};
};

USTRUCT(BlueprintType)
struct FSmart_PowerConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bPowerAutoConnectEnabled{};
    UPROPERTY(BlueprintReadWrite) int32 PowerConnectMode{};
    UPROPERTY(BlueprintReadWrite) int32 PowerConnectRange{};
    UPROPERTY(BlueprintReadWrite) int32 PowerConnectReserved{};
};

USTRUCT(BlueprintType)
struct FSmart_BuildingBehaviorConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bExtendEnabled{true};
    UPROPERTY(BlueprintReadWrite) bool bExtendPowerEnabled{};
    UPROPERTY(BlueprintReadWrite) bool bAutoHoldOnGridChange{false};
    UPROPERTY(BlueprintReadWrite) bool bApplyImmediately{};
};

USTRUCT(BlueprintType)
struct FSmart_HUDConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bShowHUD{};
    UPROPERTY(BlueprintReadWrite) float HUDScale{};
    UPROPERTY(BlueprintReadWrite) float HUDPositionX{0.02f};
    UPROPERTY(BlueprintReadWrite) float HUDPositionY{0.25f};
    UPROPERTY(BlueprintReadWrite) int32 HUDTheme{};
};

USTRUCT(BlueprintType)
struct FSmart_ArrowsConfigSection {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) bool bShowArrows{};
    UPROPERTY(BlueprintReadWrite) bool bShowArrowOrbit{true};
    UPROPERTY(BlueprintReadWrite) bool bShowArrowLabels{true};
};

/* Nested struct whose field names match the config asset's section keys. Used only to fill from
 * the config system; values are copied into the flat FSmart_ConfigStruct below. */
USTRUCT(BlueprintType)
struct FSmart_ConfigStruct_Sections {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FSmart_BeltConfigSection BeltAutoConnect;
    UPROPERTY(BlueprintReadWrite) FSmart_PipeConfigSection PipeAutoConnect;
    UPROPERTY(BlueprintReadWrite) FSmart_PowerConfigSection PowerAutoConnect;
    UPROPERTY(BlueprintReadWrite) FSmart_BuildingBehaviorConfigSection BuildingBehavior;
    UPROPERTY(BlueprintReadWrite) FSmart_HUDConfigSection HUD;
    UPROPERTY(BlueprintReadWrite) FSmart_ArrowsConfigSection Arrows;
};

/* Struct generated from Mod Configuration Asset '/SmartFoundations/SmartFoundations/Config/Smart_Config' */
/* Property order matches Blueprint config UI (grouped by feature, usage-frequency top-to-bottom) */
USTRUCT(BlueprintType)
struct FSmart_ConfigStruct {
    GENERATED_BODY()
public:

    // ── Belt Auto-Connect ──

    UPROPERTY(BlueprintReadWrite)
    bool bAutoConnectEnabled{};

    UPROPERTY(BlueprintReadWrite)
    bool bAutoConnectDistributors{};

    UPROPERTY(BlueprintReadWrite)
    int32 AutoConnectMode{};

    UPROPERTY(BlueprintReadWrite)
    int32 BeltLevelMain{};

    UPROPERTY(BlueprintReadWrite)
    int32 BeltLevelToBuilding{};

    // Belt routing mode: 0=Default, 1=Curve, 2=Straight
    UPROPERTY(BlueprintReadWrite)
    int32 BeltRoutingMode{};

    UPROPERTY(BlueprintReadWrite)
    bool bStackableBeltEnabled{};

    // ── Pipe Auto-Connect ──

    UPROPERTY(BlueprintReadWrite)
    bool bPipeAutoConnectEnabled{};

    UPROPERTY(BlueprintReadWrite)
    int32 PipeLevelMain{};

    UPROPERTY(BlueprintReadWrite)
    int32 PipeLevelToBuilding{};

    // Pipe routing mode: 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical
    UPROPERTY(BlueprintReadWrite)
    int32 PipeRoutingMode{};

    UPROPERTY(BlueprintReadWrite)
    bool PipeIndicator{};

    // ── Power Auto-Connect ──

    UPROPERTY(BlueprintReadWrite)
    bool bPowerAutoConnectEnabled{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerConnectMode{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerConnectRange{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerConnectReserved{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerPoleMk1MaxConnections{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerPoleMk2MaxConnections{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerPoleMk3MaxConnections{};

    UPROPERTY(BlueprintReadWrite)
    int32 PowerPoleMk4MaxConnections{};

    // ── Extend ──

    // Extend feature enabled (Issue #257) - persistent toggle from settings menu
    UPROPERTY(BlueprintReadWrite)
    bool bExtendEnabled{true};

    // Power Extend: include power poles when using Extend feature (Issue #229)
    UPROPERTY(BlueprintReadWrite)
    bool bExtendPowerEnabled{};

    // ── Scaling ──

    // Auto-Hold: automatically lock hologram position after any grid modification (Issue #273)
    // Unlike Scaled Extend, this lock can be manually released by pressing the vanilla Hold key
    UPROPERTY(BlueprintReadWrite)
    bool bAutoHoldOnGridChange{false};

    // ── Smart Panel ──

    // Apply settings immediately without clicking Apply button
    UPROPERTY(BlueprintReadWrite)
    bool bApplyImmediately{};

    // ── HUD (set and forget) ──

    UPROPERTY(BlueprintReadWrite)
    bool bShowHUD{};

    UPROPERTY(BlueprintReadWrite)
    float HUDScale{};

    // HUD Position (0.0-1.0 normalized screen coordinates) - Issue #175
    UPROPERTY(BlueprintReadWrite)
    float HUDPositionX{0.02f};  // Default: 2% from left edge

    UPROPERTY(BlueprintReadWrite)
    float HUDPositionY{0.25f};  // Default: 25% from top edge

    // HUD color theme: 0=Default (FICSIT Orange), 1=Dark, 2=Classic, 3=High Contrast, 4=Minimal, 5=Monochrome
    UPROPERTY(BlueprintReadWrite)
    int32 HUDTheme{};

    // ── Arrows (set and forget) ──

    UPROPERTY(BlueprintReadWrite)
    bool bShowArrows{};

    // Enable animated orbiting of directional arrows around the hologram
    UPROPERTY(BlueprintReadWrite)
    bool bShowArrowOrbit{true};

    // Show X/Y/Z text labels on directional arrows
    UPROPERTY(BlueprintReadWrite)
    bool bShowArrowLabels{true};

    /* Retrieves active configuration value and returns object of this struct containing it.
     * The config asset is organized into sections, so we fill the nested sections struct and copy
     * the values into this flat struct (keeping all existing consumers unchanged). */
    static FSmart_ConfigStruct GetActiveConfig(UObject* WorldContext) {
        FSmart_ConfigStruct ConfigStruct{};
        FSmart_ConfigStruct_Sections Sections{};
        FConfigId ConfigId{"SmartFoundations", ""};
        if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)) {
            UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
            ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{FSmart_ConfigStruct_Sections::StaticStruct(), &Sections});
        }

        // Belt Auto-Connect
        ConfigStruct.bAutoConnectEnabled      = Sections.BeltAutoConnect.bAutoConnectEnabled;
        ConfigStruct.bAutoConnectDistributors = Sections.BeltAutoConnect.bAutoConnectDistributors;
        ConfigStruct.BeltLevelMain            = Sections.BeltAutoConnect.BeltLevelMain;
        ConfigStruct.BeltLevelToBuilding      = Sections.BeltAutoConnect.BeltLevelToBuilding;
        ConfigStruct.BeltRoutingMode          = Sections.BeltAutoConnect.BeltRoutingMode;
        ConfigStruct.bStackableBeltEnabled    = Sections.BeltAutoConnect.bStackableBeltEnabled;

        // Pipe Auto-Connect
        ConfigStruct.bPipeAutoConnectEnabled  = Sections.PipeAutoConnect.bPipeAutoConnectEnabled;
        ConfigStruct.PipeLevelMain            = Sections.PipeAutoConnect.PipeLevelMain;
        ConfigStruct.PipeLevelToBuilding      = Sections.PipeAutoConnect.PipeLevelToBuilding;
        ConfigStruct.PipeRoutingMode          = Sections.PipeAutoConnect.PipeRoutingMode;
        ConfigStruct.PipeIndicator            = Sections.PipeAutoConnect.PipeIndicator;

        // Power Auto-Connect
        ConfigStruct.bPowerAutoConnectEnabled = Sections.PowerAutoConnect.bPowerAutoConnectEnabled;
        ConfigStruct.PowerConnectMode         = Sections.PowerAutoConnect.PowerConnectMode;
        ConfigStruct.PowerConnectRange        = Sections.PowerAutoConnect.PowerConnectRange;
        ConfigStruct.PowerConnectReserved     = Sections.PowerAutoConnect.PowerConnectReserved;

        // Building Behavior (Extend + Scaling + Smart Panel)
        ConfigStruct.bExtendEnabled           = Sections.BuildingBehavior.bExtendEnabled;
        ConfigStruct.bExtendPowerEnabled      = Sections.BuildingBehavior.bExtendPowerEnabled;
        ConfigStruct.bAutoHoldOnGridChange    = Sections.BuildingBehavior.bAutoHoldOnGridChange;
        ConfigStruct.bApplyImmediately        = Sections.BuildingBehavior.bApplyImmediately;

        // HUD
        ConfigStruct.bShowHUD                 = Sections.HUD.bShowHUD;
        ConfigStruct.HUDScale                 = Sections.HUD.HUDScale;
        ConfigStruct.HUDPositionX             = Sections.HUD.HUDPositionX;
        ConfigStruct.HUDPositionY             = Sections.HUD.HUDPositionY;
        ConfigStruct.HUDTheme                 = Sections.HUD.HUDTheme;

        // Arrows
        ConfigStruct.bShowArrows              = Sections.Arrows.bShowArrows;
        ConfigStruct.bShowArrowOrbit          = Sections.Arrows.bShowArrowOrbit;
        ConfigStruct.bShowArrowLabels         = Sections.Arrows.bShowArrowLabels;

        // AutoConnectMode and PowerPoleMk1-4MaxConnections are not menu settings; they keep the
        // flat struct defaults above (this matches prior behavior, where they were never config-filled).
        return ConfigStruct;
    }
};
