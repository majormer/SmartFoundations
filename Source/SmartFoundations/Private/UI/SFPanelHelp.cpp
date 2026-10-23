// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFPanelStyle.h"
#include "UI/SFFontLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "UI/SFContextHelp.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
USFContextHelp* SFPanelStyle::MakeHelp(UObject* Outer, FName Name, FText Title, FText Summary, FText Details)
{
    auto* Help = NewObject<USFContextHelp>(Outer, Name);
    Help->InitializeHelp(FText::Join(FText::FromString(TEXT("\n\n")), Title, Summary, Details));
    return Help;
}

UWidget* SFPanelStyle::MakeHelpLabelRow(UObject* Outer, UWidget* Label, USFContextHelp* Help)
{
    auto* Row = NewObject<UHorizontalBox>(Outer);
    Label->RemoveFromParent();
    auto* LabelSlot = Row->AddChildToHorizontalBox(Label);
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LabelSlot->SetVerticalAlignment(VAlign_Center);
    auto* Button = Help->GetHelpButton();
    Button->RemoveFromParent();
    auto* ButtonSlot = Row->AddChildToHorizontalBox(Button);
    ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    ButtonSlot->SetVerticalAlignment(VAlign_Center);
    ButtonSlot->SetPadding(FMargin(8, 0, 0, 0));
    return Row;
}

TSharedRef<SWidget> SFPanelStyle::MakeSlateHelp(FText Guidance, TSharedPtr<SWidget>& OutButton)
{
    const auto Expanded = MakeShared<bool>(false);
    static const FButtonStyle Style = MakeHelpButtonStyle();
    OutButton = SNew(SButton).ButtonStyle(&Style).ToolTipText(Guidance)
        .OnClicked_Lambda([Expanded]{ *Expanded = !*Expanded; return FReply::Handled(); })
        [SNew(STextBlock).Text(FText::FromString(TEXT("?"))).Font(SFFont::Get(14)).ColorAndOpacity(Accent)];
    return SNew(STextBlock).Text(Guidance).Font(SFFont::Get(13)).ColorAndOpacity(LightText).AutoWrapText(true)
        .Visibility_Lambda([Expanded]{ return *Expanded ? EVisibility::Visible : EVisibility::Collapsed; });
}

