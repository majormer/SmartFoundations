// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USmartSettingsFormWidget - #427 Smart Restore redesign: the two-tab Restore region
 * (Grid Presets | Modules), built PROGRAMMATICALLY inside the Blueprint's RestoreSidePanel
 * border at construct time. Design record: docs/Features/SmartRestore/
 * DESIGN_Restore_Redesign_Decisions.md.
 *
 * Why programmatic: the Smart_Settings_Form_Widget Blueprint is hazardous to edit (section
 * subobject nulling, cooked-size gates), and this codebase already builds UI in C++ wholesale
 * (Walk panel, recipe detail rows). BuildRestoreTabUI REPLACES the border's designer content
 * with a new tree and REPARENTS the still-relevant bound widgets (dropdown, name/description
 * inputs, action buttons) into it - the Blueprint asset itself is untouched.
 */

#include "UI/SmartSettingsFormWidgetImpl.h"
#include "UI/SFPanelStyle.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "FGRecipeManager.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

namespace
{
	// The Restore palette + button/input helpers now live in the shared SFPanelStyle header so
	// Smart Restore, Smart Upgrade, and future panels share ONE Smart! visual language. These
	// file-local aliases keep the existing Restore call sites unchanged (zero behavior change).
	const FLinearColor& SFRestoreAccent = SFPanelStyle::Accent;
	const FLinearColor& SFRestoreMutedPanel = SFPanelStyle::MutedPanel;
	const FLinearColor& SFRestoreLightText = SFPanelStyle::LightText;
	const FLinearColor& SFRestoreDimText = SFPanelStyle::DimText;

	FButtonStyle MakeRestoreButtonStyle(bool bAccent) { return SFPanelStyle::MakeButtonStyle(bAccent); }
	void StyleRestoreInputLight(UEditableTextBox* Input) { SFPanelStyle::StyleInputLight(Input); }
}

// ============================================================================
// Construction
// ============================================================================

UButton* USmartSettingsFormWidget::MakeRestoreActionButton(const FText& Label, UButton* /*StyleReference*/, UTextBlock** OutLabel)
{
	// (StyleReference retired 2026-07-03: copying the BP's light-grey style produced grey-on-grey
	// text; all restore buttons now share the dark Smart-Panel-aligned style with light labels.)
	UButton* Btn = NewObject<UButton>(this);
	if (!Btn)
	{
		return nullptr;
	}

	Btn->SetStyle(MakeRestoreButtonStyle(/*bAccent*/ false));

	UTextBlock* LabelText = NewObject<UTextBlock>(this);
	if (LabelText)
	{
		LabelText->SetText(Label);
		LabelText->SetFont(SFFont::Get(11));
		LabelText->SetColorAndOpacity(FSlateColor(SFRestoreLightText));
		LabelText->SetJustification(ETextJustify::Center);
		Btn->AddChild(LabelText);
	}

	if (OutLabel)
	{
		*OutLabel = LabelText;
	}
	return Btn;
}

