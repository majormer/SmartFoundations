#include "UI/SmartUpgradePanel.h"
#include "SmartFoundations.h"
#include "FGPlayerController.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFHudService.h"
#include "Services/SFChainActorService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Styling/SlateTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Public/SFRCO.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGunBuild.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

namespace
{
	bool IsConveyorUpgradeFamily(ESFUpgradeFamily Family)
	{
		return Family == ESFUpgradeFamily::Belt || Family == ESFUpgradeFamily::Lift;
	}

	FString GetPanelFamilyDisplayName(ESFUpgradeFamily Family)
	{
		return IsConveyorUpgradeFamily(Family)
			? FString(TEXT("Conveyors"))
			: USFUpgradeAuditService::GetFamilyDisplayName(Family);
	}
}

void USmartUpgradePanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogSmartFoundations, Log, TEXT("Upgrade Panel: NativeConstruct"));

	// ========== BACKGROUND & HEADER LAYOUT ==========

	// Set BackgroundBorder to auto-size to its content (MainLayout)
	if (BackgroundBorder)
	{
		if (UCanvasPanelSlot* BGSlot = Cast<UCanvasPanelSlot>(BackgroundBorder->Slot))
		{
			BGSlot->SetAutoSize(true);
		}
	}

	// Hide redundant RefreshButton (Scan button in Radius tab does the same thing)
	if (RefreshButton)
	{
		RefreshButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Hide legacy CancelButton (SharedCancelButton is used instead)
	if (CancelButton)
	{
		CancelButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Set ContextHeaderText to fill available space (pushes CloseButton to the right)
	if (ContextHeaderText)
	{
		if (UHorizontalBoxSlot* HeaderTextSlot = Cast<UHorizontalBoxSlot>(ContextHeaderText->Slot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;
			HeaderTextSlot->SetSize(FillSize);
			HeaderTextSlot->SetVerticalAlignment(VAlign_Center);
			HeaderTextSlot->SetPadding(FMargin(4.0f, 4.0f, 0.0f, 4.0f));
		}
	}

	// Set CloseButton alignment in header
	if (CloseButton)
	{
		if (UHorizontalBoxSlot* CloseSlot = Cast<UHorizontalBoxSlot>(CloseButton->Slot))
		{
			CloseSlot->SetVerticalAlignment(VAlign_Center);
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
			CloseSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
		}
	}

	// ========== LOCALIZE BLUEPRINT LABELS ==========
	{
		auto SetLabel = [this](const TCHAR* WidgetName, const FText& Text)
		{
			if (UTextBlock* TB = Cast<UTextBlock>(GetWidgetFromName(FName(WidgetName))))
			{
				TB->SetText(Text);
			}
		};

		// Header
		if (ContextHeaderText)
		{
			ContextHeaderText->SetText(LOCTEXT("Upgrade_Title", "Smart! Upgrade Panel"));
		}

		// Tab buttons
		SetLabel(TEXT("RadiusTabButtonText"), LOCTEXT("Upgrade_Tab_Radius", "Radius"));
		SetLabel(TEXT("TraversalTabButtonText"), LOCTEXT("Upgrade_Tab_Network", "Network"));
		SetLabel(TEXT("TriageTabButtonText"), LOCTEXT("Upgrade_Tab_Triage", "Triage"));

		// Radius tab labels
		SetLabel(TEXT("RadiusSliderLabel"), LOCTEXT("Upgrade_RadiusLabel", "Radius (m):"));
		SetLabel(TEXT("RadiusScanButtonText"), LOCTEXT("Upgrade_Btn_Scan", "Scan"));
		SetLabel(TEXT("EntireMapButtonText"), LOCTEXT("Upgrade_Btn_EntireMap", "Entire Map"));
		SetLabel(TEXT("RadiusTargetTierLabel"), LOCTEXT("Upgrade_TargetTier", "Target Tier:"));
		SetLabel(TEXT("RadiusCostHeaderText"), LOCTEXT("Upgrade_EstimatedCost", "Estimated Cost"));

		// Traversal tab labels
		SetLabel(TEXT("TraversalScanButtonText"), LOCTEXT("Upgrade_Btn_ScanNetwork", "Scan Network"));
		SetLabel(TEXT("TraversalTargetTierLabel"), LOCTEXT("Upgrade_TargetTier", "Target Tier:"));
		SetLabel(TEXT("TraversalCostHeaderText"), LOCTEXT("Upgrade_EstimatedCost", "Estimated Cost"));
		SetLabel(TEXT("CrossSplittersLabel"), LOCTEXT("Upgrade_CrossSplitters", "Cross Splitters/Mergers"));
		SetLabel(TEXT("CrossStorageLabel"), LOCTEXT("Upgrade_CrossStorage", "Cross Storage Containers"));
		SetLabel(TEXT("CrossTrainPlatformsLabel"), LOCTEXT("Upgrade_CrossTrains", "Cross Train Platforms"));

		// Bottom buttons
		if (SharedUpgradeButtonText)
		{
			SharedUpgradeButtonText->SetText(LOCTEXT("Upgrade_Btn_Upgrade", "UPGRADE"));
		}
		SetLabel(TEXT("SharedCancelButtonText"), LOCTEXT("Upgrade_Btn_Cancel", "Cancel"));
		SetLabel(TEXT("SharedCloseButtonText"), LOCTEXT("Upgrade_Btn_Close", "Close"));
		SetLabel(TEXT("CloseButtonText"), LOCTEXT("Upgrade_Btn_X", "X"));

		// Triage button labels
		SetLabel(TEXT("TriageDetectButtonText"), LOCTEXT("Triage_Btn_Detect", "Detect Issues"));
		SetLabel(TEXT("TriageRepairButtonText"), LOCTEXT("Triage_Btn_Repair", "Repair All Issues"));
	}

	// ========== TABBED UI BINDINGS ==========

	// Bind tab buttons
	if (RadiusTabButton)
	{
		RadiusTabButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnRadiusTabClicked);
	}
	if (TraversalTabButton)
	{
		TraversalTabButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnTraversalTabClicked);
	}
	if (TriageTabButton)
	{
		TriageTabButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnTriageTabClicked);
	}

	// Configure new radius spinbox (RadiusSliderSpinBox)
	if (RadiusSliderSpinBox)
	{
		RadiusSliderSpinBox->SetMinValue(0.0f);
		RadiusSliderSpinBox->SetMinSliderValue(4.0f);
		RadiusSliderSpinBox->SetMaxValue(10000.0f);
		RadiusSliderSpinBox->SetMaxSliderValue(1000.0f);
		RadiusSliderSpinBox->SetValue(CurrentRadiusMeters);
		RadiusSliderSpinBox->SetDelta(4.0f);
		RadiusSliderSpinBox->OnValueChanged.AddDynamic(this, &USmartUpgradePanel::OnRadiusValueChanged);
		UE_LOG(LogSmartFoundations, Log, TEXT("Upgrade Panel: RadiusSliderSpinBox configured"));
	}

	// Bind radius scan button
	if (RadiusScanButton)
	{
		RadiusScanButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnRefreshButtonClicked);
	}

	// Bind "Entire Map" button - scans all buildables with no radius limit
	if (EntireMapButton)
	{
		EntireMapButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnEntireMapButtonClicked);
	}

	// Bind radius target tier combo box
	if (RadiusTargetTierComboBox)
	{
		RadiusTargetTierComboBox->OnSelectionChanged.AddDynamic(this, &USmartUpgradePanel::OnTargetTierChanged);
		RadiusTargetTierComboBox->SetVisibility(ESlateVisibility::Collapsed);

		// Style dropdown items for readability on dark background
		FTableRowStyle RadiusItemStyle = RadiusTargetTierComboBox->GetItemStyle();
		RadiusItemStyle.TextColor = FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));
		RadiusItemStyle.SelectedTextColor = FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f));
		RadiusItemStyle.ActiveBrush.DrawAs = ESlateBrushDrawType::Box;
		RadiusItemStyle.ActiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		RadiusItemStyle.InactiveBrush.DrawAs = ESlateBrushDrawType::Box;
		RadiusItemStyle.InactiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		RadiusTargetTierComboBox->SetItemStyle(RadiusItemStyle);
	}

	// Initialize radius cost display
	if (RadiusCostDetailsText)
	{
		RadiusCostDetailsText->SetText(LOCTEXT("Upgrade_SelectFamily", "Select a family to see costs"));
	}

	// Bind traversal scan button
	if (TraversalScanButton)
	{
		TraversalScanButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnTraversalScanClicked);
	}

	// Bind traversal target tier combo box
	if (TraversalTargetTierComboBox)
	{
		TraversalTargetTierComboBox->OnSelectionChanged.AddDynamic(this, &USmartUpgradePanel::OnTargetTierChanged);
		TraversalTargetTierComboBox->SetVisibility(ESlateVisibility::Collapsed);

		// Style dropdown items for readability on dark background
		FTableRowStyle TraversalItemStyle = TraversalTargetTierComboBox->GetItemStyle();
		TraversalItemStyle.TextColor = FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));
		TraversalItemStyle.SelectedTextColor = FSlateColor(FLinearColor(1.0f, 0.6f, 0.2f, 1.0f));
		TraversalItemStyle.ActiveBrush.DrawAs = ESlateBrushDrawType::Box;
		TraversalItemStyle.ActiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		TraversalItemStyle.InactiveBrush.DrawAs = ESlateBrushDrawType::Box;
		TraversalItemStyle.InactiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		TraversalTargetTierComboBox->SetItemStyle(TraversalItemStyle);
	}

	// Initialize traversal UI
	if (TraversalAnchorText)
	{
		TraversalAnchorText->SetText(LOCTEXT("Upgrade_TraversalHint", "Aim at a belt, pipe, or pole to scan its network"));
	}
	if (TraversalCostDetailsText)
	{
		TraversalCostDetailsText->SetText(LOCTEXT("Upgrade_ScanCosts", "Scan network to see costs"));
	}

	// ========== TRIAGE TAB INIT ==========

	// Set warning text
	if (TriageWarningText)
	{
		TriageWarningText->SetText(LOCTEXT("Triage_Warning",
			"These tools diagnose conveyor chain issues after mass upgrades or save/load. "
			"Repair purges zombie chains, rebuilds split chains, and re-registers orphaned conveyor tick groups after load. "
			"Save before repair; after repair, save, reload, and run Detect again."));
	}

	// Initialize result text labels (collapsed by default)
	if (TriageDetectResultText)
	{
		TriageDetectResultText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TriageRepairButton)
	{
		TriageRepairButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TriageRepairResultText)
	{
		TriageRepairResultText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Bind triage buttons
	if (TriageDetectButton)
	{
		TriageDetectButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnTriageDetectClicked);
	}
	if (TriageRepairButton)
	{
		TriageRepairButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnTriageRepairClicked);
	}

	// Set default checkbox states
	if (CrossSplittersCheckBox)
	{
		CrossSplittersCheckBox->SetIsChecked(true);
	}
	if (CrossStorageCheckBox)
	{
		CrossStorageCheckBox->SetIsChecked(true);
	}
	if (CrossTrainPlatformsCheckBox)
	{
		CrossTrainPlatformsCheckBox->SetIsChecked(false);
	}

	// Bind shared buttons
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnUpgradeButtonClicked);
		SharedUpgradeButton->SetIsEnabled(false);
	}
	if (SharedCancelButton)
	{
		SharedCancelButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnCancelButtonClicked);
	}
	if (SharedCloseButton)
	{
		SharedCloseButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnCloseButtonClicked);
	}

	// Initialize shared status
	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Upgrade_Ready", "Ready"));
	}

	// Start with Radius tab active
	SwitchToTab(ESmartUpgradeTab::Radius);

	// ========== LEGACY BINDINGS (backward compatibility) ==========

	// Bind close button click event (legacy)
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnCloseButtonClicked);
	}

	// Bind refresh button click event
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnRefreshButtonClicked);
	}

	// Bind cancel button click event
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnCancelButtonClicked);
	}

	// Configure legacy radius spinbox
	if (RadiusSpinBox)
	{
		RadiusSpinBox->SetMinValue(4.0f);
		RadiusSpinBox->SetMinSliderValue(4.0f);
		RadiusSpinBox->SetMaxValue(10000.0f);
		RadiusSpinBox->SetMaxSliderValue(1000.0f);
		RadiusSpinBox->SetValue(CurrentRadiusMeters);
		RadiusSpinBox->SetDelta(4.0f);
		RadiusSpinBox->OnValueChanged.AddDynamic(this, &USmartUpgradePanel::OnRadiusValueChanged);
	}

	// Bind legacy upgrade button click event
	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnUpgradeButtonClicked);
		UpgradeButton->SetIsEnabled(false);
	}

	// Bind legacy target tier combo box change event
	if (TargetTierComboBox)
	{
		TargetTierComboBox->OnSelectionChanged.AddDynamic(this, &USmartUpgradePanel::OnTargetTierChanged);
		TargetTierComboBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Initialize legacy cost display
	if (CostDetailsText)
	{
		CostDetailsText->SetText(LOCTEXT("Upgrade_SelectFamily", "Select a family to see costs"));
	}

	// Subscribe to audit service events
	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		if (USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService())
		{
			AuditService->OnAuditProgressUpdated.AddDynamic(this, &USmartUpgradePanel::UpdateAuditProgress);
			AuditService->OnAuditCompleted.AddDynamic(this, &USmartUpgradePanel::UpdateAuditUI);

			// If we already have results, show them
			if (AuditService->HasValidResults())
			{
				UpdateAuditUI(AuditService->GetLastResult());
			}
			else if (!AuditService->IsAuditInProgress())
			{
				if (StatusText)
				{
					StatusText->SetText(LOCTEXT("Upgrade_NoAudit", "No audit snapshot. Press Scan to start."));
				}
				if (SharedStatusText)
				{
					SharedStatusText->SetText(LOCTEXT("Upgrade_PressScan", "Press Scan to find upgradeable items"));
				}
			}
		}
	}
}

