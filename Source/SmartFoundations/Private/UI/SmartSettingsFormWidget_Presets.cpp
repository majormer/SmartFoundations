// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USmartSettingsFormWidget - preset apply/save/delete/export/import + counter populate + apply/reset + mouse drag + recipe. Split out of SmartSettingsFormWidget.cpp (slice U1,
 * pure impl-split: only .cpp bodies move, the .h (BindWidget members + UFUNCTIONs) is byte-
 * identical, so the Smart_SettingsForm_Widget BP contract is unchanged. No behavior change.
 */

#include "UI/SmartSettingsFormWidgetImpl.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USmartSettingsFormWidget::OnApplyPresetClicked()
{
    if (!CachedSubsystem.IsValid() || !PresetDropdown)
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    const FString SelectedName = PresetDropdown->GetSelectedOption();
    if (SelectedName.IsEmpty())
    {
        return;
    }

    bool bFound = false;
    const FSFRestorePreset Preset = RestoreSvc->LoadPreset(SelectedName, bFound);
    if (!bFound)
    {
        return;
    }

    if (RestoreSvc->ApplyPreset(Preset))
    {
        const bool bWasApplyImmediately = bApplyImmediately;
        const bool bWasApplyImmediatelyEnabled = ApplyImmediatelyCheckBox ? ApplyImmediatelyCheckBox->GetIsEnabled() : true;
        PopulateFromCounterState(CachedSubsystem.Get());
        bApplyImmediately = bWasApplyImmediately;
        if (ApplyImmediatelyCheckBox)
        {
            ApplyImmediatelyCheckBox->SetIsChecked(bApplyImmediately);
            ApplyImmediatelyCheckBox->SetIsEnabled(bWasApplyImmediatelyEnabled);
        }
        CacheCurrentStateAsApplied();
    }
}

void USmartSettingsFormWidget::OnSavePresetClicked()
{
    if (!CachedSubsystem.IsValid() || !PresetNameInput)
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    const FString Name = PresetNameInput->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty())
    {
        return;
    }

    if (RestoreSvc->PresetExists(Name))
    {
        PendingConfirmCallback = [this, Name]()
        {
            USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
            if (!Svc)
            {
                return;
            }

            FSFRestorePreset Preset = Svc->CaptureCurrentState(Name, GetCaptureFlags());
            Preset.Description = GetPresetDescriptionText();
            if (Svc->SavePreset(Preset))
            {
                RefreshPresetDropdown(Name);
            }
        };
        bWaitingForConfirmation = true;
        ShowConfirmationDialog(
            LOCTEXT("Panel_Restore_OverwriteTitle", "Overwrite Preset").ToString(),
            FText::Format(LOCTEXT("Panel_Restore_OverwriteMessage", "A preset named '{0}' already exists. Overwrite?"), FText::FromString(Name)).ToString(),
            FLinearColor(1.0f, 0.6f, 0.0f, 1.0f));
        return;
    }

    FSFRestorePreset Preset = RestoreSvc->CaptureCurrentState(Name, GetCaptureFlags());
    Preset.Description = GetPresetDescriptionText();
    if (RestoreSvc->SavePreset(Preset))
    {
        RefreshPresetDropdown(Name);
    }
}

void USmartSettingsFormWidget::OnDeletePresetClicked()
{
    if (!CachedSubsystem.IsValid() || !PresetDropdown)
    {
        return;
    }

    const FString SelectedName = PresetDropdown->GetSelectedOption();
    if (SelectedName.IsEmpty())
    {
        return;
    }

    PendingConfirmCallback = [this, SelectedName]()
    {
        USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
        if (!Svc)
        {
            return;
        }

        if (Svc->DeletePreset(SelectedName))
        {
            RefreshPresetDropdown();
        }
    };
    bWaitingForConfirmation = true;
    ShowConfirmationDialog(
        LOCTEXT("Panel_Restore_DeleteTitle", "Delete Preset").ToString(),
        FText::Format(LOCTEXT("Panel_Restore_DeleteMessage", "Delete preset '{0}'?"), FText::FromString(SelectedName)).ToString(),
        FLinearColor(1.0f, 0.3f, 0.3f, 1.0f));
}

void USmartSettingsFormWidget::OnUpdatePresetClicked()
{
    if (!CachedSubsystem.IsValid() || !PresetDropdown)
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    const FString SelectedName = PresetDropdown->GetSelectedOption();
    if (SelectedName.IsEmpty())
    {
        return;
    }

    FSFRestorePreset Preset = RestoreSvc->CaptureCurrentState(SelectedName, GetCaptureFlags());
    Preset.Description = GetPresetDescriptionText();
    if (RestoreSvc->SavePreset(Preset))
    {
        RefreshPresetDropdown(SelectedName);
    }
}

