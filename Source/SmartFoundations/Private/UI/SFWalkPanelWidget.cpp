// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SFWalkPanelWidget.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkTypes.h"
#include "UI/SFFontLibrary.h"  // SFFont::Get — small multi-script font for the dense table cells
#include "Blueprint/WidgetLayoutLibrary.h"  // GetViewportScale for DPI-correct drag
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Styling/SlateTypes.h"  // FTableRowStyle for the combo item style
#include "Styling/SlateBrush.h"  // ESlateBrushDrawType
#include "Engine/Texture2D.h"
#include "Components/SlateWrapperTypes.h"  // FSlateChildSize / ESlateSizeRule
#include "GameFramework/PlayerController.h"
#include "FGPlayerController.h"  // AFGPlayerController for GetHighestUnlocked*Tier (no transitive unity-build reliance)

#define LOCTEXT_NAMESPACE "SmartFoundations"   // walk-panel UI strings gather into the same namespace as the rest of the mod

// Dark rounded button style matching the Smart Panel's buttons (CloseButton/Apply/Reset, captured via AdaMCP
// describe_widget). Without it, a built UButton falls back to the default LIGHT-GREY Slate style, which is what
// made the gold "X" / labels read poorly. Values are the Smart Panel CloseButton's exact WidgetStyle brushes.
static FButtonStyle SFPanelButtonStyle()
{
    auto MakeBrush = [](const FLinearColor& Fill)
    {
        FSlateBrush B;
        B.TintColor = FSlateColor(Fill);
        B.DrawAs = ESlateBrushDrawType::RoundedBox;
        B.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
        B.OutlineSettings.Color = FSlateColor(FLinearColor(0.695f, 0.695f, 0.695f, 1.0f));
        B.OutlineSettings.Width = 1.0f;
        B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        return B;
    };
    FButtonStyle Style;
    Style.SetNormal(MakeBrush(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f)));
    Style.SetHovered(MakeBrush(FLinearColor(0.25f, 0.25f, 0.30f, 1.0f)));
    Style.SetPressed(MakeBrush(FLinearColor(0.10f, 0.10f, 0.12f, 1.0f)));
    Style.SetNormalPadding(FMargin(12.0f, 1.5f, 12.0f, 1.5f));
    Style.SetPressedPadding(FMargin(12.0f, 1.5f, 12.0f, 1.5f));
    return Style;
}

// Display-name helpers for the conveyance tier + routing-mode selectors (mirror the auto-connect setter ranges).
static FString SFTierName(int32 Tier)
{
    // "Auto" is localized; "Mk%d" is an untranslated model designation (kept as-is, matching the Smart Panel).
    return Tier <= 0 ? LOCTEXT("Walk_Opt_Auto", "Auto").ToString() : FString::Printf(TEXT("Mk%d"), Tier);
}

static FString SFRoutingName(bool bPipe, int32 Mode)
{
    if (bPipe)
    {
        // 0=Auto 1=Auto2D 2=Straight 3=Curve 4=Noodle 5=HorizontalToVertical (SetAutoConnectPipeRoutingMode range)
        switch (FMath::Clamp(Mode, 0, 5))
        {
        case 1:  return LOCTEXT("Walk_Opt_Auto2D",   "Auto 2D").ToString();
        case 2:  return LOCTEXT("Walk_Opt_Straight", "Straight").ToString();
        case 3:  return LOCTEXT("Walk_Opt_Curve",    "Curve").ToString();
        case 4:  return LOCTEXT("Walk_Opt_Noodle",   "Noodle").ToString();
        case 5:  return LOCTEXT("Walk_Opt_HToV",     "H->V").ToString();
        default: return LOCTEXT("Walk_Opt_Auto",     "Auto").ToString();
        }
    }
    // 0=Default 1=Curve 2=Straight (SetAutoConnectBeltRoutingMode range)
    switch (FMath::Clamp(Mode, 0, 2))
    {
    case 1:  return LOCTEXT("Walk_Opt_Curve",    "Curve").ToString();
    case 2:  return LOCTEXT("Walk_Opt_Straight", "Straight").ToString();
    default: return LOCTEXT("Walk_Opt_Default",  "Default").ToString();
    }
}

DEFINE_LOG_CATEGORY_STATIC(LogSmartWalkUI, Log, All);

void USFWalkPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // The entire panel — frame, backdrop, header, Tier/Routing/Direction dropdowns, editable segment table and
    // footer — is built in Refresh() into the BindWidget SegmentListBox; the BP is just that mount point. (The old
    // BP title/summary/steer-button widgets were removed; the runtime "X" button binds OnClose itself, and the
    // segment cells + dropdowns wire up in Refresh().)
    Refresh();
}

FReply USFWalkPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    // Panel is modal (UIOnly) while up, so the game K toggle can't fire — hide on K (toggle) or Escape.
    if (Key == EKeys::K || Key == EKeys::Escape)
    {
        if (USFSubsystem* S = GetSubsystem())
        {
            S->ToggleWalkPanel();   // hide → back to steer
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USFWalkPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        bDraggingPanel = true;
        DragMouseStart = InMouseEvent.GetScreenSpacePosition();
        DragOffsetStart = PanelOffset;
        return FReply::Handled().CaptureMouse(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USFWalkPanelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel)
    {
        const float DPI = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
        PanelOffset = DragOffsetStart + (InMouseEvent.GetScreenSpacePosition() - DragMouseStart) / DPI;
        // Translate the outer WalkBackdrop Border (the BP backdrop that wraps the content) so the brush AND the
        // content move together. Both are stable widgets (not rebuilt), so the offset persists across Refresh.
        if (WalkBackdrop)
        {
            WalkBackdrop->SetRenderTranslation(PanelOffset);
        }
        else if (SegmentListBox)
        {
            SegmentListBox->SetRenderTranslation(PanelOffset);
        }
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USFWalkPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        bDraggingPanel = false;
        // Return keyboard focus to the panel so K/Escape work again (the drag's mouse-capture stole it — that's why
        // closing only worked after clicking a button).
        return FReply::Handled().ReleaseMouseCapture().SetUserFocus(TakeWidget(), EFocusCause::SetDirectly);
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

USFSubsystem* USFWalkPanelWidget::GetSubsystem()
{
    if (!CachedSubsystem.IsValid())
    {
        CachedSubsystem = USFSubsystem::Get(GetWorld());
    }
    return CachedSubsystem.Get();
}

void USFWalkPanelWidget::OnClose()     { if (USFSubsystem* S = GetSubsystem()) { S->ToggleWalkPanel(); } }  // hide (keeps drag pos); walk stays active

void USFWalkPanelWidget::OnBackToScaling()
{
    // Explicit exit from walk back to grid scaling: end the walk (this also tears down THIS panel), then reopen the
    // Smart Panel. ExitWalkMode must run FIRST — OnToggleSettingsForm re-routes to the walk panel while a walk is
    // active, so clearing the walk first lets it open the Smart Panel instead. Don't touch `this`/members after.
    USFSubsystem* S = GetSubsystem();
    if (!S) { return; }
    S->ExitWalkMode();
    S->OnToggleSettingsForm();
}

void USFWalkPanelWidget::OnApply()
{
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!W) { return; }

    // Push any staged (typed-but-not-live) edits: per affected segment, overlay its staged fields on its current
    // values and apply once.
    if (PendingEdits.Num() > 0)
    {
        const TArray<FSFWalkSegmentView> Views = W->GetSegmentViews();
        TSet<int32> Affected;
        for (const TPair<FIntPoint, float>& P : PendingEdits) { Affected.Add(P.Key.X); }
        for (int32 SegIdx : Affected)
        {
            if (!Views.IsValidIndex(SegIdx)) { continue; }
            const FSFWalkSegmentView& V = Views[SegIdx];
            float Adv = V.Advance, Turn = V.TurnDegrees, Rise = V.Rise, Shift = V.Shift;   // cm + deg
            if (const float* P = PendingEdits.Find(FIntPoint(SegIdx, 0))) { Adv  = (*P) * 100.0f; }
            if (const float* P = PendingEdits.Find(FIntPoint(SegIdx, 1))) { Turn = *P; }
            if (const float* P = PendingEdits.Find(FIntPoint(SegIdx, 2))) { Rise = (*P) * 100.0f; }
            if (const float* P = PendingEdits.Find(FIntPoint(SegIdx, 3))) { Shift = (*P) * 100.0f; }
            W->SetSegmentAtIndex(SegIdx, Adv, Turn, Rise, Shift);
        }
        PendingEdits.Empty();
    }

    W->RerouteSpans();   // re-route every span with the current settings/edits
    Refresh();
}

void USFWalkPanelWidget::OnApplyImmediatelyChanged(bool bIsChecked)
{
    bApplyImmediately = bIsChecked;
    if (bApplyImmediately && PendingEdits.Num() > 0)
    {
        OnApply();   // switching to live → flush whatever was staged
    }
}

void USFWalkPanelWidget::CloseWidget()
{
    // CRITICAL: restore game input + hide the cursor, or closing the panel leaves the player captured in
    // UI-only input with no widget = total loss of control (force-close). Reverse what OpenWalkPanel set.
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
    RemoveFromParent();
}

void USFWalkPanelWidget::Refresh()
{
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!W)
    {
        return;
    }

    const TArray<FSFWalkSegmentView> Views = W->GetSegmentViews();
    const bool bPipe = W->GetConveyanceType() == ESFWalkConveyanceType::Pipe;
    const auto& AC = S->GetAutoConnectRuntimeSettings();
    const int32 Tier = bPipe ? AC.PipeTierMain : AC.BeltTierMain;
    const int32 RouteMode = bPipe ? AC.PipeRoutingMode : AC.BeltRoutingMode;
    const float HeadDeg = Views.Num() > 0 ? Views.Last().ExitHeadingDeg : 0.0f;

    if (!SegmentListBox)
    {
        return;
    }
    SegmentListBox->ClearChildren();
    CellBindings.Reset();   // drop last frame's cell bindings; the rebuilt SpinBoxes register fresh ones

    // Fixed-size, scrollable panel. The BP places SegmentListBox in an unbounded top-left region, so the list used to
    // grow until it ate the screen. Constrain it to a SizeBox (fixed dimensions) + dark Border backdrop; keep the
    // header + selectors pinned; and put the (unbounded) segment list in a ScrollBox so it SCROLLS instead of growing.
    USizeBox* Frame = NewObject<USizeBox>(this);
    Frame->SetWidthOverride(500.0f);
    Frame->SetHeightOverride(600.0f);
    if (UVerticalBoxSlot* FrameSlot = SegmentListBox->AddChildToVerticalBox(Frame))
    {
        // The VBox slot defaults to Fill, which stretches the SizeBox to full width/height and defeats the size
        // overrides (that was the full-screen backdrop). Pin it top-left so the 440x560 frame is actually respected.
        FrameSlot->SetHorizontalAlignment(HAlign_Left);
        FrameSlot->SetVerticalAlignment(VAlign_Top);
    }

    // The dark canvas background is a designer-set Border in the Blueprint now (WalkBackdrop, wrapping SegmentListBox).
    // A runtime UBorder::SetBrush does NOT reliably push the tint/alpha to the live SBorder (it kept rendering
    // near-transparent or wrong-coloured), whereas a BP-serialized brush renders correctly — exactly like the Smart
    // Panel's BackgroundPanel. So the C++ just builds the content column; the BP provides the matching dark canvas.
    UVerticalBox* Col = NewObject<UVerticalBox>(this);
    Frame->SetContent(Col);

    // Branded header: SMART! logo + "Smart! Walking" title — matches the Smart Panel's header.
    UHorizontalBox* HeaderRow = NewObject<UHorizontalBox>(this);
    if (UTexture2D* Logo = LoadObject<UTexture2D>(nullptr, TEXT("/SmartFoundations/SmartFoundations/UI/Smart-Logo.Smart-Logo")))
    {
        UImage* LogoImg = NewObject<UImage>(this);
        LogoImg->SetBrushFromTexture(Logo, false);
        LogoImg->SetDesiredSizeOverride(FVector2D(22.0f, 22.0f));
        if (UHorizontalBoxSlot* LogoSlot = HeaderRow->AddChildToHorizontalBox(LogoImg))
        {
            LogoSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
            LogoSlot->SetVerticalAlignment(VAlign_Center);
        }
    }
    if (UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(
            MakeCell(TEXT("Smart! Walking"), FLinearColor(1.0f, 0.6f, 0.2f, 1.0f), 14)))
    {
        TitleSlot->SetVerticalAlignment(VAlign_Center);
        TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));   // push the close button to the right edge
    }
    // Close (X) — a reliable click-to-close (K/Escape can miss when focus is on a cell or just after a drag).
    UButton* CloseBtn = NewObject<UButton>(this);
    CloseBtn->SetStyle(SFPanelButtonStyle());   // dark rounded fill like the Smart Panel — gold X on dark, never on light grey
    UTextBlock* CloseX = NewObject<UTextBlock>(this);
    CloseX->SetText(FText::FromString(TEXT("X")));
    CloseX->SetFont(SFFont::Get(13));
    CloseX->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)));   // gold @13 — readable on the dark button
    CloseBtn->AddChild(CloseX);
    CloseBtn->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnClose);
    if (UHorizontalBoxSlot* CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseBtn))
    {
        CloseSlot->SetVerticalAlignment(VAlign_Center);
    }
    Col->AddChildToVerticalBox(HeaderRow);

    // Path summary (always visible above the scroll area). Fully localized via FText::Format: the count drives an ICU
    // plural (segment/segments), "head"/"deg" are part of the gathered format, the compass stays untranslated, and the
    // conveyance word is its own LOCTEXT. (Tier now lives in the Tier dropdown, so the old trailing slot is gone.)
    {
        FFormatOrderedArguments SummaryArgs;
        SummaryArgs.Add(Views.Num());                                       // {0} segment count + plural selector
        SummaryArgs.Add(FText::FromString(SFHeadingToCompass16(HeadDeg)));  // {1} compass (intentionally untranslated)
        SummaryArgs.Add(FMath::RoundToInt(HeadDeg));                        // {2} head heading, degrees
        SummaryArgs.Add(bPipe ? LOCTEXT("Walk_Conveyance_Pipe", "Pipe")
                              : LOCTEXT("Walk_Conveyance_Belt", "Belt"));   // {3} conveyance word
        const FText SummaryText = FText::Format(
            LOCTEXT("Walk_Summary", "{0} {0}|plural(one=segment,other=segments)  ·  head {1} {2} deg  ·  {3}"),
            SummaryArgs);
        Col->AddChildToVerticalBox(MakeCell(SummaryText, FLinearColor(0.9f, 0.9f, 0.9f, 1.0f), 11));   // white summary — match the Smart Panel
    }

    // Pinned setting dropdowns (mirror the Smart Panel) — apply live to the walk's spans via the auto-connect setters.
    {
        AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
        const int32 MaxTier = bPipe ? S->GetHighestUnlockedPipeTier(PC) : S->GetHighestUnlockedBeltTier(PC);
        TArray<FString> TierOpts;
        for (int32 t = 0; t <= MaxTier; ++t) { TierOpts.Add(SFTierName(t)); }
        if (UWidget* TierRow = MakeComboRow(LOCTEXT("Walk_Lbl_Tier", "Tier"), TierOpts, FMath::Clamp(Tier, 0, MaxTier), TierCombo))
        {
            Col->AddChildToVerticalBox(TierRow);
            if (TierCombo) { TierCombo->OnSelectionChanged.AddDynamic(this, &USFWalkPanelWidget::OnTierComboChanged); }
        }

        const int32 RouteMax = bPipe ? 5 : 2;
        TArray<FString> RouteOpts;
        for (int32 r = 0; r <= RouteMax; ++r) { RouteOpts.Add(SFRoutingName(bPipe, r)); }
        if (UWidget* RouteRow = MakeComboRow(LOCTEXT("Walk_Lbl_Routing", "Routing"), RouteOpts, FMath::Clamp(RouteMode, 0, RouteMax), RoutingCombo))
        {
            Col->AddChildToVerticalBox(RouteRow);
            if (RoutingCombo) { RoutingCombo->OnSelectionChanged.AddDynamic(this, &USFWalkPanelWidget::OnRoutingComboChanged); }
        }

        // Pipe Style (Normal/Clean) — pipe-only, and ONLY when clean pipes are unlocked in-game (a research unlock).
        // "Clean" = no flow indicators (the Build_Pipeline_NoIndicator class); it is a pipe CLASS, not a routing mode.
        // Mirrors the Smart Panel's Flow Indicator control, which gates on the same AreCleanPipesUnlocked predicate.
        if (bPipe && PC && S->AreCleanPipesUnlocked(PC))
        {
            TArray<FString> StyleOpts;
            StyleOpts.Add(LOCTEXT("Walk_Opt_Normal", "Normal").ToString());   // index 0 = with flow indicators (bPipeIndicator = true)
            StyleOpts.Add(LOCTEXT("Walk_Opt_Clean",  "Clean").ToString());    // index 1 = no indicators       (bPipeIndicator = false)
            if (UWidget* StyleRow = MakeComboRow(LOCTEXT("Walk_Lbl_PipeStyle", "Pipe Style"), StyleOpts, AC.bPipeIndicator ? 0 : 1, IndicatorCombo))
            {
                Col->AddChildToVerticalBox(StyleRow);
                if (IndicatorCombo) { IndicatorCombo->OnSelectionChanged.AddDynamic(this, &USFWalkPanelWidget::OnIndicatorComboChanged); }
            }
        }

        if (!bPipe)   // belt direction only (pipes have no direction)
        {
            TArray<FString> DirOpts;
            DirOpts.Add(LOCTEXT("Walk_Opt_Forward",  "Forward").ToString());
            DirOpts.Add(LOCTEXT("Walk_Opt_Backward", "Backward").ToString());
            if (UWidget* DirRow = MakeComboRow(LOCTEXT("Walk_Lbl_Direction", "Direction"), DirOpts, FMath::Clamp(AC.StackableBeltDirection, 0, 1), DirectionCombo))
            {
                Col->AddChildToVerticalBox(DirRow);
                if (DirectionCombo) { DirectionCombo->OnSelectionChanged.AddDynamic(this, &USFWalkPanelWidget::OnDirectionComboChanged); }
            }
        }
    }

    // Pinned column header for the editable table.
    Col->AddChildToVerticalBox(MakeHeaderRow());

    // Scrollable editable segment table — one row per segment, each value an editable cell. Fills the remaining height.
    UScrollBox* SegmentScroll = NewObject<UScrollBox>(this);
    if (UVerticalBoxSlot* ScrollSlot = Col->AddChildToVerticalBox(SegmentScroll))
    {
        ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    for (const FSFWalkSegmentView& V : Views)
    {
        if (UWidget* Row = MakeSegmentRow(V))
        {
            SegmentScroll->AddChild(Row);
        }
    }

    // Footer — "‹ Scaling" (back to the Smart Panel) + Apply + an "apply immediately" toggle. (Build is always LMB;
    // cancel = holster the build gun.)
    UHorizontalBox* Footer = NewObject<UHorizontalBox>(this);

    // Back to scaling: exit the walk and reopen the Smart Panel (grid scaling) — the explicit way out of walk mode.
    UButton* ScalingBtn = NewObject<UButton>(this);
    ScalingBtn->SetStyle(SFPanelButtonStyle());
    UTextBlock* ScalingText = NewObject<UTextBlock>(this);
    ScalingText->SetText(LOCTEXT("Walk_Btn_Scaling", "‹ Scaling"));   // ‹ = back chevron (glyph kept inside the translated unit)
    ScalingText->SetFont(SFFont::Get(11));
    ScalingText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)));   // gold on dark, like Apply/X
    ScalingBtn->AddChild(ScalingText);
    ScalingBtn->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnBackToScaling);
    if (UHorizontalBoxSlot* ScalingSlot = Footer->AddChildToHorizontalBox(ScalingBtn))
    {
        ScalingSlot->SetPadding(FMargin(4.0f, 4.0f));
        ScalingSlot->SetVerticalAlignment(VAlign_Center);
    }

    UButton* ApplyBtn = NewObject<UButton>(this);
    ApplyBtn->SetStyle(SFPanelButtonStyle());   // dark rounded fill like the Smart Panel buttons
    UTextBlock* ApplyText = NewObject<UTextBlock>(this);
    ApplyText->SetText(LOCTEXT("Panel_Btn_Apply", "Apply"));   // reuse the Smart Panel's existing Apply key
    ApplyText->SetFont(SFFont::Get(11));
    ApplyText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)));   // gold on dark (matches the Smart Panel Apply/Reset)
    ApplyBtn->AddChild(ApplyText);
    ApplyBtn->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnApply);
    if (UHorizontalBoxSlot* ApplySlot = Footer->AddChildToHorizontalBox(ApplyBtn))
    {
        ApplySlot->SetPadding(FMargin(4.0f, 4.0f));
        ApplySlot->SetVerticalAlignment(VAlign_Center);
    }

    UCheckBox* ImmCheck = NewObject<UCheckBox>(this);
    ImmCheck->SetIsChecked(bApplyImmediately);
    ImmCheck->OnCheckStateChanged.AddDynamic(this, &USFWalkPanelWidget::OnApplyImmediatelyChanged);
    if (UHorizontalBoxSlot* CheckSlot = Footer->AddChildToHorizontalBox(ImmCheck))
    {
        CheckSlot->SetPadding(FMargin(12.0f, 4.0f, 3.0f, 4.0f));
        CheckSlot->SetVerticalAlignment(VAlign_Center);
    }
    if (UHorizontalBoxSlot* LblSlot = Footer->AddChildToHorizontalBox(
            MakeCell(LOCTEXT("Walk_ApplyImmediately", "apply immediately"), FLinearColor(0.9f, 0.9f, 0.9f, 1.0f), 10)))
    {
        LblSlot->SetVerticalAlignment(VAlign_Center);
    }

    Col->AddChildToVerticalBox(Footer);
}