void USmartUpgradePanel::OnCloseButtonClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Close button clicked"));
	ClosePanel();
}

void USmartUpgradePanel::OnRefreshButtonClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Refresh button clicked"));
	RefreshAudit();
}

void USmartUpgradePanel::OnCancelButtonClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Cancel button clicked"));
	CancelAudit();
}

void USmartUpgradePanel::OnEntireMapButtonClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Entire Map button clicked - scanning with no radius limit"));
	CurrentRadiusMeters = 0.0f;

	// Update spinbox to show 0 (visual indicator that "entire map" is active)
	if (RadiusSliderSpinBox)
	{
		RadiusSliderSpinBox->SetValue(0.0f);
	}

	// Trigger the scan immediately
	RefreshAudit();
}

void USmartUpgradePanel::OnRadiusValueChanged(float NewValue)
{
	// 0 = entire map (no radius limit), otherwise snap to 4m increments
	if (NewValue < 2.0f)
	{
		CurrentRadiusMeters = 0.0f;
	}
	else
	{
		CurrentRadiusMeters = FMath::RoundToFloat(NewValue / 4.0f) * 4.0f;
		CurrentRadiusMeters = FMath::Clamp(CurrentRadiusMeters, 4.0f, 10000.0f);
	}

	// Update spinbox if snapped value differs
	if (RadiusSpinBox && FMath::Abs(RadiusSpinBox->GetValue() - CurrentRadiusMeters) > 0.1f)
	{
		RadiusSpinBox->SetValue(CurrentRadiusMeters);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Radius changed to %.0fm"), CurrentRadiusMeters);
}

void USmartUpgradePanel::OnUpgradeButtonClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Upgrade button clicked - Family=%d Tier=%d IsRadiusTab=%d"),
		static_cast<int32>(SelectedFamily), SelectedTier, (ActiveTab == ESmartUpgradeTab::Radius) ? 1 : 0);

	// In traversal mode, we may have items at multiple tiers - don't require SelectedTier
	bool bIsTraversalMode = (ActiveTab == ESmartUpgradeTab::Traversal) && CachedTraversalResult.IsValid();

	if (SelectedFamily == ESFUpgradeFamily::None)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_NoFamily", "No family selected - scan first"));
		}
		return;
	}

	// For radius mode, require a specific tier selection
	if (!bIsTraversalMode && SelectedTier == 0)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_SelectRow", "Select a row first to upgrade"));
		}
		return;
	}

	// Get subsystem and execution service
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!Subsystem)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_ErrSubsystem", "Error: Subsystem not available"));
		}
		return;
	}

	USFUpgradeExecutionService* ExecutionService = Subsystem->GetUpgradeExecutionService();
	if (!ExecutionService)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_ErrExecution", "Error: Execution service not available"));
		}
		return;
	}

	// Get player location for radius-based selection
	APlayerController* PC = GetOwningPlayer();
	FVector PlayerLocation = FVector::ZeroVector;
	if (PC && PC->GetPawn())
	{
		PlayerLocation = PC->GetPawn()->GetActorLocation();
	}

	// Get target tier from dropdown (or use cached value)
	int32 TargetTier = GetSelectedTargetTier();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: GetSelectedTargetTier returned %d, CachedTargetTier=%d"), TargetTier, CachedTargetTier);
	if (TargetTier == 0)
	{
		// No target selected - use max unlocked tier
		TargetTier = CachedTargetTier;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Using CachedTargetTier=%d as fallback"), TargetTier);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Starting upgrade - SourceTier=%d TargetTier=%d TraversalMode=%d"),
		SelectedTier, TargetTier, bIsTraversalMode);

	// Validate target tier
	// In traversal mode, we upgrade everything below target tier, so just need valid target
	// In radius mode, target must be greater than source
	if (bIsTraversalMode)
	{
		if (TargetTier <= 1)
		{
			if (StatusText)
			{
				StatusText->SetText(LOCTEXT("Upgrade_SelectTier", "Select a target tier from the dropdown"));
			}
			return;
		}
	}
	else if (TargetTier <= SelectedTier)
	{
		FString FamilyName = USFUpgradeAuditService::GetFamilyDisplayName(SelectedFamily);
		if (StatusText)
		{
			StatusText->SetText(FText::Format(LOCTEXT("Upgrade_NoValidTarget", "{0} Mk.{1}: No valid upgrade target selected"), FText::FromString(FamilyName), FText::AsNumber(SelectedTier)));
		}
		return;
	}

	// Build execution params
	FSFUpgradeExecutionParams Params;
	Params.Family = SelectedFamily;
	Params.SourceTier = SelectedTier;
	Params.TargetTier = TargetTier;
	Params.PlayerController = Cast<AFGPlayerController>(PC);  // For cost deduction
	Params.MaxItems = 0;  // No limit

	// Check if we're in traversal mode with valid results
	if ((ActiveTab == ESmartUpgradeTab::Traversal) && CachedTraversalResult.IsValid())
	{
		// Use specific buildables from traversal scan
		for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
		{
			if (Entry.Buildable.IsValid())
			{
				Params.SpecificBuildables.Add(Entry.Buildable);
			}
		}
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Using %d buildables from traversal scan"), Params.SpecificBuildables.Num());
	}
	else
	{
		// Radius-based selection
		Params.Origin = PlayerLocation;
		Params.Radius = CurrentRadiusMeters * 100.0f;  // Convert meters to cm
	}

	// Subscribe to completion delegate
	ExecutionService->OnUpgradeExecutionCompleted.AddDynamic(this, &USmartUpgradePanel::OnUpgradeCompleted);
	ExecutionService->OnUpgradeProgressUpdated.AddDynamic(this, &USmartUpgradePanel::OnUpgradeProgress);

	// Start the upgrade
	FString FamilyName = USFUpgradeAuditService::GetFamilyDisplayName(SelectedFamily);
	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Upgrading", "Upgrading {0} Mk.{1} \u2192 Mk.{2}..."), FText::FromString(FamilyName), FText::AsNumber(SelectedTier), FText::AsNumber(TargetTier)));
	}

	ExecutionService->StartUpgrade(Params);
}