void USmartSettingsFormWidget::OnExportPresetClicked()
{
    if (!CachedSubsystem.IsValid() || !PresetDropdown)
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    const FString SelectedName = PresetDropdown->GetSelectedOption();
    if (SelectedName.IsEmpty())
    {
        return;
    }

    bool bFound = false;
    const FSFRestorePreset Preset = RestoreSvc->LoadPreset(SelectedName, bFound);
    if (!bFound)
    {
        return;
    }

    const FString Encoded = RestoreSvc->ExportToString(Preset);
    FPlatformApplicationMisc::ClipboardCopy(*Encoded);
}

void USmartSettingsFormWidget::OnImportPresetClicked()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    FString ClipboardText;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
    if (ClipboardText.IsEmpty())
    {
        return;
    }

    bool bSuccess = false;
    const FSFRestorePreset Preset = RestoreSvc->ImportFromString(ClipboardText, bSuccess);
    if (!bSuccess)
    {
        return;
    }

    if (RestoreSvc->SavePreset(Preset))
    {
        RefreshPresetDropdown(Preset.Name);
    }
}

void USmartSettingsFormWidget::OnImportFromExtendClicked()
{
    UpdateExtendImportButtonState();

    if (!CachedSubsystem.IsValid() || !PresetNameInput)
    {
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore][UI] ImportFromExtend clicked but widget state is invalid: CachedSubsystem=%d PresetNameInput=%d"),
            CachedSubsystem.IsValid() ? 1 : 0,
            PresetNameInput ? 1 : 0);
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc || !RestoreSvc->IsLastExtendAvailable())
    {
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore][UI] ImportFromExtend unavailable: RestoreSvc=%s LastExtendAvailable=%d"),
            RestoreSvc ? TEXT("valid") : TEXT("null"),
            RestoreSvc ? (RestoreSvc->IsLastExtendAvailable() ? 1 : 0) : 0);
        return;
    }

    FString Name = PresetNameInput->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty())
    {
        Name = FString::Printf(TEXT("Extend Source %s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
        PresetNameInput->SetText(FText::FromString(Name));
    }

    bool bSuccess = false;
    FSFRestorePreset Preset = RestoreSvc->ImportFromLastExtend(Name, GetCaptureFlags(), bSuccess);
    if (!bSuccess)
    {
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore][UI] ImportFromLastExtend failed for preset '%s'"),
            *Name);
        return;
    }

    Preset.CaptureFlags.bGrid = true;
    Preset.GridCounters = FIntVector(1, 1, 1);
    Preset.Description = GetPresetDescriptionText();

    const bool bApplied = RestoreSvc->ApplyPreset(Preset);
    SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
        TEXT("[SmartRestore][UI] ImportFromExtend staged for editing: preset='%s' applied=%d hasTopology=%d childHolograms=%d grid=(%d,%d,%d)"),
        *Name,
        bApplied ? 1 : 0,
        Preset.bHasExtendTopology ? 1 : 0,
        Preset.ExtendCloneTopology.ChildHolograms.Num(),
        Preset.GridCounters.X,
        Preset.GridCounters.Y,
        Preset.GridCounters.Z);

    if (bApplied)
    {
        const bool bWasApplyImmediately = bApplyImmediately;
        const bool bWasApplyImmediatelyEnabled = ApplyImmediatelyCheckBox ? ApplyImmediatelyCheckBox->GetIsEnabled() : true;
        PopulateFromCounterState(CachedSubsystem.Get());
        bApplyImmediately = bWasApplyImmediately;
        if (ApplyImmediatelyCheckBox)
        {
            ApplyImmediatelyCheckBox->SetIsChecked(bApplyImmediately);
            ApplyImmediatelyCheckBox->SetIsEnabled(bWasApplyImmediatelyEnabled);
        }
        CacheCurrentStateAsApplied();
    }
}

void USmartSettingsFormWidget::OnPresetSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateRestorePresetDetails(SelectedItem);

    if (SelectionType == ESelectInfo::Direct || !CachedSubsystem.IsValid() || SelectedItem.IsEmpty())
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    bool bFound = false;
    const FSFRestorePreset Preset = RestoreSvc->LoadPreset(SelectedItem, bFound);
    if (bFound)
    {
        PopulateSmartPanelFromPreset(Preset);
    }
}

