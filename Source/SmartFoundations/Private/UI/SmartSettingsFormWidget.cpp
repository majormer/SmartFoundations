// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SmartSettingsFormWidget.h"
#include "UI/SmartSettingsFormWidgetImpl.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USmartSettingsFormWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: NativeConstruct called"));

    // Switch designer-placed (and localized) labels/fields to the in-game multi-script font.
    SFFont::ApplyToWidgetTree(WidgetTree);

    // The Apply/Reset/Close header buttons are fixed-width in the designer (sized for English),
    // so longer localized labels (e.g. Arabic) get clipped. Fit them to content next tick, once
    // the localized labels, font, and first layout pass are all in place.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &USmartSettingsFormWidget::FitHeaderButtonRow));
    }

    // Check if Blueprint widgets are bound
    if (!BackgroundPanel)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: BackgroundPanel not bound from Blueprint"));
        return;
    }

    if (!TitleText)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: TitleText not bound from Blueprint"));
        return;
    }

    if (!ContentContainer)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: ContentContainer not bound from Blueprint"));
        return;
    }

    if (!CloseButton)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: CloseButton not bound from Blueprint"));
        return;
    }

    // Set up button handlers
    CloseButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnCloseButtonClicked);

    if (ApplyBtn)
    {
        ApplyBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnApplyButtonClicked);
    }

    // Issue #165: Reset button - zeros spacing, steps, stagger, and rotation
    if (ResetBtn)
    {
        ResetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnResetButtonClicked);
    }

    // #356 Smart Walking entry button
    if (WalkPathButton)
    {
        WalkPathButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnWalkPathButtonClicked);
        // Style ("Smart Restore"-matching grey box) and the "Smart Walking" label now live in the Blueprint
        // (WalkPathButton.WidgetStyle + WalkPathLabel text/colour) — no fragile runtime GetChildAt override.
        // Contextual: only show Walk Path for a seed that can start a walk (stackable belt/pipe support). Collapsed
        // otherwise so it takes NO space — the panel never widens for it, and whatever's below (Smart Restore) reflows up.
        USFSubsystem* WalkSub = USFSubsystem::Get(GetWorld());
        WalkPathButton->SetVisibility((WalkSub && WalkSub->IsCurrentHologramWalkable())
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // Set up confirmation dialog button handlers
    if (ConfirmYesButton)
    {
        ConfirmYesButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnConfirmYesClicked);
    }
    if (ConfirmNoButton)
    {
        ConfirmNoButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnConfirmNoClicked);
    }

    // Hide confirmation dialog by default
    if (ConfirmationSizeBox)
    {
        ConfirmationSizeBox->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (PresetNameInput)
    {
        PresetNameInput->WidgetStyle.TextStyle.Font.Size = 10;
        PresetNameInput->SetMinDesiredWidth(220.0f);
        PresetNameInput->SynchronizeProperties();
    }
    if (PresetDescriptionInput)
    {
        PresetDescriptionInput->WidgetStyle.TextStyle.Font.Size = 10;
        PresetDescriptionInput->WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(FLinearColor::Black);
        PresetDescriptionInput->WidgetStyle.ForegroundColor = FSlateColor(FLinearColor::Black);
        PresetDescriptionInput->SetMinDesiredWidth(220.0f);
        PresetDescriptionInput->SynchronizeProperties();
    }

    // Set default title
    TitleText->SetText(LOCTEXT("Panel_Title", "Smart! Panel"));

    // Configure widget to block all mouse input
    SetIsFocusable(true);

    // Configure Grid SpinBox inputs (min 1, integer values)
    // Slider capped at 100, but typing allows up to 999
    auto ConfigureGridSpinBox = [this](USpinBox* SpinBox)
    {
        if (SpinBox)
        {
            SpinBox->SetForegroundColor(FSlateColor(FLinearColor::Black));
            SpinBox->SetMinValue(1.0f);
            SpinBox->SetMaxValue(999.0f);
            SpinBox->SetMinSliderValue(1.0f);
            SpinBox->SetMaxSliderValue(100.0f);  // Spin capped at 100, type for higher
            SpinBox->SetDelta(1.0f);
            SpinBox->SetMinFractionalDigits(0);
            SpinBox->SetMaxFractionalDigits(0);
            SpinBox->SetMinDesiredWidth(400.0f);  // Wider for easier interaction

            // Increase font size for readability
            SpinBox->SetFont(SFFont::Get(18));

            SpinBox->OnValueCommitted.AddDynamic(this, &USmartSettingsFormWidget::OnSpinBoxValueCommitted);
            SpinBox->OnValueChanged.AddDynamic(this, &USmartSettingsFormWidget::OnGridSpinBoxValueChanged);
        }
    };

    // Configure Spacing/Steps/Stagger SpinBox inputs (allow negative, float values)
    auto ConfigureFloatSpinBox = [this](USpinBox* SpinBox)
    {
        if (SpinBox)
        {
            SpinBox->SetForegroundColor(FSlateColor(FLinearColor::Black));
            SpinBox->SetMinValue(-999.0f);
            SpinBox->SetMaxValue(999.0f);
            SpinBox->SetMinSliderValue(-30.0f);
            SpinBox->SetMaxSliderValue(30.0f);
            SpinBox->SetDelta(0.5f);
            SpinBox->SetMinFractionalDigits(1);
            SpinBox->SetMaxFractionalDigits(1);
            SpinBox->SetMinDesiredWidth(400.0f);  // Wider for easier interaction

            // Increase font size for readability
            SpinBox->SetFont(SFFont::Get(18));

            SpinBox->OnValueCommitted.AddDynamic(this, &USmartSettingsFormWidget::OnSpinBoxValueCommitted);
            SpinBox->OnValueChanged.AddDynamic(this, &USmartSettingsFormWidget::OnSpinBoxValueChanged);
        }
    };

    // Grid inputs: minimum 1, integer values (direction handled by toggle)
    ConfigureGridSpinBox(GridXInput);
    ConfigureGridSpinBox(GridYInput);
    ConfigureGridSpinBox(GridZInput);

    // Spacing/Steps/Stagger inputs: minimum 0, float values in meters
    ConfigureFloatSpinBox(SpacingXInput);
    ConfigureFloatSpinBox(SpacingYInput);
    ConfigureFloatSpinBox(SpacingZInput);
    ConfigureFloatSpinBox(StepsXInput);
    ConfigureFloatSpinBox(StepsYInput);
    ConfigureFloatSpinBox(StaggerXInput);
    ConfigureFloatSpinBox(StaggerYInput);
    ConfigureFloatSpinBox(StaggerZXInput);
    ConfigureFloatSpinBox(StaggerZYInput);

    // Configure Rotation SpinBox (degrees, not meters)
    auto ConfigureRotationSpinBox = [this](USpinBox* SpinBox)
    {
        if (SpinBox)
        {
            SpinBox->SetForegroundColor(FSlateColor(FLinearColor::Black));
            SpinBox->SetMinValue(-180.0f);
            SpinBox->SetMaxValue(180.0f);
            SpinBox->SetMinSliderValue(-45.0f);
            SpinBox->SetMaxSliderValue(45.0f);
            SpinBox->SetDelta(1.0f);  // 1 degree per step
            SpinBox->SetMinFractionalDigits(1);
            SpinBox->SetMaxFractionalDigits(1);
            SpinBox->SetMinDesiredWidth(400.0f);

            SpinBox->SetFont(SFFont::Get(18));

            SpinBox->OnValueCommitted.AddDynamic(this, &USmartSettingsFormWidget::OnSpinBoxValueCommitted);
            SpinBox->OnValueChanged.AddDynamic(this, &USmartSettingsFormWidget::OnSpinBoxValueChanged);
        }
    };
    ConfigureRotationSpinBox(RotationZInput);

    // Configure SizeBox widths for all SpinBoxes (override blueprint constraints)
    auto ConfigureSizeBox = [](USizeBox* SizeBox)
    {
        if (SizeBox)
        {
            SizeBox->SetWidthOverride(150.0f);
        }
    };

    ConfigureSizeBox(GridXInputSizeBox);
    ConfigureSizeBox(GridYInputSizeBox);
    ConfigureSizeBox(GridZInputSizeBox);
    ConfigureSizeBox(SpacingXInputSizeBox);
    ConfigureSizeBox(SpacingYInputSizeBox);
    ConfigureSizeBox(SpacingZInputSizeBox);
    ConfigureSizeBox(StepsXInputSizeBox);
    ConfigureSizeBox(StepsYInputSizeBox);
    ConfigureSizeBox(StaggerXInputSizeBox);
    ConfigureSizeBox(StaggerYInputSizeBox);
    ConfigureSizeBox(StaggerZXInputSizeBox);
    ConfigureSizeBox(StaggerZYInputSizeBox);
    ConfigureSizeBox(RotationZInputSizeBox);

    // Configure ComboBox styles to use standard Box instead of RoundedBox
    // Also copies the WidgetStyle (button appearance) from a reference ComboBox
    auto ConfigureComboBoxStyle = [](UComboBoxString* ComboBox, UComboBoxString* ReferenceComboBox = nullptr)
    {
        if (!ComboBox) return;

        // Copy WidgetStyle from reference if provided (for consistent button appearance)
        if (ReferenceComboBox)
        {
            ComboBox->SetWidgetStyle(ReferenceComboBox->GetWidgetStyle());
        }

        // Get the current item style and modify the brushes to use Box instead of RoundedBox
        FTableRowStyle ItemStyle = ComboBox->GetItemStyle();

        // Change active/inactive brushes from RoundedBox to Box
        ItemStyle.ActiveBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.ActiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.InactiveBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.InactiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.EvenRowBackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.EvenRowBackgroundHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.OddRowBackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
        ItemStyle.OddRowBackgroundHoveredBrush.DrawAs = ESlateBrushDrawType::Box;

        ComboBox->SetItemStyle(ItemStyle);
    };

    ConfigureComboBoxStyle(RecipeComboBox);
    ConfigureComboBoxStyle(BeltTierMainComboBox);
    ConfigureComboBoxStyle(BeltTierToBuildingComboBox);
    ConfigureComboBoxStyle(StackableBeltDirectionComboBox, BeltTierToBuildingComboBox);  // Copy style from Factory Belt
    ConfigureComboBoxStyle(PipeTierMainComboBox);
    ConfigureComboBoxStyle(PipeTierToBuildingComboBox);
    ConfigureComboBoxStyle(PipeRoutingModeComboBox, PipeTierToBuildingComboBox);  // Copy style from To Building
    ConfigureComboBoxStyle(HypertubeRoutingModeComboBox, PipeRoutingModeComboBox);  // [#405] Copy style from Pipe Routing Mode
    ConfigureComboBoxStyle(BeltRoutingModeComboBox, PipeRoutingModeComboBox);  // Copy style from Pipe Routing Mode
    ConfigureComboBoxStyle(PowerGridAxisComboBox);
    ConfigureComboBoxStyle(PowerReservedComboBox);
    ConfigureComboBoxStyle(PresetDropdown, BeltTierMainComboBox);
    ConfigureComboBoxStyle(RotationAxisComboBox, BeltTierMainComboBox);  // [#372] copy belt-combo style

    // Bind Apply Immediately checkbox
    // NOTE: Do NOT set checkbox state here - PopulateFromCounterState() is called BEFORE NativeConstruct
    // and correctly initializes bApplyImmediately from global config. Setting it here would reset to false.
    if (ApplyImmediatelyCheckBox)
    {
        ApplyImmediatelyCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnApplyImmediatelyChanged);
    }

    // NOTE: Contextual text blocks (AutoConnect*, Recipe*) visibility is managed
    // by PopulateFromCounterState(), which is called BEFORE NativeConstruct.
    // Do NOT reset visibility here or it will override the populated state.

    // Bind grid direction toggle buttons
    if (GridXDirToggle)
    {
        GridXDirToggle->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnGridXDirToggleClicked);
    }
    if (GridYDirToggle)
    {
        GridYDirToggle->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnGridYDirToggleClicked);
    }
    if (GridZDirToggle)
    {
        GridZDirToggle->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnGridZDirToggleClicked);
    }

    // Bind recipe ComboBox selection handler
    // Note: ComboBox styling (WidgetStyle, ItemStyle) is configured in the Blueprint
    if (RecipeComboBox)
    {
        RecipeComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnRecipeSelectionChanged);
    }

    // Bind clear recipe button
    if (ClearRecipeButton)
    {
        ClearRecipeButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnClearRecipeButtonClicked);
    }

    if (ApplyPresetBtn)
    {
        ApplyPresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnApplyPresetClicked);
    }
    if (SavePresetBtn)
    {
        SavePresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnSavePresetClicked);
    }
    if (DeletePresetBtn)
    {
        DeletePresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnDeletePresetClicked);
    }
    if (UpdatePresetBtn)
    {
        UpdatePresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnUpdatePresetClicked);
    }
    if (ExportPresetBtn)
    {
        ExportPresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnExportPresetClicked);
    }
    if (ImportPresetBtn)
    {
        ImportPresetBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnImportPresetClicked);
    }
    if (ImportFromExtendBtn)
    {
        ImportFromExtendBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnImportFromExtendClicked);
    }
    if (PresetDropdown)
    {
        PresetDropdown->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPresetSelectionChanged);
    }
    if (RestoreSectionToggle)
    {
        RestoreSectionToggle->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnRestoreSectionToggleClicked);
    }

    // Bind belt auto-connect controls
    if (BeltEnabledCheckBox)
    {
        BeltEnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltEnabledChanged);
    }
    if (BeltTierMainComboBox)
    {
        BeltTierMainComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltTierMainChanged);
    }
    if (BeltTierToBuildingComboBox)
    {
        BeltTierToBuildingComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltTierToBuildingChanged);
    }
    if (BeltChainCheckBox)
    {
        BeltChainCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltChainChanged);
    }
    if (StackableBeltDirectionComboBox)
    {
        StackableBeltDirectionComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnStackableBeltDirectionChanged);
    }
    if (BeltRoutingModeComboBox)
    {
        BeltRoutingModeComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltRoutingModeChanged);
    }

    // [#372] Rotation progression-axis selector (X = yaw builds up along the run, Y = rows fan out)
    if (RotationAxisComboBox)
    {
        RotationAxisComboBox->ClearOptions();
        RotationAxisComboBox->AddOption(TEXT("X"));
        RotationAxisComboBox->AddOption(TEXT("Y"));
        RotationAxisComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnRotationAxisChanged);
    }

    // Bind pipe auto-connect controls
    if (PipeEnabledCheckBox)
    {
        PipeEnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPipeEnabledChanged);
    }
    if (PipeTierMainComboBox)
    {
        PipeTierMainComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPipeTierMainChanged);
    }
    if (PipeTierToBuildingComboBox)
    {
        PipeTierToBuildingComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPipeTierToBuildingChanged);
    }
    if (PipeIndicatorCheckBox)
    {
        PipeIndicatorCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPipeIndicatorChanged);
    }
    if (PipeRoutingModeComboBox)
    {
        PipeRoutingModeComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPipeRoutingModeChanged);
    }
    if (HypertubeEnabledCheckBox)
    {
        HypertubeEnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnHypertubeEnabledChanged);
    }
    if (HypertubeRoutingModeComboBox)
    {
        HypertubeRoutingModeComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnHypertubeRoutingModeChanged);
    }

    // Bind power auto-connect controls
    if (PowerEnabledCheckBox)
    {
        PowerEnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPowerEnabledChanged);
    }
    if (PowerGridAxisComboBox)
    {
        PowerGridAxisComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPowerGridAxisChanged);
    }
    if (PowerReservedComboBox)
    {
        PowerReservedComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnPowerReservedChanged);
    }

    // Cache the two draggable canvas slots. [#352 restructure] Header and content now live
    // INSIDE BackgroundPanel (real layout containers; content under a layout-true ScaleBox),
    // so the panel drags as one slot and ComboBox popups anchor where they render.
    BackgroundPanelSlot     = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot);
    RestoreSidePanelSlot    = RestoreSidePanel ? Cast<UCanvasPanelSlot>(RestoreSidePanel->Slot) : nullptr;

    // ========== DARK THEME STYLING ==========

    // Dark backdrop colour now lives in the Blueprint (BackgroundPanel + RestoreSidePanel BrushColor =
    // 0.02,0.02,0.04,0.92) as the single source of truth — so the editor preview matches the game, and the Walk
    // panel mirrors the exact same value. (Was a runtime SetBrushColor override the BP designer view didn't reflect.)

    // Orange accent on title
    if (TitleText)
    {
        TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
    }

    // Orange accent on existing section headers
    if (AutoConnectHeaderText)
    {
        AutoConnectHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
    }
    if (RecipeHeaderText)
    {
        RecipeHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
    }
    if (RestoreSectionHeader)
    {
        RestoreSectionHeader->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
    }

    // Set all TextBlocks in ContentContainer to light gray for dark background
    if (ContentContainer)
    {
        const FSlateColor LightTextColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));

        // Recursive lambda to set text color on all UTextBlock descendants
        TFunction<void(UWidget*)> SetTextColors = [&](UWidget* Parent)
        {
            if (!Parent) return;

            if (UTextBlock* TextBlock = Cast<UTextBlock>(Parent))
            {
                TextBlock->SetColorAndOpacity(LightTextColor);
            }

            if (UPanelWidget* Panel = Cast<UPanelWidget>(Parent))
            {
                for (int32 i = 0; i < Panel->GetChildrenCount(); i++)
                {
                    SetTextColors(Panel->GetChildAt(i));
                }
            }
        };

        SetTextColors(ContentContainer);

        // Re-apply orange accent on headers (overridden by recursive pass)
        if (AutoConnectHeaderText)
        {
            AutoConnectHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
        }
        if (RecipeHeaderText)
        {
            RecipeHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f)));
        }
        if (GridWarningText)
        {
            GridWarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f)));
        }
        if (RestoreSectionHeader)
        {
            RestoreSectionHeader->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
        }
    }

    UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Blueprint widgets bound successfully"));

    // === Localize Blueprint Labels ===
    // Override default text set in the Widget Blueprint with LOCTEXT for runtime localization.
    // Uses GetWidgetFromName() to find TextBlock widgets by their Blueprint name.
    {
        auto SetLabel = [this](const TCHAR* WidgetName, const FText& Text)
        {
            if (UTextBlock* TB = Cast<UTextBlock>(GetWidgetFromName(FName(WidgetName))))
            {
                TB->SetText(Text);
            }
        };

        // Panel labels now live in the Blueprint, re-keyed to their original LOCTEXT keys so existing
        // translations still resolve (gathered from assets). Only the non-localized "X" glyph stays in code.
        SetLabel(TEXT("CloseButtonText"), FText::FromString(TEXT("X")));  // glyph close button (top-right of header), not a localized word
    }

    UpdateRestoreButtonTextColors();
    UpdateExtendImportButtonState();

    // === Extend Mode Overrides ===
    // Applied AFTER default setup since PopulateFromCounterState runs before NativeConstruct.
    // Hides unsupported transforms: Grid Z, Spacing Z, all Stagger.
    if (bIsExtendMode)
    {
        // Override title
        if (TitleText)
        {
            TitleText->SetText(LOCTEXT("Panel_Title_Extend", "Smart! Extend"));
        }

        // Helper to collapse a named widget in the tree
        auto CollapseByName = [this](const TCHAR* WidgetName)
        {
            if (UWidget* W = GetWidgetFromName(FName(WidgetName)))
            {
                W->SetVisibility(ESlateVisibility::Collapsed);
            }
        };

        // Hide Grid Z row (not used in Extend)
        CollapseByName(TEXT("GridZRow"));

        // Hide Spacing Z row (not used in Extend)
        CollapseByName(TEXT("SpacingZRow"));

        // Hide entire Stagger section (blocked during Extend)
        CollapseByName(TEXT("StaggerSectionHeader"));
        CollapseByName(TEXT("StaggerXRow"));
        CollapseByName(TEXT("StaggerYRow"));
        CollapseByName(TEXT("StaggerZXRow"));
        CollapseByName(TEXT("StaggerZYRow"));

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Extend mode overrides applied - title, Grid Z, Spacing Z, Stagger hidden"));
    }
}