void USmartUpgradePanel::RefreshAudit()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: RefreshAudit() requested"));

	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		APlayerController* PC = GetOwningPlayer();
		UWorld* World = GetWorld();

		if (PC && World)
		{
			FSFUpgradeAuditParams Params;
			Params.Origin = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			Params.Radius = CurrentRadiusMeters * 100.0f;  // Convert meters to cm
			Params.bStoreEntries = true;

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Starting audit with radius %.0fm (%.0f cm)"),
				CurrentRadiusMeters, Params.Radius);

			// Trigger via RCO if we're a client, otherwise direct call
			if (World->GetNetMode() == NM_Client)
			{
				TArray<AActor*> RCOActors;
				UGameplayStatics::GetAllActorsOfClass(World, USFRCO::StaticClass(), RCOActors);

				for (AActor* Actor : RCOActors)
				{
					if (USFRCO* RCO = Cast<USFRCO>(Actor))
					{
						if (RCO->GetOuter() == PC)
						{
							RCO->Server_StartUpgradeAudit(Params);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Sent Refresh request via SFRCO"));
							return;
						}
					}
				}
				UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: Could not find SFRCO instance for Refresh"));
			}
			else
			{
				// Local/Server direct call
				if (USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService())
				{
					AuditService->StartAudit(Params);
				}
			}
		}
	}
}

void USmartUpgradePanel::CancelAudit()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: CancelAudit() requested"));

	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		APlayerController* PC = GetOwningPlayer();
		UWorld* World = GetWorld();

		if (PC && World)
		{
			if (World->GetNetMode() == NM_Client)
			{
				TArray<AActor*> RCOActors;
				UGameplayStatics::GetAllActorsOfClass(World, USFRCO::StaticClass(), RCOActors);

				for (AActor* Actor : RCOActors)
				{
					if (USFRCO* RCO = Cast<USFRCO>(Actor))
					{
						if (RCO->GetOuter() == PC)
						{
							RCO->Server_CancelUpgradeAudit();
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Sent Cancel request via SFRCO"));
							return;
						}
					}
				}
			}
			else
			{
				// Local/Server direct call
				if (USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService())
				{
					AuditService->CancelAudit();
				}
			}
		}
	}
}

