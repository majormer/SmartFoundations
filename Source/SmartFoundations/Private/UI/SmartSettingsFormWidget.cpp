#include "UI/SmartSettingsFormWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"
#include "InputCoreTypes.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"

#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Services/SFRecipeManagementService.h"
#include "Hologram/FGHologram.h"
#include "Services/SFHudService.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "FGPlayerController.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USmartSettingsFormWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogSmartFoundations, Log, TEXT("Settings Form: NativeConstruct called"));

    // Check if Blueprint widgets are bound
    if (!BackgroundPanel)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: BackgroundPanel not bound from Blueprint"));
        return;
    }

    if (!TitleText)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: TitleText not bound from Blueprint"));
        return;
    }

    if (!ContentContainer)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: ContentContainer not bound from Blueprint"));
        return;
    }

    if (!CloseButton)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: CloseButton not bound from Blueprint"));
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
            FSlateFontInfo FontInfo = SpinBox->GetFont();
            FontInfo.Size = 18;
            SpinBox->SetFont(FontInfo);

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
            FSlateFontInfo FontInfo = SpinBox->GetFont();
            FontInfo.Size = 18;
            SpinBox->SetFont(FontInfo);

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

            FSlateFontInfo FontInfo = SpinBox->GetFont();
            FontInfo.Size = 18;
            SpinBox->SetFont(FontInfo);

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
    ConfigureComboBoxStyle(BeltRoutingModeComboBox, PipeRoutingModeComboBox);  // Copy style from Pipe Routing Mode
    ConfigureComboBoxStyle(PowerGridAxisComboBox);
    ConfigureComboBoxStyle(PowerReservedComboBox);

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

    // Cache canvas slots for dragging and compute relative offsets
    BackgroundPanelSlot     = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot);
    TitleTextSlot           = TitleText ? Cast<UCanvasPanelSlot>(TitleText->Slot) : nullptr;
    ContentContainerSlot    = ContentContainer ? Cast<UCanvasPanelSlot>(ContentContainer->Slot) : nullptr;
    ApplyButtonSlot         = ApplyBtn ? Cast<UCanvasPanelSlot>(ApplyBtn->Slot) : nullptr;
    ResetButtonSlot         = ResetBtn ? Cast<UCanvasPanelSlot>(ResetBtn->Slot) : nullptr;
    CloseButtonSlot         = CloseButton ? Cast<UCanvasPanelSlot>(CloseButton->Slot) : nullptr;
    SmartLogoSlot           = SmartLogoImage ? Cast<UCanvasPanelSlot>(SmartLogoImage->Slot) : nullptr;

    if (BackgroundPanelSlot)
    {
        const FVector2D BackgroundPos = BackgroundPanelSlot->GetPosition();

        if (TitleTextSlot)
        {
            TitleOffset = TitleTextSlot->GetPosition() - BackgroundPos;
        }
        if (ContentContainerSlot)
        {
            ContentOffset = ContentContainerSlot->GetPosition() - BackgroundPos;
        }
        if (ApplyButtonSlot)
        {
            ApplyOffset = ApplyButtonSlot->GetPosition() - BackgroundPos;
        }
        if (ResetButtonSlot)
        {
            ResetOffset = ResetButtonSlot->GetPosition() - BackgroundPos;
        }
        if (CloseButtonSlot)
        {
            CloseOffset = CloseButtonSlot->GetPosition() - BackgroundPos;
        }
        if (SmartLogoSlot)
        {
            LogoOffset = SmartLogoSlot->GetPosition() - BackgroundPos;
        }
    }

    // ========== DARK THEME STYLING ==========

    // Dark background on BackgroundPanel
    if (BackgroundPanel)
    {
        BackgroundPanel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.92f));
    }

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
    }

    UE_LOG(LogSmartFoundations, Log, TEXT("Settings Form: Blueprint widgets bound successfully"));

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

        // Button labels
        SetLabel(TEXT("ApplyBtnText"), LOCTEXT("Panel_Btn_Apply", "Apply"));
        SetLabel(TEXT("ResetBtnText"), LOCTEXT("Panel_Btn_Reset", "Reset"));
        SetLabel(TEXT("CloseButtonText"), LOCTEXT("Panel_Btn_Close", "Close"));
        SetLabel(TEXT("ClearRecipeButtonText"), LOCTEXT("Panel_Btn_ClearRecipe", "Clear"));
        SetLabel(TEXT("ConfirmYesText"), LOCTEXT("Panel_Btn_Continue", "Continue"));
        SetLabel(TEXT("ConfirmNoText"), LOCTEXT("Panel_Btn_Cancel", "Cancel"));

        // Apply Immediately label
        SetLabel(TEXT("ApplyImmediatelyLabel"), LOCTEXT("Panel_ApplyImmediately", "Apply Immediately:"));

        // Section headers
        SetLabel(TEXT("GridSectionHeader"), LOCTEXT("Panel_Section_Grid", "Grid"));
        SetLabel(TEXT("SpacingSectionHeader"), LOCTEXT("Panel_Section_Spacing", "Spacing"));
        SetLabel(TEXT("StepsSectionHeader"), LOCTEXT("Panel_Section_Steps", "Steps"));
        SetLabel(TEXT("StaggerSectionHeader"), LOCTEXT("Panel_Section_Stagger", "Stagger"));
        SetLabel(TEXT("RotationSectionHeader"), LOCTEXT("Panel_Section_Rotation", "Rotation"));

        // Grid row labels
        SetLabel(TEXT("GridXLabel"), LOCTEXT("Panel_GridX", "Grid [X]:"));
        SetLabel(TEXT("GridYLabel"), LOCTEXT("Panel_GridY", "Grid [Y]:"));
        SetLabel(TEXT("GridZLabel"), LOCTEXT("Panel_GridZ", "Grid [Z]:"));

        // Spacing row labels
        SetLabel(TEXT("SpacingXLabel"), LOCTEXT("Panel_SpacingX", "Spacing [X]:"));
        SetLabel(TEXT("SpacingYLabel"), LOCTEXT("Panel_SpacingY", "Spacing [Y]:"));
        SetLabel(TEXT("SpacingZLabel"), LOCTEXT("Panel_SpacingZ", "Spacing [Z]:"));

        // Spacing unit labels
        SetLabel(TEXT("SpacingXUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("SpacingYUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("SpacingZUnit"), LOCTEXT("Panel_Unit_Meters", "m"));

        // Steps row labels
        SetLabel(TEXT("StepsXLabel"), LOCTEXT("Panel_StepsX", "Steps [X]:"));
        SetLabel(TEXT("StepsYLabel"), LOCTEXT("Panel_StepsY", "Steps [Y]:"));

        // Steps unit labels
        SetLabel(TEXT("StepsXUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("StepsYUnit"), LOCTEXT("Panel_Unit_Meters", "m"));

        // Stagger row labels
        SetLabel(TEXT("StaggerXLabel"), LOCTEXT("Panel_StaggerX", "Stagger [X]:"));
        SetLabel(TEXT("StaggerYLabel"), LOCTEXT("Panel_StaggerY", "Stagger [Y]:"));
        SetLabel(TEXT("StaggerZXLabel"), LOCTEXT("Panel_StaggerZX", "Stagger [ZX]:"));
        SetLabel(TEXT("StaggerZYLabel"), LOCTEXT("Panel_StaggerZY", "Stagger [ZY]:"));

        // Stagger unit labels
        SetLabel(TEXT("StaggerXUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("StaggerYUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("StaggerZXUnit"), LOCTEXT("Panel_Unit_Meters", "m"));
        SetLabel(TEXT("StaggerZYUnit"), LOCTEXT("Panel_Unit_Meters", "m"));

        // Rotation row label and unit
        SetLabel(TEXT("RotationZLabel"), LOCTEXT("Panel_RotationZ", "Rotation [Z]:"));
        SetLabel(TEXT("RotationZUnit"), LOCTEXT("Panel_Unit_Degrees", "\u00B0"));

        // Belt auto-connect labels
        SetLabel(TEXT("BeltEnabledLabel"), LOCTEXT("Panel_AC_BeltEnabled", "Belt Auto-Connect:"));
        SetLabel(TEXT("BeltTierMainLabel"), LOCTEXT("Panel_AC_MainTier", "Main Tier:"));
        SetLabel(TEXT("BeltTierToBuildingLabel"), LOCTEXT("Panel_AC_ToBuilding", "To Building:"));
        SetLabel(TEXT("BeltChainLabel"), LOCTEXT("Panel_AC_Chain", "Chain:"));
        SetLabel(TEXT("StackableBeltDirectionLabel"), LOCTEXT("Panel_AC_Direction", "Direction:"));
        SetLabel(TEXT("BeltRoutingModeLabel"), LOCTEXT("Panel_AC_Routing", "Routing:"));

        // Pipe auto-connect labels
        SetLabel(TEXT("PipeEnabledLabel"), LOCTEXT("Panel_AC_PipeEnabled", "Pipe Auto-Connect:"));
        SetLabel(TEXT("PipeTierMainLabel"), LOCTEXT("Panel_AC_MainTier", "Main Tier:"));
        SetLabel(TEXT("PipeTierToBuildingLabel"), LOCTEXT("Panel_AC_ToBuilding", "To Building:"));
        SetLabel(TEXT("PipeIndicatorLabel"), LOCTEXT("Panel_AC_FlowIndicator", "Flow Indicator:"));
        SetLabel(TEXT("PipeRoutingModeLabel"), LOCTEXT("Panel_AC_Routing", "Routing:"));

        // Power auto-connect labels
        SetLabel(TEXT("PowerEnabledLabel"), LOCTEXT("Panel_AC_PowerEnabled", "Power Auto-Connect:"));
        SetLabel(TEXT("PowerGridAxisLabel"), LOCTEXT("Panel_AC_GridAxis", "Grid Axis:"));
        SetLabel(TEXT("PowerReservedLabel"), LOCTEXT("Panel_AC_Reserved", "Reserved:"));
    }

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
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: Cannot populate - Subsystem is null"));
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

    // Populate Grid values (show absolute value, track direction separately)
    // Direction is determined by sign: positive = true, negative = false
    bGridXPositive = State.GridCounters.X >= 0;
    bGridYPositive = State.GridCounters.Y >= 0;
    bGridZPositive = State.GridCounters.Z >= 0;

    // Cache direction state for cancel/revert
    bLastAppliedGridXPositive = bGridXPositive;
    bLastAppliedGridYPositive = bGridYPositive;
    bLastAppliedGridZPositive = bGridZPositive;

    // Populate Grid values using SpinBox SetValue (absolute count)
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

    // Update direction toggle labels
    UpdateGridDirectionLabel(GridXDirLabel, bGridXPositive);
    UpdateGridDirectionLabel(GridYDirLabel, bGridYPositive);
    UpdateGridDirectionLabel(GridZDirLabel, bGridZPositive);

    // Populate Spacing values (display in meters, internal state is centimeters)
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

    // Populate Steps values (display in meters, internal state is centimeters)
    if (StepsXInput)
    {
        StepsXInput->SetValue(State.StepsX / 100.0f);
    }
    if (StepsYInput)
    {
        StepsYInput->SetValue(State.StepsY / 100.0f);
    }

    // Populate Stagger values (display in meters, internal state is centimeters)
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

    // Rotation (degrees, not cm - no conversion needed)
    if (RotationZInput)
    {
        RotationZInput->SetValue(State.RotationZ);
    }

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
        }
    }

    if (bHasAnyAutoConnectContext)
    {
        if (AutoConnectHeaderText)
        {
            AutoConnectHeaderText->SetText(LOCTEXT("Panel_AutoConnectHeader", "Auto-Connect"));
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
            RecipeHeaderText->SetText(LOCTEXT("Panel_RecipeHeader", "Recipes"));
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
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: No cached subsystem reference"));
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
        UE_LOG(LogSmartFoundations, Error, TEXT("Settings Form: No cached subsystem reference"));
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
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && BackgroundPanelSlot)
    {
        bIsDragging = true;
        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        DragOffset = LocalMouse - BackgroundPanelSlot->GetPosition();
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
        const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        const FVector2D NewBackgroundPos = LocalMouse - DragOffset;

        BackgroundPanelSlot->SetPosition(NewBackgroundPos);

        if (TitleTextSlot)
        {
            TitleTextSlot->SetPosition(NewBackgroundPos + TitleOffset);
        }
        if (ContentContainerSlot)
        {
            ContentContainerSlot->SetPosition(NewBackgroundPos + ContentOffset);
        }
        if (SmartLogoSlot)
        {
            SmartLogoSlot->SetPosition(NewBackgroundPos + LogoOffset);
        }
        if (ApplyButtonSlot)
        {
            ApplyButtonSlot->SetPosition(NewBackgroundPos + ApplyOffset);
        }
        if (ResetButtonSlot)
        {
            ResetButtonSlot->SetPosition(NewBackgroundPos + ResetOffset);
        }
        if (CloseButtonSlot)
        {
            CloseButtonSlot->SetPosition(NewBackgroundPos + CloseOffset);
        }
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
        UE_LOG(LogSmartFoundations, Warning, TEXT("Settings Form: Invalid recipe selection index %d"), SelectedIndex);
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

        // Set font to match HUD style
        FSlateFontInfo FontInfo = TextWidget->GetFont();
        FontInfo.Size = 14;
        TextWidget->SetFont(FontInfo);

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

FString USmartSettingsFormWidget::BeltTierToDisplayString(int32 Tier) const
{
    if (Tier <= 0)
    {
        // Show what "Auto" resolves to
        if (CachedSubsystem.IsValid())
        {
            int32 HighestTier = CachedSubsystem->GetHighestUnlockedBeltTier(
                Cast<AFGPlayerController>(GetOwningPlayer()));
            return FString::Printf(TEXT("Auto (Mk%d)"), HighestTier);
        }
        return TEXT("Auto");
    }
    return FString::Printf(TEXT("Mk%d"), Tier);
}

int32 USmartSettingsFormWidget::DisplayStringToBeltTier(const FString& DisplayString) const
{
    if (DisplayString.StartsWith(TEXT("Auto")))
    {
        return 0;
    }

    // Parse "Mk1", "Mk2", etc.
    if (DisplayString.StartsWith(TEXT("Mk")))
    {
        FString TierStr = DisplayString.RightChop(2);
        return FCString::Atoi(*TierStr);
    }

    return 0;
}

void USmartSettingsFormWidget::PopulateBeltTierComboBoxes()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Get highest unlocked belt tier
    AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
    int32 HighestUnlocked = CachedSubsystem->GetHighestUnlockedBeltTier(PC);

    // Build list of available tiers: Auto (0), then Mk1 through highest unlocked
    CachedBeltTiers.Empty();
    CachedBeltTiers.Add(0);  // Auto
    for (int32 Tier = 1; Tier <= HighestUnlocked; Tier++)
    {
        CachedBeltTiers.Add(Tier);
    }

    // Populate Main tier ComboBox
    if (BeltTierMainComboBox)
    {
        BeltTierMainComboBox->ClearOptions();
        for (int32 Tier : CachedBeltTiers)
        {
            BeltTierMainComboBox->AddOption(BeltTierToDisplayString(Tier));
        }
    }

    // Populate To Building tier ComboBox
    if (BeltTierToBuildingComboBox)
    {
        BeltTierToBuildingComboBox->ClearOptions();
        for (int32 Tier : CachedBeltTiers)
        {
            BeltTierToBuildingComboBox->AddOption(BeltTierToDisplayString(Tier));
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Populated belt tier ComboBoxes with %d options (highest unlocked: Mk%d)"),
        CachedBeltTiers.Num(), HighestUnlocked);
}

void USmartSettingsFormWidget::UpdateBeltAutoConnectControls()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const auto& Settings = CachedSubsystem->GetAutoConnectRuntimeSettings();

    // Update Enabled checkbox
    if (BeltEnabledCheckBox)
    {
        BeltEnabledCheckBox->SetIsChecked(Settings.bEnabled);
    }

    // Update Main tier ComboBox
    if (BeltTierMainComboBox)
    {
        int32 Index = CachedBeltTiers.Find(Settings.BeltTierMain);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        BeltTierMainComboBox->SetSelectedIndex(Index);
    }

    // Update To Building tier ComboBox
    if (BeltTierToBuildingComboBox)
    {
        // Restore original event binding (in case it was repurposed for stackable poles)
        BeltTierToBuildingComboBox->OnSelectionChanged.Clear();
        BeltTierToBuildingComboBox->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnBeltTierToBuildingChanged);

        int32 Index = CachedBeltTiers.Find(Settings.BeltTierToBuilding);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        BeltTierToBuildingComboBox->SetSelectedIndex(Index);
    }

    // Update Chain checkbox
    if (BeltChainCheckBox)
    {
        BeltChainCheckBox->SetIsChecked(Settings.bChainDistributors);
        BeltChainCheckBox->SetVisibility(ESlateVisibility::Visible);
    }

    // Restore visibility for regular belt controls, hide stackable-only controls
    if (BeltTierToBuildingLabel)
    {
        BeltTierToBuildingLabel->SetVisibility(ESlateVisibility::Visible);
    }
    if (BeltTierToBuildingComboBox)
    {
        BeltTierToBuildingComboBox->SetVisibility(ESlateVisibility::Visible);
    }
    if (StackableBeltDirectionRow)
    {
        StackableBeltDirectionRow->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BeltChainLabel)
    {
        BeltChainLabel->SetVisibility(ESlateVisibility::Visible);
    }

    // Update Routing Mode ComboBox
    if (BeltRoutingModeComboBox)
    {
        BeltRoutingModeComboBox->ClearOptions();
        BeltRoutingModeComboBox->AddOption(TEXT("Default"));
        BeltRoutingModeComboBox->AddOption(TEXT("Curve"));
        BeltRoutingModeComboBox->AddOption(TEXT("Straight"));

        int32 ModeIndex = FMath::Clamp(Settings.BeltRoutingMode, 0, 2);
        BeltRoutingModeComboBox->SetSelectedIndex(ModeIndex);
    }
    if (BeltRoutingModeRow)
    {
        BeltRoutingModeRow->SetVisibility(ESlateVisibility::Visible);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Updated belt controls - Enabled=%d, Main=%d, ToBuilding=%d, Chain=%d, RoutingMode=%d"),
        Settings.bEnabled, Settings.BeltTierMain, Settings.BeltTierToBuilding, Settings.bChainDistributors, Settings.BeltRoutingMode);
}

void USmartSettingsFormWidget::OnBeltEnabledChanged(bool bIsChecked)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    CachedSubsystem->SetAutoConnectBeltEnabled(bIsChecked);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Belt auto-connect enabled changed to %d"), bIsChecked);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnBeltTierMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Tier = DisplayStringToBeltTier(SelectedItem);
    CachedSubsystem->SetAutoConnectBeltTierMain(Tier);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Belt tier main changed to %d (%s)"), Tier, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnBeltTierToBuildingChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Tier = DisplayStringToBeltTier(SelectedItem);
    CachedSubsystem->SetAutoConnectBeltTierToBuilding(Tier);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Belt tier to building changed to %d (%s)"), Tier, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnBeltChainChanged(bool bIsChecked)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    CachedSubsystem->SetAutoConnectBeltChain(bIsChecked);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Belt chain changed to %d"), bIsChecked);
}

