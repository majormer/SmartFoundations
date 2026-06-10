// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SmartUpgradePanel.h"
#include "UI/SmartUpgradePanelImpl.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"


void USmartUpgradePanel::NativeDestruct()
{
	// [UPGRADE-MP] The traversal-result delegate is static (the panel news up a throwaway
	// traversal service per scan) - unbind so a dead widget is never broadcast to.
	USFUpgradeTraversalService::OnClientTraversalResultReceived.RemoveAll(this);
	Super::NativeDestruct();
}

void USmartUpgradePanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogSmartUI, Log, TEXT("Upgrade Panel: NativeConstruct"));

	// Switch designer-placed (and localized) labels to the in-game multi-script font so
	// Arabic/Persian/Thai/CJK render correctly. Runtime-built rows route through SFFont::Get below.
	SFFont::ApplyToWidgetTree(WidgetTree);

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
		UE_LOG(LogSmartUI, Log, TEXT("Upgrade Panel: RadiusSliderSpinBox configured"));
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
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Close button clicked"));
	ClosePanel();
}

void USmartUpgradePanel::OnRefreshButtonClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Refresh button clicked"));
	RefreshAudit();
}

void USmartUpgradePanel::OnCancelButtonClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Cancel button clicked"));
	CancelAudit();
}

void USmartUpgradePanel::OnEntireMapButtonClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Entire Map button clicked - scanning with no radius limit"));
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

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Radius changed to %.0fm"), CurrentRadiusMeters);
}

void USmartUpgradePanel::OnUpgradeButtonClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Upgrade button clicked - Family=%d Tier=%d IsRadiusTab=%d"),
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
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: GetSelectedTargetTier returned %d, CachedTargetTier=%d"), TargetTier, CachedTargetTier);
	if (TargetTier == 0)
	{
		// No target selected - use max unlocked tier
		TargetTier = CachedTargetTier;
		UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Using CachedTargetTier=%d as fallback"), TargetTier);
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Starting upgrade - SourceTier=%d TargetTier=%d TraversalMode=%d"),
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
		UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Using %d buildables from traversal scan"), Params.SpecificBuildables.Num());
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

	// [UPGRADE-MP] On a network client the whole execution pipeline (hologram replacement, actor
	// destruction, connection repair, chain rebuild, cost) is server-authoritative world mutation
	// - route the request through the RCO. The result comes back via Client_ReceiveUpgradeResult
	// and is injected into the LOCAL execution service, so the delegate subscriptions above fire
	// exactly as in SP. Never fall through to a client-side run (client-only actors + desync).
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		TArray<AActor*> RCOActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), USFRCO::StaticClass(), RCOActors);
		for (AActor* Actor : RCOActors)
		{
			if (USFRCO* RCO = Cast<USFRCO>(Actor))
			{
				if (RCO->GetOuter() == PC)
				{
					RCO->Server_StartUpgrade(Params);
					UE_LOG(LogSmartUI, Verbose, TEXT("Upgrade Panel: Sent upgrade request via SFRCO"));
					return;
				}
			}
		}
		UE_LOG(LogSmartUI, Warning, TEXT("Upgrade Panel: Could not find SFRCO instance for upgrade execution"));
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_ErrRCO", "Error: server connection unavailable"));
		}
		return;
	}

	ExecutionService->StartUpgrade(Params);
}

void USmartUpgradePanel::RefreshAudit()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: RefreshAudit() requested"));

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

			UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Starting audit with radius %.0fm (%.0f cm)"),
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
							UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Sent Refresh request via SFRCO"));
							return;
						}
					}
				}
				UE_LOG(LogSmartUI, Warning, TEXT("Upgrade Panel: Could not find SFRCO instance for Refresh"));
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
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: CancelAudit() requested"));

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
							UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Sent Cancel request via SFRCO"));
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
	UVerticalBox* ActiveResultsContainer = RadiusAuditResultsContainer;

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
			// FactoryFont is a single-face font (no Bold weight); the orange color distinguishes the header.
			SectionHeader->SetFont(SFFont::Get(12));
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
					FSlateFontInfo RowFont = SFFont::Get(11);
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
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: ClosePanel() called"));

	// Get player controller
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogSmartUI, Warning, TEXT("Upgrade Panel: No owning player controller"));
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

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Closed and input restored"));
}

FReply USmartUpgradePanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle ESC key to close panel
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: ESC key pressed - closing"));
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

#undef LOCTEXT_NAMESPACE