void USmartUpgradePanel::UpdateAuditProgress(float ProgressPercent, int32 ScannedCount)
{
	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_AuditProgress", "Auditing world... {0}% ({1} scanned)"), FText::AsNumber(FMath::RoundToInt(ProgressPercent)), FText::AsNumber(ScannedCount)));
	}

	if (CancelButton)
	{
		CancelButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void USmartUpgradePanel::UpdateAuditUI(const FSFUpgradeAuditResult& Result)
{
	if (!Result.bSuccess)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_AuditFailed", "Audit failed or was canceled."));
		}
		return;
	}

	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_AuditComplete", "Audit Complete: {0} upgradeable found across {1} structures."), FText::AsNumber(Result.TotalUpgradeable), FText::AsNumber(Result.TotalScanned)));
	}

	if (CancelButton)
	{
		CancelButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Cache the result for row selection
	CachedAuditResult = Result;

	// Clear row data map
	RowDataMap.Empty();

	// Get the active results container (new tabbed UI or legacy)
	UVerticalBox* ActiveResultsContainer = RadiusAuditResultsContainer ? RadiusAuditResultsContainer : AuditResultsContainer;

	// Clear existing results
	if (ActiveResultsContainer)
	{
		ActiveResultsContainer->ClearChildren();

		TArray<FSFUpgradeFamilyResult> DisplayFamilyResults;
		bool bAddedConveyors = false;
		for (const FSFUpgradeFamilyResult& FamilyResult : Result.FamilyResults)
		{
			if (IsConveyorUpgradeFamily(FamilyResult.Family))
			{
				if (bAddedConveyors)
				{
					continue;
				}

				FSFUpgradeFamilyResult CombinedResult;
				CombinedResult.Family = ESFUpgradeFamily::Belt;

				for (const FSFUpgradeFamilyResult& CandidateResult : Result.FamilyResults)
				{
					if (!IsConveyorUpgradeFamily(CandidateResult.Family)) continue;
					CombinedResult.TotalCount += CandidateResult.TotalCount;
					CombinedResult.UpgradeableCount += CandidateResult.UpgradeableCount;

					for (const FSFUpgradeTierBucket& CandidateBucket : CandidateResult.TierBuckets)
					{
						FSFUpgradeTierBucket* CombinedBucket = CombinedResult.TierBuckets.FindByPredicate([&CandidateBucket](const FSFUpgradeTierBucket& Bucket)
						{
							return Bucket.Tier == CandidateBucket.Tier;
						});

						if (!CombinedBucket)
						{
							const int32 NewIndex = CombinedResult.TierBuckets.AddDefaulted();
							CombinedBucket = &CombinedResult.TierBuckets[NewIndex];
							CombinedBucket->Tier = CandidateBucket.Tier;
						}

						CombinedBucket->Count += CandidateBucket.Count;
						CombinedBucket->Entries.Append(CandidateBucket.Entries);
					}
				}

				CombinedResult.TierBuckets.Sort([](const FSFUpgradeTierBucket& A, const FSFUpgradeTierBucket& B)
				{
					return A.Tier < B.Tier;
				});

				DisplayFamilyResults.Add(MoveTemp(CombinedResult));
				bAddedConveyors = true;
				continue;
			}

			DisplayFamilyResults.Add(FamilyResult);
		}

		// Populate container with family sections and columnar tier rows
		for (const FSFUpgradeFamilyResult& FamilyResult : DisplayFamilyResults)
		{
			// Skip empty families
			bool bHasAnyItems = false;
			for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
			{
				if (Bucket.Count > 0) { bHasAnyItems = true; break; }
			}
			if (!bHasAnyItems) continue;

			// --- Section Header ---
			FString SectionName = GetPanelFamilyDisplayName(FamilyResult.Family);

			UTextBlock* SectionHeader = NewObject<UTextBlock>(this);
			SectionHeader->SetText(FText::FromString(SectionName));
			FSlateFontInfo HeaderFont = SectionHeader->GetFont();
			HeaderFont.Size = 12;
			HeaderFont.TypefaceFontName = FName("Bold");
			SectionHeader->SetFont(HeaderFont);
			SectionHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.886f, 0.498f, 0.118f, 1.0f))); // Orange

			UVerticalBoxSlot* HeaderSlot = ActiveResultsContainer->AddChildToVerticalBox(SectionHeader);
			if (HeaderSlot)
			{
				HeaderSlot->SetPadding(FMargin(4.0f, 6.0f, 0.0f, 2.0f));
			}

			// --- Tier Rows ---
			for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
			{
				if (Bucket.Count > 0)
				{
					// Calculate upgradeable count for this bucket
					int32 UpgradeableCount = 0;
					for (const FSFUpgradeAuditEntry& Entry : Bucket.Entries)
					{
						if (Entry.IsUpgradeable())
						{
							UpgradeableCount++;
						}
					}

					// Build tier display name (short, without family prefix)
					FString TierName = FString::Printf(TEXT("Mk.%d"), Bucket.Tier);
					FString DisplayName;
					if (FamilyResult.Family == ESFUpgradeFamily::WallOutletSingle)
					{
						DisplayName = FText::Format(LOCTEXT("UpgradeRow_WallOutlet", "Wall Outlet Mk.{0}"), FText::AsNumber(Bucket.Tier)).ToString();
					}
					else if (FamilyResult.Family == ESFUpgradeFamily::WallOutletDouble)
					{
						DisplayName = FText::Format(LOCTEXT("UpgradeRow_DoubleWallOutlet", "Double Wall Outlet Mk.{0}"), FText::AsNumber(Bucket.Tier)).ToString();
					}
					else
					{
						DisplayName = FString::Printf(TEXT("%s Mk.%d"), *SectionName, Bucket.Tier);
					}

					// Row text color
					FLinearColor RowTextColor = UpgradeableCount > 0
						? FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)
						: FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

					// --- Border wrapper for selection highlighting ---
					UBorder* RowBorder = NewObject<UBorder>(this);
					RowBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
					RowBorder->SetPadding(FMargin(2.0f, 1.0f, 2.0f, 1.0f));
					RowBorder->SetToolTipText(FText::Format(LOCTEXT("Upgrade_ClickToSelect", "Click to select {0} for upgrade"), FText::FromString(DisplayName)));

					// --- HorizontalBox for columnar layout ---
					UHorizontalBox* RowHBox = NewObject<UHorizontalBox>(this);

					// Column 1: Count (right-aligned, fixed width)
					UTextBlock* CountText = NewObject<UTextBlock>(this);
					CountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Bucket.Count)));
					FSlateFontInfo RowFont = CountText->GetFont();
					RowFont.Size = 11;
					CountText->SetFont(RowFont);
					CountText->SetColorAndOpacity(FSlateColor(RowTextColor));
					CountText->SetJustification(ETextJustify::Right);

					USizeBox* CountSizeBox = NewObject<USizeBox>(this);
					CountSizeBox->SetWidthOverride(50.0f);
					CountSizeBox->AddChild(CountText);

					UHorizontalBoxSlot* CountSlot = RowHBox->AddChildToHorizontalBox(CountSizeBox);
					if (CountSlot)
					{
						CountSlot->SetVerticalAlignment(VAlign_Center);
					}

					// Column 2: " x " separator
					UTextBlock* SepText = NewObject<UTextBlock>(this);
					SepText->SetText(LOCTEXT("Upgrade_Separator", " x "));
					SepText->SetFont(RowFont);
					SepText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));

					UHorizontalBoxSlot* SepSlot = RowHBox->AddChildToHorizontalBox(SepText);
					if (SepSlot)
					{
						SepSlot->SetVerticalAlignment(VAlign_Center);
					}

					// Column 3: Tier name
					UTextBlock* NameText = NewObject<UTextBlock>(this);
					NameText->SetText(FText::FromString(TierName));
					NameText->SetFont(RowFont);
					NameText->SetColorAndOpacity(FSlateColor(RowTextColor));

					UHorizontalBoxSlot* NameSlot = RowHBox->AddChildToHorizontalBox(NameText);
					if (NameSlot)
					{
						NameSlot->SetVerticalAlignment(VAlign_Center);
					}

					// Column 4: Upgradeable count (if any)
					if (UpgradeableCount > 0)
					{
						UTextBlock* UpgradeText = NewObject<UTextBlock>(this);
						UpgradeText->SetText(FText::Format(LOCTEXT("Upgrade_UpgradeableCount", "({0} upgradeable)"), FText::AsNumber(UpgradeableCount)));
						UpgradeText->SetFont(RowFont);
						UpgradeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 0.4f, 1.0f))); // Green tint

						UHorizontalBoxSlot* UpgradeSlot = RowHBox->AddChildToHorizontalBox(UpgradeText);
						if (UpgradeSlot)
						{
							UpgradeSlot->SetVerticalAlignment(VAlign_Center);
							UpgradeSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
						}
					}

					RowBorder->AddChild(RowHBox);
					ActiveResultsContainer->AddChildToVerticalBox(RowBorder);

					// Store mapping for click detection
					RowDataMap.Add(RowBorder, FRowData{FamilyResult.Family, Bucket.Tier, DisplayName});
				}
			}
		}
	}
}

void USmartUpgradePanel::ClosePanel()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: ClosePanel() called"));

	// Get player controller
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Upgrade Panel: No owning player controller"));
		RemoveFromParent();
		return;
	}

	// Remove from viewport
	RemoveFromParent();

	// Restore game input mode
	PC->bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);

	// Re-enable HUD via subsystem
	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		if (USFHudService* HudService = Subsystem->GetHudService())
		{
			HudService->SetHUDSuppressed(false);
		}

		// Clear the widget reference in subsystem
		Subsystem->ClearUpgradePanelWidget();
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Closed and input restored"));
}

FReply USmartUpgradePanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle ESC key to close panel
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: ESC key pressed - closing"));
		ClosePanel();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USmartUpgradePanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bIsDragging = true;
		DragOffset = InMouseEvent.GetScreenSpacePosition();
		InitialPanelPosition = GetRenderTransform().Translation;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// Handle left click on rows
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Check if we clicked on a row in the results container
		FVector2D LocalMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		// Iterate through row data map to find which row was clicked
		for (const auto& Pair : RowDataMap)
		{
			UWidget* Widget = Pair.Key;
			if (Widget && Widget->IsVisible())
			{
				// Get widget geometry
				FGeometry WidgetGeometry = Widget->GetCachedGeometry();
				FVector2D WidgetLocalPos = WidgetGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
				FVector2D WidgetSize = WidgetGeometry.GetLocalSize();

				// Check if click is within widget bounds
				if (WidgetLocalPos.X >= 0 && WidgetLocalPos.X <= WidgetSize.X &&
					WidgetLocalPos.Y >= 0 && WidgetLocalPos.Y <= WidgetSize.Y)
				{
					const FRowData& Data = Pair.Value;
					OnRowSelected(Data.Family, Data.Tier);
					return FReply::Handled();
				}
			}
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USmartUpgradePanel::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bIsDragging)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USmartUpgradePanel::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentMousePos - DragOffset;

		// Update widget position by adding delta to the position at drag start
		SetRenderTranslation(InitialPanelPosition + Delta);

		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USmartUpgradePanel::OnRowSelected(ESFUpgradeFamily Family, int32 Tier)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Row selected - Family=%d Tier=%d"), static_cast<int32>(Family), Tier);

	// Get player location
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn())
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_NoPlayerLoc", "Cannot find player location"));
		}
		return;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find closest instance of this family/tier from cached results
	float ClosestDistSq = FLT_MAX;
	FVector ClosestLocation = FVector::ZeroVector;
	bool bFoundAny = false;

	for (const FSFUpgradeFamilyResult& FamilyResult : CachedAuditResult.FamilyResults)
	{
		if (FamilyResult.Family != Family && !(IsConveyorUpgradeFamily(Family) && IsConveyorUpgradeFamily(FamilyResult.Family)))
		{
			continue;
		}

		for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
		{
			if (Bucket.Tier != Tier)
			{
				continue;
			}

			for (const FSFUpgradeAuditEntry& Entry : Bucket.Entries)
			{
				float DistSq = FVector::DistSquared(PlayerLocation, Entry.Location);
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestLocation = Entry.Location;
					bFoundAny = true;
				}
			}
		}
	}

	if (!bFoundAny)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_NoInstances", "No instances found (entries not stored)"));
		}
		return;
	}

	// Calculate distance and direction
	float DistanceMeters = FMath::Sqrt(ClosestDistSq) / 100.0f;  // Convert cm to meters
	FString Direction = GetCardinalDirection(PlayerLocation, ClosestLocation);

	// Update status text
	FString FamilyName = GetPanelFamilyDisplayName(Family);
	FString DisplayName;
	if (Family == ESFUpgradeFamily::WallOutletSingle)
	{
		DisplayName = FText::Format(LOCTEXT("UpgradeRow_WallOutlet", "Wall Outlet Mk.{0}"), FText::AsNumber(Tier)).ToString();
	}
	else if (Family == ESFUpgradeFamily::WallOutletDouble)
	{
		DisplayName = FText::Format(LOCTEXT("UpgradeRow_DoubleWallOutlet", "Double Wall Outlet Mk.{0}"), FText::AsNumber(Tier)).ToString();
	}
	else
	{
		DisplayName = FString::Printf(TEXT("%s Mk.%d"), *FamilyName, Tier);
	}

	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Nearest", "Nearest {0}: {1}m {2}"), FText::FromString(DisplayName), FText::FromString(FString::Printf(TEXT("%.0f"), DistanceMeters)), FText::FromString(Direction)));
	}

	// Store selection and update row highlighting
	SelectedFamily = Family;
	SelectedTier = Tier;

	// Highlight selected row, dim others
	const FLinearColor SelectedBgColor(0.886f, 0.498f, 0.118f, 0.3f);  // Translucent orange
	const FLinearColor DefaultBgColor(0.0f, 0.0f, 0.0f, 0.0f);         // Transparent

	for (const auto& Pair : RowDataMap)
	{
		if (UBorder* RowBorder = Cast<UBorder>(Pair.Key))
		{
			const FRowData& Data = Pair.Value;
			bool bIsSelected = (Data.Family == Family && Data.Tier == Tier);
			RowBorder->SetBrushColor(bIsSelected ? SelectedBgColor : DefaultBgColor);
		}
	}

	// Populate target tier dropdown based on selected family
	PopulateTargetTierDropdown();

	// Update cost display
	UpdateCostDisplay();

	// Enable upgrade button now that a row is selected
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->SetIsEnabled(true);
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(true);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Nearest %s is %.0fm %s at %s"),
		*DisplayName, DistanceMeters, *Direction, *ClosestLocation.ToString());
}