void USmartSettingsFormWidget::OnRestoreSectionToggleClicked()
{
    UWidget* RestorePanel = RestoreSidePanel ? Cast<UWidget>(RestoreSidePanel) : Cast<UWidget>(RestoreContainer);
    if (!RestorePanel)
    {
        return;
    }

    const bool bVisible = RestorePanel->GetVisibility() != ESlateVisibility::Collapsed;
    RestorePanel->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void USmartSettingsFormWidget::RefreshPresetDropdown(const FString& PreferredSelection)
{
    if (!PresetDropdown || !CachedSubsystem.IsValid())
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    const FString PreviousSelection = PresetDropdown->GetSelectedOption();
    PresetDropdown->ClearOptions();

    const TArray<FString> Names = RestoreSvc->GetPresetNames();
    for (const FString& Name : Names)
    {
        PresetDropdown->AddOption(Name);
    }

    FString SelectionToApply;
    if (!PreferredSelection.IsEmpty() && Names.Contains(PreferredSelection))
    {
        SelectionToApply = PreferredSelection;
    }
    else if (!PreviousSelection.IsEmpty() && Names.Contains(PreviousSelection))
    {
        SelectionToApply = PreviousSelection;
    }

    if (!SelectionToApply.IsEmpty())
    {
        PresetDropdown->SetSelectedOption(SelectionToApply);
        UpdateRestorePresetDetails(SelectionToApply);
    }
    else if (Names.Num() > 0)
    {
        PresetDropdown->SetSelectedIndex(0);
        UpdateRestorePresetDetails(Names[0]);
    }
    else
    {
        UpdateRestorePresetDetails(FString());
    }
}

void USmartSettingsFormWidget::UpdateRestorePresetDetails(const FString& PresetName)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    if (PresetName.IsEmpty())
    {
        if (PresetDescriptionInput)
        {
            PresetDescriptionInput->SetText(FText::GetEmpty());
        }
        if (PresetCreatedAtValue)
        {
            PresetCreatedAtValue->SetText(LOCTEXT("Panel_Restore_NotSaved", "Not saved"));
        }
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc)
    {
        return;
    }

    bool bFound = false;
    const FSFRestorePreset Preset = RestoreSvc->LoadPreset(PresetName, bFound);
    if (!bFound)
    {
        return;
    }

    if (PresetDescriptionInput)
    {
        PresetDescriptionInput->SetText(FText::FromString(Preset.Description));
    }
    if (PresetCreatedAtValue)
    {
        PresetCreatedAtValue->SetText(FText::FromString(FormatPresetTimestampForDisplay(Preset.CreatedAt)));
    }
}

void USmartSettingsFormWidget::PopulateSmartPanelFromPreset(const FSFRestorePreset& Preset)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const FSFCounterState PendingState = BuildPendingCounterStateFromPreset(Preset);
    PopulateCounterInputsFromState(PendingState);
    SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
        TEXT("[SmartRestore][UI] Loaded preset '%s' into Smart Panel pending values: grid=%d spacing=%d steps=%d stagger=%d rotation=%d"),
        *Preset.Name,
        Preset.CaptureFlags.bGrid ? 1 : 0,
        Preset.CaptureFlags.bSpacing ? 1 : 0,
        Preset.CaptureFlags.bSteps ? 1 : 0,
        Preset.CaptureFlags.bStagger ? 1 : 0,
        Preset.CaptureFlags.bRotation ? 1 : 0);
}

FSFCounterState USmartSettingsFormWidget::BuildPendingCounterStateFromPreset(const FSFRestorePreset& Preset) const
{
    FSFCounterState State = CachedSubsystem.IsValid()
        ? CachedSubsystem->GetCounterState()
        : FSFCounterState();

    if (Preset.CaptureFlags.bGrid)
    {
        State.GridCounters = Preset.GridCounters;
    }
    if (Preset.CaptureFlags.bSpacing)
    {
        State.SpacingX = Preset.SpacingX;
        State.SpacingY = Preset.SpacingY;
        State.SpacingZ = Preset.SpacingZ;
    }
    if (Preset.CaptureFlags.bSteps)
    {
        State.StepsX = Preset.StepsX;
        State.StepsY = Preset.StepsY;
    }
    if (Preset.CaptureFlags.bStagger)
    {
        State.StaggerX = Preset.StaggerX;
        State.StaggerY = Preset.StaggerY;
        State.StaggerZX = Preset.StaggerZX;
        State.StaggerZY = Preset.StaggerZY;
    }
    if (Preset.CaptureFlags.bRotation)
    {
        State.RotationZ = Preset.RotationZ;
    }

    return State;
}