void USmartSettingsFormWidget::OnStackableBeltDirectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Convert "Forward"/"Backward" to 0/1
    int32 Direction = 0;  // Default to Forward
    if (SelectedItem == TEXT("Backward"))
    {
        Direction = 1;
    }

    CachedSubsystem->SetAutoConnectStackableBeltDirection(Direction);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Stackable belt direction changed to %d (%s)"), Direction, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnBeltRoutingModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Convert display string to routing mode index
    int32 RoutingMode = 0;  // Default
    if (SelectedItem == TEXT("Curve")) RoutingMode = 1;
    else if (SelectedItem == TEXT("Straight")) RoutingMode = 2;

    CachedSubsystem->SetAutoConnectBeltRoutingMode(RoutingMode);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Belt routing mode changed to %d (%s)"), RoutingMode, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

// ============================================================================
// Pipe Auto-Connect Controls
// ============================================================================

FString USmartSettingsFormWidget::PipeTierToDisplayString(int32 Tier) const
{
    if (Tier <= 0)
    {
        // Show what "Auto" resolves to
        if (CachedSubsystem.IsValid())
        {
            int32 HighestTier = CachedSubsystem->GetHighestUnlockedPipeTier(
                Cast<AFGPlayerController>(GetOwningPlayer()));
            return FString::Printf(TEXT("Auto (Mk%d)"), HighestTier);
        }
        return TEXT("Auto");
    }
    return FString::Printf(TEXT("Mk%d"), Tier);
}