void USmartSettingsFormWidget::PopulateFromCounterState(USFSubsystem* Subsystem)
{
    if (!Subsystem)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Cannot populate - Subsystem is null"));
        return;
    }

    // Cache subsystem reference for apply button
    CachedSubsystem = Subsystem;

    // Initialize Apply Immediately from global config
    const FSmart_ConfigStruct& Config = Subsystem->GetCachedConfig();
    bApplyImmediately = Config.bApplyImmediately;
    if (ApplyImmediatelyCheckBox)
    {
        ApplyImmediatelyCheckBox->SetIsChecked(bApplyImmediately);
    }

    // Get current counter state
    const FSFCounterState& State = Subsystem->GetCounterState();

    // Cache the initial state for cancel/revert on Escape
    LastAppliedState = State;

    PopulateCounterInputsFromState(State);

    // Cache direction state for cancel/revert
    bLastAppliedGridXPositive = bGridXPositive;
    bLastAppliedGridYPositive = bGridYPositive;
    bLastAppliedGridZPositive = bGridZPositive;

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Populated from CounterState - Grid[%d,%d,%d] Spacing[%d,%d,%d] Steps[%d,%d] Stagger[%d,%d,%d,%d] Rotation[%.1f]"),
        State.GridCounters.X, State.GridCounters.Y, State.GridCounters.Z,
        State.SpacingX, State.SpacingY, State.SpacingZ,
        State.StepsX, State.StepsY,
        State.StaggerX, State.StaggerY, State.StaggerZX, State.StaggerZY,
        State.RotationZ);

    // Log widget binding status
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Widget bindings - GridX=%d SpacingX=%d StepsX=%d StaggerX=%d"),
        GridXInput != nullptr, SpacingXInput != nullptr, StepsXInput != nullptr, StaggerXInput != nullptr);

    // Save Extend mode flag — actual UI overrides applied in NativeConstruct (after default setup)
    bIsExtendMode = Subsystem->IsExtendModeActive();

    if (AutoConnectHeaderText)
    {
        AutoConnectHeaderText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (AutoConnectSummaryText)
    {
        AutoConnectSummaryText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BeltAutoConnectContainer)
    {
        BeltAutoConnectContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (PipeAutoConnectContainer)
    {
        PipeAutoConnectContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (HypertubeAutoConnectContainer)
    {
        HypertubeAutoConnectContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (PowerAutoConnectContainer)
    {
        PowerAutoConnectContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (RecipeHeaderText)
    {
        RecipeHeaderText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (RecipeDetailsContainer)
    {
        RecipeDetailsContainer->ClearChildren();
        RecipeDetailsContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (RecipeComboBox)
    {
        RecipeComboBox->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (RecipeIcon)
    {
        RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ClearRecipeButton)
    {
        ClearRecipeButton->SetVisibility(ESlateVisibility::Collapsed);
    }

    RefreshPresetDropdown();
    UpdateExtendImportButtonState();

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // During Extend mode, skip auto-connect and recipe sections entirely.
    // Recipe is determined by the source building, and auto-connect is handled by Extend wiring.
    if (bIsExtendMode)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Extend mode - skipping auto-connect and recipe sections"));
        return;
    }

    USFSubsystem* SubsystemPtr = CachedSubsystem.Get();

    auto DescribeBeltTier = [](int32 Tier) -> FString
    {
        if (Tier <= 0)
        {
            return TEXT("Auto");
        }
        return FString::Printf(TEXT("Mk%d"), Tier);
    };

    auto DescribePipeTier = [](int32 Tier) -> FString
    {
        if (Tier <= 0)
        {
            return TEXT("Auto");
        }
        return FString::Printf(TEXT("Mk%d"), Tier);
    };

    auto DescribePowerAxis = [](int32 AxisMode) -> FString
    {
        switch (AxisMode)
        {
        case 1:  return TEXT("X only");
        case 2:  return TEXT("Y only");
        case 3:  return TEXT("X+Y");
        default: return TEXT("Auto");
        }
    };

    FString AutoConnectLines;
    bool bHasAnyAutoConnectContext = false;

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Checking auto-connect context..."));

    if (USFAutoConnectService* AutoConnectService = SubsystemPtr->GetAutoConnectService())
    {
        if (AFGHologram* ActiveHologram = SubsystemPtr->GetActiveHologram())
        {
            const auto& Settings = SubsystemPtr->GetAutoConnectRuntimeSettings();

            const bool bIsDistributor = AutoConnectService->IsDistributorHologram(ActiveHologram);
            const bool bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram);
            const bool bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram);
            const bool bIsStackableConveyorPole = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram);
            const bool bIsStackablePipeSupport = AutoConnectService->IsStackablePipelineSupportHologram(ActiveHologram);
            const bool bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram);
            const bool bIsStackableHypertubeSupport = AutoConnectService->IsStackableHypertubeSupportHologram(ActiveHologram);

            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Hologram=%s, IsDistributor=%d, IsPipeJunction=%d, IsPowerPole=%d, IsBeltSupport=%d, IsStackablePipeSupport=%d, IsPassthroughPipe=%d"),
                *ActiveHologram->GetClass()->GetName(), bIsDistributor, bIsPipeJunction, bIsPowerPole, bIsStackableConveyorPole, bIsStackablePipeSupport, bIsPassthroughPipe);

            if (bIsDistributor)
            {
                bHasAnyAutoConnectContext = true;

                // Show belt auto-connect controls instead of summary text
                if (BeltAutoConnectContainer)
                {
                    PopulateBeltTierComboBoxes();
                    UpdateBeltAutoConnectControls();
                    BeltAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    // Fallback to summary text if controls not available
                    AutoConnectLines += FString::Printf(
                        TEXT("Belts: %s, Main=%s, ToBuildings=%s, Chain=%s"),
                        Settings.bEnabled ? TEXT("On") : TEXT("Off"),
                        *DescribeBeltTier(Settings.BeltTierMain),
                        *DescribeBeltTier(Settings.BeltTierToBuilding),
                        Settings.bChainDistributors ? TEXT("On") : TEXT("Off"));
                }
            }

            if (bIsPipeJunction)
            {
                bHasAnyAutoConnectContext = true;

                // Show pipe auto-connect controls instead of summary text
                if (PipeAutoConnectContainer)
                {
                    PopulatePipeTierComboBoxes();
                    UpdatePipeAutoConnectControls();
                    PipeAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    // Fallback to summary text if controls not available
                    if (!AutoConnectLines.IsEmpty())
                    {
                        AutoConnectLines += TEXT("\n");
                    }
                    AutoConnectLines += FString::Printf(
                        TEXT("Pipes: %s, Main=%s, ToBuildings=%s, Style=%s"),
                        Settings.bPipeAutoConnectEnabled ? TEXT("On") : TEXT("Off"),
                        *DescribePipeTier(Settings.PipeTierMain),
                        *DescribePipeTier(Settings.PipeTierToBuilding),
                        Settings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean"));
                }
            }

            if (bIsPowerPole)
            {
                bHasAnyAutoConnectContext = true;

                // Show power auto-connect controls instead of summary text
                if (PowerAutoConnectContainer)
                {
                    PopulatePowerComboBoxes();
                    UpdatePowerAutoConnectControls();
                    PowerAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    // Fallback to summary text if controls not available
                    if (!AutoConnectLines.IsEmpty())
                    {
                        AutoConnectLines += TEXT("\n");
                    }
                    AutoConnectLines += FString::Printf(
                        TEXT("Power: %s, Reserved=%d, Axis=%s"),
                        Settings.bConnectPower ? TEXT("On") : TEXT("Off"),
                        Settings.PowerReserved,
                        *DescribePowerAxis(Settings.PowerGridAxis));
                }
            }

            if (bIsStackableConveyorPole)
            {
                bHasAnyAutoConnectContext = true;

                // Show stackable conveyor pole auto-connect controls
                if (BeltAutoConnectContainer)
                {
                    PopulateBeltTierComboBoxes();
                    UpdateStackableBeltAutoConnectControls();
                    BeltAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    // Fallback to summary text if controls not available
                    if (!AutoConnectLines.IsEmpty())
                    {
                        AutoConnectLines += TEXT("\n");
                    }
                    AutoConnectLines += FString::Printf(
                        TEXT("Stackable Belt Pole: %s, Tier=%s, Direction=%s"),
                        Settings.bStackableBeltEnabled ? TEXT("On") : TEXT("Off"),
                        *DescribeBeltTier(Settings.BeltTierMain),
                        Settings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward"));
                }
            }

            if (bIsStackablePipeSupport || bIsPassthroughPipe)
            {
                bHasAnyAutoConnectContext = true;

                // Show pipe auto-connect controls (shared for stackable pipe supports and floor holes)
                if (PipeAutoConnectContainer)
                {
                    PopulatePipeTierComboBoxes();
                    UpdateStackablePipeAutoConnectControls();
                    PipeAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    // Fallback to summary text if controls not available
                    if (!AutoConnectLines.IsEmpty())
                    {
                        AutoConnectLines += TEXT("\n");
                    }
                    AutoConnectLines += FString::Printf(
                        TEXT("%s: %s, Tier=%s, Style=%s"),
                        bIsPassthroughPipe ? TEXT("Floor Hole Pipe") : TEXT("Stackable Pipe Support"),
                        Settings.bPipeAutoConnectEnabled ? TEXT("On") : TEXT("Off"),
                        *DescribePipeTier(Settings.PipeTierMain),
                        Settings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean"));
                }
            }

            if (bIsStackableHypertubeSupport)
            {
                bHasAnyAutoConnectContext = true;

                // Show hypertube auto-connect controls (enable checkbox + routing style; enable is independent from pipe)
                if (HypertubeAutoConnectContainer)
                {
                    UpdateHypertubeAutoConnectControls();
                    HypertubeAutoConnectContainer->SetVisibility(ESlateVisibility::Visible);
                }
                else
                {
                    if (!AutoConnectLines.IsEmpty())
                    {
                        AutoConnectLines += TEXT("\n");
                    }
                    AutoConnectLines += FString::Printf(
                        TEXT("Stackable Hypertube Support: Enabled=%d Routing=%d"),
                        Settings.bHypertubeAutoConnectEnabled ? 1 : 0,
                        Settings.HypertubeRoutingMode);
                }
            }
        }
    }

    if (bHasAnyAutoConnectContext)
    {
        if (AutoConnectHeaderText)
        {
            // header text set in the Blueprint now (re-keyed Panel_AutoConnectHeader)
            AutoConnectHeaderText->SetVisibility(ESlateVisibility::Visible);
        }
        if (AutoConnectSummaryText)
        {
            AutoConnectSummaryText->SetText(FText::FromString(AutoConnectLines));
            AutoConnectSummaryText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    FString RecipeSummary;

    int32 CurrentIndex = 0;
    int32 TotalRecipes = 0;
    SubsystemPtr->GetRecipeDisplayInfo(CurrentIndex, TotalRecipes);
    TSubclassOf<UFGRecipe> ActiveRecipe = SubsystemPtr->GetActiveRecipe();

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Recipe check - ActiveRecipe=%s, TotalRecipes=%d"),
        ActiveRecipe ? *ActiveRecipe->GetName() : TEXT("null"), TotalRecipes);

    bool bIsProductionBuilding = false;
    bool bIsCompatible = true;
    if (AFGHologram* ActiveHologram = SubsystemPtr->GetActiveHologram())
    {
        if (UClass* HologramClass = ActiveHologram->GetBuildClass())
        {
            // Check if this is a production building by checking class hierarchy
            // BUT exclude auto-connect holograms (distributors, pipe junctions, power poles)
            // which may inherit from AFGBuildableFactory but don't use recipes
            bIsProductionBuilding = HologramClass->IsChildOf(AFGBuildableFactory::StaticClass())
                                    && !bHasAnyAutoConnectContext;
            if (ActiveRecipe)
            {
                bIsCompatible = SubsystemPtr->IsRecipeCompatibleWithHologram(ActiveRecipe, HologramClass);
            }
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Recipe compatibility check - HologramClass=%s, bIsProductionBuilding=%d, bIsCompatible=%d, bHasAutoConnectContext=%d"),
                *HologramClass->GetName(), bIsProductionBuilding, bIsCompatible, bHasAnyAutoConnectContext);
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: No active hologram for recipe check"));
    }

    // Show recipe section for production buildings (excludes auto-connect holograms)
    if (bIsProductionBuilding)
    {
        if (ActiveRecipe && TotalRecipes > 0 && bIsCompatible)
        {
            const FString RecipeWithDetails = SubsystemPtr->GetRecipeWithInputsOutputs(ActiveRecipe);
            const bool bRecipeActive = SubsystemPtr->IsRecipeModeActive();
            RecipeSummary = bRecipeActive
                ? FText::Format(LOCTEXT("Panel_Recipe_Active", "Recipe* {0}/{1}: {2}"), FText::AsNumber(CurrentIndex + 1), FText::AsNumber(TotalRecipes), FText::FromString(RecipeWithDetails)).ToString()
                : FText::Format(LOCTEXT("Panel_Recipe", "Recipe {0}/{1}: {2}"), FText::AsNumber(CurrentIndex + 1), FText::AsNumber(TotalRecipes), FText::FromString(RecipeWithDetails)).ToString();
        }
        else if (TotalRecipes > 0)
        {
            // Show that recipes are available but none selected
            RecipeSummary = FText::Format(LOCTEXT("Panel_Recipe_NoneSelected_Count", "No recipe selected ({0} available)"), FText::AsNumber(TotalRecipes)).ToString();
        }
        else
        {
            // Production building but no compatible recipes
            RecipeSummary = LOCTEXT("Panel_Recipe_NoCompatible", "No compatible recipes").ToString();
        }
    }

    if (!RecipeSummary.IsEmpty())
    {
        if (RecipeHeaderText)
        {
            // header text set in the Blueprint now (re-keyed Panel_RecipeHeader)
            RecipeHeaderText->SetVisibility(ESlateVisibility::Visible);
        }

        // Populate recipe details container with icons
        if (RecipeDetailsContainer)
        {
            PopulateRecipeDetails(ActiveRecipe);
            RecipeDetailsContainer->SetVisibility(ESlateVisibility::Visible);
        }

        // Populate recipe ComboBox if available
        if (RecipeComboBox && bIsProductionBuilding)
        {
            if (USFRecipeManagementService* RecipeService = SubsystemPtr->GetRecipeManagementService())
            {
                RecipeComboBox->ClearOptions();
                CachedRecipeList.Empty();

                // Add "None Selected" as first option (index 0)
                // CachedRecipeList[0] will be nullptr to represent no selection
                RecipeComboBox->AddOption(LOCTEXT("Panel_Recipe_NoneSelected", "None Selected").ToString());
                CachedRecipeList.Add(nullptr);

                const TArray<TSubclassOf<UFGRecipe>>& Recipes = RecipeService->GetSortedFilteredRecipes();
                int32 SelectedIndex = 0;  // Default to "None Selected"

                for (int32 i = 0; i < Recipes.Num(); i++)
                {
                    TSubclassOf<UFGRecipe> Recipe = Recipes[i];
                    FString Label = RecipeService->GetRecipeComboBoxLabel(Recipe);
                    RecipeComboBox->AddOption(Label);
                    CachedRecipeList.Add(Recipe);

                    // Track which one is currently active (offset by 1 for "None Selected")
                    if (Recipe == ActiveRecipe)
                    {
                        SelectedIndex = i + 1;
                    }
                }

                // Set current selection
                RecipeComboBox->SetSelectedIndex(SelectedIndex);

                RecipeComboBox->SetVisibility(ESlateVisibility::Visible);

                // Show Clear button alongside recipe controls
                if (ClearRecipeButton)
                {
                    ClearRecipeButton->SetVisibility(ESlateVisibility::Visible);
                }

                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Populated RecipeComboBox with %d recipes (+None), selected index=%d"),
                    Recipes.Num(), SelectedIndex);
            }
        }

        // Update recipe icon for currently active recipe
        UpdateRecipeIcon(ActiveRecipe);
    }
    else
    {
        if (RecipeComboBox)
        {
            // Hide ComboBox when no recipes available
            RecipeComboBox->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (RecipeIcon)
        {
            RecipeIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (RecipeDetailsContainer)
        {
            RecipeDetailsContainer->ClearChildren();
            RecipeDetailsContainer->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Initialize grid warning display based on current values
    UpdateGridWarningDisplay();
}

void USmartSettingsFormWidget::FitHeaderButtonRow()
{
    // [#352 restructure] The header buttons live in a fixed-width row under the title now
    // (HeaderButtonRowBox, 285px = the content column, split into equal thirds), so each
    // button is ~93px. All that remains of the old canvas-band math is the localized label
    // fit: shrink any label that would clip in its third.
    const float LabelAvail = 80.0f;
    auto FitLabel = [this, LabelAvail](const TCHAR* WidgetName)
    {
        UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(FName(WidgetName)));
        if (!Label)
        {
            return;
        }
        FSlateFontInfo Font = Label->GetFont();
        for (int32 Guard = 0; Guard < 16; ++Guard)
        {
            Label->ForceLayoutPrepass();
            if (Label->GetDesiredSize().X <= LabelAvail || Font.Size <= 9.0f)
            {
                break;
            }
            Font.Size -= 1.0f;
            Label->SetFont(Font);
        }
    };
    FitLabel(TEXT("ApplyBtnText"));
    FitLabel(TEXT("ResetBtnText"));
    FitLabel(TEXT("CloseButtonText"));
}

void USmartSettingsFormWidget::OnWalkPathButtonClicked()
{
    UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Walk Path button clicked"));
    // #356: close the panel FIRST (so its input/HUD reset doesn't clobber the walk widget), then enter
    // Smart Walking on the held buildable and open the dedicated Walk widget.
    TWeakObjectPtr<USFSubsystem> Sub = CachedSubsystem;
    CloseForm();
    if (Sub.IsValid())
    {
        Sub->EnterWalkMode();
        Sub->OpenWalkPanel();
    }
}

void USmartSettingsFormWidget::OnCloseButtonClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Close button clicked"));
    CloseForm();
}

void USmartSettingsFormWidget::CancelAndClose()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Cancel and close (toggle key or escape)"));
    RevertToLastAppliedState();
    CloseForm();
}

#undef LOCTEXT_NAMESPACE
