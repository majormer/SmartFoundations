// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/EditableTextBox.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/SizeBox.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "HUD/SFHUDTypes.h"
#include "Features/Restore/SFRestoreTypes.h"
#include "SmartSettingsFormWidget.generated.h"

// Forward declarations
class USFSubsystem;
class UCanvasPanelSlot;

/**
 * Smart! Settings Form Widget
 * Provides visual interface for adjusting Smart! settings
 * 
 * Uses UFGInteractWidget as base class (correct Satisfactory pattern):
 * - Displayed via CreateWidget + AddToViewport
 * - UI components bound via BindWidget from Blueprint
 * - Populates values from CounterState on open
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USmartSettingsFormWidget : public UFGInteractWidget
{
    GENERATED_BODY()

public:
    // Called by subsystem to populate form with current values
    void PopulateFromCounterState(USFSubsystem* Subsystem);
    
    // Called by subsystem when toggle key pressed while form is open
    // Reverts unapplied changes and closes the form
    void CancelAndClose();

protected:
    // Core UI Components (bound from Blueprint)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BackgroundPanel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> ContentContainer;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    // SpinBox inputs (bound from Blueprint) - BindWidgetOptional for graceful fallback
    // Grid inputs: integer values, min 1, delta 1
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> GridXInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> GridXInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> GridYInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> GridYInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> GridZInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> GridZInputSizeBox;

    // Grid direction toggle buttons (+ or - direction per axis)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> GridXDirToggle;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> GridYDirToggle;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> GridZDirToggle;
    
    // Direction toggle button labels (show "+" or "-")
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GridXDirLabel;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GridYDirLabel;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GridZDirLabel;
    
    // === Grid Total Display & Warning ===
    
    // Grid total display (shows "Grid Total: X objects")
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GridTotalText;
    
    // Grid warning text (shows warning message for large grids)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> GridWarningText;

    // Spacing inputs: float values in meters, min 0, delta 0.5
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> SpacingXInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SpacingXInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> SpacingYInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SpacingYInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> SpacingZInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SpacingZInputSizeBox;

    // Steps inputs: float values in meters, min 0, delta 0.5
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StepsXInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StepsXInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StepsYInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StepsYInputSizeBox;

    // Stagger inputs: float values in meters, min 0, delta 0.5
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StaggerXInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StaggerXInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StaggerYInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StaggerYInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StaggerZXInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StaggerZXInputSizeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> StaggerZYInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> StaggerZYInputSizeBox;

    // Rotation inputs: float values in degrees, min -180, max 180, delta 1
    // Phase 1: Only Z-axis (horizontal arc)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> RotationZInput;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> RotationZInputSizeBox;

    // Apply button (bound from Blueprint)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UButton> ApplyBtn;
    
    // Issue #165: Reset button - zeros spacing, steps, stagger, and rotation (not grid counters)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UButton> ResetBtn;
    
    // Apply Immediately checkbox (near Apply button)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> ApplyImmediatelyCheckBox;

    // Contextual section headers and summaries (optional)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> AutoConnectHeaderText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> AutoConnectSummaryText;
    
    // === Belt Auto-Connect Controls ===
    
    // Container for belt auto-connect controls (visibility toggled based on hologram type)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> BeltAutoConnectContainer;
    
    // Belt auto-connect enabled checkbox
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> BeltEnabledCheckBox;
    
    // Belt tier for main connections (distributor-to-distributor)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> BeltTierMainComboBox;
    
    // Belt tier for building connections (distributor-to-building)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> BeltTierToBuildingComboBox;
    
    // Stackable belt direction row (visibility toggled for stackable poles only)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> StackableBeltDirectionRow;
    
    // Stackable belt direction (Forward/Backward)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> StackableBeltDirectionComboBox;
    
    // Chain distributors checkbox (manifold mode)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> BeltChainCheckBox;
    
    // Belt routing mode ComboBox (Default, Curve, Straight)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> BeltRoutingModeComboBox;
    
    // Belt routing mode row (visibility toggled for distributors)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> BeltRoutingModeRow;
    
    // Label widgets for hiding irrelevant text
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> BeltTierToBuildingLabel;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> BeltChainLabel;
    
    // Direction label for stackable poles
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StackableBeltDirectionLabel;

    // === Pipe Auto-Connect Controls ===
    
    // Container for pipe auto-connect controls (visibility toggled based on hologram type)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> PipeAutoConnectContainer;
    
    // Pipe auto-connect enabled checkbox
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> PipeEnabledCheckBox;
    
    // Pipe tier for main connections (junction-to-junction)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PipeTierMainComboBox;
    
    // Pipe tier for building connections (junction-to-building)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PipeTierToBuildingComboBox;
    
    // Label widgets for hiding irrelevant text/rows
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> PipeTierMainRow;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> PipeTierToBuildingRow;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PipeTierMainLabel;
    
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PipeTierToBuildingLabel;
    
    // Pipe flow indicator row (only shown if clean pipes are unlocked)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> PipeIndicatorRow;
    
    // Pipe flow indicator checkbox (true = with flow indicators, false = clean/no indicators)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> PipeIndicatorCheckBox;
    
    // Pipe routing mode ComboBox (Auto, Auto2D, Straight, Curve, Noodle, HorizontalToVertical)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PipeRoutingModeComboBox;
    
    // Pipe routing mode label
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PipeRoutingModeLabel;

    // === Power Auto-Connect Controls ===
    
    // Container for power auto-connect controls (visibility toggled based on hologram type)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> PowerAutoConnectContainer;
    
    // Power auto-connect enabled checkbox
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> PowerEnabledCheckBox;
    
    // Power grid axis ComboBox (Auto, X, Y, X+Y)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PowerGridAxisComboBox;
    
    // Power reserved slots ComboBox (0-5)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PowerReservedComboBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RecipeHeaderText;

    // Container for recipe details with icons (replaces RecipeSummaryText)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> RecipeDetailsContainer;

    // Recipe selection ComboBox
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> RecipeComboBox;

    // Recipe icon display (next to ComboBox)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> RecipeIcon;
    
    // Smart! logo image in header
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> SmartLogoImage;
    
    // Clear recipe button
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ClearRecipeButton;

    // === Smart Restore Controls ===

    // Restore section container (collapsible)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> RestoreContainer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> RestoreSidePanel;

    // Preset dropdown — populated from GetPresetNames()
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> PresetDropdown;

    // Preset name input for saving
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> PresetNameInput;

    // Preset description/metadata display
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> PresetDescriptionInput;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PresetCreatedAtValue;

    // Action buttons
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ApplyPresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SavePresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> DeletePresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> UpdatePresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ExportPresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ImportPresetBtn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ImportFromExtendBtn;

    // Capture checklist checkboxes
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureGridCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureSpacingCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureStepsCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureStaggerCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureRotationCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureRecipeCheckBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> CaptureAutoConnectCheckBox;

    // Section toggle for collapsible Restore section
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RestoreSectionToggle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RestoreSectionHeader;
    
    // === Confirmation Dialog Widgets ===
    
    // SizeBox container for confirmation dialog (controls size, hidden by default)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> ConfirmationSizeBox;
    
    // Border inside SizeBox for background styling
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> ConfirmationOverlay;
    
    // Confirmation dialog title text
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ConfirmationTitle;
    
    // Confirmation dialog message text
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ConfirmationMessage;
    
    // Confirmation "Continue" button
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ConfirmYesButton;
    
    // Confirmation "Cancel" button
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ConfirmNoButton;

    // Native widget lifecycle
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Auto-size the Apply/Reset/Close header buttons to their (localized) labels and re-tile
     *  the row, so longer translations (e.g. RTL) are not clipped by the fixed designer width. */
    void FitHeaderButtonRow();

    // Keyboard handling for Escape key
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // Mouse handling for dragging (right mouse button)
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    // Cached reference to subsystem for apply
    TWeakObjectPtr<USFSubsystem> CachedSubsystem;

    // Dragging support
    bool bIsDragging = false;
    bool bIsDraggingRestorePanel = false;
    FVector2D DragOffset = FVector2D::ZeroVector;

    // Cached canvas slots for moving the whole panel together
    UCanvasPanelSlot* BackgroundPanelSlot = nullptr;
    UCanvasPanelSlot* TitleTextSlot = nullptr;
    UCanvasPanelSlot* ContentContainerSlot = nullptr;
    UCanvasPanelSlot* ApplyButtonSlot = nullptr;
    UCanvasPanelSlot* ResetButtonSlot = nullptr;
    UCanvasPanelSlot* CloseButtonSlot = nullptr;
    UCanvasPanelSlot* SmartLogoSlot = nullptr;
    UCanvasPanelSlot* RestoreSidePanelSlot = nullptr;

    // Relative offsets from the background panel for header/content widgets
    FVector2D TitleOffset = FVector2D::ZeroVector;
    FVector2D ContentOffset = FVector2D::ZeroVector;
    FVector2D ApplyOffset = FVector2D::ZeroVector;
    FVector2D ResetOffset = FVector2D::ZeroVector;
    FVector2D CloseOffset = FVector2D::ZeroVector;
    FVector2D LogoOffset = FVector2D::ZeroVector;

    // Issue #231: Snapshot all SpinBox values on any change. On commit, UMG may
    // overwrite internal values from stale text. We restore from our snapshot.
    TMap<USpinBox*, float> SpinBoxValueSnapshot;
    bool bRestoringSpinBoxValues = false;

    // Grid direction state (true = positive, false = negative)
    bool bGridXPositive = true;
    bool bGridYPositive = true;
    bool bGridZPositive = true;
    
    // Apply immediately mode (false = defer to Apply button or Enter key)
    bool bApplyImmediately = false;
    
    // Extend mode flag (set in PopulateFromCounterState, applied in NativeConstruct)
    bool bIsExtendMode = false;
    
    // Tracks if Apply Immediately was enabled before being auto-disabled for large grids
    bool bApplyImmediatelyWasEnabled = false;
    
    // === Large Grid Warning Thresholds ===
    static constexpr int32 GRID_WARNING_THRESHOLD_CAUTION = 100;   // Yellow warning
    static constexpr int32 GRID_WARNING_THRESHOLD_WARNING = 500;   // Orange warning, confirmation required
    static constexpr int32 GRID_WARNING_THRESHOLD_DANGER = 1000;   // Red warning, strong confirmation required
    
    // Cached last-applied state for cancel/revert on Escape
    FSFCounterState LastAppliedState;
    bool bLastAppliedGridXPositive = true;
    bool bLastAppliedGridYPositive = true;
    bool bLastAppliedGridZPositive = true;

    // Cached recipe array for ComboBox index mapping
    TArray<TSubclassOf<class UFGRecipe>> CachedRecipeList;

    // Button click handlers
    UFUNCTION()
    void OnCloseButtonClicked();

    UFUNCTION()
    void OnApplyButtonClicked();
    
    // Issue #165: Reset spacing, steps, stagger, and rotation to zero
    UFUNCTION()
    void OnResetButtonClicked();
    
    // Actually apply the current values (called after confirmation if needed)
    void ApplyCurrentValues();

    // ComboBox selection handler
    UFUNCTION()
    void OnRecipeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Clear recipe button handler
    UFUNCTION()
    void OnClearRecipeButtonClicked();

    // === Smart Restore Handlers ===

    UFUNCTION()
    void OnApplyPresetClicked();

    UFUNCTION()
    void OnSavePresetClicked();

    UFUNCTION()
    void OnDeletePresetClicked();

    UFUNCTION()
    void OnUpdatePresetClicked();

    UFUNCTION()
    void OnExportPresetClicked();

    UFUNCTION()
    void OnImportPresetClicked();

    UFUNCTION()
    void OnImportFromExtendClicked();

    UFUNCTION()
    void OnPresetSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnRestoreSectionToggleClicked();

    // Refresh the preset dropdown from disk
    void RefreshPresetDropdown(const FString& PreferredSelection = FString());

    // Update description/timestamp display for the selected preset
    void UpdateRestorePresetDetails(const FString& PresetName);

    // Populate the main Smart Panel counter inputs from a selected Restore preset without applying them yet
    void PopulateSmartPanelFromPreset(const FSFRestorePreset& Preset);

    // Build the counter state the preset would apply, preserving current state for uncaptured groups
    FSFCounterState BuildPendingCounterStateFromPreset(const FSFRestorePreset& Preset) const;

    // Update counter input widgets from a state snapshot without triggering Apply Immediately
    void PopulateCounterInputsFromState(const FSFCounterState& State);

    // Read optional description text from the Restore panel
    FString GetPresetDescriptionText() const;

    // Format saved UTC timestamps for compact local display
    FString FormatPresetTimestampForDisplay(const FString& IsoTimestamp) const;

    // Build capture flags from checkbox state
    FSFRestoreCaptureFlags GetCaptureFlags() const;

    // Update Import from Extend button enabled state
    void UpdateExtendImportButtonState();

    // Keep Restore action text readable on light button backgrounds
    void UpdateRestoreButtonTextColors();
    
    // === Grid Direction Toggle Handlers ===
    
    UFUNCTION()
    void OnGridXDirToggleClicked();
    
    UFUNCTION()
    void OnGridYDirToggleClicked();
    
    UFUNCTION()
    void OnGridZDirToggleClicked();
    
    // Update direction label text based on state
    void UpdateGridDirectionLabel(UTextBlock* Label, bool bPositive);
    
    // === SpinBox Value Changed Handlers ===
    
    UFUNCTION()
    void OnSpinBoxValueChanged(float Value);
    
    UFUNCTION()
    void OnSpinBoxValueCommitted(float Value, ETextCommit::Type CommitMethod);
    
    // Grid SpinBox value changed - updates warning display
    UFUNCTION()
    void OnGridSpinBoxValueChanged(float Value);
    
    // === Apply Immediately Checkbox Handler ===
    
    UFUNCTION()
    void OnApplyImmediatelyChanged(bool bIsChecked);
    
    // === Form Close/Cancel Helpers ===
    
    // Close form and restore HUD/input
    void CloseForm();
    
    // Revert to last applied state (for Escape cancel)
    void RevertToLastAppliedState();
    
    // Cache current state as last applied
    void CacheCurrentStateAsApplied();
    
    // Check if any text input has keyboard focus
    bool IsAnyTextInputFocused() const;

    // Update recipe icon from selected recipe
    void UpdateRecipeIcon(TSubclassOf<class UFGRecipe> Recipe);
    
    // Populate recipe details container with icons and text
    void PopulateRecipeDetails(TSubclassOf<class UFGRecipe> Recipe);
    
    // Helper to create a recipe detail row with icon and text
    UWidget* CreateRecipeDetailRow(UTexture2D* Icon, const FString& Text, const FLinearColor& TextColor);
    
    // === Belt Auto-Connect Handlers ===
    
    // Populate belt tier ComboBoxes with unlocked tiers only
    void PopulateBeltTierComboBoxes();
    
    // Update belt auto-connect controls from runtime settings
    void UpdateBeltAutoConnectControls();
    
    // Belt enabled checkbox changed
    UFUNCTION()
    void OnBeltEnabledChanged(bool bIsChecked);
    
    // Belt tier main ComboBox changed
    UFUNCTION()
    void OnBeltTierMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Belt tier to building ComboBox changed
    UFUNCTION()
    void OnBeltTierToBuildingChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Belt chain checkbox changed
    UFUNCTION()
    void OnBeltChainChanged(bool bIsChecked);
    
    // Stackable belt direction changed (repurposed from BeltTierToBuilding)
    UFUNCTION()
    void OnStackableBeltDirectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Belt routing mode ComboBox changed
    UFUNCTION()
    void OnBeltRoutingModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Cached list of unlocked belt tiers (0=Auto, 1-6=Mk1-Mk6)
    TArray<int32> CachedBeltTiers;
    
    // Helper to convert tier value to display string
    FString BeltTierToDisplayString(int32 Tier) const;
    
    // Helper to convert display string back to tier value
    int32 DisplayStringToBeltTier(const FString& DisplayString) const;
    
    // === Pipe Auto-Connect Handlers ===
    
    // Populate pipe tier ComboBoxes with unlocked tiers only
    void PopulatePipeTierComboBoxes();
    
    // Update pipe auto-connect controls from runtime settings
    void UpdatePipeAutoConnectControls();
    
    // Update stackable belt pole auto-connect controls from runtime settings
    void UpdateStackableBeltAutoConnectControls();
    
    // Update stackable pipe support auto-connect controls from runtime settings
    void UpdateStackablePipeAutoConnectControls();
    
    // Pipe enabled checkbox changed
    UFUNCTION()
    void OnPipeEnabledChanged(bool bIsChecked);
    
    // Pipe tier main ComboBox changed
    UFUNCTION()
    void OnPipeTierMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Pipe tier to building ComboBox changed
    UFUNCTION()
    void OnPipeTierToBuildingChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Pipe indicator checkbox changed
    UFUNCTION()
    void OnPipeIndicatorChanged(bool bIsChecked);
    
    // Pipe routing mode ComboBox changed
    UFUNCTION()
    void OnPipeRoutingModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Cached list of unlocked pipe tiers (0=Auto, 1-2=Mk1-Mk2)
    TArray<int32> CachedPipeTiers;
    
    // Helper to convert pipe tier value to display string
    FString PipeTierToDisplayString(int32 Tier) const;
    
    // Helper to convert display string back to pipe tier value
    int32 DisplayStringToPipeTier(const FString& DisplayString) const;
    
    // === Power Auto-Connect Handlers ===
    
    // Populate power ComboBoxes
    void PopulatePowerComboBoxes();
    
    // Update power auto-connect controls from runtime settings
    void UpdatePowerAutoConnectControls();
    
    // Power enabled checkbox changed
    UFUNCTION()
    void OnPowerEnabledChanged(bool bIsChecked);
    
    // Power grid axis ComboBox changed
    UFUNCTION()
    void OnPowerGridAxisChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Power reserved slots ComboBox changed
    UFUNCTION()
    void OnPowerReservedChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // Helper to convert grid axis value to display string
    FString PowerGridAxisToDisplayString(int32 Axis) const;
    
    // Helper to convert display string back to grid axis value
    int32 DisplayStringToPowerGridAxis(const FString& DisplayString) const;
    
    // === Large Grid Warning System ===
    
    // Calculate current grid total from SpinBox values
    int32 CalculateGridTotal() const;
    
    // Update grid total display and warning text based on current values
    void UpdateGridWarningDisplay();
    
    // Check if confirmation is required before applying (grid >= 500)
    bool RequiresApplyConfirmation() const;
    
    // Show confirmation dialog for large grid operations
    // Returns true if user confirmed, false if cancelled
    void ShowLargeGridConfirmation(TFunction<void()> OnConfirmed);
    
    // Handle Apply Immediately auto-disable for large grids
    void UpdateApplyImmediatelyState();
    
    // Pending confirmation callback (stored while dialog is shown)
    TFunction<void()> PendingConfirmCallback;
    
    // Flag to track if we're waiting for confirmation
    bool bWaitingForConfirmation = false;
    
    // === Confirmation Dialog Handlers ===
    
    // Show the confirmation dialog with specified message
    void ShowConfirmationDialog(const FString& Title, const FString& Message, const FLinearColor& TitleColor);
    
    // Hide the confirmation dialog
    void HideConfirmationDialog();
    
    // Confirmation button handlers
    UFUNCTION()
    void OnConfirmYesClicked();
    
    UFUNCTION()
    void OnConfirmNoClicked();
};