UWidget* USFWalkPanelWidget::MakeCell(const FString& Text, const FLinearColor& Color, int32 FontSize)
{
    UTextBlock* Cell = NewObject<UTextBlock>(this);
    Cell->SetText(FText::FromString(Text));
    Cell->SetColorAndOpacity(FSlateColor(Color));
    Cell->SetFont(SFFont::Get(FontSize));   // shrink from the default ~24pt so the dense columns don't collide
    return Cell;
}

UWidget* USFWalkPanelWidget::MakeCell(const FText& Text, const FLinearColor& Color, int32 FontSize)
{
    UTextBlock* Cell = NewObject<UTextBlock>(this);
    Cell->SetText(Text);   // already-localized FText (LOCTEXT) — not flattened through FromString
    Cell->SetColorAndOpacity(FSlateColor(Color));
    Cell->SetFont(SFFont::Get(FontSize));
    return Cell;
}

UWidget* USFWalkPanelWidget::MakeSegmentRow(const FSFWalkSegmentView& View)
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    const FLinearColor Col(0.9f, 0.9f, 0.9f, 1.0f);   // white # / Exit cells (match the headers); the active segment is marked by the ">" prefix

    // Fixed-width columns so every row lines up with the header. Advance/Rise/Shift are edited in METERS (the view
    // stores cm); ApplyCellEdit converts back. Turn is degrees.
    AddFixedCell(Row, MakeCell(FString::Printf(TEXT("%s%d"), View.bActive ? TEXT(">") : TEXT(""), View.Index), Col), 34.0f);
    AddFixedCell(Row, MakeEditCell(View.Index, 0, View.Advance / 100.0f, 1.0f, 54.0f, 1.0f), 84.0f);   // Advance 1–54m: min 1m (no 0-length span), max 54m (belt/pipe safe max under the 56m spline limit); longer = add a segment
    AddFixedCell(Row, MakeEditCell(View.Index, 1, View.TurnDegrees, -270.0f, 270.0f, 5.0f), 66.0f);   // up to ±270° — a wide loop that arcs back into itself
    AddFixedCell(Row, MakeEditCell(View.Index, 2, View.Rise / 100.0f, -200.0f, 200.0f, 1.0f), 66.0f);
    AddFixedCell(Row, MakeEditCell(View.Index, 3, View.Shift / 100.0f, -200.0f, 200.0f, 1.0f), 66.0f);
    AddFixedCell(Row, MakeCell(FString::Printf(TEXT("%s %.0f"), *SFHeadingToCompass16(View.ExitHeadingDeg), View.ExitHeadingDeg), Col), 110.0f);

    return Row;
}

