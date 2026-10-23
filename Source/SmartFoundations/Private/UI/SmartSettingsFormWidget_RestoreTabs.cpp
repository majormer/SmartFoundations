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
#include "UI/SFContextHelp.h"
#include "UI/SFInterfaceTaskStrip.h"
#include "Hologram/FGHologram.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ExpandableArea.h"
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

	auto MakeSectionLabel = [this](UVerticalBox* Parent, const FText& Text, const FMargin& Pad, USFContextHelp* Help = nullptr)
	{
		UTextBlock* TB = NewObject<UTextBlock>(this);
		TB->SetText(Text);
		TB->SetFont(SFFont::Get(16));
		TB->SetColorAndOpacity(FSlateColor(SFRestoreLightText));
		if (UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(Help ? SFPanelStyle::MakeHelpLabelRow(this, TB, Help) : TB))
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
	Root->AddChildToVerticalBox(SFPanelStyle::MakeRule(this,true))->SetPadding(FMargin(0,0,0,12));


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
		AddRow(Root, { SFPanelStyle::WrapTabButton(GridPresetsTabButton), SFPanelStyle::WrapTabButton(ModulesTabButton) }, FMargin(6.0f, 2.0f));
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
    auto* PresetHelp = SFPanelStyle::MakeHelp(this, TEXT("PresetHelp"),
        LOCTEXT("Help_PresetsTitle", "Grid Presets: placement settings"),
        LOCTEXT("Help_PresetsSummary", "Save grid, spacing and placement choices. A preset is settings, not a captured building layout."),
        LOCTEXT("Help_PresetsBody", "Select a preset to inspect it; selection alone changes nothing. Load to Panel returns to Scaling with those settings. Apply Immediately decides whether they update the preview immediately or wait for Apply.\nSave from Panel creates an entry from the current fields. Update replaces the selected entry; Delete removes it after confirmation. Export Code copies a shareable entry; Import Code reads one from the clipboard. Importing does not build anything."));

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

	MakeSectionLabel(GridTab, LOCTEXT("Restore_SavedPresets", "Saved grid presets"), FMargin(6,12,6,6), PresetHelp);
    GridTab->AddChildToVerticalBox(PresetHelp)->SetPadding(FMargin(6,0,6,12));
	Reparent(PresetDropdown, GridTab, FMargin(6.0f, 4.0f));
	GridPresetDetailsText = MakeDetailsPane(GridTab);

	{
		// #427 feedback round: "Apply & Build" is retired - the panel blocks hologram interaction
		// anyway, so Load to Panel is THE load action (build gun + recipe + auto-connect + staged
		// values, honoring Apply Immediately). ApplyPresetBtn stays orphaned (not in the tree).
		UTextBlock* LoadLabel = nullptr;
		LoadToPanelBtn = MakeRestoreActionButton(LOCTEXT("Restore_LoadToPanel", "Load to Panel"), nullptr, &LoadLabel);
		LoadToPanelBtnLabel = LoadLabel;
        if (LoadLabel) LoadLabel->SetColorAndOpacity(SFPanelStyle::NearBlackText);
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
	MakeSectionLabel(GridTab, LOCTEXT("Restore_NameLabel", "Name"), FMargin(6,8,6,2));
	Reparent(PresetNameInput, GridTab, FMargin(6.0f, 2.0f));
	MakeSectionLabel(GridTab, LOCTEXT("Restore_DescriptionLabel", "Description"), FMargin(6,8,6,2));
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
    auto* ModuleHelp = SFPanelStyle::MakeHelp(this, TEXT("ModuleHelp"),
        LOCTEXT("Help_ModulesTitle", "Modules: captured layouts"),
        LOCTEXT("Help_ModulesSummary", "Save a layout captured by Extend, then load it as a new building preview."),
        LOCTEXT("Help_ModulesBody", "Preview or build a Smart Extend to populate the Extend clipboard. Give that capture a name and choose Save as Module to add it to the library.\nSelecting a saved Module only shows its contents. Load Module confirms the recipe change and starts a scalable layout preview; it does not construct or charge materials. Aim and build normally when ready. The chosen buildings, unlocks, native restrictions and normal costs still apply. Delete and Share affect the library entry, not buildings already in the world."));

	{
		// Same muted pane + dim section-label styling as the Grid Presets tab - the earlier
		// orange-brown wash made the two tabs read as different schemes (round-2 feedback).
		UBorder* ClipPane = NewObject<UBorder>(this);
		ClipPane->SetBrushColor(SFRestoreMutedPanel);
		ClipPane->SetPadding(FMargin(8.0f, 6.0f));

		UVerticalBox* ClipVB = NewObject<UVerticalBox>(this);

		UTextBlock* ClipHeader = NewObject<UTextBlock>(this);
		ClipHeader->SetText(LOCTEXT("Restore_Clipboard_Header", "Extend clipboard"));
		ClipHeader->SetFont(SFFont::Get(16));
		ClipHeader->SetColorAndOpacity(FSlateColor(SFRestoreLightText));
		ClipVB->AddChildToVerticalBox(SFPanelStyle::MakeHelpLabelRow(this, ClipHeader, ModuleHelp));
        ClipVB->AddChildToVerticalBox(ModuleHelp)->SetPadding(FMargin(0,4,0,12));

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
		MakeSectionLabel(ClipVB, LOCTEXT("Restore_NameLabel", "Name"), FMargin(0,8,0,2));
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
	ModuleDropdown->Font = SFFont::Get(14);
	ModuleDropdown->ForegroundColor = FSlateColor(SFPanelStyle::NearBlackText);
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
		ModuleApplyBtn = MakeRestoreActionButton(LOCTEXT("Restore_LoadModule", "Load Module"), ApplyPresetBtn, &ModApplyLabel);
		ModuleApplyBtnLabel = ModApplyLabel;
        if (ModApplyLabel) ModApplyLabel->SetColorAndOpacity(SFPanelStyle::NearBlackText);
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
		ModTab->AddChildToVerticalBox(SFPanelStyle::MakeRule(this))->SetPadding(FMargin(6,12));
	MakeSectionLabel(ModTab, LOCTEXT("Restore_ShareModules", "Share"), FMargin(6,4,6,6));
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
    WidthPin->SetWidthOverride(620);
	RestoreSidePanel->SetPadding(FMargin(0));
	// Keep tabs/header visible while longer descriptions and libraries scroll.
	RestoreTabSwitcher->RemoveFromParent();
	auto* Scroll = NewObject<UScrollBox>(this); Scroll->AddChild(RestoreTabSwitcher);
    auto* ScrollSlot = Root->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetPadding(FMargin(0,12));
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
	if (bWaitingForConfirmation || PendingPanelPreset.IsValid()) return;
	ActiveRestoreTabIndex = FMath::Clamp(TabIndex, 0, 1);
	if (RestoreTabSwitcher)
	{
		RestoreTabSwitcher->SetActiveWidgetIndex(ActiveRestoreTabIndex);
	}

    SFPanelStyle::StyleTab(GridPresetsTabButton, ActiveRestoreTabIndex == 0);
    SFPanelStyle::StyleTab(ModulesTabButton, ActiveRestoreTabIndex == 1);

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
    if (TaskContentSwitcher) return;
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
        PendingPanelPreset = MakeShared<const FSFRestorePreset>(Preset);
        PresetLoadAttempts = 120;
        PresetLoadSettleTicks = 2;
        // Build-gun recipe changes complete asynchronously. Keep the saved values
        // staged until registration has settled on the matching native hologram.
        if (WidgetTree) WidgetTree->ForEachWidget([](UWidget* Widget)
        {
            if (auto* Strip = Cast<USFInterfaceTaskStrip>(Widget)) Strip->SetActiveTask(3);
        });
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

void USmartSettingsFormWidget::TickPendingPresetLoad()
{
    if (!PendingPanelPreset.IsValid() || !CachedSubsystem.IsValid()) return;
    CachedSubsystem->PollForActiveHologram();
    AFGHologram* Hologram = CachedSubsystem->GetActiveHologram();
    const bool bReady = IsValid(Hologram) && Hologram->GetBuildClass() && Hologram->GetRecipe()
        && Hologram->GetRecipe()->GetName() == PendingPanelPreset->BuildingClassName;
    if (bReady && PresetLoadSettleTicks-- <= 0)
    {
        const FSFRestorePreset Preset = *PendingPanelPreset;
        PendingPanelPreset.Reset();
        if (Preset.CaptureFlags.bAutoConnect)
            CachedSubsystem->SetAutoConnectRuntimeSettingsFromPreset(Preset.AutoConnect);
        PopulateSmartPanelFromPreset(Preset);
        ShowRestorePanel(false);
        if (bApplyImmediately) OnApplyButtonClicked();
        return;
    }
    if (!bReady) PresetLoadSettleTicks = 2;
    if (--PresetLoadAttempts > 0) return;
    PendingPanelPreset.Reset();
    PlacementContext = Hologram;
    RefreshContextControls();
    if (GridPresetDetailsText) GridPresetDetailsText->SetText(LOCTEXT("Restore_PanelLoadTimeout", "The building preview did not become ready. Your saved preset is unchanged; select it again when the build gun is ready."));
    if (WidgetTree) WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        if (auto* Strip = Cast<USFInterfaceTaskStrip>(Widget)) Strip->SetActiveTask(3);
    });
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
		LOCTEXT("Restore_LoadModule", "Load Module").ToString(),
		FText::Format(
			LOCTEXT("Restore_ModuleLoadMessage", "Load this saved layout ({0} parts) and switch to {1}? You can then aim, scale and build it. Normal construction costs apply."),
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
    for (UButton* Button : {LoadToPanelBtn.Get(), UpdatePresetBtn.Get(), DeletePresetBtn.Get(), ExportPresetBtn.Get()})
    {
        if (Button) Button->SetIsEnabled(false);
    }
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
	for (UButton* Button : {UpdatePresetBtn.Get(), DeletePresetBtn.Get(), ExportPresetBtn.Get()})
        if (Button) Button->SetIsEnabled(true);
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
    for (UButton* Button : {ModuleApplyBtn.Get(), ModuleDeleteBtn.Get(), ModuleExportBtn.Get()})
    {
        if (Button) Button->SetIsEnabled(false);
    }
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
    for (UButton* Button : {ModuleDeleteBtn.Get(), ModuleExportBtn.Get()})
    {
        if (Button) Button->SetIsEnabled(true);
    }
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
        // Unavailable capture is read-only, keeping its explanation readable.
        // Save eligibility remains owned by the unchanged native capture check.
        ModuleNameInput->SetIsEnabled(true);
        ModuleNameInput->SetIsReadOnly(!bAvailable);
        if (!bAvailable && !bModuleNameShowsUnavailable)
        {
            SuspendedModuleName = ModuleNameInput->GetText();
            bModuleNameShowsUnavailable = true;
            ModuleNameInput->SetText(LOCTEXT("Restore_ModuleName_NoCapture", "Capture a Smart Extend first"));
        }
        else if (bAvailable && bModuleNameShowsUnavailable)
        {
            ModuleNameInput->SetText(SuspendedModuleName);
            bModuleNameShowsUnavailable = false;
        }
        ModuleNameInput->SetHintText(bAvailable
            ? LOCTEXT("Restore_ModuleName_Hint", "New Module name")
            : LOCTEXT("Restore_ModuleName_NoCapture", "Capture a Smart Extend first"));
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
		else if (Role == TEXT("lane_segment")) Label = Count == 1 ? LOCTEXT("Role_RouteSegment", "route segment").ToString() : LOCTEXT("Role_RouteSegments", "route segments").ToString();
		else if (Role == TEXT("route_support")) Label = Count == 1 ? LOCTEXT("Role_RouteSupport", "route support").ToString() : LOCTEXT("Role_RouteSupports", "route supports").ToString();
		else if (Role == TEXT("wire_cost")) Label = Count == 1 ? LOCTEXT("Role_CableAllowance", "cable cost allowance").ToString() : LOCTEXT("Role_CableAllowances", "cable cost allowances").ToString();
		else Label = Count == 1 ? LOCTEXT("Role_Component", "component").ToString() : LOCTEXT("Role_Components", "components").ToString();
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
