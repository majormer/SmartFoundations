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
    // #427 Grid Preset "Apply & Build": switch the build gun to the preset's building and apply
    // recipe/auto-connect (service), then RESTORE = SET THE PANEL - values go in as pending and
    // the panel's own apply path commits them, honoring the Apply Immediately toggle. A restored
    // preset behaves exactly like the user typed those values (per-build one-shot, #371).
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

    if (RestoreSvc->ApplyPreset(Preset, /*bIncludeCounterState*/ false))
    {
        // Stage the preset's values into the panel inputs (pending, uncommitted).
        PopulateSmartPanelFromPreset(Preset);

        if (bApplyImmediately)
        {
            // Commit like pressing Apply (includes the large-grid confirmation gate).
            OnApplyButtonClicked();
        }
        else
        {
            SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
                TEXT("[SmartRestore][UI] Preset '%s' staged in panel (Apply Immediately off - fine-tune then Apply)"),
                *Preset.Name);
        }
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
        // #427: no silent no-ops - the empty-name save was the classic "I pressed Save and
        // nothing happened" (FoundationFun, 2026-07-03). Tell the user what's missing.
        if (GridPresetDetailsText)
        {
            GridPresetDetailsText->SetText(LOCTEXT("Restore_SaveNeedsName",
                "Enter a name in the 'New preset name' field below, then press Save from Panel."));
        }
        return;
    }

    // #427 flush-then-capture: Save records what's ON SCREEN (the parsed panel inputs), not the
    // possibly-stale committed state. Grid-tab saves are always pure Grid Presets - strip any
    // staged Extend topology so a save during a Module stamp session can't silently become a
    // Module (the Modules tab owns that flow).
    auto CaptureGridPresetFromPanel = [this](USFRestoreService* Svc, const FString& PresetName) -> FSFRestorePreset
    {
        FSFRestorePreset Preset = Svc->CapturePanelState(PresetName, GetCaptureFlags(), ReadPanelCounterState());
        Preset.ExtendCloneTopology = FSFCloneTopology();
        Preset.NormalizeKind();
        Preset.Description = GetPresetDescriptionText();
        return Preset;
    };

    if (RestoreSvc->PresetExists(Name))
    {
        PendingConfirmCallback = [this, Name, CaptureGridPresetFromPanel]()
        {
            USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
            if (!Svc)
            {
                return;
            }

            const FSFRestorePreset Preset = CaptureGridPresetFromPanel(Svc, Name);
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

    const FSFRestorePreset Preset = CaptureGridPresetFromPanel(RestoreSvc, Name);
    if (RestoreSvc->SavePreset(Preset))
    {
        // Refresh selects the new preset; prepend an explicit acknowledgement to its details.
        RefreshPresetDropdown(Name);
        if (GridPresetDetailsText)
        {
            GridPresetDetailsText->SetText(FText::Format(LOCTEXT("Restore_SavedAck", "Saved '{0}'.\n{1}"),
                FText::FromString(Name), GridPresetDetailsText->GetText()));
        }
    }
    else if (GridPresetDetailsText)
    {
        GridPresetDetailsText->SetText(FText::Format(LOCTEXT("Restore_SaveFailed",
            "Could not save '{0}' - check the log (name may collide with another preset's file)."),
            FText::FromString(Name)));
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
    // #427: Update now CONFIRMS before clobbering (it was the one destructive action without a
    // dialog - Save-overwrite and Delete both had one), captures the ON-SCREEN panel values
    // (flush-then-capture), and stays a pure Grid Preset (topology stripped).
    if (!CachedSubsystem.IsValid() || !PresetDropdown || bWaitingForConfirmation)
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

    PendingConfirmCallback = [this, SelectedName]()
    {
        USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
        if (!Svc)
        {
            return;
        }

        FSFRestorePreset Preset = Svc->CapturePanelState(SelectedName, GetCaptureFlags(), ReadPanelCounterState());
        Preset.ExtendCloneTopology = FSFCloneTopology();
        Preset.NormalizeKind();
        Preset.Description = GetPresetDescriptionText();
        if (Svc->SavePreset(Preset))
        {
            RefreshPresetDropdown(SelectedName);
        }
    };
    bWaitingForConfirmation = true;
    ShowConfirmationDialog(
        LOCTEXT("Panel_Restore_UpdateTitle", "Update Preset").ToString(),
        FText::Format(LOCTEXT("Panel_Restore_UpdateMessage", "Overwrite preset '{0}' with the current panel values?"), FText::FromString(SelectedName)).ToString(),
        FLinearColor(1.0f, 0.6f, 0.0f, 1.0f));
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
        // #427: route by kind - a pasted Module code lands on (and switches to) the Modules tab,
        // a Grid Preset code on the Grid Presets tab. The code self-identifies via its topology.
        if (Preset.IsModule())
        {
            RefreshModuleDropdown(Preset.Name);
            SetActiveRestoreTab(1);
        }
        else
        {
            RefreshPresetDropdown(Preset.Name);
            SetActiveRestoreTab(0);
        }
    }
}

void USmartSettingsFormWidget::OnImportFromExtendClicked()
{
    // #427 "Save as Module": promote the Extend clipboard (the transient capture buffer - a live
    // preview counts, not just a built Extend) into a saved Module in the library. Capture + save
    // ONLY - no apply. Applying is the library's explicit Apply action, which enters the stamp
    // session. Replaces the old flow that captured AND immediately applied.
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
    if (!RestoreSvc || !RestoreSvc->IsLastExtendAvailable())
    {
        UpdateClipboardSlot();
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore][UI] Save-as-Module unavailable: RestoreSvc=%s LastExtendAvailable=%d"),
            RestoreSvc ? TEXT("valid") : TEXT("null"),
            RestoreSvc ? (RestoreSvc->IsLastExtendAvailable() ? 1 : 0) : 0);
        return;
    }

    FString Name = ModuleNameInput ? ModuleNameInput->GetText().ToString().TrimStartAndEnd() : FString();
    if (Name.IsEmpty())
    {
        Name = FString::Printf(TEXT("Module %s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
        if (ModuleNameInput)
        {
            ModuleNameInput->SetText(FText::FromString(Name));
        }
    }

    bool bSuccess = false;
    FSFRestorePreset Preset = RestoreSvc->ImportFromLastExtend(Name, FSFRestoreCaptureFlags(), bSuccess);
    if (!bSuccess)
    {
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Warning,
            TEXT("[SmartRestore][UI] Save-as-Module: ImportFromLastExtend failed for '%s'"), *Name);
        return;
    }

    // Unit semantics: a Module stores ONE clone unit at 1x1x1; applying re-enters the scalable
    // session and you size it there (capture-at-1-clone is a fully useful Module).
    Preset.CaptureFlags.bGrid = true;
    Preset.GridCounters = FIntVector(1, 1, 1);

    if (RestoreSvc->SavePreset(Preset))
    {
        SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
            TEXT("[SmartRestore][UI] Saved Module '%s' (%d parts) - no apply; use the library's Apply to stamp it"),
            *Name, Preset.ExtendCloneTopology.ChildHolograms.Num());
        RefreshModuleDropdown(Name);
        SetActiveRestoreTab(1);
        UpdateClipboardSlot();
    }
}