int32 USmartSettingsFormWidget::DisplayStringToPipeTier(const FString& DisplayString) const
{
    if (DisplayString.StartsWith(TEXT("Auto")))
    {
        return 0;
    }

    // Parse "Mk1", "Mk2"
    if (DisplayString.StartsWith(TEXT("Mk")))
    {
        FString TierStr = DisplayString.RightChop(2);
        return FCString::Atoi(*TierStr);
    }

    return 0;
}

void USmartSettingsFormWidget::PopulatePipeTierComboBoxes()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Get highest unlocked pipe tier
    AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
    int32 HighestUnlocked = CachedSubsystem->GetHighestUnlockedPipeTier(PC);

    // Build list of available tiers: Auto (0), then Mk1 through highest unlocked
    CachedPipeTiers.Empty();
    CachedPipeTiers.Add(0);  // Auto
    for (int32 Tier = 1; Tier <= HighestUnlocked; Tier++)
    {
        CachedPipeTiers.Add(Tier);
    }

    // Populate Main tier ComboBox
    if (PipeTierMainComboBox)
    {
        PipeTierMainComboBox->ClearOptions();
        for (int32 Tier : CachedPipeTiers)
        {
            PipeTierMainComboBox->AddOption(PipeTierToDisplayString(Tier));
        }
    }

    // Populate To Building tier ComboBox
    if (PipeTierToBuildingComboBox)
    {
        PipeTierToBuildingComboBox->ClearOptions();
        for (int32 Tier : CachedPipeTiers)
        {
            PipeTierToBuildingComboBox->AddOption(PipeTierToDisplayString(Tier));
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Populated pipe tier ComboBoxes with %d options (highest unlocked: Mk%d)"),
        CachedPipeTiers.Num(), HighestUnlocked);
}