void SFPanelStyle::ApplyControlHelp(UWidgetTree* Tree)
{
    if (!Tree) return;
    TMap<UWidget*,FString> BoundNames;
    UObject* Owner = Tree->GetOuter();
    for (TFieldIterator<FObjectPropertyBase> It(Owner->GetClass()); It; ++It)
        if (auto* Control = Cast<UWidget>(It->GetObjectPropertyValue_InContainer(Owner))) BoundNames.Add(Control, It->GetName());
    Tree->ForEachWidget([&BoundNames](UWidget* Widget)
    {
        if (!Cast<USpinBox>(Widget) && !Cast<UComboBoxString>(Widget) && !Cast<UCheckBox>(Widget) && !Cast<UButton>(Widget)) return;
        const FString Name = BoundNames.Contains(Widget) ? BoundNames.FindChecked(Widget) : Widget->GetName();
        FText Help;
        if (Name.StartsWith(TEXT("Grid")) && Name.EndsWith(TEXT("DirToggle"))) Help = LOCTEXT("Tip_GridSide", "Choose the positive or negative side of this building-grid axis. This changes placement direction, not copy count or belt flow.");
        else if (Name.StartsWith(TEXT("Grid"))) Help = LOCTEXT("Tip_Grid", "Number of copies on this Panel axis. The + / - control chooses the side. The Panel uses the building's grid axes, independently of Player Relative wheel controls.");
        else if (Name.StartsWith(TEXT("Spacing"))) Help = LOCTEXT("Tip_Spacing", "Additional spacing on this grid axis, in metres. The building's default placement interval is included separately.");
        else if (Name.StartsWith(TEXT("Steps"))) Help = LOCTEXT("Tip_Steps", "Raises or lowers successive copies on this horizontal axis, in metres.");
        else if (Name.StartsWith(TEXT("StaggerZ"))) Help = LOCTEXT("Tip_StackStagger", "Horizontal offset between successive vertical layers, in metres.");
        else if (Name.StartsWith(TEXT("Stagger"))) Help = LOCTEXT("Tip_Stagger", "Offsets neighbouring rows along the other horizontal grid axis, in metres.");
        else if (Name == TEXT("RotationZInput") || Name == TEXT("RotationAxisComboBox")) Help = LOCTEXT("Tip_GridRotation", "Progressive rotation between grid copies, in degrees. This bends the repeated layout; it is separate from native wall-support rotation.");
        else if (Name == TEXT("WallRotationInput")) Help = LOCTEXT("Tip_WallRotation", "Turns this pipe or Hypertube wall support in native 45-degree steps. The grid and Auto-Connect follow the support's run direction. Native placement restrictions still apply.");
        else if (Name == TEXT("ApplyImmediatelyCheckBox")) Help = LOCTEXT("Tip_Immediate", "When on, grid placement edits update the preview as you change them. When off, use Apply to commit them. Connection settings, production recipe, wall rotation and sign supports are live and are kept on close. Neither action constructs a building.");
        else if (Name == TEXT("ApplyBtn")) Help = LOCTEXT("Tip_Apply", "Commit the staged placement settings to the preview. Aim and build normally afterward; construction uses normal material costs.");
        else if (Name == TEXT("ResetBtn")) Help = LOCTEXT("Tip_ResetPlacement", "Reset placement values and update the preview immediately. This does not delete presets, reset the window position or reset HUD appearance.");
        else if (Name == TEXT("LiftUsageModeComboBox")) Help = LOCTEXT("LiftMode_Help", "Choose where the lift absorbs the height difference. Native placement and costs apply.");
        else if (Name.Contains(TEXT("RoutingMode"))) Help = LOCTEXT("Tip_Routing", "Choose the native route style for Auto-Connect previews. A style does not bypass length, slope, attachment or other placement restrictions.");
        else if (Name.StartsWith(TEXT("BeltTier")) || Name.StartsWith(TEXT("PipeTier"))) Help = LOCTEXT("Tip_ConduitTier", "Tier of new Auto-Connect routes. Auto uses the supported unlocked tier; this does not upgrade an existing network.");
        else if (Name == TEXT("BeltChainCheckBox")) Help = LOCTEXT("Tip_ChainDistributors", "Connect compatible distributors along the scaled line, using the selected tier and normal construction costs.");
        else if (Name == TEXT("StackableBeltDirectionComboBox")) Help = LOCTEXT("Tip_BeltFlow", "Choose the belt flow direction for these support connections. This is separate from which side of the grid is repeated.");
        else if (Name == TEXT("PipeIndicatorCheckBox")) Help = LOCTEXT("Tip_PipeIndicator", "Choose flow indicators for new pipes when the corresponding native pipe recipes are unlocked.");
        else if (Name == TEXT("BeltEnabledCheckBox") || Name == TEXT("PipeEnabledCheckBox") || Name == TEXT("HypertubeEnabledCheckBox") || Name == TEXT("PowerEnabledCheckBox")) Help = LOCTEXT("Tip_AutoConnect", "Include compatible new connections in the placement preview and material cost. Existing native connection and construction rules still apply.");
        else if (Name == TEXT("PowerGridAxisComboBox")) Help = LOCTEXT("Tip_PowerAxis", "Choose which grid direction to link with power cables. Native connection capacity and normal cable costs apply.");
        else if (Name == TEXT("PowerReservedComboBox")) Help = LOCTEXT("Tip_PowerReserved", "Leave this many power connections free for your other cables.");
        else if (Name == TEXT("ScaleDaisyChainPowerCheckBox")) Help = LOCTEXT("Tip_DaisyPower", "Link supported scaled buildings with power cables, preserving native connection capacity and normal costs.");
        else if (Name == TEXT("SignSupportModeButton")) Help = LOCTEXT("Tip_SignSupports", "Auto includes the native sign support and its cost. None uses the supported sign-only placement and removes the support cost.");
        else if (Name == TEXT("RecipeComboBox")) Help = LOCTEXT("Tip_ProductionRecipe", "Production recipe for the repeated buildings, separate from the building recipe held in the build gun. Native unlocks and compatibility apply.");
        else if (Name == TEXT("RadiusSliderSpinBox")) Help = LOCTEXT("Tip_UpgradeRadius", "Scan around your current position, in metres. Zero means the entire map. Scanning does not replace buildings or spend materials.");
        else if (Name == TEXT("RadiusScanButton")) Help = LOCTEXT("Tip_UpgradeScan", "Find supported buildings in the chosen area. Select a result and target tier, then review materials before Upgrade.");
        else if (Name == TEXT("EntireMapButton")) Help = LOCTEXT("Tip_UpgradeMap", "Set the area to the entire map and scan it. This can take longer than a bounded Radius scan.");
        else if (Name.Contains(TEXT("TargetTier"))) Help = LOCTEXT("Tip_TargetTier", "Tier to apply to the selected results. Logistics can change tiers in either direction where supported; other families only offer upgrades.");
        else if (Name == TEXT("SelectTargetButton")) Help = LOCTEXT("Tip_UpgradeTarget", "Arm one world click outside the window. Panel clicks stay in the interface; Escape cancels selection. A miss retains the previous target.");
        else if (Name == TEXT("TraversalScanButton")) Help = LOCTEXT("Tip_NetworkScan", "Scan from the displayed target using the crossing options. Changing the target requires a new scan before Upgrade.");
        else if (Name == TEXT("SharedUpgradeButton")) Help = LOCTEXT("Tip_UpgradeExecute", "Replace the selected scan results with the chosen target tier. This executes the upgrade and settles normal material costs and refunds; it is not a preview button.");
        else if (Name == TEXT("SharedCancelButton")) Help = LOCTEXT("Tip_CancelScan", "Cancel the pending area scan. This does not undo or cancel an executed upgrade.");
        else if (Name == TEXT("PreserveDismantleGroupCheckBox")) Help = LOCTEXT("Tip_UpgradeGroups", "Transfer each successfully replaced building's exact Smart group membership. Failed or skipped originals keep their objects and groups. Off skips Smart's transfer and leaves native behavior intact.");
        else if (Name.Contains(TEXT("CrossSplitter")) || Name.Contains(TEXT("CrossStorage")) || Name.Contains(TEXT("CrossTrain"))) Help = LOCTEXT("Tip_NetworkCrossing", "Allow the network scan to continue through this kind of connected building. This changes scan scope, not the buildings themselves.");
        else if (Name == TEXT("LoadToPanelBtn")) Help = LOCTEXT("Tip_LoadPreset", "Load this preset and return to Scaling to inspect its values. Apply Immediately controls whether the preview updates now or waits for Apply.");
        else if (Name == TEXT("ModuleApplyBtn")) Help = LOCTEXT("Tip_LoadModule", "Confirm a recipe change and start this saved layout as a scalable preview. Buildings are only constructed and charged when you build normally.");
        else if (Name == TEXT("ImportFromExtendBtn")) Help = LOCTEXT("Tip_SaveModule", "Save the current Extend clipboard under the entered name. This adds a library entry; it does not place the layout.");
        else if (Name == TEXT("SavePresetBtn")) Help = LOCTEXT("Tip_SavePreset", "Save the current Panel values as a named placement preset. Saving does not construct buildings.");
        else if (Name == TEXT("UpdatePresetBtn")) Help = LOCTEXT("Tip_UpdatePreset", "Replace the selected saved preset with the current Panel values after confirmation.");
        else if (Name.Contains(TEXT("DeletePreset")) || Name == TEXT("ModuleDeleteBtn")) Help = LOCTEXT("Tip_DeleteEntry", "Delete the selected library entry after confirmation. Buildings already in the world are unaffected.");
        else if (Name.Contains(TEXT("Export"))) Help = LOCTEXT("Tip_ExportEntry", "Copy the selected saved entry to the clipboard as a shareable code.");
        else if (Name.Contains(TEXT("Import"))) Help = LOCTEXT("Tip_ImportEntry", "Read a shared code from the clipboard and add it to the appropriate library. Importing does not build or apply the entry.");
        if (!Help.IsEmpty()) Widget->SetToolTipText(Help);
    });
}
#undef LOCTEXT_NAMESPACE