void USFWalkPanelWidget::AddFixedCell(UHorizontalBox* Row, UWidget* Content, float Width)
{
    if (!Row || !Content) { return; }
    USizeBox* Box = NewObject<USizeBox>(this);
    Box->SetWidthOverride(Width);
    Box->SetContent(Content);
    if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Box))
    {
        CellSlot->SetPadding(FMargin(3.0f, 2.0f));
        CellSlot->SetVerticalAlignment(VAlign_Center);
    }
}

UWidget* USFWalkPanelWidget::MakeHeaderRow()
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    const FLinearColor H(0.9f, 0.9f, 0.9f, 1.0f);   // white column headers — match the Smart Panel's light setting labels
    AddFixedCell(Row, MakeCell(TEXT("#"), H), 34.0f);   // glyph — kept untranslated
    AddFixedCell(Row, MakeCell(LOCTEXT("Walk_Col_Advance", "Advance m"), H), 84.0f);
    AddFixedCell(Row, MakeCell(LOCTEXT("Walk_Col_Turn",    "Turn"),      H), 66.0f);
    AddFixedCell(Row, MakeCell(LOCTEXT("Walk_Col_Rise",    "Rise m"),    H), 66.0f);
    AddFixedCell(Row, MakeCell(LOCTEXT("Walk_Col_Shift",   "Shift m"),   H), 66.0f);
    AddFixedCell(Row, MakeCell(LOCTEXT("Walk_Col_Exit",    "Exit"),      H), 110.0f);
    return Row;
}

