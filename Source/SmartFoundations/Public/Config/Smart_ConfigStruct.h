#pragma once
#include "CoreMinimal.h"
#include "Configuration/ConfigManager.h"
#include "Engine/Engine.h"
#include "Smart_ConfigStruct.generated.h"

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

    /* Retrieves active configuration value and returns object of this struct containing it */
    static FSmart_ConfigStruct GetActiveConfig(UObject* WorldContext) {
        FSmart_ConfigStruct ConfigStruct{};
        FConfigId ConfigId{"SmartFoundations", ""};
        if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)) {
            UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
            ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{FSmart_ConfigStruct::StaticStruct(), &ConfigStruct});
        }
        return ConfigStruct;
    }
};

