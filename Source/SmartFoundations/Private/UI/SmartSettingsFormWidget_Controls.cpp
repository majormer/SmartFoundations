// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USmartSettingsFormWidget - belt/pipe/power tier controls + grid direction + key input + spinbox + confirmation dialogs. Split out of SmartSettingsFormWidget.cpp (slice U1,
 * pure impl-split: only .cpp bodies move, the .h (BindWidget members + UFUNCTIONs) is byte-
 * identical, so the Smart_SettingsForm_Widget BP contract is unchanged. No behavior change.
 */

#include "UI/SmartSettingsFormWidgetImpl.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

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

void USmartSettingsFormWidget::OnRotationAxisChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct)
    {
        return;  // Ignore programmatic changes (our own SetSelectedIndex during populate)
    }
    if (!CachedSubsystem.IsValid())
    {
        return;
    }
    // [#372] Always-yaw rotation; this only selects whether the yaw progresses along X-clones or Y-rows.
    FSFCounterState NewState = CachedSubsystem->GetCounterState();
    NewState.RotationAxis = (SelectedItem == TEXT("Y")) ? ESFScaleAxis::Y : ESFScaleAxis::X;
    CachedSubsystem->UpdateCounterState(NewState);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: Rotation progression axis -> %s"), *SelectedItem);
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

FReply USmartSettingsFormWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Issue #230: Up/Down stepping on a focused SpinBox.
    //
    // Engine bug: SSpinBox::OnKeyDown commits the arrowed value and calls ExitTextMode(), but
    // ExitTextMode() only flips widget visibility - it leaves the (now hidden) inner EditableText
    // focused and still holding the pre-edit text. When focus finally leaves the box, that stale
    // text is committed (TextField_OnTextCommitted, OnUserMovedFocus) and reverts the arrowed value.
    //
    // We handle the step ourselves in the tunnel (preview) phase, before the SpinBox sees the key,
    // and pull focus out of the stale EditableText so the revert can never fire. Left/Right are left
    // alone so the caret can still move while typing.
    const FKey Key = InKeyEvent.GetKey();
    const bool bUp = (Key == EKeys::Up);
    const bool bDown = (Key == EKeys::Down);
    if (bUp || bDown)
    {
        if (USpinBox* Focused = GetFocusedSpinBox())
        {
            // Take focus OFF the inner EditableText so its stale pre-edit text can't revert our step.
            // Focusing the SpinBox with a Mouse cause is deliberate: SSpinBox::OnFocusReceived only
            // re-enters text mode for Navigation/SetDirectly causes (SetKeyboardFocus() uses SetDirectly,
            // which is why it failed), so a Mouse-cause focus leaves the box in display mode with no
            // editable text holding stale text. Moving focus also commits+exits any active text entry,
            // honoring a value the user typed; we then apply the step on top.
            if (FSlateApplication::IsInitialized())
            {
                if (TSharedPtr<SWidget> SlateWidget = Focused->GetCachedWidget())
                {
                    FSlateApplication::Get().SetUserFocus(InKeyEvent.GetUserIndex(), SlateWidget, EFocusCause::Mouse);
                }
            }

            float Step = Focused->GetDelta();
            if (Step <= 0.0f)
            {
                Step = 1.0f;
            }

            // SSpinBox clamps to its min/max inside CommitValue, and firing SetValue runs the bound
            // OnValueChanged handler (grid warning refresh + immediate-mode apply), so no extra work here.
            Focused->SetValue(Focused->GetValue() + (bUp ? Step : -Step));

            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Settings Form: arrow-stepped %s to %.2f"),
                *Focused->GetName(), Focused->GetValue());
            return FReply::Handled();
        }
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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

USpinBox* USmartSettingsFormWidget::GetFocusedSpinBox() const
{
    USpinBox* const All[] = {
        GridXInput, GridYInput, GridZInput,
        SpacingXInput, SpacingYInput, SpacingZInput,
        StepsXInput, StepsYInput,
        StaggerXInput, StaggerYInput, StaggerZXInput, StaggerZYInput,
        RotationZInput
    };
    for (USpinBox* SpinBox : All)
    {
        if (SpinBox && SpinBox->HasKeyboardFocus())
        {
            return SpinBox;
        }
    }
    return nullptr;
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

    // #351: populate the Routing combobox for stackable conveyor poles too. It was only populated in
    // the distributor path (UpdateBeltAutoConnectControls), so on a stackable pole it was empty and
    // never dropped down. Stackable belt auto-connect honors BeltRoutingMode (SFAutoConnectService_Stackable).
    if (BeltRoutingModeComboBox)
    {
        BeltRoutingModeComboBox->ClearOptions();
        BeltRoutingModeComboBox->AddOption(TEXT("Default"));
        BeltRoutingModeComboBox->AddOption(TEXT("Curve"));
        BeltRoutingModeComboBox->AddOption(TEXT("Straight"));
        BeltRoutingModeComboBox->SetSelectedIndex(FMath::Clamp(Settings.BeltRoutingMode, 0, 2));
    }
    if (BeltRoutingModeRow)
    {
        BeltRoutingModeRow->SetVisibility(ESlateVisibility::Visible);
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