USpinBox* USFWalkPanelWidget::MakeEditCell(int32 SegIndex, int32 FieldId, float DisplayValue, float MinV, float MaxV, float Delta)
{
    USpinBox* Spin = NewObject<USpinBox>(this);
    Spin->SetMinValue(MinV);
    Spin->SetMaxValue(MaxV);
    Spin->SetMinSliderValue(MinV);
    Spin->SetMaxSliderValue(MaxV);
    Spin->SetDelta(Delta);
    Spin->SetMinFractionalDigits(0);
    Spin->SetMaxFractionalDigits(1);
    Spin->SetFont(SFFont::Get(10));   // match the dense table cells
    Spin->SetForegroundColor(FSlateColor(FLinearColor::Black));   // setter pushes to the live Slate widget (matches Smart Panel)

    USFWalkCellBinding* Binding = NewObject<USFWalkCellBinding>(this);
    Binding->Owner = this;
    Binding->SegmentIndex = SegIndex;
    Binding->FieldId = FieldId;
    Spin->OnValueCommitted.AddDynamic(Binding, &USFWalkCellBinding::OnCommitted);   // commit (focus-loss / Enter) → apply + full table refresh
    Spin->OnValueChanged.AddDynamic(Binding, &USFWalkCellBinding::OnChanged);       // live (drag / arrow / wheel) → apply, no table rebuild
    CellBindings.Add(Binding);

    // Set the initial value LAST, under the suppress guard: SetValue() fires OnValueChanged, and without the guard each
    // cell's initial set on every Refresh would apply as though the user dragged it (mirrors the Smart Panel guard).
    bSuppressLiveEdit = true;
    Spin->SetValue(DisplayValue);
    bSuppressLiveEdit = false;
    return Spin;
}