void USmartSettingsFormWidget::UpdatePipeAutoConnectControls()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const auto& Settings = CachedSubsystem->GetAutoConnectRuntimeSettings();

    // Update Enabled checkbox
    if (PipeEnabledCheckBox)
    {
        PipeEnabledCheckBox->SetIsChecked(Settings.bPipeAutoConnectEnabled);
    }

    // Update Main tier ComboBox
    if (PipeTierMainComboBox)
    {
        int32 Index = CachedPipeTiers.Find(Settings.PipeTierMain);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        PipeTierMainComboBox->SetSelectedIndex(Index);
    }

    // Update To Building tier ComboBox
    if (PipeTierToBuildingComboBox)
    {
        int32 Index = CachedPipeTiers.Find(Settings.PipeTierToBuilding);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        PipeTierToBuildingComboBox->SetSelectedIndex(Index);
    }

    // Ensure both rows are visible for standard pipe junctions
    if (PipeTierMainRow) PipeTierMainRow->SetVisibility(ESlateVisibility::Visible);
    if (PipeTierToBuildingRow) PipeTierToBuildingRow->SetVisibility(ESlateVisibility::Visible);

    // Update Flow Indicator checkbox and row visibility
    // Only show if clean pipes are unlocked - otherwise flow indicators are implicit
    AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
    bool bCleanPipesUnlocked = CachedSubsystem->AreCleanPipesUnlocked(PC);

    if (PipeIndicatorRow)
    {
        PipeIndicatorRow->SetVisibility(bCleanPipesUnlocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (PipeIndicatorCheckBox)
    {
        // If clean pipes not unlocked, force to true (with indicators)
        if (!bCleanPipesUnlocked)
        {
            PipeIndicatorCheckBox->SetIsChecked(true);
        }
        else
        {
            PipeIndicatorCheckBox->SetIsChecked(Settings.bPipeIndicator);
        }
    }

    // Update Routing Mode ComboBox
    if (PipeRoutingModeComboBox)
    {
        PipeRoutingModeComboBox->ClearOptions();
        PipeRoutingModeComboBox->AddOption(TEXT("Auto"));
        PipeRoutingModeComboBox->AddOption(TEXT("Auto 2D"));
        PipeRoutingModeComboBox->AddOption(TEXT("Straight"));
        PipeRoutingModeComboBox->AddOption(TEXT("Curve"));
        PipeRoutingModeComboBox->AddOption(TEXT("Noodle"));
        PipeRoutingModeComboBox->AddOption(TEXT("Horiz→Vert"));

        int32 ModeIndex = FMath::Clamp(Settings.PipeRoutingMode, 0, 5);
        PipeRoutingModeComboBox->SetSelectedIndex(ModeIndex);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Updated pipe controls - Enabled=%d, Main=%d, ToBuilding=%d, Indicator=%d, RoutingMode=%d, CleanUnlocked=%d"),
        Settings.bPipeAutoConnectEnabled, Settings.PipeTierMain, Settings.PipeTierToBuilding, Settings.bPipeIndicator, Settings.PipeRoutingMode, bCleanPipesUnlocked);
}

void USmartSettingsFormWidget::OnPipeEnabledChanged(bool bIsChecked)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    CachedSubsystem->SetAutoConnectPipeEnabled(bIsChecked);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Pipe auto-connect enabled changed to %d"), bIsChecked);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnPipeTierMainChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Tier = DisplayStringToPipeTier(SelectedItem);
    CachedSubsystem->SetAutoConnectPipeTierMain(Tier);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Pipe tier main changed to %d (%s)"), Tier, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnPipeTierToBuildingChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Tier = DisplayStringToPipeTier(SelectedItem);
    CachedSubsystem->SetAutoConnectPipeTierToBuilding(Tier);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Pipe tier to building changed to %d (%s)"), Tier, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnPipeIndicatorChanged(bool bIsChecked)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    CachedSubsystem->SetAutoConnectPipeIndicator(bIsChecked);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Pipe indicator changed to %d (%s)"), bIsChecked, bIsChecked ? TEXT("Normal") : TEXT("Clean"));
}

