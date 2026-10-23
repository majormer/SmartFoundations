// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SFInterfaceTaskStrip.h"
#include "UI/SFPanelStyle.h"
#include "UI/SFContextHelp.h"
#include "UI/SFFontLibrary.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Subsystem/SFSubsystem.h"
#include "Core/SFNativeHit.h"
#include "FGOutlineComponent.h"
#include "Buildables/FGBuildable.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
void USmartSettingsFormWidget::BuildExtendControls(UVerticalBox* Body)
{
    ExtendControls = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ExtendControls"));
    Body->AddChildToVerticalBox(ExtendControls)->SetPadding(FMargin(0,0,0,12));
    auto Label = [this](const FText& Text, const TCHAR* Name)
    {
        auto* Widget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Widget->SetFont(SFFont::Get(14)); Widget->SetColorAndOpacity(SFPanelStyle::LightText);
        Widget->SetText(Text); Widget->SetAutoWrapText(true); return Widget;
    };
    auto* Heading = Label(LOCTEXT("ExtendPanel_Heading", "Copy an existing factory module"), TEXT("ExtendHeading"));
    auto* Help = SFPanelStyle::MakeHelp(this, TEXT("ExtendHelp"), LOCTEXT("Help_ExtendTitle", "How to use Extend"),
        LOCTEXT("ExtendPanel_Summary", "Select an existing building of the same type as the one held in your build gun."),
        LOCTEXT("ExtendPanel_Help", "Select Source arms one world click outside the window; Escape cancels it. The source stays in place. Smart copies supported connected buildings, belts, pipes, lifts, supports and power. Use the placement controls below for copies, rows, layers and transforms. Normal placement rules and costs apply. Apply updates the preview; close the Panel and build normally. Hovering the same building with the matching recipe remains a shortcut. Restore > Modules can save the captured layout."));
    ExtendControls->AddChildToVerticalBox(SFPanelStyle::MakeHelpLabelRow(this, Heading, Help));
    ExtendControls->AddChildToVerticalBox(Help);
    ExtendSourceSummary = Label(FText::GetEmpty(), TEXT("ExtendSourceSummary"));
    ExtendControls->AddChildToVerticalBox(ExtendSourceSummary)->SetPadding(FMargin(0,8));
    auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* Select = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectExtendSourceButton"));
    ExtendSelectLabel = Label(LOCTEXT("ExtendPanel_Select", "Select Source"), TEXT("SelectExtendSourceLabel"));
    ExtendSelectLabel->SetAutoWrapText(false);
    Select->SetContent(ExtendSelectLabel); Select->SetStyle(SFPanelStyle::MakeButtonStyle(false));
    Select->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnSelectExtendSource);
    Buttons->AddChildToHorizontalBox(Select)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Direction = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExtendDirectionButton"));
    Direction->SetContent(Label(LOCTEXT("ExtendPanel_Direction", "Change Direction"), TEXT("ExtendDirectionLabel")));
    if (auto* Caption = Cast<UTextBlock>(Direction->GetContent())) Caption->SetAutoWrapText(false);
    Direction->SetStyle(SFPanelStyle::MakeButtonStyle(false));
    Direction->SetIsEnabled(false);
    Direction->OnClicked.AddDynamic(this, &USmartSettingsFormWidget::OnExtendDirection);
    Buttons->AddChildToHorizontalBox(Direction)->SetPadding(FMargin(8,0,0,0));
    ExtendControls->AddChildToVerticalBox(Buttons);
    ExtendSourceStatus = Label(LOCTEXT("ExtendPanel_PickHint", "Select a source, or close the Panel and aim at it with the matching building equipped."), TEXT("ExtendSourceStatus"));
    ExtendSourceStatus->SetColorAndOpacity(SFPanelStyle::DimText);
    ExtendControls->AddChildToVerticalBox(ExtendSourceStatus)->SetPadding(FMargin(0,8));
    ExtendControls->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        ExtendPickLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ExtendPickLayer"));
        ExtendPickLayer->SetBrushColor(FLinearColor::Transparent); ExtendPickLayer->SetCursor(EMouseCursor::Crosshairs);
        auto* LayerSlot = Canvas->AddChildToCanvas(ExtendPickLayer);
        LayerSlot->SetAnchors(FAnchors(0,0,1,1)); LayerSlot->SetOffsets(FMargin(0)); LayerSlot->SetZOrder(-10);
        ExtendPickLayer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void USmartSettingsFormWidget::ShowExtendPanel(bool bShow)
{
    if (bWaitingForConfirmation || HasPendingPresetLoad()) return;
    CancelExtendSourceSelection(); bShowingExtendPanel = bShow;
    if (ExtendControls) ExtendControls->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    const bool bRestore = TaskContentSwitcher && TaskContentSwitcher->GetActiveWidgetIndex() == 1;
    WidgetTree->ForEachWidget([bShow, bRestore](UWidget* Widget)
    {
        if (auto* Strip = Cast<USFInterfaceTaskStrip>(Widget)) Strip->SetActiveTask(bShow ? 4 : bRestore ? 3 : 0);
        if (Widget->GetFName() == TEXT("ScalingHelpButton")) Widget->SetVisibility(bShow || bRestore ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    });
    RefreshExtendPanel();
}

void USmartSettingsFormWidget::RefreshExtendPanel()
{
    if (!bShowingExtendPanel || !ExtendSourceSummary || !CachedSubsystem.IsValid()) return;
    auto* Service = CachedSubsystem->GetExtendService();
    auto* Source = Service ? Service->GetCurrentTarget() : nullptr;
    if (auto* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT("ExtendDirectionButton")))) Button->SetIsEnabled(Source != nullptr);
    if (!Source)
    {
        ExtendSourceSummary->SetText(LOCTEXT("ExtendPanel_NoSource", "No source selected. Equip the matching factory building, then select the existing module to copy."));
        return;
    }
    const auto Topology = Service->GetLastCloneTopology();
    const int32 Parts = Topology.IsValid() ? Topology->ChildHolograms.Num() : 0;
    ExtendSourceSummary->SetText(FText::Format(LOCTEXT("ExtendPanel_Source", "Source: {0}\nCaptured companions: {1}. Copies: {2}; rows: {3}; layers: {4}. The solid source stays in place; holograms are new construction."),
        Source->mDisplayName, FText::AsNumber(Parts), FText::AsNumber(Service->GetExtendCloneCount()),
        FText::AsNumber(Service->GetExtendRowCount()), FText::AsNumber(Service->GetExtendLayerCount())));
}