void USmartSettingsFormWidget::PopulateCounterInputsFromState(const FSFCounterState& State)
{
    const bool bWasApplyImmediately = bApplyImmediately;
    bApplyImmediately = false;

    bGridXPositive = State.GridCounters.X >= 0;
    bGridYPositive = State.GridCounters.Y >= 0;
    bGridZPositive = State.GridCounters.Z >= 0;

    if (GridXInput)
    {
        GridXInput->SetValue(static_cast<float>(FMath::Abs(State.GridCounters.X)));
    }
    if (GridYInput)
    {
        GridYInput->SetValue(static_cast<float>(FMath::Abs(State.GridCounters.Y)));
    }
    if (GridZInput)
    {
        GridZInput->SetValue(static_cast<float>(FMath::Abs(State.GridCounters.Z)));
    }

    UpdateGridDirectionLabel(GridXDirLabel, bGridXPositive);
    UpdateGridDirectionLabel(GridYDirLabel, bGridYPositive);
    UpdateGridDirectionLabel(GridZDirLabel, bGridZPositive);

    if (SpacingXInput)
    {
        SpacingXInput->SetValue(State.SpacingX / 100.0f);
    }
    if (SpacingYInput)
    {
        SpacingYInput->SetValue(State.SpacingY / 100.0f);
    }
    if (SpacingZInput)
    {
        SpacingZInput->SetValue(State.SpacingZ / 100.0f);
    }

    if (StepsXInput)
    {
        StepsXInput->SetValue(State.StepsX / 100.0f);
    }
    if (StepsYInput)
    {
        StepsYInput->SetValue(State.StepsY / 100.0f);
    }

    if (StaggerXInput)
    {
        StaggerXInput->SetValue(State.StaggerX / 100.0f);
    }
    if (StaggerYInput)
    {
        StaggerYInput->SetValue(State.StaggerY / 100.0f);
    }
    if (StaggerZXInput)
    {
        StaggerZXInput->SetValue(State.StaggerZX / 100.0f);
    }
    if (StaggerZYInput)
    {
        StaggerZYInput->SetValue(State.StaggerZY / 100.0f);
    }

    if (RotationZInput)
    {
        RotationZInput->SetValue(State.RotationZ);
    }

    // [#372] Reflect the rotation progression axis (X-clones vs Y-rows). Direct selection so it
    // doesn't re-fire OnRotationAxisChanged.
    if (RotationAxisComboBox)
    {
        RotationAxisComboBox->SetSelectedIndex(State.RotationAxis == ESFScaleAxis::Y ? 1 : 0);
    }

    bApplyImmediately = bWasApplyImmediately;
    UpdateGridWarningDisplay();
}

FString USmartSettingsFormWidget::GetPresetDescriptionText() const
{
    return PresetDescriptionInput
        ? PresetDescriptionInput->GetText().ToString().TrimStartAndEnd()
        : FString();
}

FString USmartSettingsFormWidget::FormatPresetTimestampForDisplay(const FString& IsoTimestamp) const
{
    if (IsoTimestamp.IsEmpty())
    {
        return LOCTEXT("Panel_Restore_NotSaved", "Not saved").ToString();
    }

    FDateTime UtcTimestamp;
    if (!FDateTime::ParseIso8601(*IsoTimestamp, UtcTimestamp))
    {
        return IsoTimestamp;
    }

    const FTimespan LocalOffset = FDateTime::Now() - FDateTime::UtcNow();
    const FDateTime LocalTimestamp = UtcTimestamp + LocalOffset;
    return LocalTimestamp.ToString(TEXT("%Y-%m-%d %H:%M Local"));
}

FSFRestoreCaptureFlags USmartSettingsFormWidget::GetCaptureFlags() const
{
    FSFRestoreCaptureFlags Flags;
    Flags.bGrid = CaptureGridCheckBox ? CaptureGridCheckBox->IsChecked() : true;
    Flags.bSpacing = CaptureSpacingCheckBox ? CaptureSpacingCheckBox->IsChecked() : true;
    Flags.bSteps = CaptureStepsCheckBox ? CaptureStepsCheckBox->IsChecked() : true;
    Flags.bStagger = CaptureStaggerCheckBox ? CaptureStaggerCheckBox->IsChecked() : true;
    Flags.bRotation = CaptureRotationCheckBox ? CaptureRotationCheckBox->IsChecked() : true;
    Flags.bRecipe = CaptureRecipeCheckBox ? CaptureRecipeCheckBox->IsChecked() : true;
    Flags.bAutoConnect = CaptureAutoConnectCheckBox ? CaptureAutoConnectCheckBox->IsChecked() : true;
    return Flags;
}

void USmartSettingsFormWidget::UpdateExtendImportButtonState()
{
    if (!ImportFromExtendBtn)
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
    ImportFromExtendBtn->SetIsEnabled(RestoreSvc && RestoreSvc->IsLastExtendAvailable());
    UpdateRestoreButtonTextColors();
}

void USmartSettingsFormWidget::UpdateRestoreButtonTextColors()
{
    const FSlateColor BlackText(FLinearColor::Black);
    const FSlateColor DisabledImportText(FLinearColor(0.55f, 0.55f, 0.55f, 1.0f));

    auto SetTextColor = [this](const TCHAR* WidgetName, const FSlateColor& Color)
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(WidgetName))))
        {
            TextBlock->SetColorAndOpacity(Color);
        }
    };

    SetTextColor(TEXT("RestoreSectionHeader"), BlackText);
    SetTextColor(TEXT("ApplyPresetBtnText"), BlackText);
    SetTextColor(TEXT("SavePresetBtnText"), BlackText);
    SetTextColor(TEXT("DeletePresetBtnText"), BlackText);
    SetTextColor(TEXT("UpdatePresetBtnText"), BlackText);
    SetTextColor(TEXT("ExportPresetBtnText"), BlackText);
    SetTextColor(TEXT("ImportPresetBtnText"), BlackText);
    SetTextColor(TEXT("ImportFromExtendBtnText"),
        ImportFromExtendBtn && ImportFromExtendBtn->GetIsEnabled() ? BlackText : DisabledImportText);
}