void USmartSettingsFormWidget::BuildRestoreTabUI()
{
	// The BP's RestoreSidePanel border is the single designer dependency. Without it (very old
	// widget asset), the legacy layout stays as-is.
	if (!RestoreSidePanel)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("[SmartRestore][UI] No RestoreSidePanel bound - tabbed Restore UI skipped"));
		return;
	}

	auto Reparent = [](UWidget* W, UVerticalBox* NewParent, const FMargin& Pad, float FillOrAuto = -1.0f)
	{
		if (!W || !NewParent)
		{
			return;
		}
		W->RemoveFromParent();
		if (UVerticalBoxSlot* VSlot = NewParent->AddChildToVerticalBox(W))
		{
			VSlot->SetPadding(Pad);
			if (FillOrAuto > 0.0f)
			{
				VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		W->SetVisibility(ESlateVisibility::Visible);
	};

	auto AddRow = [this](UVerticalBox* Parent, const TArray<UWidget*>& Widgets, const FMargin& RowPad) -> UHorizontalBox*
	{
		UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
		for (UWidget* W : Widgets)
		{
			if (!W)
			{
				continue;
			}
			W->RemoveFromParent();
			if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(W))
			{
				HSlot->SetPadding(FMargin(2.0f, 0.0f));
				HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				HSlot->SetVerticalAlignment(VAlign_Center);
			}
			W->SetVisibility(ESlateVisibility::Visible);
		}
		if (UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(Row))
		{
			VSlot->SetPadding(RowPad);
		}
		return Row;
	};

	auto MakeSectionLabel = [this](UVerticalBox* Parent, const FText& Text, const FMargin& Pad)
	{
		UTextBlock* TB = NewObject<UTextBlock>(this);
		TB->SetText(Text);
		TB->SetFont(SFFont::Get(10));
		TB->SetColorAndOpacity(FSlateColor(SFRestoreDimText));
		if (UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(TB))
		{
			VSlot->SetPadding(Pad);
		}
	};

	auto MakeDetailsPane = [this](UVerticalBox* Parent) -> UTextBlock*
	{
		UBorder* Pane = NewObject<UBorder>(this);
		Pane->SetBrushColor(SFRestoreMutedPanel);
		Pane->SetPadding(FMargin(8.0f, 6.0f));
		UTextBlock* Text = NewObject<UTextBlock>(this);
		Text->SetFont(SFFont::Get(11));
		Text->SetColorAndOpacity(FSlateColor(SFRestoreLightText));
		Text->SetAutoWrapText(true);
		Pane->SetContent(Text);
		if (UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(Pane))
		{
			VSlot->SetPadding(FMargin(6.0f, 4.0f));
		}
		return Text;
	};

	// ── Root ────────────────────────────────────────────────────────────────
	UVerticalBox* Root = NewObject<UVerticalBox>(this);

	UTextBlock* Header = NewObject<UTextBlock>(this);
	Header->SetText(LOCTEXT("Restore_Title", "Smart Restore"));
	Header->SetFont(SFFont::Get(15));
	Header->SetColorAndOpacity(FSlateColor(SFRestoreAccent));
	if (UVerticalBoxSlot* HeaderSlot = Root->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(8.0f, 8.0f, 8.0f, 4.0f));
	}

	// ── Tab bar ─────────────────────────────────────────────────────────────
	{
		UButton* StyleRef = SavePresetBtn ? SavePresetBtn.Get() : ApplyPresetBtn.Get();
		UTextBlock* GridLabel = nullptr;
		UTextBlock* ModLabel = nullptr;
		GridPresetsTabButton = MakeRestoreActionButton(LOCTEXT("Restore_Tab_GridPresets", "Grid Presets"), StyleRef, &GridLabel);
		ModulesTabButton = MakeRestoreActionButton(LOCTEXT("Restore_Tab_Modules", "Modules"), StyleRef, &ModLabel);
		GridPresetsTabLabel = GridLabel;
		ModulesTabLabel = ModLabel;
		if (GridPresetsTabButton)
		{
			GridPresetsTabButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnGridPresetsTabClicked);
		}
		if (ModulesTabButton)
		{
			ModulesTabButton->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnModulesTabClicked);
		}
		AddRow(Root, { GridPresetsTabButton, ModulesTabButton }, FMargin(6.0f, 2.0f));
	}

	// ── Switcher ────────────────────────────────────────────────────────────
	RestoreTabSwitcher = NewObject<UWidgetSwitcher>(this);
	if (UVerticalBoxSlot* SwitcherSlot = Root->AddChildToVerticalBox(RestoreTabSwitcher))
	{
		SwitcherSlot->SetPadding(FMargin(0.0f));
	}

	// ════════ Tab 0: GRID PRESETS ════════
	// A Grid Preset = a full snapshot of the panel's values. Selecting only shows details;
	// Load to Panel stages values; Apply & Build switches the gun + stages + honors Apply
	// Immediately. Save reads the ON-SCREEN values (flush-then-capture).
	UVerticalBox* GridTab = NewObject<UVerticalBox>(this);

	// Restyle the reparented BP buttons to the dark Smart-Panel-aligned scheme (their designer
	// style is light-grey, which fought the dark panel).
	{
		const FButtonStyle DarkStyle = MakeRestoreButtonStyle(/*bAccent*/ false);
		for (UButton* BpButton : { SavePresetBtn.Get(), DeletePresetBtn.Get(), UpdatePresetBtn.Get(),
			ExportPresetBtn.Get(), ImportPresetBtn.Get(), ImportFromExtendBtn.Get() })
		{
			if (BpButton)
			{
				BpButton->SetStyle(DarkStyle);
			}
		}
	}

	Reparent(PresetDropdown, GridTab, FMargin(6.0f, 4.0f));
	GridPresetDetailsText = MakeDetailsPane(GridTab);

	{
		// #427 feedback round: "Apply & Build" is retired - the panel blocks hologram interaction
		// anyway, so Load to Panel is THE load action (build gun + recipe + auto-connect + staged
		// values, honoring Apply Immediately). ApplyPresetBtn stays orphaned (not in the tree).
		UTextBlock* LoadLabel = nullptr;
		LoadToPanelBtn = MakeRestoreActionButton(LOCTEXT("Restore_LoadToPanel", "Load to Panel"), nullptr, &LoadLabel);
		LoadToPanelBtnLabel = LoadLabel;
		if (LoadToPanelBtn)
		{
			LoadToPanelBtn->SetStyle(MakeRestoreButtonStyle(/*bAccent*/ true));
			LoadToPanelBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnLoadToPanelClicked);
		}
		AddRow(GridTab, { LoadToPanelBtn }, FMargin(6.0f, 2.0f));
		AddRow(GridTab, { UpdatePresetBtn, DeletePresetBtn }, FMargin(6.0f, 2.0f));
	}

	MakeSectionLabel(GridTab, LOCTEXT("Restore_SaveCard", "Save current panel as a new preset:"), FMargin(8.0f, 8.0f, 8.0f, 0.0f));
	if (PresetNameInput)
	{
		PresetNameInput->SetHintText(LOCTEXT("Restore_PresetName_Hint", "New preset name"));
		StyleRestoreInputLight(PresetNameInput);
	}
	if (PresetDescriptionInput)
	{
		PresetDescriptionInput->SetHintText(LOCTEXT("Restore_PresetDesc_Hint", "Description (optional)"));
		StyleRestoreInputLight(PresetDescriptionInput);
	}
	Reparent(PresetNameInput, GridTab, FMargin(6.0f, 2.0f));
	Reparent(PresetDescriptionInput, GridTab, FMargin(6.0f, 2.0f));
	AddRow(GridTab, { SavePresetBtn }, FMargin(6.0f, 2.0f));

	MakeSectionLabel(GridTab, LOCTEXT("Restore_ShareCard", "Share:"), FMargin(8.0f, 8.0f, 8.0f, 0.0f));
	AddRow(GridTab, { ExportPresetBtn, ImportPresetBtn }, FMargin(6.0f, 2.0f));

	RestoreTabSwitcher->AddChild(GridTab);

	// ════════ Tab 1: MODULES ════════
	// A Module = a captured Extend manifold unit; Apply auto-equips the source building and
	// enters the rescalable stamp session. The top slot is the "Extend clipboard" - the
	// previously-invisible transient capture buffer, made visible (#427 capture model).
	UVerticalBox* ModTab = NewObject<UVerticalBox>(this);

	{
		// Same muted pane + dim section-label styling as the Grid Presets tab - the earlier
		// orange-brown wash made the two tabs read as different schemes (round-2 feedback).
		UBorder* ClipPane = NewObject<UBorder>(this);
		ClipPane->SetBrushColor(SFRestoreMutedPanel);
		ClipPane->SetPadding(FMargin(8.0f, 6.0f));

		UVerticalBox* ClipVB = NewObject<UVerticalBox>(this);

		UTextBlock* ClipHeader = NewObject<UTextBlock>(this);
		ClipHeader->SetText(LOCTEXT("Restore_Clipboard_Header", "Extend clipboard"));
		ClipHeader->SetFont(SFFont::Get(10));
		ClipHeader->SetColorAndOpacity(FSlateColor(SFRestoreDimText));
		ClipVB->AddChildToVerticalBox(ClipHeader);

		ClipboardSummaryText = NewObject<UTextBlock>(this);
		ClipboardSummaryText->SetFont(SFFont::Get(11));
		ClipboardSummaryText->SetColorAndOpacity(FSlateColor(SFRestoreLightText));
		ClipboardSummaryText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* ClipTextSlot = ClipVB->AddChildToVerticalBox(ClipboardSummaryText))
		{
			ClipTextSlot->SetPadding(FMargin(0.0f, 4.0f));
		}

		ModuleNameInput = NewObject<UEditableTextBox>(this);
		if (PresetNameInput)
		{
			ModuleNameInput->WidgetStyle = PresetNameInput->WidgetStyle;
		}
		StyleRestoreInputLight(ModuleNameInput);
		ModuleNameInput->SetHintText(LOCTEXT("Restore_ModuleName_Hint", "New Module name"));
		ModuleNameInput->WidgetStyle.TextStyle.Font.Size = 10;
		ClipVB->AddChildToVerticalBox(ModuleNameInput);

		// "Save as Module" = the repurposed Import-from-Extend button (same handler slot, new
		// flow: capture + save, NO apply). Reparent so it keeps its BP style.
		if (ImportFromExtendBtn)
		{
			ImportFromExtendBtn->RemoveFromParent();
			if (UVerticalBoxSlot* SaveSlot = ClipVB->AddChildToVerticalBox(ImportFromExtendBtn))
			{
				SaveSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
			}
			ImportFromExtendBtn->SetVisibility(ESlateVisibility::Visible);
		}

		ClipPane->SetContent(ClipVB);
		if (UVerticalBoxSlot* ClipSlot = ModTab->AddChildToVerticalBox(ClipPane))
		{
			ClipSlot->SetPadding(FMargin(6.0f, 4.0f));
		}
	}

	MakeSectionLabel(ModTab, LOCTEXT("Restore_ModuleLibrary", "Saved Modules:"), FMargin(8.0f, 6.0f, 8.0f, 0.0f));

	ModuleDropdown = NewObject<UComboBoxString>(this);
	if (PresetDropdown)
	{
		ModuleDropdown->SetWidgetStyle(PresetDropdown->GetWidgetStyle());
		ModuleDropdown->SetItemStyle(PresetDropdown->GetItemStyle());
	}
	ModuleDropdown->OnSelectionChanged.AddDynamic(this, &USmartSettingsFormWidget::OnModuleSelectionChanged);
	if (UVerticalBoxSlot* ModListSlot = ModTab->AddChildToVerticalBox(ModuleDropdown))
	{
		ModListSlot->SetPadding(FMargin(6.0f, 4.0f));
	}

	ModuleDetailsText = MakeDetailsPane(ModTab);

	{
		UTextBlock* ModApplyLabel = nullptr;
		ModuleApplyBtn = MakeRestoreActionButton(LOCTEXT("Restore_ModuleApply", "Apply"), ApplyPresetBtn, &ModApplyLabel);
		ModuleApplyBtnLabel = ModApplyLabel;
		if (ModuleApplyBtn)
		{
			// Accent = the tab's primary action, mirroring Load to Panel on the Grid tab.
			ModuleApplyBtn->SetStyle(MakeRestoreButtonStyle(/*bAccent*/ true));
		}
		ModuleDeleteBtn = MakeRestoreActionButton(LOCTEXT("Restore_ModuleDelete", "Delete"), DeletePresetBtn);
		ModuleExportBtn = MakeRestoreActionButton(LOCTEXT("Restore_ModuleExport", "Export Code"), ExportPresetBtn);
		ModuleImportBtn = MakeRestoreActionButton(LOCTEXT("Restore_ModuleImport", "Import Code"), ImportPresetBtn);
		if (ModuleApplyBtn)
		{
			ModuleApplyBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnModuleApplyClicked);
		}
		if (ModuleDeleteBtn)
		{
			ModuleDeleteBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnModuleDeleteClicked);
		}
		if (ModuleExportBtn)
		{
			ModuleExportBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnModuleExportClicked);
		}
		if (ModuleImportBtn)
		{
			ModuleImportBtn->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnModuleImportClicked);
		}
		AddRow(ModTab, { ModuleApplyBtn, ModuleDeleteBtn }, FMargin(6.0f, 2.0f));
		AddRow(ModTab, { ModuleExportBtn, ModuleImportBtn }, FMargin(6.0f, 2.0f));
	}

	RestoreTabSwitcher->AddChild(ModTab);

	// Replace the border's designer content wholesale. Orphaned legacy widgets (capture
	// checkboxes, old section labels, PresetCreatedAtValue) stay alive via their BindWidget
	// UPROPERTYs but are no longer in the tree - the details panes supersede them.
	//
	// The border only PAINTS its canvas-slot rect, and our content is taller than the designer
	// height - everything below the designer rect floated against the world with no backdrop
	// (maintainer feedback 2026-07-03). Auto-size the slot so the border's dark brush tracks the
	// content height; a SizeBox pins the designer WIDTH so auto-size can't collapse it to the
	// children's desired width.
	USizeBox* WidthPin = NewObject<USizeBox>(this);
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(RestoreSidePanel->Slot))
	{
		WidthPin->SetWidthOverride(PanelSlot->GetSize().X);
		PanelSlot->SetAutoSize(true);
	}
	WidthPin->AddChild(Root);
	RestoreSidePanel->SetContent(WidthPin);

	SetActiveRestoreTab(0);
	RefreshModuleDropdown();
	UpdateClipboardSlot();

	// Live clipboard refresh while the panel exists: the slot mirrors the current Extend
	// preview/build state (previews count - #427 capture model), which changes outside any
	// widget event.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<USmartSettingsFormWidget> WeakThis(this);
		World->GetTimerManager().SetTimer(ClipboardRefreshTimerHandle,
			FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->UpdateClipboardSlot();
				}
			}),
			0.75f, /*bLoop*/ true);
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("[SmartRestore][UI] Tabbed Restore UI constructed (Grid Presets | Modules)"));
}

