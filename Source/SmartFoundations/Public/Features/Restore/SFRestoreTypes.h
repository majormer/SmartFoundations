// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Scaling/SFScalingTypes.h"
#include "Features/Spacing/SFSpacingTypes.h"
#include "SFRestoreTypes.generated.h"

/**
 * Preset format versioning rules (#427):
 * - ADDITIVE change (new optional field with a safe default) -> bump SF_RESTORE_PRESET_VERSION only.
 * - BREAKING change (renamed/removed field, changed meaning)  -> bump the version AND add a branch
 *   to the migration chain in JsonToPreset so every older on-disk/shared preset still loads.
 * - NEVER reorder or remove serialized enum tokens; append only.
 * - Missing fields on migration must default to the LEGACY-IMPLICIT value (what the code did before
 *   the field existed), NOT to a struct default chosen for new presets. Example: RotationAxis
 *   defaults to X because pre-#372 rotation always progressed along X.
 *
 * Version history:
 *   1 -> original release format.
 *   2 -> Extend clone topology embedded (bHasExtendTopology + extendCloneTopology).
 *   3 -> #427: PresetKind discriminant (Grid Preset vs Module), axis/mode selector capture
 *        (spacingMode/spacingAxis/stepsAxis/staggerAxis/rotationAxis as string tokens),
 *        explicit production-recipe state (hasProductionRecipe - "No recipe" is a first-class
 *        restorable value, distinct from the legacy "don't touch" empty string).
 */
static constexpr int32 SF_RESTORE_PRESET_VERSION = 3;

/** Compact sharing format prefix. The digit is a frozen literal (it identifies "a Smart Restore
 *  code", it is NOT the preset version - that lives in the body JSON's "version" field). */
static const FString SF_RESTORE_EXPORT_PREFIX = TEXT("SR1:");

/**
 * Preset kind discriminant (#427): drives the Grid Presets / Modules tab split in the Restore UI.
 * - GridPreset: a full snapshot of the Smart Panel's values (grid + transforms + recipes +
 *   auto-connect). Restoring populates the panel like the user typed the values.
 * - Module: a captured Extend manifold unit (factory + connections up to the distributors/
 *   junctions). Restoring enters the rescalable stamp session (replay).
 * Kind is DERIVED, never trusted from data: NormalizeKind() runs after every load, capture, and
 * import, so the flag/topology pair can never desync.
 */
UENUM(BlueprintType)
enum class ESFRestorePresetKind : uint8
{
	GridPreset = 0 UMETA(DisplayName = "Grid Preset"),
	Module = 1 UMETA(DisplayName = "Module")
};

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

	// === Axis / mode selectors (v3, #427 lossless capture) ===
	// Captured with their owning group's flag. Defaults are the LEGACY-IMPLICIT values (what
	// pre-v3 behavior was), so migrated v2 presets restore exactly as the day they were saved -
	// especially RotationAxis (#419/#372: pre-field rotation always progressed along X).

	UPROPERTY() ESFSpacingMode SpacingMode = ESFSpacingMode::None;
	UPROPERTY() ESFScaleAxis SpacingAxis = ESFScaleAxis::X;
	UPROPERTY() ESFScaleAxis StepsAxis = ESFScaleAxis::X;
	UPROPERTY() ESFScaleAxis StaggerAxis = ESFScaleAxis::ZX;
	UPROPERTY() ESFScaleAxis RotationAxis = ESFScaleAxis::X;

	// === Recipe (stored when CaptureFlags.bRecipe is true) ===

	/** Recipe class name (e.g. "Recipe_Screw_C"). Empty if no recipe captured. */
	UPROPERTY() FString RecipeClassName;

	/** v3 (#427): explicit production-recipe state. True = RecipeClassName is a deliberate
	 *  selection; false = "No recipe" was the captured state and restore ACTIVELY CLEARS the
	 *  production recipe (via the same path as the panel's Clear Recipe button). Migrated v2
	 *  presets with an empty RecipeClassName get CaptureFlags.bRecipe=false instead, preserving
	 *  their legacy "don't touch" semantics. */
	UPROPERTY() bool bHasProductionRecipe = false;

	// === Auto-Connect (stored when CaptureFlags.bAutoConnect is true) ===

	UPROPERTY() FSFRestoreAutoConnectState AutoConnect;

	// === Extend topology (stored for presets imported from Extend) ===

	UPROPERTY() bool bHasExtendTopology = false;
	UPROPERTY() FSFCloneTopology ExtendCloneTopology;

	// === Kind (v3, #427) ===

	/** Grid Preset vs Module. DERIVED - see NormalizeKind(). */
	UPROPERTY() ESFRestorePresetKind PresetKind = ESFRestorePresetKind::GridPreset;

	// === Metadata ===

	UPROPERTY() int32 Version = SF_RESTORE_PRESET_VERSION;
	UPROPERTY() FString Description;
	UPROPERTY() FString CreatedAt;
	UPROPERTY() FString UpdatedAt;

	/**
	 * #427: the single chokepoint that keeps Kind authoritative. Kind is derived from topology
	 * presence; bHasExtendTopology is kept as the derived back-compat shim. Called after every
	 * load, capture, AND import - an imported code carrying extendCloneTopology is correctly
	 * classified Module regardless of what its kind field claimed.
	 */
	void NormalizeKind()
	{
		const bool bIsModule = ExtendCloneTopology.ChildHolograms.Num() > 0;
		PresetKind = bIsModule ? ESFRestorePresetKind::Module : ESFRestorePresetKind::GridPreset;
		bHasExtendTopology = bIsModule;
	}

	bool IsModule() const { return PresetKind == ESFRestorePresetKind::Module; }
};