void USmartSettingsFormWidget::OnPresetSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // #427: selecting ONLY shows details (read-only pane). The old auto-load into the panel is
    // gone - it made users believe selection had already applied the preset. Loading is now the
    // explicit "Load to Panel" action; committing is "Apply & Build". Note: the old
    // UpdateRestorePresetDetails is deliberately NOT called - it wrote the selected preset's
    // description into the AUTHORING input (the overloaded-description-box bug).
    UpdateGridPresetDetails(SelectedItem);
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

    // [#427] On open: dock to the main panel and refresh both tabs + the clipboard slot.
    if (!bVisible)
    {
        UpdateRestoreDockPosition();
        RefreshPresetDropdown();
        RefreshModuleDropdown();
        UpdateClipboardSlot();
    }
}

void USmartSettingsFormWidget::RefreshPresetDropdown(const FString& PreferredSelection)
{
    // #427: this list is the GRID PRESETS tab - kind-filtered. Modules live in their own list
    // (RefreshModuleDropdown); the tab itself conveys the kind, so no per-row marker is needed.
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

    TArray<FString> Names;
    for (const FSFRestorePreset& Preset : RestoreSvc->LoadAllPresets())
    {
        if (!Preset.IsModule())
        {
            Names.Add(Preset.Name);
            PresetDropdown->AddOption(Preset.Name);
        }
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
    else if (Names.Num() > 0)
    {
        SelectionToApply = Names[0];
    }

    if (!SelectionToApply.IsEmpty())
    {
        PresetDropdown->SetSelectedOption(SelectionToApply);
    }
    UpdateGridPresetDetails(SelectionToApply);
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
        // [#372/#419] progression axis rides with rotation; PopulateCounterInputsFromState
        // reflects it into RotationAxisComboBox so it round-trips through the panel apply.
        State.RotationAxis = Preset.RotationAxis;
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
    // #427 FULL SNAPSHOT (decided): a Grid Preset is a complete snapshot of the panel - grid,
    // transforms, axis/mode selectors, recipes, and auto-connect. The selective 7-checkbox
    // capture is retired from the UI (the checkboxes are orphaned by the tab rebuild); OLD
    // partial presets still restore only their captured groups via their stored flags.
    return FSFRestoreCaptureFlags();
}

void USmartSettingsFormWidget::UpdateExtendImportButtonState()
{
    // [#427] The old greyed-with-no-reason Import-from-Extend button became the "Save as Module"
    // action inside the visible clipboard slot, which shows WHY it's unavailable.
    UpdateClipboardSlot();
    UpdateRestoreButtonTextColors();
}

void USmartSettingsFormWidget::UpdateRestoreButtonTextColors()
{
    // #427 restyle: the Restore buttons now use the DARK Smart-Panel-aligned style, so their
    // labels are LIGHT (the old black-on-light scheme became black-on-dark = invisible).
    const FSlateColor LightText(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));
    const FSlateColor DisabledText(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f));
    const FSlateColor BlackText(FLinearColor::Black);

    auto SetTextColor = [this](const TCHAR* WidgetName, const FSlateColor& Color)
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(FName(WidgetName))))
        {
            TextBlock->SetColorAndOpacity(Color);
        }
    };

    // These two live on the MAIN panel's light-grey buttons - keep black there.
    SetTextColor(TEXT("RestoreSectionHeader"), BlackText);
    SetTextColor(TEXT("WalkPathLabel"), BlackText);   // #356 entry button on the main panel

    SetTextColor(TEXT("ApplyPresetBtnText"), LightText);   // orphaned (Apply & Build retired), harmless
    SetTextColor(TEXT("SavePresetBtnText"), LightText);
    SetTextColor(TEXT("DeletePresetBtnText"), LightText);
    SetTextColor(TEXT("UpdatePresetBtnText"), LightText);
    SetTextColor(TEXT("ExportPresetBtnText"), LightText);
    SetTextColor(TEXT("ImportPresetBtnText"), LightText);
    SetTextColor(TEXT("ImportFromExtendBtnText"),
        ImportFromExtendBtn && ImportFromExtendBtn->GetIsEnabled() ? LightText : DisabledText);
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