void USFWalkPanelWidget::ApplyCellEditLive(int32 Index, int32 FieldId, float NewValue)
{
    // Live path (USpinBox OnValueChanged: drag / arrow / wheel). MUST NOT call Refresh() — that does
    // SegmentListBox->ClearChildren() and would destroy the very spinbox the user is dragging.
    if (bSuppressLiveEdit) { return; }   // the programmatic SetValue in MakeEditCell, not a user edit

    if (!bApplyImmediately)
    {
        // Not live — stage it (the SpinBox already shows the typed value); the Apply button will push all staged edits.
        PendingEdits.Add(FIntPoint(Index, FieldId), NewValue);
        return;
    }

    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!W) { return; }

    const TArray<FSFWalkSegmentView> Views = W->GetSegmentViews();
    if (!Views.IsValidIndex(Index)) { return; }
    const FSFWalkSegmentView& V = Views[Index];

    float Adv = V.Advance, Turn = V.TurnDegrees, Rise = V.Rise, Shift = V.Shift;   // cm + deg
    switch (FieldId)
    {
    case 0: Adv = NewValue * 100.0f; break;    // Advance m -> cm
    case 1: Turn = NewValue; break;             // Turn deg
    case 2: Rise = NewValue * 100.0f; break;    // Rise m -> cm
    case 3: Shift = NewValue * 100.0f; break;   // Shift m -> cm
    default: break;
    }

    W->SetSegmentAtIndex(Index, Adv, Turn, Rise, Shift);   // RepositionFrom updates the live 3D preview — NO Refresh() here
}