void USmartSettingsFormWidget::OnSelectExtendSource()
{
    if (bSelectingExtendSource) { CancelExtendSourceSelection(); return; }
    if (!bShowingExtendPanel || !ExtendPickLayer || bWaitingForConfirmation || HasPendingPresetLoad()) return;
    bSelectingExtendSource = true; ExtendPickLayer->SetVisibility(ESlateVisibility::Visible);
    ExtendSelectLabel->SetText(LOCTEXT("ExtendPanel_CancelSelect", "Cancel Selection"));
    ExtendSourceStatus->SetText(LOCTEXT("ExtendPanel_ClickSource", "Click one matching factory building in the world. Panel clicks stay in the interface. Escape cancels selection."));
}

void USmartSettingsFormWidget::CancelExtendSourceSelection()
{
    const bool bWasSelecting = bSelectingExtendSource;
    ClearExtendHover();
    bSelectingExtendSource = false;
    if (bWasSelecting && ExtendSourceStatus)
    {
        auto* Service = CachedSubsystem.IsValid() ? CachedSubsystem->GetExtendService() : nullptr;
        ExtendSourceStatus->SetText(Service && Service->IsExtendModeActive()
            ? LOCTEXT("ExtendPanel_Ready", "Source selected. Adjust copies, rows, layers and direction below. Close the Panel to inspect and build the preview at normal cost.")
            : LOCTEXT("ExtendPanel_PickHint", "Select a source, or close the Panel and aim at it with the matching building equipped."));
    }
    if (ExtendPickLayer) ExtendPickLayer->SetVisibility(ESlateVisibility::Collapsed);
    if (ExtendSelectLabel) ExtendSelectLabel->SetText(LOCTEXT("ExtendPanel_Select", "Select Source"));
}

FReply USmartSettingsFormWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    const FVector2D Position = Event.GetScreenSpacePosition();
    if (!bSelectingExtendSource || Event.GetEffectingButton() != EKeys::LeftMouseButton
        || !ExtendPickLayer || !ExtendPickLayer->GetCachedGeometry().IsUnderLocation(Position)
        || (BackgroundPanel && BackgroundPanel->GetCachedGeometry().IsUnderLocation(Position)))
        return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
    CancelExtendSourceSelection(); // Single shot, including a miss; never pass to gameplay.
    AFGBuildable* Source = ResolveExtendSource(Position);
    auto* Service = CachedSubsystem.IsValid() ? CachedSubsystem->GetExtendService() : nullptr;
    if (!Service || !Service->CanSelectPanelSource(Source, CachedSubsystem->GetActiveHologram()))
    {
        ExtendSourceStatus->SetText(LOCTEXT("ExtendPanel_Miss", "No compatible source selected. Hold the same building type and enable Extend in settings. Your previous preview is retained."));
        return FReply::Handled();
    }
    // Changing an existing source or a nontrivial placement is destructive only
    // after acceptance. The tab itself and a miss never clear or apply drafts.
    bool bHasTransform = !bGridXPositive || !bGridYPositive || !bGridZPositive;
    for (USpinBox* Input : {SpacingXInput.Get(), SpacingYInput.Get(), SpacingZInput.Get(), StepsXInput.Get(), StepsYInput.Get(), StaggerXInput.Get(), StaggerYInput.Get(), StaggerZXInput.Get(), StaggerZYInput.Get(), RotationZInput.Get()})
        bHasTransform |= Input && !FMath::IsNearlyZero(Input->GetValue());
    const bool bReplace = bHasTransform || Service->IsExtendModeActive() || (GridXInput && GridXInput->GetValue() > 1)
        || (GridYInput && GridYInput->GetValue() > 1) || (GridZInput && GridZInput->GetValue() > 1);
    if (bReplace && Service->GetCurrentTarget() != Source)
    {
        TWeakObjectPtr<AFGBuildable> WeakSource(Source);
        PendingConfirmCallback = [this, WeakSource]() { AcceptExtendSource(WeakSource.Get()); };
        ShowConfirmationDialog(LOCTEXT("ExtendPanel_ConfirmTitle", "Replace current preview?").ToString(),
            LOCTEXT("ExtendPanel_ConfirmBody", "Selecting this source replaces the current unbuilt placement and its staged edits. Existing buildings stay in place. Continue?").ToString(), SFPanelStyle::Accent);
    }
    else AcceptExtendSource(Source);
    return FReply::Handled();
}

