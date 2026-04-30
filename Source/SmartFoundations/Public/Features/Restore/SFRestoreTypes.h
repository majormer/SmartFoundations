// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFRestoreTypes - Data model for Smart Restore Enhanced
 *
 * Defines the preset struct and capture flags used by USFRestoreService
 * to capture, serialize, and restore Smart Panel configuration.
 *
 * Key types:
 * - FSFRestoreCaptureFlags - Which field groups the user chose to include
 * - FSFRestoreAutoConnectState - Snapshot of auto-connect runtime settings
 * - FSFRestorePreset - A single named preset with all capturable fields
 */

#pragma once

#include "CoreMinimal.h"
#include "SFRestoreTypes.generated.h"

/** Current preset format version. Increment when fields are added/changed. */
static constexpr int32 SF_RESTORE_PRESET_VERSION = 1;

/** Compact sharing format prefix */
static const FString SF_RESTORE_EXPORT_PREFIX = TEXT("SR1:");

/**
 * Capture flags — which field groups the user chose to include when saving.
 * The save dialog defaults to all flags checked; the user unchecks
 * fields they do not want stored in the preset.
 */
USTRUCT(BlueprintType)
struct FSFRestoreCaptureFlags
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bGrid = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bSpacing = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bSteps = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bStagger = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bRotation = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bRecipe = true;

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	bool bAutoConnect = true;
};

/**
 * Auto-connect settings snapshot.
 * Mirrors FAutoConnectRuntimeSettings from SFSubsystem but as a standalone
 * USTRUCT for independent serialization. Stored only when
 * FSFRestoreCaptureFlags::bAutoConnect is true.
 */
USTRUCT(BlueprintType)
struct FSFRestoreAutoConnectState
{
	GENERATED_BODY()

	// Belt
	UPROPERTY() bool bBeltEnabled = true;
	UPROPERTY() int32 BeltTierMain = 0;
	UPROPERTY() int32 BeltTierToBuilding = 0;
	UPROPERTY() bool bChainDistributors = true;
	UPROPERTY() int32 BeltRoutingMode = 0;

	// Pipe
	UPROPERTY() bool bPipeEnabled = true;
	UPROPERTY() int32 PipeTierMain = 0;
	UPROPERTY() int32 PipeTierToBuilding = 0;
	UPROPERTY() bool bPipeIndicator = true;
	UPROPERTY() int32 PipeRoutingMode = 0;

	// Power
	UPROPERTY() bool bPowerEnabled = true;
	UPROPERTY() int32 PowerGridAxis = 0;
	UPROPERTY() int32 PowerReserved = 0;
};

/**
 * A single Smart Restore preset.
 *
 * BuildingClassPath is always captured (identifies what building to place).
 * All other field groups are optional based on CaptureFlags — fields whose
 * flag is false are not serialized to JSON and are ignored on restore
 * (the current Smart Panel state for those fields is left untouched).
 *
 * Field types match FSFCounterState exactly:
 * - Grid: FIntVector (supports negative for directional arrays)
 * - Spacing/Steps/Stagger: int32 in UE units (cm)
 * - Rotation: float in degrees
 */
USTRUCT(BlueprintType)
struct FSFRestorePreset
{
	GENERATED_BODY()

	// === Identity ===

	/** User-defined preset name */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	FString Name;

	/** Building class name (e.g. "Build_Constructor_C").
	 *  Uses short class name rather than full path for resilience across
	 *  game updates. Resolved via FindFirstObject at restore time. */
	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	FString BuildingClassName;

	// === Capture Flags ===

	UPROPERTY(BlueprintReadWrite, Category = "Smart|Restore")
	FSFRestoreCaptureFlags CaptureFlags;

	// === Grid (stored when CaptureFlags.bGrid is true) ===

	UPROPERTY() FIntVector GridCounters = FIntVector(1, 1, 1);

	// === Spacing (stored when CaptureFlags.bSpacing is true) ===

	UPROPERTY() int32 SpacingX = 0;
	UPROPERTY() int32 SpacingY = 0;
	UPROPERTY() int32 SpacingZ = 0;

	// === Steps (stored when CaptureFlags.bSteps is true) ===

	UPROPERTY() int32 StepsX = 0;
	UPROPERTY() int32 StepsY = 0;

	// === Stagger (stored when CaptureFlags.bStagger is true) ===

	UPROPERTY() int32 StaggerX = 0;
	UPROPERTY() int32 StaggerY = 0;
	UPROPERTY() int32 StaggerZX = 0;
	UPROPERTY() int32 StaggerZY = 0;

	// === Rotation (stored when CaptureFlags.bRotation is true) ===

	UPROPERTY() float RotationZ = 0.0f;

	// === Recipe (stored when CaptureFlags.bRecipe is true) ===

	/** Recipe class name (e.g. "Recipe_Screw_C"). Empty if no recipe captured. */
	UPROPERTY() FString RecipeClassName;

	// === Auto-Connect (stored when CaptureFlags.bAutoConnect is true) ===

	UPROPERTY() FSFRestoreAutoConnectState AutoConnect;

	// === Metadata ===

	UPROPERTY() int32 Version = SF_RESTORE_PRESET_VERSION;
	UPROPERTY() FString CreatedAt;
	UPROPERTY() FString UpdatedAt;
};