void USmartSettingsFormWidget::SetActiveRestoreTab(int32 TabIndex)
{
	ActiveRestoreTabIndex = FMath::Clamp(TabIndex, 0, 1);
	if (RestoreTabSwitcher)
	{
		RestoreTabSwitcher->SetActiveWidgetIndex(ActiveRestoreTabIndex);
	}

	// Active tab = orange accent button + near-black label; idle = dark button + light label.
	if (GridPresetsTabButton)
	{
		GridPresetsTabButton->SetStyle(MakeRestoreButtonStyle(ActiveRestoreTabIndex == 0));
	}
	if (ModulesTabButton)
	{
		ModulesTabButton->SetStyle(MakeRestoreButtonStyle(ActiveRestoreTabIndex == 1));
	}
	if (GridPresetsTabLabel)
	{
		GridPresetsTabLabel->SetColorAndOpacity(FSlateColor(ActiveRestoreTabIndex == 0 ? FLinearColor(0.05f, 0.05f, 0.05f, 1.0f) : SFRestoreLightText));
	}
	if (ModulesTabLabel)
	{
		ModulesTabLabel->SetColorAndOpacity(FSlateColor(ActiveRestoreTabIndex == 1 ? FLinearColor(0.05f, 0.05f, 0.05f, 1.0f) : SFRestoreLightText));
	}

	if (ActiveRestoreTabIndex == 1)
	{
		UpdateClipboardSlot();
	}
}