void USmartSettingsFormWidget::AcceptExtendSource(AFGBuildable* Source)
{
    auto* Service = CachedSubsystem.IsValid() ? CachedSubsystem->GetExtendService() : nullptr;
    if (!Service || !Service->SelectPanelSource(Source, CachedSubsystem->GetActiveHologram()))
    {
        if (ExtendSourceStatus) ExtendSourceStatus->SetText(LOCTEXT("ExtendPanel_Stale", "The source is no longer available or cannot be extended. Select a valid matching source again."));
        return;
    }
    PopulateFromCounterState(CachedSubsystem.Get());
    PlacementContext = CachedSubsystem->GetActiveHologram();
    ShowExtendPanel(true);
    ExtendSourceStatus->SetText(LOCTEXT("ExtendPanel_Ready", "Source selected. Adjust copies, rows, layers and direction below. Close the Panel to inspect and build the preview at normal cost."));
}

void USmartSettingsFormWidget::OnExtendDirection()
{
    if (CachedSubsystem.IsValid()) if (auto* Service = CachedSubsystem->GetExtendService())
        if (Service->IsExtendModeActive()) { Service->CycleExtendDirection(1); RefreshExtendPanel(); }
}

AFGBuildable* USmartSettingsFormWidget::ResolveExtendSource(const FVector2D& Position)
{
    if (!GetWorld() || !ExtendPickLayer || !ExtendPickLayer->GetCachedGeometry().IsUnderLocation(Position)
        || (BackgroundPanel && BackgroundPanel->GetCachedGeometry().IsUnderLocation(Position))) return nullptr;
    FVector2D Pixel, Viewport; USlateBlueprintLibrary::AbsoluteToViewport(this, Position, Pixel, Viewport);
    FVector Start, Direction; AFGBuildable* Source = nullptr;
    if (GetOwningPlayer() && GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X, Pixel.Y, Start, Direction))
    {
        FCollisionQueryParams Query; Query.AddIgnoredActor(GetOwningPlayerPawn());
        if (CachedSubsystem.IsValid() && CachedSubsystem->GetActiveHologram()) Query.AddIgnoredActor(CachedSubsystem->GetActiveHologram());
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * 5000.f, ECC_WorldStatic, Query))
            Source = Cast<AFGBuildable>(SFNativeHit::ResolveActor(Hit));
    }
    return Source;
}

void USmartSettingsFormWidget::ClearExtendHover()
{
    if (auto* Source = ExtendHoveredSource.Get())
        if (auto* Outline = UFGOutlineComponent::Get(GetWorld()))
            if (Outline->GetOutlineStateColorForActor(Source) == EOutlineColor::OC_USABLE) Outline->HideOutline(Source);
    ExtendHoveredSource.Reset();
}

void USmartSettingsFormWidget::UpdateExtendHover(const FVector2D& Position)
{
    auto* Source = ResolveExtendSource(Position);
    auto* Service = CachedSubsystem.IsValid() ? CachedSubsystem->GetExtendService() : nullptr;
    if (!Service || !Service->CanSelectPanelSource(Source, CachedSubsystem->GetActiveHologram())) Source = nullptr;
    if (ExtendHoveredSource.Get() == Source) return;
    ClearExtendHover();
    if (!Source && ExtendSourceStatus)
        ExtendSourceStatus->SetText(LOCTEXT("ExtendPanel_ClickSource", "Click one matching factory building in the world. Panel clicks stay in the interface. Escape cancels selection."));
    if (Source)
    {
        ExtendSourceStatus->SetText(FText::Format(LOCTEXT("ExtendPanel_HoverSource", "Compatible source: {0}. Click to select; Escape cancels."), Source->mDisplayName));
        if (auto* Outline = UFGOutlineComponent::Get(GetWorld()))
            if (Outline->GetOutlineStateColorForActor(Source) == EOutlineColor::OC_NONE)
            { Outline->ShowOutline(Source, EOutlineColor::OC_USABLE); ExtendHoveredSource = Source; }
    }
}

#undef LOCTEXT_NAMESPACE