void USmartSettingsFormWidget::CloseForm()
{
    // Restore HUD visibility
    if (CachedSubsystem.IsValid())
    {
        if (USFHudService* HudService = CachedSubsystem->GetHudService())
        {
            HudService->SetHUDSuppressed(false);
        }
    }

    // Restore game input mode and hide cursor
    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        PC->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
    }

    // Remove widget from viewport
    RemoveFromParent();

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Closed and input restored"));
}

void USmartSettingsFormWidget::OnApplyButtonClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Apply button clicked"));

    // Don't process if we're already waiting for confirmation
    if (bWaitingForConfirmation)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Already waiting for confirmation, ignoring apply"));
        return;
    }

    if (!CachedSubsystem.IsValid())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: No cached subsystem reference"));
        return;
    }

    // Check if confirmation is required for large grids
    if (RequiresApplyConfirmation())
    {
        // Show confirmation dialog and defer the actual apply
        ShowLargeGridConfirmation([this]()
        {
            // This lambda is called when user confirms
            ApplyCurrentValues();
        });
        return;
    }

    // No confirmation needed, apply directly
    ApplyCurrentValues();
}

void USmartSettingsFormWidget::OnResetButtonClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Reset button clicked - zeroing spacing, steps, stagger, and rotation"));

    // Zero out spacing SpinBoxes
    if (SpacingXInput) SpacingXInput->SetValue(0.0f);
    if (SpacingYInput) SpacingYInput->SetValue(0.0f);
    if (SpacingZInput) SpacingZInput->SetValue(0.0f);

    // Zero out steps SpinBoxes
    if (StepsXInput) StepsXInput->SetValue(0.0f);
    if (StepsYInput) StepsYInput->SetValue(0.0f);

    // Zero out stagger SpinBoxes
    if (StaggerXInput) StaggerXInput->SetValue(0.0f);
    if (StaggerYInput) StaggerYInput->SetValue(0.0f);
    if (StaggerZXInput) StaggerZXInput->SetValue(0.0f);
    if (StaggerZYInput) StaggerZYInput->SetValue(0.0f);

    // Zero out rotation SpinBox
    if (RotationZInput) RotationZInput->SetValue(0.0f);

    // Apply the reset values immediately (regardless of Apply Immediately mode)
    OnApplyButtonClicked();
}

void USmartSettingsFormWidget::ApplyCurrentValues()
{
    if (!CachedSubsystem.IsValid())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: No cached subsystem reference"));
        return;
    }

    // Parse values from editable text inputs
    FSFCounterState NewState = CachedSubsystem->GetCounterState();

    // Read Grid values from SpinBox (absolute count, apply direction from toggle state)
    if (GridXInput)
    {
        int32 Count = FMath::Max(1, FMath::RoundToInt(GridXInput->GetValue()));
        NewState.GridCounters.X = bGridXPositive ? Count : -Count;
    }
    if (GridYInput)
    {
        int32 Count = FMath::Max(1, FMath::RoundToInt(GridYInput->GetValue()));
        NewState.GridCounters.Y = bGridYPositive ? Count : -Count;
    }
    if (GridZInput)
    {
        int32 Count = FMath::Max(1, FMath::RoundToInt(GridZInput->GetValue()));
        NewState.GridCounters.Z = bGridZPositive ? Count : -Count;
    }

    // Read Spacing values from SpinBox (meters in UI -> centimeters in state)
    if (SpacingXInput)
    {
        NewState.SpacingX = FMath::RoundToInt(SpacingXInput->GetValue() * 100.0f);
    }
    if (SpacingYInput)
    {
        NewState.SpacingY = FMath::RoundToInt(SpacingYInput->GetValue() * 100.0f);
    }
    if (SpacingZInput)
    {
        NewState.SpacingZ = FMath::RoundToInt(SpacingZInput->GetValue() * 100.0f);
    }

    // Read Steps values from SpinBox (meters in UI -> centimeters in state)
    if (StepsXInput)
    {
        NewState.StepsX = FMath::RoundToInt(StepsXInput->GetValue() * 100.0f);
    }
    if (StepsYInput)
    {
        NewState.StepsY = FMath::RoundToInt(StepsYInput->GetValue() * 100.0f);
    }

    // Read Stagger values from SpinBox (meters in UI -> centimeters in state)
    if (StaggerXInput)
    {
        NewState.StaggerX = FMath::RoundToInt(StaggerXInput->GetValue() * 100.0f);
    }
    if (StaggerYInput)
    {
        NewState.StaggerY = FMath::RoundToInt(StaggerYInput->GetValue() * 100.0f);
    }
    if (StaggerZXInput)
    {
        NewState.StaggerZX = FMath::RoundToInt(StaggerZXInput->GetValue() * 100.0f);
    }
    if (StaggerZYInput)
    {
        NewState.StaggerZY = FMath::RoundToInt(StaggerZYInput->GetValue() * 100.0f);
    }

    // Read Rotation value from SpinBox (degrees - no conversion needed)
    if (RotationZInput)
    {
        NewState.RotationZ = RotationZInput->GetValue();
    }

    // Log parsed values before applying
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Parsed values - Grid[%d,%d,%d] Spacing[%d,%d,%d] Steps[%d,%d] Stagger[%d,%d,%d,%d] Rotation[%.1f]"),
        NewState.GridCounters.X, NewState.GridCounters.Y, NewState.GridCounters.Z,
        NewState.SpacingX, NewState.SpacingY, NewState.SpacingZ,
        NewState.StepsX, NewState.StepsY,
        NewState.StaggerX, NewState.StaggerY, NewState.StaggerZX, NewState.StaggerZY,
        NewState.RotationZ);

    // Apply the new state (updates GridStateService, local CounterState, and triggers HUD refresh)
    CachedSubsystem->UpdateCounterState(NewState);

    // Regenerate child holograms to reflect new grid counts and reposition with new spacing/steps/stagger
    // During Extend mode, UpdateCounterState already triggers OnScaledExtendStateChanged which handles regeneration
    if (!CachedSubsystem->IsExtendModeActive())
    {
        CachedSubsystem->RegenerateChildHologramGrid();
    }

    // Refresh recipe details to update grid totals with new grid size
    if (RecipeDetailsContainer && RecipeComboBox)
    {
        int32 SelectedIndex = RecipeComboBox->GetSelectedIndex();
        // Index 0 is "None Selected", actual recipes start at index 1
        if (SelectedIndex > 0 && SelectedIndex < CachedRecipeList.Num())
        {
            TSubclassOf<UFGRecipe> SelectedRecipe = CachedRecipeList[SelectedIndex];
            if (SelectedRecipe)
            {
                PopulateRecipeDetails(SelectedRecipe);
            }
        }
    }

    // Cache the applied state for cancel/revert
    CacheCurrentStateAsApplied();

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: State applied and grid regenerated"));
}