void USmartSettingsFormWidget::OnGridPresetsTabClicked()
{
	SetActiveRestoreTab(0);
}

void USmartSettingsFormWidget::OnModulesTabClicked()
{
	SetActiveRestoreTab(1);
}

// ============================================================================
// Dock (Q8: one draggable unit)
// ============================================================================

void USmartSettingsFormWidget::UpdateRestoreDockPosition()
{
	if (!BackgroundPanelSlot || !RestoreSidePanelSlot || !BackgroundPanel)
	{
		return;
	}

	// Dock to the RENDERED right edge, not the canvas slot's designer size: the slot is wider
	// than the visible panel (ScaleBox'd content), which parked the docked Restore panel far off
	// to the side (maintainer feedback 2026-07-03). Slot positions live in root-widget local
	// space (the drag code relies on that), so convert the panel's right edge into that space.
	const FGeometry& BgGeo = BackgroundPanel->GetCachedGeometry();
	const FGeometry& RootGeo = GetCachedGeometry();
	if (BgGeo.GetLocalSize().X <= 1.0f || RootGeo.GetLocalSize().X <= 1.0f)
	{
		// No layout pass yet (first construct frame): sane provisional offset; the deferred
		// next-tick dock call corrects it once geometry exists.
		RestoreSidePanelSlot->SetPosition(BackgroundPanelSlot->GetPosition() + FVector2D(320.0f, 0.0f));
		return;
	}

	const FVector2D RightEdgeAbs = BgGeo.LocalToAbsolute(FVector2D(BgGeo.GetLocalSize().X, 0.0f));
	const FVector2D RootLocal = RootGeo.AbsoluteToLocal(RightEdgeAbs);
	RestoreSidePanelSlot->SetPosition(RootLocal + FVector2D(8.0f, 0.0f));
}