void USmartSettingsFormWidget::OnPipeRoutingModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Convert display string to routing mode index
    int32 RoutingMode = 0;
    if (SelectedItem == TEXT("Auto")) RoutingMode = 0;
    else if (SelectedItem == TEXT("Auto 2D")) RoutingMode = 1;
    else if (SelectedItem == TEXT("Straight")) RoutingMode = 2;
    else if (SelectedItem == TEXT("Curve")) RoutingMode = 3;
    else if (SelectedItem == TEXT("Noodle")) RoutingMode = 4;
    else if (SelectedItem == TEXT("Horiz→Vert")) RoutingMode = 5;

    CachedSubsystem->SetAutoConnectPipeRoutingMode(RoutingMode);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Pipe routing mode changed to %d (%s)"), RoutingMode, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

// ============================================================================
// Power Auto-Connect Controls
// ============================================================================

FString USmartSettingsFormWidget::PowerGridAxisToDisplayString(int32 Axis) const
{
    switch (Axis)
    {
    case 0: return TEXT("Auto");
    case 1: return TEXT("X");
    case 2: return TEXT("Y");
    case 3: return TEXT("X+Y");
    default: return TEXT("Auto");
    }
}

int32 USmartSettingsFormWidget::DisplayStringToPowerGridAxis(const FString& DisplayString) const
{
    if (DisplayString == TEXT("Auto")) return 0;
    if (DisplayString == TEXT("X")) return 1;
    if (DisplayString == TEXT("Y")) return 2;
    if (DisplayString == TEXT("X+Y")) return 3;
    return 0;
}

void USmartSettingsFormWidget::PopulatePowerComboBoxes()
{
    // Populate Grid Axis ComboBox
    if (PowerGridAxisComboBox)
    {
        PowerGridAxisComboBox->ClearOptions();
        PowerGridAxisComboBox->AddOption(TEXT("Auto"));
        PowerGridAxisComboBox->AddOption(TEXT("X"));
        PowerGridAxisComboBox->AddOption(TEXT("Y"));
        PowerGridAxisComboBox->AddOption(TEXT("X+Y"));
    }

    // Populate Reserved Slots ComboBox (0-5)
    if (PowerReservedComboBox)
    {
        PowerReservedComboBox->ClearOptions();
        for (int32 i = 0; i <= 5; i++)
        {
            PowerReservedComboBox->AddOption(FString::Printf(TEXT("%d"), i));
        }
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Populated power ComboBoxes"));
}

void USmartSettingsFormWidget::UpdatePowerAutoConnectControls()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const auto& Settings = CachedSubsystem->GetAutoConnectRuntimeSettings();

    // Update Enabled checkbox
    if (PowerEnabledCheckBox)
    {
        PowerEnabledCheckBox->SetIsChecked(Settings.bConnectPower);
    }

    // Update Grid Axis ComboBox
    if (PowerGridAxisComboBox)
    {
        PowerGridAxisComboBox->SetSelectedOption(PowerGridAxisToDisplayString(Settings.PowerGridAxis));
    }

    // Update Reserved Slots ComboBox
    if (PowerReservedComboBox)
    {
        PowerReservedComboBox->SetSelectedOption(FString::Printf(TEXT("%d"), Settings.PowerReserved));
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Updated power controls - Enabled=%d, GridAxis=%d, Reserved=%d"),
        Settings.bConnectPower, Settings.PowerGridAxis, Settings.PowerReserved);
}

void USmartSettingsFormWidget::OnPowerEnabledChanged(bool bIsChecked)
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    CachedSubsystem->SetAutoConnectPowerEnabled(bIsChecked);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Power auto-connect enabled changed to %d"), bIsChecked);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnPowerGridAxisChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Axis = DisplayStringToPowerGridAxis(SelectedItem);
    CachedSubsystem->SetAutoConnectPowerGridAxis(Axis);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Power grid axis changed to %d (%s)"), Axis, *SelectedItem);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

void USmartSettingsFormWidget::OnPowerReservedChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes
    }

    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    int32 Reserved = FCString::Atoi(*SelectedItem);
    CachedSubsystem->SetAutoConnectPowerReserved(Reserved);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Power reserved slots changed to %d"), Reserved);

    if (bApplyImmediately)
    {
        CachedSubsystem->TriggerAutoConnectRefresh();
    }
}

// ========================================
// Grid Direction Toggle Handlers
// ========================================

void USmartSettingsFormWidget::OnGridXDirToggleClicked()
{
    bGridXPositive = !bGridXPositive;
    UpdateGridDirectionLabel(GridXDirLabel, bGridXPositive);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Grid X direction toggled to %s"), bGridXPositive ? TEXT("+") : TEXT("-"));
}

void USmartSettingsFormWidget::OnGridYDirToggleClicked()
{
    bGridYPositive = !bGridYPositive;
    UpdateGridDirectionLabel(GridYDirLabel, bGridYPositive);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Grid Y direction toggled to %s"), bGridYPositive ? TEXT("+") : TEXT("-"));
}

void USmartSettingsFormWidget::OnGridZDirToggleClicked()
{
    bGridZPositive = !bGridZPositive;
    UpdateGridDirectionLabel(GridZDirLabel, bGridZPositive);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Grid Z direction toggled to %s"), bGridZPositive ? TEXT("+") : TEXT("-"));
}

void USmartSettingsFormWidget::UpdateGridDirectionLabel(UTextBlock* Label, bool bPositive)
{
    if (Label)
    {
        Label->SetText(FText::FromString(bPositive ? TEXT("+") : TEXT("-")));
    }
}

// ========================================
// Keyboard Input Handling
// ========================================

void USmartSettingsFormWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