void USFWalkPanelWidget::ApplyCellEdit(int32 Index, int32 FieldId, float NewValue)
{
    // Commit path (USpinBox OnValueCommitted: focus-loss / Enter). Apply via the shared live path, then do the FULL
    // table rebuild — safe here because focus has left the spinbox — to finalize the derived Exit-heading column.
    ApplyCellEditLive(Index, FieldId, NewValue);
    if (bApplyImmediately) { Refresh(); }
}

void USFWalkCellBinding::OnCommitted(float NewValue, ETextCommit::Type CommitType)
{
    if (Owner.IsValid())
    {
        Owner->ApplyCellEdit(SegmentIndex, FieldId, NewValue);
    }
}

void USFWalkCellBinding::OnChanged(float NewValue)
{
    if (Owner.IsValid())
    {
        Owner->ApplyCellEditLive(SegmentIndex, FieldId, NewValue);
    }
}

UWidget* USFWalkPanelWidget::MakeComboRow(const FText& Label, const TArray<FString>& Options, int32 SelectedIndex, TObjectPtr<UComboBoxString>& OutCombo)
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);

    // Label cell (header orange).
    if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeCell(Label, FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))))
    {
        LabelSlot->SetPadding(FMargin(4.0f, 2.0f));
    }

    UComboBoxString* Combo = NewObject<UComboBoxString>(this);
    // Style the dropdown LIST items via OnGenerateWidget (each item = a black SF-font TextBlock); the CLOSED box's
    // selected text is styled by Combo->Font / Combo->ForegroundColor below (both public UPROPERTYs).
    Combo->OnGenerateWidgetEvent.BindDynamic(this, &USFWalkPanelWidget::MakeComboItemWidget);
    // Standard Box item brushes (mirror the Smart Panel's ConfigureComboBoxStyle).
    FTableRowStyle ItemStyle = Combo->GetItemStyle();
    ItemStyle.ActiveBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.ActiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.InactiveBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.InactiveHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.EvenRowBackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.EvenRowBackgroundHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.OddRowBackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
    ItemStyle.OddRowBackgroundHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
    Combo->SetItemStyle(ItemStyle);

    // Closed-box selected text: black on the light combo box — matches the Smart Panel combos (which set
    // ForegroundColor=black + a bold font). Font/ForegroundColor are public UPROPERTYs; assign them before the
    // underlying widget constructs (when the panel is shown) so they take effect.
    Combo->Font = SFFont::Get(11);
    Combo->ForegroundColor = FSlateColor(FLinearColor::Black);

    for (const FString& Opt : Options) { Combo->AddOption(Opt); }
    if (Options.IsValidIndex(SelectedIndex)) { Combo->SetSelectedIndex(SelectedIndex); }

    if (UHorizontalBoxSlot* ComboSlot = Row->AddChildToHorizontalBox(Combo))
    {
        ComboSlot->SetPadding(FMargin(4.0f, 2.0f));
    }
    OutCombo = Combo;
    return Row;
}