FSFCounterState USmartSettingsFormWidget::ReadPanelCounterState() const
{
    // Parse the panel inputs into a counter state WITHOUT committing anything. Shared by the
    // panel apply path and the #427 flush-then-capture Save path (so Save records what's on
    // screen, side-effect free).
    FSFCounterState NewState = CachedSubsystem.IsValid()
        ? CachedSubsystem->GetCounterState()
        : FSFCounterState();

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

    // [#372/#427] The rotation progression axis combo is a PENDING input like the spinboxes -
    // read it here so a preset's staged axis commits with Apply (and Save captures it).
    if (RotationAxisComboBox)
    {
        NewState.RotationAxis = RotationAxisComboBox->GetSelectedIndex() == 1 ? ESFScaleAxis::Y : ESFScaleAxis::X;
    }

    return NewState;
}

void USmartSettingsFormWidget::ApplyCurrentValues()
{
    if (!CachedSubsystem.IsValid())
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: No cached subsystem reference"));
        return;
    }

    // Parse values from the panel inputs (shared with the flush-then-capture Save path)
    FSFCounterState NewState = ReadPanelCounterState();

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

        // [#427/Q8] The Restore panel is DOCKED - a right-drag anywhere (including over the
        // Restore region) moves the whole unit via the main panel's slot.
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
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USmartSettingsFormWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging && BackgroundPanelSlot)
    {
        // [#352 restructure] The panel is one canvas child; dragging moves one slot.
        // [#427/Q8] The Restore side panel rides along - one draggable unit.
        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        BackgroundPanelSlot->SetPosition(LocalMouse - DragOffset);
        UpdateRestoreDockPosition();
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