FReply USmartSettingsFormWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Escape key behavior depends on state
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        // If confirmation dialog is open, dismiss it
        if (bWaitingForConfirmation)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Escape pressed - dismissing confirmation dialog"));
            OnConfirmNoClicked();
            return FReply::Handled();
        }

        // Otherwise, cancel unapplied changes and close form
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Escape pressed - reverting and closing"));
        RevertToLastAppliedState();
        CloseForm();
        return FReply::Handled();
    }

    // Enter key applies changes (only when a SpinBox has focus)
    if (InKeyEvent.GetKey() == EKeys::Enter && IsAnyTextInputFocused())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Enter pressed with SpinBox focused - applying"));
        OnApplyButtonClicked();
        return FReply::Handled();
    }

    // Issue #212: Tab/Shift+Tab navigates between SpinBox fields in logical order
    if (InKeyEvent.GetKey() == EKeys::Tab && IsAnyTextInputFocused())
    {
        // Build ordered list of all SpinBoxes for tab navigation
        TArray<USpinBox*> TabOrder;
        if (GridXInput) TabOrder.Add(GridXInput);
        if (GridYInput) TabOrder.Add(GridYInput);
        if (GridZInput) TabOrder.Add(GridZInput);
        if (SpacingXInput) TabOrder.Add(SpacingXInput);
        if (SpacingYInput) TabOrder.Add(SpacingYInput);
        if (SpacingZInput) TabOrder.Add(SpacingZInput);
        if (StepsXInput) TabOrder.Add(StepsXInput);
        if (StepsYInput) TabOrder.Add(StepsYInput);
        if (StaggerXInput) TabOrder.Add(StaggerXInput);
        if (StaggerYInput) TabOrder.Add(StaggerYInput);
        if (StaggerZXInput) TabOrder.Add(StaggerZXInput);
        if (StaggerZYInput) TabOrder.Add(StaggerZYInput);
        if (RotationZInput) TabOrder.Add(RotationZInput);

        // Find currently focused SpinBox
        int32 CurrentIndex = INDEX_NONE;
        for (int32 i = 0; i < TabOrder.Num(); ++i)
        {
            if (TabOrder[i]->HasKeyboardFocus())
            {
                CurrentIndex = i;
                break;
            }
        }

        if (CurrentIndex != INDEX_NONE && TabOrder.Num() > 1)
        {
            int32 NextIndex;
            if (InKeyEvent.IsShiftDown())
            {
                // Shift+Tab: go backward (wrap around)
                NextIndex = (CurrentIndex - 1 + TabOrder.Num()) % TabOrder.Num();
            }
            else
            {
                // Tab: go forward (wrap around)
                NextIndex = (CurrentIndex + 1) % TabOrder.Num();
            }

            TabOrder[NextIndex]->SetKeyboardFocus();
            UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Tab navigation %d -> %d"), CurrentIndex, NextIndex);
            return FReply::Handled();
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ========================================
// SpinBox Value Handlers
// ========================================

void USmartSettingsFormWidget::OnSpinBoxValueChanged(float Value)
{
    // Called during slider drag or mouse wheel - only apply in immediate mode
    if (bApplyImmediately)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: SpinBox value changed (immediate mode)"));
        OnApplyButtonClicked();
    }
}

void USmartSettingsFormWidget::OnGridSpinBoxValueChanged(float Value)
{
    // Update warning display whenever grid values change
    UpdateGridWarningDisplay();

    // Also handle immediate mode apply (if still enabled - will be auto-disabled for large grids)
    if (bApplyImmediately)
    {
        UE_LOG(LogSmartFoundations, Verbose, TEXT("Settings Form: Grid SpinBox value changed (immediate mode)"));
        OnApplyButtonClicked();
    }
}

void USmartSettingsFormWidget::OnSpinBoxValueCommitted(float Value, ETextCommit::Type CommitMethod)
{
    // Enter key is now handled globally in NativeOnKeyDown to avoid false triggers
    // from SpinBox button clicks which also fire OnEnter

    // Only apply on focus loss in immediate mode
    if (bApplyImmediately && (CommitMethod == ETextCommit::Default || CommitMethod == ETextCommit::OnUserMovedFocus))
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: SpinBox focus lost (immediate mode) - applying"));
        OnApplyButtonClicked();
    }
}

// ========================================
// Apply Immediately Checkbox Handler
// ========================================

void USmartSettingsFormWidget::OnApplyImmediatelyChanged(bool bIsChecked)
{
    bApplyImmediately = bIsChecked;
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Apply immediately mode changed to %d"), bIsChecked);
}

// ========================================
// State Cache/Revert Helpers
// ========================================

void USmartSettingsFormWidget::CacheCurrentStateAsApplied()
{
    if (CachedSubsystem.IsValid())
    {
        LastAppliedState = CachedSubsystem->GetCounterState();
        bLastAppliedGridXPositive = bGridXPositive;
        bLastAppliedGridYPositive = bGridYPositive;
        bLastAppliedGridZPositive = bGridZPositive;
    }
}

void USmartSettingsFormWidget::RevertToLastAppliedState()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    // Restore the last applied state
    CachedSubsystem->UpdateCounterState(LastAppliedState);
    CachedSubsystem->RegenerateChildHologramGrid();

    // Restore direction state
    bGridXPositive = bLastAppliedGridXPositive;
    bGridYPositive = bLastAppliedGridYPositive;
    bGridZPositive = bLastAppliedGridZPositive;

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Reverted to last applied state"));
}

bool USmartSettingsFormWidget::IsAnyTextInputFocused() const
{
    auto IsSpinBoxFocused = [](USpinBox* SpinBox) -> bool
    {
        return SpinBox && SpinBox->HasKeyboardFocus();
    };

    return IsSpinBoxFocused(GridXInput) ||
           IsSpinBoxFocused(GridYInput) ||
           IsSpinBoxFocused(GridZInput) ||
           IsSpinBoxFocused(SpacingXInput) ||
           IsSpinBoxFocused(SpacingYInput) ||
           IsSpinBoxFocused(SpacingZInput) ||
           IsSpinBoxFocused(StepsXInput) ||
           IsSpinBoxFocused(StepsYInput) ||
           IsSpinBoxFocused(StaggerXInput) ||
           IsSpinBoxFocused(StaggerYInput) ||
           IsSpinBoxFocused(StaggerZXInput) ||
           IsSpinBoxFocused(StaggerZYInput) ||
           IsSpinBoxFocused(RotationZInput);
}

// ========================================
// Large Grid Warning System
// ========================================

int32 USmartSettingsFormWidget::CalculateGridTotal() const
{
    int32 GridX = GridXInput ? FMath::Max(1, FMath::RoundToInt(GridXInput->GetValue())) : 1;
    int32 GridY = GridYInput ? FMath::Max(1, FMath::RoundToInt(GridYInput->GetValue())) : 1;
    int32 GridZ = GridZInput ? FMath::Max(1, FMath::RoundToInt(GridZInput->GetValue())) : 1;

    return GridX * GridY * GridZ;
}