FString USmartUpgradePanel::GetCardinalDirection(const FVector& FromLocation, const FVector& ToLocation)
{
	// Calculate direction vector (ignoring Z)
	FVector2D Direction2D(ToLocation.X - FromLocation.X, ToLocation.Y - FromLocation.Y);
	Direction2D.Normalize();

	// Get angle in degrees (0 = East, 90 = North in UE coordinates)
	// UE uses X = forward (North), Y = right (East)
	float AngleRad = FMath::Atan2(Direction2D.Y, Direction2D.X);
	float AngleDeg = FMath::RadiansToDegrees(AngleRad);

	// Convert to compass bearing (0 = North, 90 = East)
	// In UE: X+ is North, Y+ is East
	// Atan2(Y, X) gives angle from X axis
	// So 0 degrees = East, 90 = North, 180 = West, -90 = South
	// We want: 0 = North, 90 = East, 180 = South, 270 = West
	float CompassAngle = 90.0f - AngleDeg;  // Rotate so 0 = North
	if (CompassAngle < 0) CompassAngle += 360.0f;
	if (CompassAngle >= 360.0f) CompassAngle -= 360.0f;

	// 16-point compass (22.5 degrees per segment)
	// N = 0, NNE = 22.5, NE = 45, ENE = 67.5, E = 90, etc.
	static const TCHAR* Directions[] = {
		TEXT("N"), TEXT("NNE"), TEXT("NE"), TEXT("ENE"),
		TEXT("E"), TEXT("ESE"), TEXT("SE"), TEXT("SSE"),
		TEXT("S"), TEXT("SSW"), TEXT("SW"), TEXT("WSW"),
		TEXT("W"), TEXT("WNW"), TEXT("NW"), TEXT("NNW")
	};

	// Each segment is 22.5 degrees, offset by 11.25 to center on direction
	int32 Index = FMath::RoundToInt((CompassAngle + 11.25f) / 22.5f) % 16;

	return Directions[Index];
}

void USmartUpgradePanel::OnUpgradeProgress(float ProgressPercent, int32 SuccessCount, int32 TotalCount)
{
	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_ProgressCount", "Upgrading... {0}/{1} ({2}%)"), FText::AsNumber(SuccessCount), FText::AsNumber(TotalCount), FText::FromString(FString::Printf(TEXT("%.0f"), ProgressPercent * 100.0f))));
	}
}

void USmartUpgradePanel::OnUpgradeCompleted(const FSFUpgradeExecutionResult& Result)
{
	// Unsubscribe from delegates
	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		if (USFUpgradeExecutionService* ExecutionService = Subsystem->GetUpgradeExecutionService())
		{
			ExecutionService->OnUpgradeExecutionCompleted.RemoveDynamic(this, &USmartUpgradePanel::OnUpgradeCompleted);
			ExecutionService->OnUpgradeProgressUpdated.RemoveDynamic(this, &USmartUpgradePanel::OnUpgradeProgress);
		}
	}

	// Update status with results
	if (StatusText)
	{
		if (Result.bCompleted)
		{
			StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Complete", "Upgrade complete: {0} success, {1} failed, {2} skipped"), FText::AsNumber(Result.SuccessCount), FText::AsNumber(Result.FailCount), FText::AsNumber(Result.SkipCount)));
		}
		else
		{
			StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Cancelled", "Upgrade cancelled: {0}/{1} completed"), FText::AsNumber(Result.SuccessCount), FText::AsNumber(Result.TotalProcessed)));
		}
	}

	// Refresh audit to show updated counts
	RefreshAudit();
}

void USmartUpgradePanel::OnTargetTierChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Target tier changed to: %s"), *SelectedItem);

	// Parse tier from selection (e.g., "Mk.4" -> 4)
	CachedTargetTier = 0;
	if (SelectedItem.StartsWith(TEXT("Mk.")))
	{
		FString TierStr = SelectedItem.RightChop(3);
		CachedTargetTier = FCString::Atoi(*TierStr);
	}

	// Update cost display
	UpdateCostDisplay();
}

void USmartUpgradePanel::PopulateTargetTierDropdown()
{
	// Use appropriate dropdown based on active tab
	UComboBoxString* ActiveComboBox = nullptr;

	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveComboBox = RadiusTargetTierComboBox ? RadiusTargetTierComboBox : TargetTierComboBox;
	}
	else
	{
		// Traversal tab - use traversal dropdown
		ActiveComboBox = TraversalTargetTierComboBox ? TraversalTargetTierComboBox : TargetTierComboBox;
	}

	if (!ActiveComboBox)
	{
		return;
	}

	// Clear existing options
	ActiveComboBox->ClearOptions();

	// In traversal mode, SelectedTier may be 0 (multiple tiers in network)
	// In that case, populate all tiers above 1
	bool bIsTraversalMode = (ActiveTab == ESmartUpgradeTab::Traversal);
	int32 MinSourceTier = bIsTraversalMode ? 1 : SelectedTier;

	if (SelectedFamily == ESFUpgradeFamily::None)
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// For radius mode, require a selected tier
	if (!bIsTraversalMode && SelectedTier == 0)
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Get max tier for this family
	int32 MaxTier = 6;  // Default for belts/lifts (Mk1-Mk6)
	if (SelectedFamily == ESFUpgradeFamily::Pipe || SelectedFamily == ESFUpgradeFamily::Pump)
	{
		MaxTier = 2;
	}
	else if (SelectedFamily == ESFUpgradeFamily::PowerPole ||
	         SelectedFamily == ESFUpgradeFamily::WallOutletSingle ||
	         SelectedFamily == ESFUpgradeFamily::WallOutletDouble)
	{
		MaxTier = 3;
	}

	// Get highest unlocked tier from subsystem
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (Subsystem && PC)
	{
		if (SelectedFamily == ESFUpgradeFamily::Belt || SelectedFamily == ESFUpgradeFamily::Lift)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedBeltTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::Pipe)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedPipeTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::PowerPole)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedPowerPoleTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::WallOutletSingle)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ false));
		}
		else if (SelectedFamily == ESFUpgradeFamily::WallOutletDouble)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ true));
		}
	}

	// Add options for tiers above source tier up to max unlocked
	// In traversal mode, MinSourceTier is 1 (show all tiers from Mk.2 up)
	bool bHasOptions = false;
	for (int32 Tier = MinSourceTier + 1; Tier <= MaxTier; ++Tier)
	{
		FString Option = FString::Printf(TEXT("Mk.%d"), Tier);
		ActiveComboBox->AddOption(Option);
		bHasOptions = true;
	}

	if (bHasOptions)
	{
		// Default to max tier
		FString DefaultOption = FString::Printf(TEXT("Mk.%d"), MaxTier);
		ActiveComboBox->SetSelectedOption(DefaultOption);
		CachedTargetTier = MaxTier;
		ActiveComboBox->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Populated tier dropdown - Source=%d Max=%d Options=%d"),
		SelectedTier, MaxTier, bHasOptions ? (MaxTier - SelectedTier) : 0);
}