FReply USmartSettingsFormWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // #351: a right-click drag does not dismiss an open ComboBox popup. Its Slate menu is anchored
        // at open time, so it would stay put while the panel moves (a detached, misplaced dropdown).
        // Close any open menus before we start dragging.
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().DismissAllMenus();
        }

        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        if (RestoreSidePanelSlot && RestoreSidePanel && RestoreSidePanel->GetVisibility() != ESlateVisibility::Collapsed)
        {
            const FVector2D RestorePos = RestoreSidePanelSlot->GetPosition();
            const FVector2D RestoreSize = RestoreSidePanelSlot->GetSize();
            const bool bMouseOverRestorePanel =
                LocalMouse.X >= RestorePos.X && LocalMouse.X <= RestorePos.X + RestoreSize.X &&
                LocalMouse.Y >= RestorePos.Y && LocalMouse.Y <= RestorePos.Y + RestoreSize.Y;

            if (bMouseOverRestorePanel)
            {
                bIsDraggingRestorePanel = true;
                DragOffset = LocalMouse - RestorePos;
                return FReply::Handled();
            }
        }

        if (BackgroundPanelSlot)
        {
            bIsDragging = true;
            DragOffset = LocalMouse - BackgroundPanelSlot->GetPosition();
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USmartSettingsFormWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        bIsDragging = false;
        bIsDraggingRestorePanel = false;
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USmartSettingsFormWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDraggingRestorePanel && RestoreSidePanelSlot)
    {
        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        RestoreSidePanelSlot->SetPosition(LocalMouse - DragOffset);
        return FReply::Handled();
    }

    if (bIsDragging && BackgroundPanelSlot)
    {
        // [#352 restructure] The panel is one canvas child; dragging moves one slot.
        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        BackgroundPanelSlot->SetPosition(LocalMouse - DragOffset);
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
void USmartSettingsFormWidget::OnRecipeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // Ignore programmatic selection (when we set it during population)
    if (SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    if (!CachedSubsystem.IsValid() || !RecipeComboBox)
    {
        return;
    }

    int32 SelectedIndex = RecipeComboBox->GetSelectedIndex();
    if (SelectedIndex < 0 || SelectedIndex >= CachedRecipeList.Num())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Invalid recipe selection index %d"), SelectedIndex);
        return;
    }

    TSubclassOf<UFGRecipe> SelectedRecipe = CachedRecipeList[SelectedIndex];

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Recipe selected - Index=%d, Recipe=%s"),
        SelectedIndex, SelectedRecipe ? *SelectedRecipe->GetName() : TEXT("None"));

    // Check if "None Selected" was chosen (index 0, nullptr recipe)
    if (SelectedIndex == 0 || !SelectedRecipe)
    {
        // Clear the recipe - same as clicking Clear button
        CachedSubsystem->ClearAllRecipes();
        CachedSubsystem->ApplyRecipeToParentHologram();

        // Clear the recipe details
        if (RecipeDetailsContainer)
        {
            RecipeDetailsContainer->ClearChildren();
        }

        // Hide the recipe icon
        if (RecipeIcon)
        {
            RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
        }

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Recipe cleared via 'None Selected'"));
        return;
    }

    // Apply the recipe selection via subsystem
    // Note: SelectedIndex includes the "None Selected" offset, so subtract 1 for actual recipe index
    if (USFRecipeManagementService* RecipeService = CachedSubsystem->GetRecipeManagementService())
    {
        RecipeService->SetActiveRecipeByIndex(SelectedIndex - 1);

        // Update the recipe details container with icons
        if (RecipeDetailsContainer)
        {
            PopulateRecipeDetails(SelectedRecipe);
        }

        // Update recipe icon
        UpdateRecipeIcon(SelectedRecipe);
    }
}

