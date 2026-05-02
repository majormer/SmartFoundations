// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFRestoreService - Smart Restore Enhanced Service
 *
 * Named preset system that captures and restores Smart Panel configuration.
 * Handles capture, apply, JSON persistence, compact string export/import,
 * and integration with Extend topology for "Import from Last Extend".
 *
 * Architecture:
 * - Reads from USFSubsystem (CounterState, ActiveRecipe, AutoConnectRuntimeSettings)
 * - Writes to USFSubsystem (UpdateCounterState, SetBuildGunRecipe, etc.)
 * - JSON file storage at {GameUserDir}/SmartFoundations/Presets/
 * - Compact sharing via SR1:<base64> format
 *
 * GUI wiring (Smart Panel dropdown, capture checklist dialog) is separate —
 * this service provides the backend that the UI calls into.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "SFRestoreService.generated.h"

class USFSubsystem;

UCLASS()
class SMARTFOUNDATIONS_API USFRestoreService : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize with owning subsystem reference */
	void Initialize(USFSubsystem* InSubsystem);

	// ==================== Capture ====================

	/**
	 * Capture the current Smart Panel state into a new preset.
	 * Reads grid, spacing, steps, stagger, rotation from CounterState,
	 * recipe from the recipe management service, and auto-connect from
	 * the runtime settings. Only populates fields whose CaptureFlag is true.
	 *
	 * @param Name          User-defined preset name
	 * @param CaptureFlags  Which field groups to include
	 * @return The captured preset (not yet persisted — call SavePreset to store)
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	FSFRestorePreset CaptureCurrentState(
		const FString& Name,
		const FSFRestoreCaptureFlags& CaptureFlags) const;

	// ==================== Apply ====================

	/**
	 * Apply a preset to the current Smart Panel state.
	 * Auto-switches the build gun to the preset's building class.
	 * Fields whose CaptureFlag was false when saved are left untouched.
	 *
	 * @param Preset  The preset to apply
	 * @return true if the preset was applied successfully, false if the
	 *         building class is unavailable or recipe lookup failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	bool ApplyPreset(const FSFRestorePreset& Preset);

	// ==================== Persistence (JSON) ====================

	/** Save a preset to disk as JSON. Overwrites if a preset with the same name exists. */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	bool SavePreset(const FSFRestorePreset& Preset);

	/** Delete a preset by name. Returns true if the file was removed. */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	bool DeletePreset(const FString& Name);

	/** Load all presets from disk. */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	TArray<FSFRestorePreset> LoadAllPresets() const;

	/** Load a single preset by name. Returns default preset if not found. */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	FSFRestorePreset LoadPreset(const FString& Name, bool& bOutFound) const;

	/** Get the list of all saved preset names (without loading full data). */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	TArray<FString> GetPresetNames() const;

	/** Check if a preset with this name already exists on disk. */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	bool PresetExists(const FString& Name) const;

	// ==================== Export / Import (compact string) ====================

	/**
	 * Export a preset to a compact clipboard-friendly string.
	 * Format: SR1:<base64-encoded-json>
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	FString ExportToString(const FSFRestorePreset& Preset) const;

	/**
	 * Import a preset from a compact string.
	 * @param Encoded  The SR1:... string
	 * @param bOutSuccess  Set to true if import succeeded
	 * @return The imported preset (only valid if bOutSuccess is true)
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	FSFRestorePreset ImportFromString(const FString& Encoded, bool& bOutSuccess) const;

	// ==================== Extend Integration ====================

	/**
	 * Check if the last Extend topology is available for import.
	 * @return true if GetCurrentTopology().bIsValid on the Extend service
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	bool IsLastExtendAvailable() const;

	/**
	 * Import from the last Extend topology.
	 * Pre-populates BuildingClassName from the source factory and recipe
	 * from the source factory's current production recipe.
	 * Grid/transform fields come from the current CounterState.
	 *
	 * @param Name          Preset name for the imported preset
	 * @param CaptureFlags  Which field groups to include
	 * @param bOutSuccess   Set to true if import succeeded
	 * @return The preset (only valid if bOutSuccess is true)
	 */
	UFUNCTION(BlueprintCallable, Category = "Smart|Restore")
	FSFRestorePreset ImportFromLastExtend(
		const FString& Name,
		const FSFRestoreCaptureFlags& CaptureFlags,
		bool& bOutSuccess) const;

	bool IsRestoreSessionActive() const { return bRestoreSessionActive; }
	const FString& GetActiveRestorePresetName() const { return ActiveRestorePresetName; }
	void ClearActiveRestoreSession(const TCHAR* Reason);

private:
	TWeakObjectPtr<USFSubsystem> Subsystem;
	bool bRestoreSessionActive = false;
	FString ActiveRestorePresetName;

	/** Get or create the preset storage directory */
	FString GetPresetsDir() const;

	/** Serialize a preset to a JSON object */
	TSharedPtr<FJsonObject> PresetToJson(const FSFRestorePreset& Preset) const;

	/** Deserialize a preset from a JSON object */
	bool JsonToPreset(const TSharedPtr<FJsonObject>& JsonObj, FSFRestorePreset& OutPreset) const;

	/** Sanitize a preset name for use as a filename (alphanumeric + underscore) */
	FString SanitizeFileName(const FString& Name) const;

	/** Get the full file path for a preset by name */
	FString GetPresetFilePath(const FString& Name) const;

	/** Validate that all preset recipes/buildables are unlocked in the current game state. */
	bool ValidatePresetUnlocks(const FSFRestorePreset& Preset, FString& OutFailureReason) const;

	void ReplayExtendTopologyWhenHologramReady(const FSFRestorePreset& Preset, int32 AttemptsRemaining, int32 SettleTicksRemaining);
};
