// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SFFontLibrary.h"
#include "UI/SFPanelStyle.h"
#include "UI/SFContextHelp.h"
#include "Blueprint/WidgetTree.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/SmartSettingsFormWidgetImpl.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/SFInterfaceTaskStrip.h"
#include "Components/WidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
void USmartSettingsFormWidget::BuildPlacementMatrix()
{
    if (!ContentContainer || !WidgetTree || GetWidgetFromName(TEXT("PlacementMatrix"))) return;
    UWidget* Header = GetWidgetFromName(TEXT("GridSectionHeader"));
    const int32 Index = Header ? ContentContainer->GetChildIndex(Header) + 1 : 0;
    if (auto* Text = Cast<UTextBlock>(Header)) Text->SetText(LOCTEXT("Panel_Placement", "Placement"));
    auto* Matrix = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("PlacementMatrix"));
    auto Cell = [Matrix](UWidget* Widget, int32 Row, int32 Column)
    {
        if (!Widget) return;
        Widget->RemoveFromParent();
        if (auto* CellSlot = Matrix->AddChildToGrid(Widget, Row, Column)) { CellSlot->SetPadding(FMargin(4, 3)); CellSlot->SetVerticalAlignment(VAlign_Center); }
    };
    auto Label = [this](FText Text, FName Name = NAME_None)
    {
        auto* Value = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name); Value->SetFont(SFFont::Get(14)); Value->SetText(Text); Value->SetColorAndOpacity(SFPanelStyle::LightText); return Value;
    };
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        auto* Text = Label(FText::FromString(Axis == 0 ? TEXT("X") : Axis == 1 ? TEXT("Y") : TEXT("Z")));
        Text->SetJustification(ETextJustify::Center); Cell(Text, 0, Axis + 1);
        Text->SetColorAndOpacity(Axis == 0 ? FLinearColor(1,.3f,.3f) : Axis == 1 ? FLinearColor(.4f,1,.4f) : FLinearColor(.5f,.6f,1));
    }
    const TArray<FText> Labels = {LOCTEXT("Panel_Grid", "Grid"), LOCTEXT("Panel_SpacingMetres", "Spacing (m)"), LOCTEXT("Panel_StepsMetres", "Steps (m)"), LOCTEXT("Panel_StaggerMetres", "Stagger (m)"), LOCTEXT("Panel_StackStaggerMetres", "Z stagger (m)")};
    const TArray<TArray<USizeBox*>> Rows = {{GridXInputSizeBox, GridYInputSizeBox, GridZInputSizeBox}, {SpacingXInputSizeBox, SpacingYInputSizeBox, SpacingZInputSizeBox}, {StepsXInputSizeBox, StepsYInputSizeBox, nullptr}, {StaggerXInputSizeBox, StaggerYInputSizeBox, nullptr}, {StaggerZXInputSizeBox, StaggerZYInputSizeBox, nullptr}};
    const TArray<UButton*> Directions = {GridXDirToggle, GridYDirToggle, GridZDirToggle};
    for (int32 Row = 0; Row < Rows.Num(); ++Row)
    {
        if (bIsExtendMode && Row >= 3) continue;
        Cell(Label(Labels[Row]), Row + 1, 0);
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            USizeBox* Box = Rows[Row][Axis];
            if (!Box) { Cell(Label(FText::FromString(TEXT("—"))), Row + 1, Axis + 1); continue; }
            Box->RemoveFromParent(); Box->SetWidthOverride(106);
            if (Row == 0)
            {
                auto* Group = WidgetTree->ConstructWidget<UHorizontalBox>();
                Group->AddChildToHorizontalBox(Box);
                if (Directions[Axis]) {Directions[Axis]->RemoveFromParent(); auto* DirectionSlot = Group->AddChildToHorizontalBox(Directions[Axis]); DirectionSlot->SetPadding(FMargin(4,0));}
                Cell(Group, Row + 1, Axis + 1);
            }
            else Cell(Box, Row + 1, Axis + 1);
        }
    }
    // Preserve the bound numeric widgets and their handlers; retire only their empty presentation rows.
    const TCHAR* OldRows[] = {TEXT("GridXRow"),TEXT("GridYRow"),TEXT("GridZRow"),TEXT("SpacingSectionHeader"),TEXT("SpacingXRow"),TEXT("SpacingYRow"),TEXT("SpacingZRow"),TEXT("StepsSectionHeader"),TEXT("StepsXRow"),TEXT("StepsYRow"),TEXT("StaggerSectionHeader"),TEXT("StaggerXRow"),TEXT("StaggerYRow"),TEXT("StaggerZXRow"),TEXT("StaggerZYRow")};
    for (const TCHAR* Name : OldRows) if (auto* Widget = GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
    // InsertChildAt does not update an already constructed Slate panel. Re-add the
    // same children in order so this also works after NativeConstruct has run.
    TArray<UWidget*> Ordered = ContentContainer->GetAllChildren();
    struct FLayout { FMargin Padding; FSlateChildSize Size; EHorizontalAlignment Horizontal; EVerticalAlignment Vertical; };
    TMap<UWidget*, FLayout> OriginalLayout;
    for (UWidget* Child : Ordered)
        if (auto* Original = Cast<UVerticalBoxSlot>(Child->Slot))
            OriginalLayout.Add(Child, {Original->GetPadding(), Original->GetSize(), Original->GetHorizontalAlignment(), Original->GetVerticalAlignment()});
    Ordered.Insert(Matrix, FMath::Clamp(Index, 0, Ordered.Num()));
    ContentContainer->ClearChildren();
    for (UWidget* Child : Ordered)
        if (auto* NewSlot = ContentContainer->AddChildToVerticalBox(Child))
            if (const auto* Original = OriginalLayout.Find(Child))
            {
                NewSlot->SetPadding(Original->Padding); NewSlot->SetSize(Original->Size);
                NewSlot->SetHorizontalAlignment(Original->Horizontal); NewSlot->SetVerticalAlignment(Original->Vertical);
            }
    // Keep family settings together; recipe and buildable-specific rows follow them.
    const TCHAR* ContextRows[] = {TEXT("RecipeHeaderText"),TEXT("RecipeControlsRow"),TEXT("RecipeDetailsContainer"),TEXT("WallRotationRow"),TEXT("SignSupportSectionHeader"),TEXT("SignSupportRow")};
    for (const TCHAR* Name : ContextRows)
        if (auto* Widget = GetWidgetFromName(Name); Widget && Widget->GetParent() == ContentContainer)
        {Widget->RemoveFromParent(); ContentContainer->AddChildToVerticalBox(Widget);}

    // Rebuild presentation around the bound controls. The old ScaleBox squeezed a
    // tall designer column into a narrow panel, including after the matrix was added.
    // Keep the native instances/delegates and use scrolling instead of shrinking text.
    UWidget* ImmediateRow = GetWidgetFromName(TEXT("ApplyImmediatelyRow"));
    if (ImmediateRow)
    {
        TArray<UWidget*> Children;
        WidgetTree->GetAllWidgets(Children);
        for (UWidget* Child : Children)
            if (auto* Text = Cast<UTextBlock>(Child); Text && Text->GetParent() == ImmediateRow)
                Text->SetText(LOCTEXT("Panel_ApplyPlacementImmediately", "Apply placement immediately:"));
    }
    UWidget* WallRow = GetWidgetFromName(TEXT("WallRotationRow"));
    UWidget* SignHeader = GetWidgetFromName(TEXT("SignSupportSectionHeader"));
    auto* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(660);
    const FVector2D View = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLocalSize();
    Width->SetHeightOverride(FMath::Max(480.f, float(View.Y - 120)));
    Width->SetContent(Root);
    BackgroundPanel->SetContent(Width);
    BackgroundPanel->SetPadding(FMargin(0));
    if (auto* LayoutSlot = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot)) LayoutSlot->SetAutoSize(true);
    auto Add = [](UVerticalBox* Parent, UWidget* Child, FMargin InPadding)
    {
        if (!Child) return;
        Child->RemoveFromParent();
        Parent->AddChildToVerticalBox(Child)->SetPadding(InPadding);
    };
    auto SectionRule = [this, &Add](UVerticalBox* Parent)
    {
        auto* Rule = WidgetTree->ConstructWidget<UBorder>();
        Rule->SetBrushColor(FLinearColor::FromSRGBColor(FColor(64,64,70)));
        Rule->SetPadding(FMargin(0));
        auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(1); Rule->SetContent(Height);
        Add(Parent, Rule, FMargin(0,12));
    };
    auto* Accent = WidgetTree->ConstructWidget<UBorder>();
    Accent->SetBrushColor(SFPanelStyle::Accent); Accent->SetPadding(FMargin(0));
    auto* AccentHeight = WidgetTree->ConstructWidget<USizeBox>(); AccentHeight->SetHeightOverride(2); Accent->SetContent(AccentHeight);
    Add(Root, Accent, FMargin(0));
    auto* WindowHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    TitleText->RemoveFromParent(); TitleText->SetText(LOCTEXT("Interface_Title", "Smart!")); TitleText->SetFont(SFFont::Get(22)); TitleText->SetColorAndOpacity(SFPanelStyle::Accent);
    auto* TitleSlot = WindowHeader->AddChildToHorizontalBox(TitleText); TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TitleSlot->SetVerticalAlignment(VAlign_Center);
    CloseButton->RemoveFromParent(); WindowHeader->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);
    // These three designer buttons were formerly drawn at half size.
    for (UButton* Button : {CloseButton.Get(),ResetBtn.Get(),ApplyBtn.Get()})
        if (Button) Button->SetRenderScale(FVector2D(1,1));
    auto* HeaderFrame = WidgetTree->ConstructWidget<UBorder>(); HeaderFrame->SetBrushColor(FLinearColor::FromSRGBColor(FColor(32,32,36))); HeaderFrame->SetPadding(FMargin(20,16,20,12)); HeaderFrame->SetContent(WindowHeader);
    Add(Root, HeaderFrame, FMargin(0));
    auto* Tasks = WidgetTree->ConstructWidget<USFInterfaceTaskStrip>(); Tasks->Initialize(CachedSubsystem.Get(), 0);
    Add(Root, Tasks, FMargin(20,8,20,12));
    auto* ScalingHelp = SFPanelStyle::MakeHelp(this, TEXT("ScalingHelp"),
        bIsExtendMode ? LOCTEXT("Help_ExtendTitle", "How to use Extend") : LOCTEXT("Help_ScalingTitle", "How to use Scaling"),
        LOCTEXT("Help_ScalingSummary", "Adjust the preview, then build normally. Panel axes stay X / Y / Z even with Player Relative controls."),
        bIsExtendMode ? LOCTEXT("Help_ExtendBody", "Extend repeats the captured group and its companions. X adds copies along the run; Y adds rows and Z adds layers. Spacing, Steps and progressive Rotation shape those copies. The source group stays in place and its wiring is preserved.\nApply updates placement without constructing. With Apply placement immediately off, grid edits wait for Apply and closing discards unapplied grid edits. Connection preferences are live and kept on close. Normal material costs and native placement restrictions apply.") : LOCTEXT("Help_ScalingBody", "Grid repeats the held building. Spacing changes the gap; Steps and Stagger offset the copies. Rotation bends the grid progressively; Wall rotation turns the native support itself.\nApply updates the preview without constructing. With Apply placement immediately off, grid edits wait for Apply; closing discards unapplied grid edits. Connection settings, production recipe, wall rotation and sign supports are live and kept on close. Normal building rules and material costs still apply."));
    if (CachedSubsystem.IsValid())
        if (auto* Hologram = CachedSubsystem->GetActiveHologram(); Hologram && Hologram->GetBuildClass())
        {
            auto* Subtitle = Label(Hologram->GetBuildClass()->GetDefaultObject<AFGBuildable>()->mDisplayName, TEXT("TaskBuildableSubtitle"));
            Subtitle->SetColorAndOpacity(SFPanelStyle::DimText); Add(Root, SFPanelStyle::MakeHelpLabelRow(this, Subtitle, ScalingHelp), FMargin(20,0,20,12));
        }
    auto* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    auto* BodyHeight = WidgetTree->ConstructWidget<USizeBox>();
    BodyHeight->SetMaxDesiredHeight(FMath::Clamp(float(View.Y - 265), 250.f, 950.f));
    TaskContentSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("TaskContentSwitcher"));
    TaskContentSwitcher->AddChild(Scroll); Scroll->AddChild(Body);
    if (RestoreSidePanel)
    {
        RestoreSidePanel->RemoveFromParent();
        RestoreSidePanel->SetPadding(FMargin(0));
        RestoreSidePanel->SetVisibility(ESlateVisibility::Visible);
        RestoreSidePanelSlot = nullptr;
        TaskContentSwitcher->AddChild(RestoreSidePanel);
    }
    TaskContentSwitcher->SetActiveWidgetIndex(0);
    BodyHeight->SetContent(TaskContentSwitcher);
    Add(Root, BodyHeight, FMargin(20,0,20,0));
    Cast<UVerticalBoxSlot>(BodyHeight->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BuildExtendControls(Body);
    Add(Body, ScalingHelp, FMargin(0,0,0,12));
    const TArray<UWidget*> BodyChildren = ContentContainer->GetAllChildren();
    for (UWidget* Child : BodyChildren)
    {
        if (Child == WalkPathButton || Child == RestoreSectionToggle || Child == ImmediateRow) continue;
        if (Child == AutoConnectHeaderText)
        {
            const bool bShowFamilyControls = Child->GetVisibility() != ESlateVisibility::Collapsed;
            Child->RemoveFromParent();
            FamilySectionHeading = SFPanelStyle::MakeSectionHeading(this, LOCTEXT("Panel_AutoConnect", "Auto-Connect"), LOCTEXT("Panel_FamilyScope", "Live settings"));
            FamilySectionHeading->SetVisibility(bShowFamilyControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            Add(Body, FamilySectionHeading, FMargin(0,16,0,8));
            continue;
        }
        if (Child == Header)
        {
            Child->RemoveFromParent();
            Add(Body, SFPanelStyle::MakeSectionHeading(this, LOCTEXT("Panel_Placement", "Placement"), LOCTEXT("Panel_Shared", "Shared controls")), FMargin(0,12,0,8));
            continue;
        }
        if (Child == WallRow)
        {
            WallSectionHeading = SFPanelStyle::MakeSectionHeading(this, LOCTEXT("Panel_WallRotation", "Wall rotation"), LOCTEXT("Panel_BuildableScope", "This buildable"));
            WallSectionHeading->SetVisibility(Child->GetVisibility());
            Add(Body, WallSectionHeading, FMargin(0,12,0,8));
        }
        if (Child->GetVisibility() != ESlateVisibility::Collapsed && (Child == AutoConnectHeaderText || Child == RecipeHeaderText || Child == WallRow || Child == SignHeader)) SectionRule(Body);
        Add(Body, Child, FMargin(0,4));
    }
    // Task navigation lives in the strip; the footer only commits placement edits.
    for (UButton* LegacyNavigation : {WalkPathButton.Get(), RestoreSectionToggle.Get()})
        if (LegacyNavigation) { LegacyNavigation->RemoveFromParent(); LegacyNavigation->SetVisibility(ESlateVisibility::Collapsed); }
    SectionRule(Root);
    PlacementFooterRule = Root->GetChildAt(Root->GetChildrenCount()-1);
    auto* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (auto* Immediate = ImmediateRow)
    {
        Immediate->RemoveFromParent(); auto* LayoutSlot = Footer->AddChildToHorizontalBox(Immediate); LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LayoutSlot->SetVerticalAlignment(VAlign_Center);
    }
    for (UButton* Button : {ResetBtn.Get(), ApplyBtn.Get()})
        if (Button) {Button->RemoveFromParent(); auto* LayoutSlot = Footer->AddChildToHorizontalBox(Button); LayoutSlot->SetPadding(FMargin(4,0)); LayoutSlot->SetVerticalAlignment(VAlign_Center);}
    Add(Root, Footer, FMargin(16,0,16,16));
    PlacementFooter = Footer;
    SFPanelStyle::ApplyToTree(WidgetTree);
    SFPanelStyle::ApplyControlHelp(WidgetTree);
    // Typography is presentation only: keep the selected multi-script family.
    WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        if (auto* Text = Cast<UTextBlock>(Widget)) {auto Font = Text->GetFont(); Font.Size = (Widget->GetName() == TEXT("TitleText") || Widget->GetName() == TEXT("RestorePresentationTitle")) ? 22 : Widget->GetName().StartsWith(TEXT("SectionTitle")) ? 16 : Widget->GetName().StartsWith(TEXT("SectionScope")) ? 12 : 14; Text->SetFont(Font);}
        if (auto* Spin = Cast<USpinBox>(Widget)) {Spin->SetFont(SFFont::Get(14)); Spin->SetMinDesiredWidth(80);}
    });
}
#undef LOCTEXT_NAMESPACE