// ============================================================================
// Grid Presets tab actions
// ============================================================================

void USmartSettingsFormWidget::OnLoadToPanelClicked()
{
	// #427 feedback round: Load to Panel is THE load action ("Apply & Build" retired - the open
	// panel blocks hologram interaction anyway). Full load: switch the build gun to the preset's
	// building + apply recipe/auto-connect (service, no counters), stage the values into the
	// panel, and honor Apply Immediately (on -> commits like pressing Apply; off -> fine-tune).
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
		if (GridPresetDetailsText)
		{
			GridPresetDetailsText->SetText(LOCTEXT("Restore_NoSelection", "Select a preset first."));
		}
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
		PopulateSmartPanelFromPreset(Preset);

		if (bApplyImmediately)
		{
			OnApplyButtonClicked();
		}

		if (GridPresetDetailsText)
		{
			GridPresetDetailsText->SetText(FText::Format(
				bApplyImmediately
					? LOCTEXT("Restore_LoadedApplied", "Loaded and applied '{0}'.")
					: LOCTEXT("Restore_LoadedStaged", "Loaded '{0}' into the panel - fine-tune, then Apply."),
				FText::FromString(Preset.Name)));
		}
	}
	else if (GridPresetDetailsText)
	{
		// Surface WHY (no silent no-ops - #427).
		FString Reason;
		RestoreSvc->ValidatePresetUnlocks(Preset, Reason);
		GridPresetDetailsText->SetText(FText::Format(
			LOCTEXT("Restore_LoadFailed", "Could not load '{0}'.\n{1}"),
			FText::FromString(Preset.Name),
			FText::FromString(Reason.IsEmpty() ? TEXT("See log for details.") : Reason)));
	}
}

// ============================================================================
// Modules tab actions
// ============================================================================

void USmartSettingsFormWidget::OnModuleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateModuleDetails(SelectedItem);
}

void USmartSettingsFormWidget::OnModuleApplyClicked()
{
	if (!CachedSubsystem.IsValid() || !ModuleDropdown || bWaitingForConfirmation)
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
	if (!RestoreSvc)
	{
		return;
	}

	const FString SelectedName = ModuleDropdown->GetSelectedOption();
	if (SelectedName.IsEmpty())
	{
		return;
	}

	bool bFound = false;
	const FSFRestorePreset Preset = RestoreSvc->LoadPreset(SelectedName, bFound);
	if (!bFound || !Preset.IsModule())
	{
		return;
	}

	const FString BuildingName = FriendlyBuildingNameFromRecipeName(Preset.BuildingClassName);
	const int32 PartCount = Preset.ExtendCloneTopology.ChildHolograms.Num();

	PendingConfirmCallback = [this, Preset, BuildingName]()
	{
		USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
		if (!Svc)
		{
			return;
		}

		// Full apply: auto-equips the source building (build-gun switch), seeds the scalable
		// session counters, applies recipe/AC, and schedules the topology replay. The stamp
		// session HUD takes over from here - close the panel so the player can aim and stamp.
		if (Svc->ApplyPreset(Preset, /*bIncludeCounterState*/ true))
		{
			SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Log,
				TEXT("[SmartRestore][UI] Module '%s' applied - entering stamp session (source: %s)"),
				*Preset.Name, *BuildingName);
			CloseForm();
		}
	};
	bWaitingForConfirmation = true;
	ShowConfirmationDialog(
		LOCTEXT("Restore_ModuleApplyTitle", "Apply Module").ToString(),
		FText::Format(
			LOCTEXT("Restore_ModuleApplyMessage", "Rebuild this saved layout ({0} parts) and switch to {1}? Hold {1} to stamp it; scroll to scale."),
			FText::AsNumber(PartCount),
			FText::FromString(BuildingName)).ToString(),
		FLinearColor(1.0f, 0.6f, 0.0f, 1.0f));
}