void USmartUpgradePanel::UpdateCostDisplay()
{
	// Use appropriate cost text based on active tab
	UTextBlock* ActiveCostText = nullptr;
	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveCostText = RadiusCostDetailsText ? RadiusCostDetailsText : CostDetailsText;
	}
	else
	{
		ActiveCostText = TraversalCostDetailsText ? TraversalCostDetailsText : CostDetailsText;
	}

	if (!ActiveCostText)
	{
		return;
	}

	// Traversal mode: use CachedTraversalResult
	if ((ActiveTab == ESmartUpgradeTab::Traversal) && CachedTraversalResult.IsValid())
	{
		if (CachedTargetTier == 0)
		{
			ActiveCostText->SetText(LOCTEXT("Upgrade_SelectTargetTier", "Select target tier"));
			return;
		}

		// Count upgradeable items by family
		TMap<ESFUpgradeFamily, int32> FamilyCounts;
		TMap<TSubclassOf<UFGItemDescriptor>, int32> TotalNetCost;
		int32 TotalUpgradeable = 0;

		for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
		{
			if (Entry.CurrentTier < CachedTargetTier && Entry.CurrentTier > 0)
			{
				ESFUpgradeFamily EntryFamily = USFUpgradeTraversalService::GetUpgradeFamily(Entry.Buildable.Get());
				FamilyCounts.FindOrAdd(EntryFamily)++;
				TotalUpgradeable++;

				// Calculate cost for this item
				TMap<TSubclassOf<UFGItemDescriptor>, int32> ItemCost;
				if (CalculateUpgradeCost(EntryFamily, Entry.CurrentTier, CachedTargetTier, 1, ItemCost))
				{
					for (const auto& Pair : ItemCost)
					{
						TotalNetCost.FindOrAdd(Pair.Key) += Pair.Value;
					}
				}
			}
		}

		if (TotalUpgradeable == 0)
		{
			ActiveCostText->SetText(LOCTEXT("Upgrade_NoItems", "No items to upgrade"));
			return;
		}

		// Build cost display string
		FString CostString = FText::Format(LOCTEXT("Upgrade_CostItems", "{0} items \u2192 Mk.{1}\n"), FText::AsNumber(TotalUpgradeable), FText::AsNumber(CachedTargetTier)).ToString();

		bool bHasCost = false;
		bool bHasRefund = false;

		for (const auto& Pair : TotalNetCost)
		{
			if (Pair.Value != 0 && Pair.Key)
			{
				FString ItemName = UFGItemDescriptor::GetItemName(Pair.Key).ToString();
				if (Pair.Value > 0)
				{
					CostString += FString::Printf(TEXT("-%d %s\n"), Pair.Value, *ItemName);
					bHasCost = true;
				}
				else
				{
					CostString += FString::Printf(TEXT("+%d %s\n"), -Pair.Value, *ItemName);
					bHasRefund = true;
				}
			}
		}

		if (!bHasCost && !bHasRefund)
		{
			CostString += LOCTEXT("Upgrade_NoNetCost", "(No net cost)").ToString();
		}

		ActiveCostText->SetText(FText::FromString(CostString));
		return;
	}

	// Radius mode: original logic
	if (SelectedFamily == ESFUpgradeFamily::None || SelectedTier == 0 || CachedTargetTier == 0)
	{
		ActiveCostText->SetText(LOCTEXT("Upgrade_SelectFamily", "Select a family to see costs"));
		return;
	}

	// Find how many items of this tier exist in the cached audit
	int32 ItemCount = 0;
	TMap<TSubclassOf<UFGItemDescriptor>, int32> NetCost;
	bool bCostAvailable = false;
	for (const FSFUpgradeFamilyResult& FamilyResult : CachedAuditResult.FamilyResults)
	{
		if (FamilyResult.Family == SelectedFamily || (IsConveyorUpgradeFamily(SelectedFamily) && IsConveyorUpgradeFamily(FamilyResult.Family)))
		{
			for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
			{
				if (Bucket.Tier == SelectedTier)
				{
					ItemCount += Bucket.Count;

					TMap<TSubclassOf<UFGItemDescriptor>, int32> FamilyNetCost;
					if (!CalculateUpgradeCost(FamilyResult.Family, SelectedTier, CachedTargetTier, Bucket.Count, FamilyNetCost))
					{
						bCostAvailable = false;
						break;
					}

					for (const auto& Pair : FamilyNetCost)
					{
						NetCost.FindOrAdd(Pair.Key) += Pair.Value;
					}
					bCostAvailable = true;
					break;
				}
			}

			if (!bCostAvailable && ItemCount > 0)
			{
				break;
			}

			if (!IsConveyorUpgradeFamily(SelectedFamily))
			{
				break;
			}
		}
	}

	if (ItemCount == 0)
	{
		ActiveCostText->SetText(LOCTEXT("Upgrade_NoItems", "No items to upgrade"));
		return;
	}

	FString FamilyName = GetPanelFamilyDisplayName(SelectedFamily);

	if (!bCostAvailable)
	{
		ActiveCostText->SetText(FText::Format(LOCTEXT("Upgrade_CostUnavailable", "{0} \u00d7 {1} Mk.{2} \u2192 Mk.{3}\n(Cost unavailable)"), FText::AsNumber(ItemCount), FText::FromString(FamilyName), FText::AsNumber(SelectedTier), FText::AsNumber(CachedTargetTier)));
		return;
	}

	// Build cost display string
	FString CostString = FText::Format(LOCTEXT("Upgrade_CostHeader", "{0} \u00d7 {1} Mk.{2} \u2192 Mk.{3}\n"), FText::AsNumber(ItemCount), FText::FromString(FamilyName), FText::AsNumber(SelectedTier), FText::AsNumber(CachedTargetTier)).ToString();

	bool bHasCost = false;
	bool bHasRefund = false;

	for (const auto& Pair : NetCost)
	{
		if (Pair.Value != 0 && Pair.Key)
		{
			FString ItemName = UFGItemDescriptor::GetItemName(Pair.Key).ToString();
			if (Pair.Value > 0)
			{
				CostString += FString::Printf(TEXT("-%d %s\n"), Pair.Value, *ItemName);
				bHasCost = true;
			}
			else
			{
				CostString += FString::Printf(TEXT("+%d %s\n"), -Pair.Value, *ItemName);
				bHasRefund = true;
			}
		}
	}

	if (!bHasCost && !bHasRefund)
	{
		CostString += LOCTEXT("Upgrade_NoNetCost", "(No net cost)").ToString();
	}

	ActiveCostText->SetText(FText::FromString(CostString));
}

int32 USmartUpgradePanel::GetSelectedTargetTier() const
{
	// Use appropriate dropdown based on active tab
	const UComboBoxString* ActiveComboBox = nullptr;

	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveComboBox = RadiusTargetTierComboBox ? RadiusTargetTierComboBox : TargetTierComboBox;
	}
	else
	{
		// Traversal tab - use traversal dropdown
		ActiveComboBox = TraversalTargetTierComboBox ? TraversalTargetTierComboBox : TargetTierComboBox;
	}

	if (!ActiveComboBox)
	{
		return 0;
	}

	FString SelectedOption = ActiveComboBox->GetSelectedOption();
	if (SelectedOption.IsEmpty())
	{
		return 0;
	}

	// Parse tier from selection (e.g., "Mk.4" -> 4)
	if (SelectedOption.StartsWith(TEXT("Mk.")))
	{
		FString TierStr = SelectedOption.RightChop(3);
		return FCString::Atoi(*TierStr);
	}

	return 0;
}