void USmartSettingsFormWidget::UpdateGridWarningDisplay()
{
    int32 GridTotal = CalculateGridTotal();

    // Update Grid Total text
    if (GridTotalText)
    {
        GridTotalText->SetText(FText::Format(LOCTEXT("Panel_GridTotal", "Grid Total: {0} holograms"), FText::AsNumber(GridTotal)));

        // Set color based on warning level
        FLinearColor TextColor;
        if (GridTotal >= GRID_WARNING_THRESHOLD_DANGER)
        {
            TextColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // Red
        }
        else if (GridTotal >= GRID_WARNING_THRESHOLD_WARNING)
        {
            TextColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
        }
        else if (GridTotal >= GRID_WARNING_THRESHOLD_CAUTION)
        {
            TextColor = FLinearColor(1.0f, 0.85f, 0.0f, 1.0f); // Yellow
        }
        else
        {
            TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);  // White
        }
        GridTotalText->SetColorAndOpacity(FSlateColor(TextColor));
    }

    // Update warning text (avoid Unicode symbols - game font doesn't support them)
    if (GridWarningText)
    {
        if (GridTotal >= GRID_WARNING_THRESHOLD_DANGER)
        {
            GridWarningText->SetText(LOCTEXT("Panel_GridWarning_Danger", "* Extreme grid - high risk of freeze/crash"));
            GridWarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)));
            GridWarningText->SetVisibility(ESlateVisibility::Visible);
        }
        else if (GridTotal >= GRID_WARNING_THRESHOLD_WARNING)
        {
            GridWarningText->SetText(LOCTEXT("Panel_GridWarning_Warning", "* Very large grid - may cause temporary freeze"));
            GridWarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f, 1.0f)));
            GridWarningText->SetVisibility(ESlateVisibility::Visible);
        }
        else if (GridTotal >= GRID_WARNING_THRESHOLD_CAUTION)
        {
            GridWarningText->SetText(LOCTEXT("Panel_GridWarning_Caution", "* Large grid"));
            GridWarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)));
            GridWarningText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            GridWarningText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Update Apply Immediately state based on grid size
    UpdateApplyImmediatelyState();
}

bool USmartSettingsFormWidget::RequiresApplyConfirmation() const
{
    return CalculateGridTotal() >= GRID_WARNING_THRESHOLD_WARNING;
}

void USmartSettingsFormWidget::UpdateApplyImmediatelyState()
{
    int32 GridTotal = CalculateGridTotal();

    if (GridTotal >= GRID_WARNING_THRESHOLD_WARNING)
    {
        // Auto-disable Apply Immediately for large grids
        if (bApplyImmediately)
        {
            bApplyImmediatelyWasEnabled = true;
            bApplyImmediately = false;

            if (ApplyImmediatelyCheckBox)
            {
                ApplyImmediatelyCheckBox->SetIsChecked(false);
                ApplyImmediatelyCheckBox->SetIsEnabled(false);
            }

            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Apply Immediately auto-disabled for large grid (%d objects)"), GridTotal);
        }
        else if (ApplyImmediatelyCheckBox)
        {
            // Keep it disabled even if user didn't have it enabled
            ApplyImmediatelyCheckBox->SetIsEnabled(false);
        }
    }
    else
    {
        // Re-enable Apply Immediately checkbox
        if (ApplyImmediatelyCheckBox)
        {
            ApplyImmediatelyCheckBox->SetIsEnabled(true);
        }

        // Restore previous state if it was auto-disabled
        if (bApplyImmediatelyWasEnabled)
        {
            bApplyImmediately = true;
            bApplyImmediatelyWasEnabled = false;

            if (ApplyImmediatelyCheckBox)
            {
                ApplyImmediatelyCheckBox->SetIsChecked(true);
            }

            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Apply Immediately restored (grid now %d objects)"), GridTotal);
        }
    }
}

void USmartSettingsFormWidget::ShowLargeGridConfirmation(TFunction<void()> OnConfirmed)
{
    int32 GridTotal = CalculateGridTotal();

    // Store callback for when user confirms
    PendingConfirmCallback = OnConfirmed;
    bWaitingForConfirmation = true;

    // Determine warning level and message
    FString Title;
    FString Message;
    FLinearColor TitleColor;

    if (GridTotal >= GRID_WARNING_THRESHOLD_DANGER)
    {
        Title = LOCTEXT("Panel_Confirm_ExtremeTitle", "EXTREME GRID WARNING!").ToString();
        Message = FText::Format(LOCTEXT("Panel_Confirm_ExtremeMsg", "You are about to generate {0} holograms.\n\nThis will likely cause a significant freeze and may crash the game.\n\nAre you sure you want to continue?"), FText::AsNumber(GridTotal)).ToString();
        TitleColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // Red
        UE_LOG(LogSmartFoundations, Warning, TEXT("Settings Form: Showing EXTREME grid confirmation for %d objects"), GridTotal);
    }
    else if (GridTotal >= GRID_WARNING_THRESHOLD_WARNING)
    {
        Title = LOCTEXT("Panel_Confirm_LargeTitle", "Large Grid Warning").ToString();
        Message = FText::Format(LOCTEXT("Panel_Confirm_LargeMsg", "You are about to generate {0} holograms.\n\nThis may cause a temporary freeze.\n\nContinue?"), FText::AsNumber(GridTotal)).ToString();
        TitleColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
        UE_LOG(LogSmartFoundations, Warning, TEXT("Settings Form: Showing large grid confirmation for %d objects"), GridTotal);
    }
    else
    {
        // No confirmation needed, just proceed
        bWaitingForConfirmation = false;
        if (OnConfirmed)
        {
            OnConfirmed();
        }
        return;
    }

    // Show the confirmation dialog
    ShowConfirmationDialog(Title, Message, TitleColor);
}

void USmartSettingsFormWidget::ShowConfirmationDialog(const FString& Title, const FString& Message, const FLinearColor& TitleColor)
{
    if (!ConfirmationSizeBox)
    {
        // No dialog widget available, just proceed
        UE_LOG(LogSmartFoundations, Warning, TEXT("Settings Form: Confirmation dialog not available, proceeding anyway"));
        if (PendingConfirmCallback)
        {
            PendingConfirmCallback();
        }
        bWaitingForConfirmation = false;
        return;
    }

    // Set dialog content
    if (ConfirmationTitle)
    {
        ConfirmationTitle->SetText(FText::FromString(Title));
        ConfirmationTitle->SetColorAndOpacity(FSlateColor(TitleColor));
    }

    if (ConfirmationMessage)
    {
        ConfirmationMessage->SetText(FText::FromString(Message));
    }

    // Show the dialog (use SizeBox for visibility control)
    ConfirmationSizeBox->SetVisibility(ESlateVisibility::Visible);

    // Focus the Cancel button by default (safer option)
    if (ConfirmNoButton)
    {
        ConfirmNoButton->SetKeyboardFocus();
    }
}