void USmartSettingsFormWidget::OnModuleDeleteClicked()
{
	if (!CachedSubsystem.IsValid() || !ModuleDropdown || bWaitingForConfirmation)
	{
		return;
	}

	const FString SelectedName = ModuleDropdown->GetSelectedOption();
	if (SelectedName.IsEmpty())
	{
		return;
	}

	PendingConfirmCallback = [this, SelectedName]()
	{
		USFRestoreService* Svc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
		if (Svc && Svc->DeletePreset(SelectedName))
		{
			RefreshModuleDropdown();
		}
	};
	bWaitingForConfirmation = true;
	ShowConfirmationDialog(
		LOCTEXT("Restore_ModuleDeleteTitle", "Delete Module").ToString(),
		FText::Format(LOCTEXT("Restore_ModuleDeleteMessage", "Delete Module '{0}'?"), FText::FromString(SelectedName)).ToString(),
		FLinearColor(1.0f, 0.3f, 0.3f, 1.0f));
}

void USmartSettingsFormWidget::OnModuleExportClicked()
{
	if (!CachedSubsystem.IsValid() || !ModuleDropdown)
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
	if (!RestoreSvc)
	{
		return;
	}

	const FString SelectedName = ModuleDropdown->GetSelectedOption();
	if (SelectedName.IsEmpty())
	{
		return;
	}

	bool bFound = false;
	const FSFRestorePreset Preset = RestoreSvc->LoadPreset(SelectedName, bFound);
	if (bFound)
	{
		FPlatformApplicationMisc::ClipboardCopy(*RestoreSvc->ExportToString(Preset));
	}
}

void USmartSettingsFormWidget::OnModuleImportClicked()
{
	// Shared import; OnImportPresetClicked routes to the right tab by the imported preset's kind.
	OnImportPresetClicked();
}

// ============================================================================
// Lists, details, clipboard
// ============================================================================

void USmartSettingsFormWidget::RefreshModuleDropdown(const FString& PreferredSelection)
{
	if (!ModuleDropdown || !CachedSubsystem.IsValid())
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
	if (!RestoreSvc)
	{
		return;
	}

	const FString PreviousSelection = ModuleDropdown->GetSelectedOption();
	ModuleDropdown->ClearOptions();

	TArray<FString> ModuleNames;
	for (const FSFRestorePreset& Preset : RestoreSvc->LoadAllPresets())
	{
		if (Preset.IsModule())
		{
			ModuleNames.Add(Preset.Name);
			ModuleDropdown->AddOption(Preset.Name);
		}
	}

	FString SelectionToApply;
	if (!PreferredSelection.IsEmpty() && ModuleNames.Contains(PreferredSelection))
	{
		SelectionToApply = PreferredSelection;
	}
	else if (!PreviousSelection.IsEmpty() && ModuleNames.Contains(PreviousSelection))
	{
		SelectionToApply = PreviousSelection;
	}
	else if (ModuleNames.Num() > 0)
	{
		SelectionToApply = ModuleNames[0];
	}

	if (!SelectionToApply.IsEmpty())
	{
		ModuleDropdown->SetSelectedOption(SelectionToApply);
	}
	UpdateModuleDetails(SelectionToApply);
}