bool USmartUpgradePanel::CalculateUpgradeCost(ESFUpgradeFamily Family, int32 SourceTier, int32 TargetTier, int32 ItemCount, TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutNetCost) const
{
	USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		return false;
	}

	// Get recipes for source and target tiers
	TSubclassOf<UFGRecipe> SourceRecipe = nullptr;
	TSubclassOf<UFGRecipe> TargetRecipe = nullptr;

	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
			SourceRecipe = Subsystem->GetBeltRecipeForTier(SourceTier);
			TargetRecipe = Subsystem->GetBeltRecipeForTier(TargetTier);
			break;
		case ESFUpgradeFamily::Lift:
		{
			static const TCHAR* LiftRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk2.Recipe_ConveyorLiftMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk3.Recipe_ConveyorLiftMk3_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk5.Recipe_ConveyorLiftMk5_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk6.Recipe_ConveyorLiftMk6_C"),
			};
			if (SourceTier >= 1 && SourceTier <= 6)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, LiftRecipeNames[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 6)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, LiftRecipeNames[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::Pipe:
			SourceRecipe = Subsystem->GetPipeRecipeForTier(SourceTier, true);
			TargetRecipe = Subsystem->GetPipeRecipeForTier(TargetTier, true);
			break;
		case ESFUpgradeFamily::PowerPole:
		{
			static const TCHAR* PoleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk2.Recipe_PowerPoleMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk3.Recipe_PowerPoleMk3_C"),
			};
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, PoleRecipeNames[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, PoleRecipeNames[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::WallOutletSingle:
		{
			static const TCHAR* WallOutletSingleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWall.Recipe_PowerPoleWall_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk2.Recipe_PowerPoleWallMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallMk3.Recipe_PowerPoleWallMk3_C"),
			};
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletSingleRecipeNames[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletSingleRecipeNames[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::WallOutletDouble:
		{
			static const TCHAR* WallOutletDoubleRecipeNames[] = {
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDouble.Recipe_PowerPoleWallDouble_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk2.Recipe_PowerPoleWallDoubleMk2_C"),
				TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleWallDoubleMk3.Recipe_PowerPoleWallDoubleMk3_C"),
			};
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletDoubleRecipeNames[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletDoubleRecipeNames[TargetTier - 1]);
			}
			break;
		}
		default:
			return false;
	}

	if (!SourceRecipe || !TargetRecipe)
	{
		return false;
	}

	const UFGRecipe* SourceCDO = SourceRecipe->GetDefaultObject<UFGRecipe>();
	const UFGRecipe* TargetCDO = TargetRecipe->GetDefaultObject<UFGRecipe>();
	if (!SourceCDO || !TargetCDO)
	{
		return false;
	}

	// Get ingredients for both recipes
	TArray<FItemAmount> SourceIngredients = SourceCDO->GetIngredients();
	TArray<FItemAmount> TargetIngredients = TargetCDO->GetIngredients();

	// Calculate net cost: target cost - source refund
	// For each item in target, add to cost
	for (const FItemAmount& Ingredient : TargetIngredients)
	{
		if (Ingredient.ItemClass)
		{
			int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
			CurrentAmount += Ingredient.Amount * ItemCount;
		}
	}

	// For each item in source, subtract from cost (refund)
	for (const FItemAmount& Ingredient : SourceIngredients)
	{
		if (Ingredient.ItemClass)
		{
			int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
			CurrentAmount -= Ingredient.Amount * ItemCount;
		}
	}

	return true;
}

void USmartUpgradePanel::OnRadiusTabClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Radius tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Radius);
}

void USmartUpgradePanel::OnTraversalTabClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Traversal tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Traversal);
}

void USmartUpgradePanel::OnTriageTabClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Triage tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Triage);
}

void USmartUpgradePanel::SwitchToTab(ESmartUpgradeTab NewTab)
{
	ActiveTab = NewTab;

	const bool bRadiusTab = (NewTab == ESmartUpgradeTab::Radius);
	const bool bTraversalTab = (NewTab == ESmartUpgradeTab::Traversal);
	const bool bTriageTab = (NewTab == ESmartUpgradeTab::Triage);

	// Toggle content visibility
	if (RadiusContent)
	{
		RadiusContent->SetVisibility(bRadiusTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TraversalContent)
	{
		TraversalContent->SetVisibility(bTraversalTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TriageContent)
	{
		TriageContent->SetVisibility(bTriageTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Update tab button appearance (visual feedback for active/inactive)
	const FLinearColor ActiveTabColor(0.886f, 0.498f, 0.118f, 1.0f);    // Satisfactory orange
	const FLinearColor InactiveTabColor(0.15f, 0.15f, 0.18f, 1.0f);     // Dark gray
	const FLinearColor ActiveTextColor(1.0f, 1.0f, 1.0f, 1.0f);         // White
	const FLinearColor InactiveTextColor(0.6f, 0.6f, 0.6f, 1.0f);       // Dim gray

	if (RadiusTabButton)
	{
		RadiusTabButton->SetBackgroundColor(bRadiusTab ? ActiveTabColor : InactiveTabColor);
	}
	if (TraversalTabButton)
	{
		TraversalTabButton->SetBackgroundColor(bTraversalTab ? ActiveTabColor : InactiveTabColor);
	}
	if (TriageTabButton)
	{
		TriageTabButton->SetBackgroundColor(bTriageTab ? ActiveTabColor : InactiveTabColor);
	}

	// Update tab text colors
	if (UTextBlock* RadiusText = RadiusTabButton ? Cast<UTextBlock>(RadiusTabButton->GetChildAt(0)) : nullptr)
	{
		RadiusText->SetColorAndOpacity(FSlateColor(bRadiusTab ? ActiveTextColor : InactiveTextColor));
	}
	if (UTextBlock* TraversalText = TraversalTabButton ? Cast<UTextBlock>(TraversalTabButton->GetChildAt(0)) : nullptr)
	{
		TraversalText->SetColorAndOpacity(FSlateColor(bTraversalTab ? ActiveTextColor : InactiveTextColor));
	}
	if (UTextBlock* TriageText = TriageTabButton ? Cast<UTextBlock>(TriageTabButton->GetChildAt(0)) : nullptr)
	{
		TriageText->SetColorAndOpacity(FSlateColor(bTriageTab ? ActiveTextColor : InactiveTextColor));
	}

	// Update shared status text based on tab
	if (SharedStatusText)
	{
		if (bRadiusTab)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_SetRadiusScan", "Set radius and press Scan to find items"));
		}
		else if (bTraversalTab)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_AimAndScan", "Aim at a buildable and press Scan Network"));
		}
		else
		{
			SharedStatusText->SetText(LOCTEXT("Triage_Ready", "Use Detect to scan for chain issues"));
		}
	}

	// Reset selection state when switching tabs
	SelectedFamily = ESFUpgradeFamily::None;
	SelectedTier = 0;
	CachedTargetTier = 0;

	// Disable upgrade button until new selection is made
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->SetIsEnabled(false);
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(false);
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Switched to %s tab"),
		bRadiusTab ? TEXT("Radius") : bTraversalTab ? TEXT("Traversal") : TEXT("Triage"));
}

void USmartUpgradePanel::OnTriageDetectClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Triage Detect clicked"));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	USFChainActorService* ChainService = Subsystem->GetChainActorService();
	if (!ChainService)
	{
		return;
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Triage_Detecting", "Scanning map for chain issues..."));
	}

	const FSFChainDiagnosticResult Result = ChainService->DetectChainActorIssues();

	if (TriageDetectResultText)
	{
		if (Result.HasIssues())
		{
			TriageDetectResultText->SetText(FText::Format(
				LOCTEXT("Triage_IssuesFound", "Found {0} zombie chain(s), {1} split chain group(s), {2} chain=NONE belt(s), and {3} orphaned tick group(s) ({4} empty, {5} live belt/lift entries, {6} connected candidate group(s)).\nRepair will purge zombies, rebuild split chains, and re-register live conveyors from orphaned tick groups. After repair, save, reload, and run Detect again."),
				FText::AsNumber(Result.ZombieChainCount),
				FText::AsNumber(Result.SplitChainCount),
				FText::AsNumber(Result.OrphanedBeltCount),
				FText::AsNumber(Result.OrphanedTickGroupCount),
				FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
				FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
				FText::AsNumber(Result.OrphanedBeltCandidates)));
		}
		else
		{
			TriageDetectResultText->SetText(LOCTEXT("Triage_NoIssues", "No issues detected. Your conveyor chains are healthy."));
		}
		TriageDetectResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Show repair button only if there are issues to fix
	if (TriageRepairButton)
	{
		TriageRepairButton->SetVisibility(Result.HasIssues() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Reset repair result when re-detecting
	if (TriageRepairResultText)
	{
		TriageRepairResultText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(Result.HasIssues()
			? LOCTEXT("Triage_IssuesSummary", "Issues found - Repair handles zombies, splits, and orphaned tick groups")
			: LOCTEXT("Triage_AllClear", "All clear — no chain issues found"));
	}
}

void USmartUpgradePanel::OnTriageRepairClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Triage Repair clicked"));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	USFChainActorService* ChainService = Subsystem->GetChainActorService();
	if (!ChainService)
	{
		return;
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Triage_Repairing", "Repairing chain issues..."));
	}

	const FSFChainRepairResult Result = ChainService->RepairAllChainActorIssues();

	if (TriageRepairResultText)
	{
		if (Result.HasAnyResults())
		{
			if (Result.HasOrphanCandidates() && !Result.OrphanedBeltReportPath.IsEmpty())
			{
				TriageRepairResultText->SetText(FText::Format(
					LOCTEXT("Triage_RepairDoneWithReport", "Repair complete. Removed {0} zombie chain(s), rebuilt {1} split group(s), and re-registered {7} live conveyor(s) from {3} orphaned tick group(s), including {4} empty group(s) and {5} live belt(s). A diagnostic report was written to: {6}"),
					FText::AsNumber(Result.ZombiesPurged),
					FText::AsNumber(Result.SplitGroupsRebuilt),
					FText::AsNumber(Result.OrphanedBeltCandidates),
					FText::AsNumber(Result.OrphanedTickGroupCount),
					FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
					FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
					FText::FromString(Result.OrphanedBeltReportPath),
					FText::AsNumber(Result.OrphanedBeltsRequeued)));
			}
			else
			{
				TriageRepairResultText->SetText(FText::Format(
					LOCTEXT("Triage_RepairDone", "Repair complete. Removed {0} zombie chain(s), rebuilt {1} split group(s), and re-registered {6} live conveyor(s) from {3} orphaned tick group(s), including {4} empty group(s) and {5} live belt(s). Save, reload, and run Detect again."),
					FText::AsNumber(Result.ZombiesPurged),
					FText::AsNumber(Result.SplitGroupsRebuilt),
					FText::AsNumber(Result.OrphanedBeltCandidates),
					FText::AsNumber(Result.OrphanedTickGroupCount),
					FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
					FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
					FText::AsNumber(Result.OrphanedBeltsRequeued)));
			}
		}
		else
		{
			TriageRepairResultText->SetText(LOCTEXT("Triage_RepairNone", "Repair complete. Nothing needed fixing."));
		}
		TriageRepairResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Hide the repair button after running
	if (TriageRepairButton)
	{
		TriageRepairButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(Result.HasOrphanCandidates()
			? LOCTEXT("Triage_RepairCompleteWithOrphans", "Chain repair complete — save, reload, and run Detect again")
			: LOCTEXT("Triage_RepairComplete", "Chain repair complete"));
	}
}

void USmartUpgradePanel::OnTraversalScanClicked()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Traversal Scan clicked"));

	// Get player controller and build gun
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_ErrNoPlayer", "Error: No player controller"));
		}
		return;
	}

	// Get the upgrade target from the current hologram - the game already knows what we're aiming at
	AFGBuildable* AnchorBuildable = nullptr;

	FVector HologramLocation = FVector::ZeroVector;
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (Character)
	{
		AFGBuildGun* BuildGun = Character->GetBuildGun();
		if (BuildGun)
		{
			UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState());
			AFGHologram* Hologram = BuildState ? BuildState->GetHologram() : nullptr;
			if (Hologram)
			{
				// Save hologram location for fallback search
				HologramLocation = Hologram->GetActorLocation();

				// GetUpgradedActor returns the buildable being targeted for upgrade (only when placement is valid)
				AActor* UpgradeTarget = Hologram->GetUpgradedActor();
				AnchorBuildable = Cast<AFGBuildable>(UpgradeTarget);

				if (AnchorBuildable)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Got upgrade target from hologram: %s"),
						*AnchorBuildable->GetClass()->GetName());
				}
			}
		}
	}

	// Fallback: If hologram didn't give us a target (e.g. same-tier),
	// use line trace to find hit point and search for nearest upgradeable buildable
	if (!AnchorBuildable)
	{
		FHitResult HitResult;
		FVector StartLocation = PC->PlayerCameraManager->GetCameraLocation();
		FVector EndLocation = StartLocation + PC->PlayerCameraManager->GetCameraRotation().Vector() * 5000.0f;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PC->GetPawn());

		// Line trace to get hit point (even if it hits instanced mesh manager)
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
		{
			// Use actor iteration to find nearest upgradeable buildable to hit point
			// This works around instanced mesh rendering where physics queries return wrong actor
			FVector SearchPoint = HitResult.ImpactPoint;
			float SearchRadius = 300.0f;
			float ClosestDist = FLT_MAX;

			for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
			{
				AFGBuildable* Buildable = *It;
				if (!Buildable) continue;

				float Dist = FVector::Dist(SearchPoint, Buildable->GetActorLocation());
				if (Dist < SearchRadius)
				{
					// Only consider upgradeable types
					ESFUpgradeFamily Family = USFUpgradeTraversalService::GetUpgradeFamily(Buildable);
					if (Family != ESFUpgradeFamily::None && Dist < ClosestDist)
					{
						ClosestDist = Dist;
						AnchorBuildable = Buildable;
					}
				}
			}

			if (AnchorBuildable)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Found buildable near aim point: %s (dist: %.1f)"),
					*AnchorBuildable->GetClass()->GetName(), ClosestDist);
			}
		}
	}

	if (!AnchorBuildable)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_AimAtTarget", "Aim at a belt, pipe, or power pole to scan its network"));
		}
		if (TraversalAnchorText)
		{
			TraversalAnchorText->SetText(LOCTEXT("Upgrade_NoTarget", "No target - aim at a buildable"));
		}
		return;
	}

	// Check if it's an upgradeable type
	ESFUpgradeFamily Family = USFUpgradeTraversalService::GetUpgradeFamily(AnchorBuildable);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: GetUpgradeFamily for %s returned %d"),
		*AnchorBuildable->GetClass()->GetName(), (int32)Family);

	if (Family == ESFUpgradeFamily::None)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_NotUpgradeable", "Target is not upgradeable - aim at a belt, pipe, or power pole"));
		}
		if (TraversalAnchorText)
		{
			TraversalAnchorText->SetText(FText::Format(LOCTEXT("Upgrade_Invalid", "Invalid: {0}"), FText::FromString(AnchorBuildable->GetClass()->GetName())));
		}
		return;
	}

	// Update anchor text
	TraversalAnchor = AnchorBuildable;
	if (TraversalAnchorText)
	{
		int32 Tier = USFUpgradeTraversalService::GetBuildableTier(AnchorBuildable);
		TraversalAnchorText->SetText(FText::Format(LOCTEXT("Upgrade_Anchor", "Anchor: {0} (Mk{1})"), FText::FromString(AnchorBuildable->GetClass()->GetName()), FText::AsNumber(Tier)));
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Upgrade_Scanning", "Scanning network..."));
	}

	// Read traversal config from checkboxes
	if (CrossSplittersCheckBox)
	{
		TraversalConfig.bCrossSplitters = CrossSplittersCheckBox->IsChecked();
	}
	if (CrossStorageCheckBox)
	{
		TraversalConfig.bCrossStorage = CrossStorageCheckBox->IsChecked();
	}
	if (CrossTrainPlatformsCheckBox)
	{
		TraversalConfig.bCrossTrainPlatforms = CrossTrainPlatformsCheckBox->IsChecked();
	}

	// Create traversal service and run scan
	USFUpgradeTraversalService* TraversalService = NewObject<USFUpgradeTraversalService>();
	CachedTraversalResult = TraversalService->TraverseNetwork(AnchorBuildable, TraversalConfig, PC);

	// Update UI with results
	UpdateTraversalUI(CachedTraversalResult);
}