void USmartSettingsFormWidget::HideConfirmationDialog()
{
    if (ConfirmationSizeBox)
    {
        ConfirmationSizeBox->SetVisibility(ESlateVisibility::Collapsed);
    }
    bWaitingForConfirmation = false;
}

void USmartSettingsFormWidget::OnConfirmYesClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: User confirmed large grid operation"));

    HideConfirmationDialog();

    // Execute the pending callback
    if (PendingConfirmCallback)
    {
        PendingConfirmCallback();
        PendingConfirmCallback = nullptr;
    }
}

void USmartSettingsFormWidget::OnConfirmNoClicked()
{
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: User cancelled large grid operation"));

    HideConfirmationDialog();
    PendingConfirmCallback = nullptr;
}

// ============================================================================
// Stackable Pole Auto-Connect Controls
// ============================================================================

void USmartSettingsFormWidget::UpdateStackableBeltAutoConnectControls()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const auto& Settings = CachedSubsystem->GetAutoConnectRuntimeSettings();

    // Log widget availability for debugging
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Stackable Belt Widget Status: Direction=%s, TierToBuilding=%s, Chain=%s"),
        StackableBeltDirectionComboBox ? TEXT("Valid") : TEXT("NULL"),
        BeltTierToBuildingComboBox ? TEXT("Valid") : TEXT("NULL"),
        BeltChainCheckBox ? TEXT("Valid") : TEXT("NULL"));

    // Update Enabled checkbox
    if (BeltEnabledCheckBox)
    {
        BeltEnabledCheckBox->SetIsChecked(Settings.bStackableBeltEnabled);
    }

    // Update Tier ComboBox (reuse main belt tier)
    if (BeltTierMainComboBox)
    {
        int32 Index = CachedBeltTiers.Find(Settings.BeltTierMain);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        BeltTierMainComboBox->SetSelectedIndex(Index);
    }

    // Use dedicated StackableBeltDirectionComboBox for direction selection
    if (StackableBeltDirectionComboBox)
    {
        StackableBeltDirectionComboBox->ClearOptions();
        StackableBeltDirectionComboBox->AddOption(TEXT("Forward"));
        StackableBeltDirectionComboBox->AddOption(TEXT("Backward"));

        int32 DirectionIndex = Settings.StackableBeltDirection;
        StackableBeltDirectionComboBox->SetSelectedIndex(DirectionIndex);
    }

    // Show direction row, hide irrelevant rows for stackable poles
    if (StackableBeltDirectionRow)
    {
        StackableBeltDirectionRow->SetVisibility(ESlateVisibility::Visible);
    }
    if (BeltTierToBuildingComboBox)
    {
        BeltTierToBuildingComboBox->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BeltTierToBuildingLabel)
    {
        BeltTierToBuildingLabel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BeltChainCheckBox)
    {
        BeltChainCheckBox->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BeltChainLabel)
    {
        BeltChainLabel->SetVisibility(ESlateVisibility::Collapsed);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Updated stackable belt controls - Enabled=%d, Tier=%d, Direction=%d"),
        Settings.bStackableBeltEnabled, Settings.BeltTierMain, Settings.StackableBeltDirection);
}

void USmartSettingsFormWidget::UpdateStackablePipeAutoConnectControls()
{
    if (!CachedSubsystem.IsValid())
    {
        return;
    }

    const auto& Settings = CachedSubsystem->GetAutoConnectRuntimeSettings();

    // Update Enabled checkbox
    if (PipeEnabledCheckBox)
    {
        PipeEnabledCheckBox->SetIsChecked(Settings.bPipeAutoConnectEnabled);
    }

    // Update To Building tier ComboBox (Stackable and Floor holes use this per user request)
    if (PipeTierToBuildingComboBox)
    {
        int32 Index = CachedPipeTiers.Find(Settings.PipeTierToBuilding);
        if (Index == INDEX_NONE)
        {
            Index = 0;  // Default to Auto if current setting not in list
        }
        PipeTierToBuildingComboBox->SetSelectedIndex(Index);
    }

    // Hide irrelevant controls for stackable pipe supports
    if (PipeTierMainRow) PipeTierMainRow->SetVisibility(ESlateVisibility::Collapsed);
    if (PipeTierToBuildingRow) PipeTierToBuildingRow->SetVisibility(ESlateVisibility::Visible);

    // Update Flow Indicator checkbox and row visibility
    // Only show if clean pipes are unlocked - otherwise flow indicators are implicit
    AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
    bool bCleanPipesUnlocked = CachedSubsystem->AreCleanPipesUnlocked(PC);

    if (PipeIndicatorRow)
    {
        PipeIndicatorRow->SetVisibility(bCleanPipesUnlocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (PipeIndicatorCheckBox)
    {
        // If clean pipes not unlocked, force to true (with indicators)
        if (!bCleanPipesUnlocked)
        {
            PipeIndicatorCheckBox->SetIsChecked(true);
        }
        else
        {
            PipeIndicatorCheckBox->SetIsChecked(Settings.bPipeIndicator);
        }
    }

    // Update Routing Mode ComboBox (same as regular pipe controls)
    if (PipeRoutingModeComboBox)
    {
        PipeRoutingModeComboBox->ClearOptions();
        PipeRoutingModeComboBox->AddOption(TEXT("Auto"));
        PipeRoutingModeComboBox->AddOption(TEXT("Auto 2D"));
        PipeRoutingModeComboBox->AddOption(TEXT("Straight"));
        PipeRoutingModeComboBox->AddOption(TEXT("Curve"));
        PipeRoutingModeComboBox->AddOption(TEXT("Noodle"));
        PipeRoutingModeComboBox->AddOption(TEXT("Horiz→Vert"));

        int32 ModeIndex = FMath::Clamp(Settings.PipeRoutingMode, 0, 5);
        PipeRoutingModeComboBox->SetSelectedIndex(ModeIndex);
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Updated stackable/floor hole pipe controls - Enabled=%d, Tier=%d, Indicator=%d, RoutingMode=%d, CleanUnlocked=%d"),
        Settings.bPipeAutoConnectEnabled, Settings.PipeTierToBuilding, Settings.bPipeIndicator, Settings.PipeRoutingMode, bCleanPipesUnlocked);
}

#undef LOCTEXT_NAMESPACE