void USmartSettingsFormWidget::UpdateGridPresetDetails(const FString& PresetName)
{
	if (!GridPresetDetailsText)
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
	if (!RestoreSvc || PresetName.IsEmpty())
	{
		GridPresetDetailsText->SetText(LOCTEXT("Restore_NoPresetSelected", "No preset selected.\nSave the current panel below, or import a shared code."));
		return;
	}

	bool bFound = false;
	const FSFRestorePreset Preset = RestoreSvc->LoadPreset(PresetName, bFound);
	if (!bFound)
	{
		GridPresetDetailsText->SetText(FText::GetEmpty());
		return;
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("%s: %s"),
		*LOCTEXT("Restore_Details_Building", "Building").ToString(),
		*FriendlyBuildingNameFromRecipeName(Preset.BuildingClassName)));
	Lines.Add(FString::Printf(TEXT("%s: %d x %d x %d"),
		*LOCTEXT("Restore_Details_Grid", "Grid").ToString(),
		Preset.GridCounters.X, Preset.GridCounters.Y, Preset.GridCounters.Z));

	TArray<FString> Extras;
	if (Preset.SpacingX != 0 || Preset.SpacingY != 0 || Preset.SpacingZ != 0)
	{
		Extras.Add(LOCTEXT("Restore_Details_Spacing", "spacing").ToString());
	}
	if (Preset.StepsX != 0 || Preset.StepsY != 0)
	{
		Extras.Add(LOCTEXT("Restore_Details_Steps", "steps").ToString());
	}
	if (Preset.StaggerX != 0 || Preset.StaggerY != 0 || Preset.StaggerZX != 0 || Preset.StaggerZY != 0)
	{
		Extras.Add(LOCTEXT("Restore_Details_Stagger", "stagger").ToString());
	}
	if (!FMath::IsNearlyZero(Preset.RotationZ))
	{
		Extras.Add(LOCTEXT("Restore_Details_Rotation", "rotation").ToString());
	}
	if (Extras.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"),
			*LOCTEXT("Restore_Details_Transforms", "Transforms").ToString(),
			*FString::Join(Extras, TEXT(", "))));
	}

	if (Preset.CaptureFlags.bRecipe)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"),
			*LOCTEXT("Restore_Details_Recipe", "Recipe").ToString(),
			Preset.bHasProductionRecipe && !Preset.RecipeClassName.IsEmpty()
				? *FriendlyBuildingNameFromRecipeName(Preset.RecipeClassName)
				: *LOCTEXT("Restore_Details_NoRecipe", "No recipe").ToString()));
	}

	if (!Preset.Description.IsEmpty())
	{
		Lines.Add(Preset.Description);
	}
	Lines.Add(FString::Printf(TEXT("%s: %s"),
		*LOCTEXT("Restore_Details_Created", "Created").ToString(),
		*FormatPresetTimestampForDisplay(Preset.CreatedAt)));

	// Availability gates Load to Panel (it switches the build gun / applies recipes).
	FString UnlockFailure;
	const bool bAvailable = RestoreSvc->ValidatePresetUnlocks(Preset, UnlockFailure);
	if (!bAvailable)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"),
			*LOCTEXT("Restore_Details_Requires", "Requires").ToString(), *UnlockFailure));
	}
	if (LoadToPanelBtn)
	{
		LoadToPanelBtn->SetIsEnabled(bAvailable);
	}

	GridPresetDetailsText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void USmartSettingsFormWidget::UpdateModuleDetails(const FString& ModuleName)
{
	if (!ModuleDetailsText)
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem.IsValid() ? CachedSubsystem->GetRestoreService() : nullptr;
	if (!RestoreSvc || ModuleName.IsEmpty())
	{
		ModuleDetailsText->SetText(LOCTEXT("Restore_NoModules", "No Modules yet.\nPreview or build a Smart Extend, then save it from the clipboard above."));
		if (ModuleApplyBtn)
		{
			ModuleApplyBtn->SetIsEnabled(false);
		}
		return;
	}

	bool bFound = false;
	const FSFRestorePreset Preset = RestoreSvc->LoadPreset(ModuleName, bFound);
	if (!bFound || !Preset.IsModule())
	{
		ModuleDetailsText->SetText(FText::GetEmpty());
		if (ModuleApplyBtn)
		{
			ModuleApplyBtn->SetIsEnabled(false);
		}
		return;
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("%s: %s"),
		*LOCTEXT("Restore_Details_Source", "Source").ToString(),
		*FriendlyBuildingNameFromRecipeName(Preset.BuildingClassName)));
	// Per-UNIT composition: the topology is one clone set; the total scales when you stamp.
	Lines.Add(FString::Printf(TEXT("%s: %s"),
		*LOCTEXT("Restore_Details_EachUnit", "Each unit").ToString(),
		*SummarizeTopologyByRole(Preset.ExtendCloneTopology)));
	if (Preset.bHasProductionRecipe && !Preset.RecipeClassName.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"),
			*LOCTEXT("Restore_Details_Recipe", "Recipe").ToString(),
			*FriendlyBuildingNameFromRecipeName(Preset.RecipeClassName)));
	}
	if (!Preset.Description.IsEmpty())
	{
		Lines.Add(Preset.Description);
	}
	Lines.Add(FString::Printf(TEXT("%s: %s"),
		*LOCTEXT("Restore_Details_Created", "Created").ToString(),
		*FormatPresetTimestampForDisplay(Preset.CreatedAt)));

	FString UnlockFailure;
	const bool bAvailable = RestoreSvc->ValidatePresetUnlocks(Preset, UnlockFailure);
	if (!bAvailable)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"),
			*LOCTEXT("Restore_Details_Requires", "Requires").ToString(), *UnlockFailure));
	}
	if (ModuleApplyBtn)
	{
		ModuleApplyBtn->SetIsEnabled(bAvailable);
	}

	ModuleDetailsText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void USmartSettingsFormWidget::UpdateClipboardSlot()
{
	if (!ClipboardSummaryText || !CachedSubsystem.IsValid())
	{
		return;
	}

	USFRestoreService* RestoreSvc = CachedSubsystem->GetRestoreService();
	USFExtendService* ExtendSvc = CachedSubsystem->GetExtendService();
	if (!RestoreSvc || !ExtendSvc)
	{
		return;
	}

	const bool bAvailable = RestoreSvc->IsLastExtendAvailable();
	if (bAvailable)
	{
		TSharedPtr<FSFCloneTopology> Topology = ExtendSvc->GetLastCloneTopology();
		if (Topology.IsValid())
		{
			// Friendly source name from the topology's parent build class (e.g. "Constructor").
			const FString SourceName = Topology->ParentBuildClass.IsEmpty()
				? LOCTEXT("Restore_Clipboard_UnknownSource", "Extend").ToString()
				: FriendlyBuildingNameFromRecipeName(Topology->ParentBuildClass);
			ClipboardSummaryText->SetText(FText::Format(
				LOCTEXT("Restore_Clipboard_Ready", "{0} manifold - {1}\nReady to save. (Replaced by your next Extend; lost on exit.)"),
				FText::FromString(SourceName),
				FText::FromString(SummarizeTopologyByRole(*Topology))));
		}
	}
	else if (ExtendSvc->IsRestoredCloneTopologyActive())
	{
		ClipboardSummaryText->SetText(LOCTEXT("Restore_Clipboard_ReplayActive",
			"A restored Module is active - finish stamping before capturing a new Extend."));
	}
	else
	{
		ClipboardSummaryText->SetText(LOCTEXT("Restore_Clipboard_Empty",
			"None yet. Preview or build a Smart Extend and it will appear here, ready to save."));
	}

	if (ImportFromExtendBtn)
	{
		if (ImportFromExtendBtn->GetIsEnabled() != bAvailable)
		{
			ImportFromExtendBtn->SetIsEnabled(bAvailable);
			// Keep the label readable across the enable flip (light vs dimmed).
			UpdateRestoreButtonTextColors();
		}
	}
	if (ModuleNameInput)
	{
		ModuleNameInput->SetIsEnabled(bAvailable);
	}
}