void USmartUpgradePanel::UpdateTraversalUI(const FSFTraversalResult& Result)
{
	if (!Result.IsValid())
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(FText::Format(LOCTEXT("Upgrade_ScanFailed", "Scan failed: {0}"), FText::FromString(Result.ErrorMessage)));
		}
		return;
	}

	// Update status text
	if (SharedStatusText)
	{
		FText FoundText = FText::Format(LOCTEXT("Upgrade_FoundItems", "Found {0} items ({1} upgradeable)"), FText::AsNumber(Result.TotalCount), FText::AsNumber(Result.UpgradeableCount));
		if (Result.bHitMaxLimit)
		{
			FoundText = FText::Format(LOCTEXT("Upgrade_FoundItemsLimit", "{0} [LIMIT HIT]"), FoundText);
		}
		SharedStatusText->SetText(FoundText);
	}

	// Clear existing results
	if (TraversalResultsContainer)
	{
		TraversalResultsContainer->ClearChildren();
	}

	// Build tier breakdown display
	if (TraversalResultsContainer)
	{
		// Create a text block for tier breakdown
		for (const auto& TierPair : Result.CountByTier)
		{
			int32 Tier = TierPair.Key;
			int32 Count = TierPair.Value;

			UTextBlock* TierText = NewObject<UTextBlock>(this);
			TierText->SetText(FText::FromString(FString::Printf(TEXT("  Mk%d: %d"), Tier, Count)));

			FSlateFontInfo FontInfo = TierText->GetFont();
			FontInfo.Size = 12;
			TierText->SetFont(FontInfo);
			TierText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

			TraversalResultsContainer->AddChild(TierText);
		}
	}

	// Set selected family from result
	SelectedFamily = Result.Family;
	SelectedTier = 0; // Will be set when user selects target tier

	// Populate target tier dropdown
	PopulateTargetTierDropdown();

	// Show the tier dropdown for traversal tab
	if (TraversalTargetTierComboBox)
	{
		TraversalTargetTierComboBox->SetVisibility(ESlateVisibility::Visible);
	}

	// Update cost display for traversal results
	UpdateCostDisplay();

	// Enable upgrade button if there are upgradeable items
	if (Result.UpgradeableCount > 0)
	{
		if (SharedUpgradeButton)
		{
			SharedUpgradeButton->SetIsEnabled(true);
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Upgrade Panel: Traversal scan complete - %d items, %d upgradeable"),
		Result.TotalCount, Result.UpgradeableCount);
}

#undef LOCTEXT_NAMESPACE