void USmartSettingsFormWidget::OnClearRecipeButtonClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Clear Recipe button clicked"));

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Use the existing ClearAllRecipes method on the subsystem
    // This is the same logic triggered by Num0 when in Recipe mode (U held)
    CachedSubsystem->ClearAllRecipes();

    // Apply the clear to the parent hologram and trigger child regeneration
    CachedSubsystem->ApplyRecipeToParentHologram();

    // Clear the ComboBox selection
    if (RecipeComboBox)
    {
        RecipeComboBox->ClearSelection();
    }

    // Clear the recipe details
    if (RecipeDetailsContainer)
    {
        RecipeDetailsContainer->ClearChildren();
    }

    // Hide the recipe icon
    if (RecipeIcon)
    {
        RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Recipe cleared via ClearAllRecipes"));
}

void USmartSettingsFormWidget::UpdateRecipeIcon(TSubclassOf<UFGRecipe> Recipe)
{
    if (!RecipeIcon)
    {
        return;
    }

    if (!Recipe)
    {
        RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    if (USFRecipeManagementService* RecipeService = CachedSubsystem->GetRecipeManagementService())
    {
        UTexture2D* IconTexture = RecipeService->GetRecipePrimaryProductIcon(Recipe);
        if (IconTexture)
        {
            RecipeIcon->SetBrushFromTexture(IconTexture);
            // Set a fixed square size for the icon (matches ComboBox height)
            RecipeIcon->SetDesiredSizeOverride(FVector2D(40.0f, 40.0f));
            RecipeIcon->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void USmartSettingsFormWidget::PopulateRecipeDetails(TSubclassOf<UFGRecipe> Recipe)
{
    if (!RecipeDetailsContainer)
    {
        return;
    }

    // Clear existing children
    RecipeDetailsContainer->ClearChildren();

    if (!Recipe || !CachedSubsystem.IsValid())
    {
        return;
    }

    UFGRecipe* RecipeCDO = Recipe->GetDefaultObject<UFGRecipe>();
    if (!RecipeCDO)
    {
        return;
    }

    // Colors matching HUD style
    const FLinearColor OutputColor(0.7f, 0.9f, 0.7f, 1.0f);   // Light green for outputs
    const FLinearColor InputColor(0.9f, 0.8f, 0.7f, 1.0f);    // Light tan for inputs

    // Get manufacturing duration to calculate per-minute rates
    // Duration is in seconds, so items per minute = (Amount / Duration) * 60
    const float ManufacturingDuration = UFGRecipe::GetManufacturingDuration(Recipe);
    const float CyclesPerMinute = (ManufacturingDuration > 0.0f) ? (60.0f / ManufacturingDuration) : 0.0f;

    // Get grid size to calculate total output for the entire grid
    // Grid total = GridX * GridY * GridZ buildings
    // Note: Grid values can be negative (direction), so use Abs() to get the count
    int32 GridTotal = 1;
    if (CachedSubsystem.IsValid())
    {
        FSFCounterState CounterState = CachedSubsystem->GetCounterState();
        // GridCounters is an FIntVector with X, Y, Z components - use absolute values since sign indicates direction
        GridTotal = FMath::Max(1, FMath::Abs(CounterState.GridCounters.X)) *
                    FMath::Max(1, FMath::Abs(CounterState.GridCounters.Y)) *
                    FMath::Max(1, FMath::Abs(CounterState.GridCounters.Z));
    }

    // No header row needed - the ComboBox provides recipe selection
    // Go directly to output/input rows with icons
    TArray<FItemAmount> Products = RecipeCDO->GetProducts();

    // Add output rows with icons
    for (int32 i = 0; i < Products.Num(); i++)
    {
        TSubclassOf<UFGItemDescriptor> ItemClass = Products[i].ItemClass;
        int32 Amount = Products[i].Amount;
        FString ItemName = UFGItemDescriptor::GetItemName(ItemClass).ToString();

        // Get item icon
        UTexture2D* ItemIcon = UFGItemDescriptor::GetSmallIcon(ItemClass);

        // Calculate per-minute rate (single building) and grid total
        float PerMinute = Amount * CyclesPerMinute;
        float GridTotalPerMinute = PerMinute * GridTotal;

        // Format amount with per-minute rate and grid total (liquids have amounts >= 1000)
        FString AmountText;
        if (Amount >= 1000)
        {
            // Liquid: show m³ and per-minute in m³
            int32 DisplayAmount = Amount / 1000;
            float PerMinuteM3 = PerMinute / 1000.0f;
            float GridTotalM3 = GridTotalPerMinute / 1000.0f;
            AmountText = FText::Format(LOCTEXT("Panel_Recipe_OutputLiquid", "\u2192 Output: {0} x{1} m\u00B3 ({2}/min) (Grid Total: {3}/min)"), FText::FromString(ItemName), FText::AsNumber(DisplayAmount), FText::AsNumber(PerMinuteM3), FText::AsNumber(GridTotalM3)).ToString();
        }
        else
        {
            AmountText = FText::Format(LOCTEXT("Panel_Recipe_Output", "\u2192 Output: {0} x{1} ({2}/min) (Grid Total: {3}/min)"), FText::FromString(ItemName), FText::AsNumber(Amount), FText::AsNumber(PerMinute), FText::AsNumber(GridTotalPerMinute)).ToString();
        }

        UWidget* OutputRow = CreateRecipeDetailRow(ItemIcon, AmountText, OutputColor);
        if (OutputRow)
        {
            RecipeDetailsContainer->AddChild(OutputRow);
        }
    }

    // Add input rows with icons
    TArray<FItemAmount> Ingredients = RecipeCDO->GetIngredients();
    for (int32 i = 0; i < Ingredients.Num(); i++)
    {
        TSubclassOf<UFGItemDescriptor> ItemClass = Ingredients[i].ItemClass;
        int32 Amount = Ingredients[i].Amount;
        FString ItemName = UFGItemDescriptor::GetItemName(ItemClass).ToString();

        // Get item icon
        UTexture2D* ItemIcon = UFGItemDescriptor::GetSmallIcon(ItemClass);

        // Calculate per-minute rate (single building) and grid total
        float PerMinute = Amount * CyclesPerMinute;
        float GridTotalPerMinute = PerMinute * GridTotal;

        // Format amount with per-minute rate and grid total (liquids have amounts >= 1000)
        FString AmountText;
        if (Amount >= 1000)
        {
            // Liquid: show m³ and per-minute in m³
            int32 DisplayAmount = Amount / 1000;
            float PerMinuteM3 = PerMinute / 1000.0f;
            float GridTotalM3 = GridTotalPerMinute / 1000.0f;
            AmountText = FText::Format(LOCTEXT("Panel_Recipe_InputLiquid", "\u2190 Input: {0} x{1} m\u00B3 ({2}/min) (Grid Total: {3}/min)"), FText::FromString(ItemName), FText::AsNumber(DisplayAmount), FText::AsNumber(PerMinuteM3), FText::AsNumber(GridTotalM3)).ToString();
        }
        else
        {
            AmountText = FText::Format(LOCTEXT("Panel_Recipe_Input", "\u2190 Input: {0} x{1} ({2}/min) (Grid Total: {3}/min)"), FText::FromString(ItemName), FText::AsNumber(Amount), FText::AsNumber(PerMinute), FText::AsNumber(GridTotalPerMinute)).ToString();
        }

        UWidget* InputRow = CreateRecipeDetailRow(ItemIcon, AmountText, InputColor);
        if (InputRow)
        {
            RecipeDetailsContainer->AddChild(InputRow);
        }
    }
}

UWidget* USmartSettingsFormWidget::CreateRecipeDetailRow(UTexture2D* Icon, const FString& Text, const FLinearColor& TextColor)
{
    // Create horizontal box for the row
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    if (!Row)
    {
        return nullptr;
    }

    // Add icon if provided
    if (Icon)
    {
        USizeBox* IconSizeBox = NewObject<USizeBox>(this);
        if (IconSizeBox)
        {
            IconSizeBox->SetWidthOverride(24.0f);
            IconSizeBox->SetHeightOverride(24.0f);

            UImage* IconImage = NewObject<UImage>(this);
            if (IconImage)
            {
                IconImage->SetBrushFromTexture(Icon);
                IconSizeBox->AddChild(IconImage);
            }

            UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSizeBox);
            if (IconSlot)
            {
                IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));  // Right padding
                IconSlot->SetVerticalAlignment(VAlign_Center);
            }
        }
    }

    // Add text
    UTextBlock* TextWidget = NewObject<UTextBlock>(this);
    if (TextWidget)
    {
        TextWidget->SetText(FText::FromString(Text));
        TextWidget->SetColorAndOpacity(FSlateColor(TextColor));

        // Set font to match HUD style (in-game multi-script font)
        TextWidget->SetFont(SFFont::Get(14));

        UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextWidget);
        if (TextSlot)
        {
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    return Row;
}

// ============================================================================
// Belt Auto-Connect Controls
// ============================================================================

#undef LOCTEXT_NAMESPACE