FString USmartSettingsFormWidget::SummarizeTopologyByRole(const FSFCloneTopology& Topology) const
{
	TMap<FString, int32> RoleCounts;
	for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
	{
		RoleCounts.FindOrAdd(Holo.Role.IsEmpty() ? TEXT("part") : Holo.Role)++;
	}

	auto RoleDisplay = [](const FString& Role, int32 Count) -> FString
	{
		FString Label;
		if (Role == TEXT("distributor"))       Label = Count == 1 ? LOCTEXT("Role_Distributor", "distributor").ToString() : LOCTEXT("Role_Distributors", "distributors").ToString();
		else if (Role == TEXT("belt_segment")) Label = Count == 1 ? LOCTEXT("Role_Belt", "belt").ToString() : LOCTEXT("Role_Belts", "belts").ToString();
		else if (Role == TEXT("lift_segment")) Label = Count == 1 ? LOCTEXT("Role_Lift", "lift").ToString() : LOCTEXT("Role_Lifts", "lifts").ToString();
		else if (Role == TEXT("pipe_segment")) Label = Count == 1 ? LOCTEXT("Role_Pipe", "pipe").ToString() : LOCTEXT("Role_Pipes", "pipes").ToString();
		else if (Role == TEXT("pipe_junction")) Label = Count == 1 ? LOCTEXT("Role_Junction", "junction").ToString() : LOCTEXT("Role_Junctions", "junctions").ToString();
		else if (Role == TEXT("power_pole"))   Label = Count == 1 ? LOCTEXT("Role_PowerPole", "power pole").ToString() : LOCTEXT("Role_PowerPoles", "power poles").ToString();
		else if (Role == TEXT("wire"))         Label = Count == 1 ? LOCTEXT("Role_Wire", "wire").ToString() : LOCTEXT("Role_Wires", "wires").ToString();
		else                                    Label = Role;
		return FString::Printf(TEXT("%d %s"), Count, *Label);
	};

	// Stable, readable order: distributors first, then conveyance, then the rest alphabetically.
	TArray<FString> Order = { TEXT("distributor"), TEXT("belt_segment"), TEXT("lift_segment"), TEXT("pipe_segment"), TEXT("pipe_junction"), TEXT("power_pole"), TEXT("wire") };
	TArray<FString> Parts;
	for (const FString& Role : Order)
	{
		if (const int32* Count = RoleCounts.Find(Role))
		{
			Parts.Add(RoleDisplay(Role, *Count));
			RoleCounts.Remove(Role);
		}
	}
	TArray<FString> Leftovers;
	RoleCounts.GetKeys(Leftovers);
	Leftovers.Sort();
	for (const FString& Role : Leftovers)
	{
		Parts.Add(RoleDisplay(Role, RoleCounts[Role]));
	}

	return Parts.Num() > 0
		? FString::Join(Parts, TEXT(", "))
		: LOCTEXT("Restore_EmptyTopology", "no parts").ToString();
}

FString USmartSettingsFormWidget::FriendlyBuildingNameFromRecipeName(const FString& RecipeClassName) const
{
	if (RecipeClassName.IsEmpty())
	{
		// Old presets could be saved without an active build gun (e.g. the June "Smelters" husk).
		return LOCTEXT("Restore_NoBuildingCaptured", "none captured").ToString();
	}

	// Try the recipe manager first (localized display name).
	if (CachedSubsystem.IsValid())
	{
		if (UWorld* World = CachedSubsystem->GetWorld())
		{
			if (AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World))
			{
				TArray<TSubclassOf<UFGRecipe>> AllRecipes;
				RecipeManager->GetAllAvailableRecipes(AllRecipes);
				for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
				{
					if (Recipe && Recipe->GetName() == RecipeClassName)
					{
						return UFGRecipe::GetRecipeName(Recipe).ToString();
					}
				}
			}
		}
	}

	// Fallback: strip the class-name scaffolding ("Recipe_ConstructorMk1_C" -> "ConstructorMk1",
	// "Build_Foundation_8x4_01_C" -> "Foundation_8x4_01").
	FString Friendly = RecipeClassName;
	Friendly.RemoveFromStart(TEXT("Recipe_"));
	Friendly.RemoveFromStart(TEXT("Build_"));
	Friendly.RemoveFromEnd(TEXT("_C"));
	return Friendly;
}

#undef LOCTEXT_NAMESPACE