UWidget* USFWalkPanelWidget::MakeComboItemWidget(FString Item)
{
    UTextBlock* T = NewObject<UTextBlock>(this);
    T->SetText(FText::FromString(Item));
    T->SetFont(SFFont::Get(9));   // compact, closer to the Smart Panel combos
    T->SetColorAndOpacity(FSlateColor(FLinearColor::Black));   // readable on the grey combo
    return T;
}

void USFWalkPanelWidget::OnTierComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct) { return; }   // ignore the programmatic SetSelectedIndex during Refresh
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W || !TierCombo) { return; }
    const int32 Idx = TierCombo->GetSelectedIndex();   // index == tier value (0=Auto, 1=Mk1, ...)
    if (W->GetConveyanceType() == ESFWalkConveyanceType::Pipe) { S->SetAutoConnectPipeTierMain(Idx); }
    else { S->SetAutoConnectBeltTierMain(Idx); }
    W->RecreateSpans();   // recreate so the new tier's belt class shows in the preview (the update path keeps the old class)
}

void USFWalkPanelWidget::OnDirectionComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct) { return; }
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W || !DirectionCombo) { return; }
    S->SetAutoConnectStackableBeltDirection(DirectionCombo->GetSelectedIndex());   // 0=Forward, 1=Backward
    W->RerouteSpans();
}

void USFWalkPanelWidget::OnRoutingComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct) { return; }
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W || !RoutingCombo) { return; }
    const int32 Idx = RoutingCombo->GetSelectedIndex();
    if (W->GetConveyanceType() == ESFWalkConveyanceType::Pipe) { S->SetAutoConnectPipeRoutingMode(Idx); }
    else { S->SetAutoConnectBeltRoutingMode(Idx); }
    W->RerouteSpans();
}

void USFWalkPanelWidget::OnIndicatorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct) { return; }   // ignore the programmatic SetSelectedIndex during Refresh
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W || !IndicatorCombo) { return; }
    S->SetAutoConnectPipeIndicator(IndicatorCombo->GetSelectedIndex() == 0);   // 0=Normal (with indicators), 1=Clean (none)
    W->RecreateSpans();   // indicator changes the pipe CLASS (Build_Pipeline vs _NoIndicator) — recreate, like the tier change
}

#undef LOCTEXT_NAMESPACE
