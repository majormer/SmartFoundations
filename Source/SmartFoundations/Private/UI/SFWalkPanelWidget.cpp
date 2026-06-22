// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SFWalkPanelWidget.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkTypes.h"
#include "UI/SFFontLibrary.h"  // SFFont::Get — small multi-script font for the dense table cells
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Engine/Texture2D.h"
#include "Components/SlateWrapperTypes.h"  // FSlateChildSize / ESlateSizeRule
#include "GameFramework/PlayerController.h"
#include "FGPlayerController.h"  // AFGPlayerController for GetHighestUnlocked*Tier (no transitive unity-build reliance)

// Display-name helpers for the conveyance tier + routing-mode selectors (mirror the auto-connect setter ranges).
static FString SFTierName(int32 Tier)
{
    return Tier <= 0 ? FString(TEXT("Auto")) : FString::Printf(TEXT("Mk%d"), Tier);
}

static FString SFRoutingName(bool bPipe, int32 Mode)
{
    if (bPipe)
    {
        // 0=Auto 1=Auto2D 2=Straight 3=Curve 4=Noodle 5=HorizontalToVertical (SetAutoConnectPipeRoutingMode range)
        static const TCHAR* const P[6] = { TEXT("Auto"), TEXT("Auto 2D"), TEXT("Straight"), TEXT("Curve"), TEXT("Noodle"), TEXT("H->V") };
        return FString(P[FMath::Clamp(Mode, 0, 5)]);
    }
    // 0=Default 1=Curve 2=Straight (SetAutoConnectBeltRoutingMode range)
    static const TCHAR* const B[3] = { TEXT("Default"), TEXT("Curve"), TEXT("Straight") };
    return FString(B[FMath::Clamp(Mode, 0, 2)]);
}

DEFINE_LOG_CATEGORY_STATIC(LogSmartWalkUI, Log, All);

void USFWalkPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("Smart Walking")));
    }

    if (AdvanceButton)   { AdvanceButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnAdvance); }
    if (BackUpButton)    { BackUpButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnBackUp); }
    if (TurnLeftButton)  { TurnLeftButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnTurnLeft); }
    if (TurnRightButton) { TurnRightButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnTurnRight); }
    if (RaiseButton)     { RaiseButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnRaise); }
    if (LowerButton)     { LowerButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnLower); }
    if (CommitButton)    { CommitButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnCommit); }
    if (CancelButton)    { CancelButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnCancel); }
    if (CloseButton)     { CloseButton->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnClose); }

    // #356 free-movement reframe: the steer buttons are redundant with the in-world scaling controls and the
    // maintainer asked to remove them ("not adding value"). The panel is now a pure segment-list overlay.
    // Collapse them in C++ so no BP re-cook is needed; bindings are left harmless (the buttons never show).
    auto Collapse = [](UWidget* W) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); } };
    Collapse(AdvanceButton);
    Collapse(BackUpButton);
    Collapse(TurnLeftButton);
    Collapse(TurnRightButton);
    Collapse(RaiseButton);
    Collapse(LowerButton);
    Collapse(CommitButton);
    Collapse(CancelButton);
    Collapse(CloseButton);
    // Title/summary are now rendered inside the runtime backdrop (Refresh), so hide the bare BP ones.
    Collapse(TitleText);
    Collapse(SummaryText);

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

USFSubsystem* USFWalkPanelWidget::GetSubsystem()
{
    if (!CachedSubsystem.IsValid())
    {
        CachedSubsystem = USFSubsystem::Get(GetWorld());
    }
    return CachedSubsystem.Get();
}

void USFWalkPanelWidget::OnAdvance()   { if (USFSubsystem* S = GetSubsystem()) { S->WalkAdvance(); } Refresh(); }
void USFWalkPanelWidget::OnBackUp()    { if (USFSubsystem* S = GetSubsystem()) { S->WalkBackUp(); } Refresh(); }
void USFWalkPanelWidget::OnTurnLeft()  { if (USFSubsystem* S = GetSubsystem()) { S->WalkNudgeActive(0.0f, -15.0f, 0.0f, 0.0f); } Refresh(); }
void USFWalkPanelWidget::OnTurnRight() { if (USFSubsystem* S = GetSubsystem()) { S->WalkNudgeActive(0.0f, 15.0f, 0.0f, 0.0f); } Refresh(); }
void USFWalkPanelWidget::OnRaise()     { if (USFSubsystem* S = GetSubsystem()) { S->WalkNudgeActive(0.0f, 0.0f, 100.0f, 0.0f); } Refresh(); }
void USFWalkPanelWidget::OnLower()     { if (USFSubsystem* S = GetSubsystem()) { S->WalkNudgeActive(0.0f, 0.0f, -100.0f, 0.0f); } Refresh(); }
void USFWalkPanelWidget::OnCommit()    { CloseWidget(); }   // done editing → close panel, back to in-world build (LMB)
void USFWalkPanelWidget::OnCancel()    { if (USFSubsystem* S = GetSubsystem()) { S->ExitWalkMode(); } CloseWidget(); }
void USFWalkPanelWidget::OnClose()     { CloseWidget(); }

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

    UBorder* Backdrop = NewObject<UBorder>(this);
    Backdrop->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.92f));
    Backdrop->SetPadding(FMargin(12.0f, 8.0f));
    Frame->SetContent(Backdrop);

    UVerticalBox* Col = NewObject<UVerticalBox>(this);
    Backdrop->SetContent(Col);

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
    }
    Col->AddChildToVerticalBox(HeaderRow);

    // Path summary (always visible above the scroll area).
    Col->AddChildToVerticalBox(MakeCell(
        FString::Printf(TEXT("%d segment%s  ·  head %s %.0f deg  ·  %s %s"),
            Views.Num(), (Views.Num() == 1) ? TEXT("") : TEXT("s"),
            *SFHeadingToCompass16(HeadDeg), HeadDeg,
            bPipe ? TEXT("Pipe") : TEXT("Belt"), *SFTierName(Tier)),
        FLinearColor(0.886f, 0.498f, 0.118f, 1.0f), 11));

    // Pinned interactive selector rows (cursor mode, click to cycle forward) — apply live to the walk's spans.
    UButton* ConvBtn = nullptr;
    if (UWidget* ConvRow = MakeSelectorRow(
            TEXT("Conveyance"),
            FString::Printf(TEXT("%s %s"), bPipe ? TEXT("Pipe") : TEXT("Belt"), *SFTierName(Tier)),
            ConvBtn))
    {
        Col->AddChildToVerticalBox(ConvRow);
        if (ConvBtn) { ConvBtn->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnConveyanceTierCycle); }
    }
    UButton* RouteBtn = nullptr;
    if (UWidget* RouteRow = MakeSelectorRow(TEXT("Routing"), SFRoutingName(bPipe, RouteMode), RouteBtn))
    {
        Col->AddChildToVerticalBox(RouteRow);
        if (RouteBtn) { RouteBtn->OnClicked.AddDynamic(this, &USFWalkPanelWidget::OnRoutingCycle); }
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

    // Footer — just Apply + an "apply immediately" toggle. (Build is always LMB; cancel = holster the build gun.)
    UHorizontalBox* Footer = NewObject<UHorizontalBox>(this);

    UButton* ApplyBtn = NewObject<UButton>(this);
    UTextBlock* ApplyText = NewObject<UTextBlock>(this);
    ApplyText->SetText(FText::FromString(TEXT("Apply")));
    ApplyText->SetFont(SFFont::Get(11));
    ApplyText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.6f, 0.0f, 1.0f)));   // brand orange
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
            MakeCell(TEXT("apply immediately"), FLinearColor(0.9f, 0.9f, 0.9f, 1.0f), 10)))
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

UWidget* USFWalkPanelWidget::MakeSegmentRow(const FSFWalkSegmentView& View)
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    const FLinearColor Col = View.bActive ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

    // Fixed-width columns so every row lines up with the header. Advance/Rise/Shift are edited in METERS (the view
    // stores cm); ApplyCellEdit converts back. Turn is degrees.
    AddFixedCell(Row, MakeCell(FString::Printf(TEXT("%s%d"), View.bActive ? TEXT(">") : TEXT(""), View.Index), Col), 34.0f);
    AddFixedCell(Row, MakeEditCell(View.Index, 0, View.Advance / 100.0f, 0.0f, 500.0f, 1.0f), 84.0f);
    AddFixedCell(Row, MakeEditCell(View.Index, 1, View.TurnDegrees, -180.0f, 180.0f, 5.0f), 66.0f);
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
    const FLinearColor H(1.0f, 0.6f, 0.2f, 1.0f);   // header orange
    AddFixedCell(Row, MakeCell(TEXT("#"), H), 34.0f);
    AddFixedCell(Row, MakeCell(TEXT("Advance m"), H), 84.0f);
    AddFixedCell(Row, MakeCell(TEXT("Turn"), H), 66.0f);
    AddFixedCell(Row, MakeCell(TEXT("Rise m"), H), 66.0f);
    AddFixedCell(Row, MakeCell(TEXT("Shift m"), H), 66.0f);
    AddFixedCell(Row, MakeCell(TEXT("Exit"), H), 110.0f);
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
    Spin->SetValue(DisplayValue);
    Spin->SetFont(SFFont::Get(10));   // match the dense table cells

    USFWalkCellBinding* Binding = NewObject<USFWalkCellBinding>(this);
    Binding->Owner = this;
    Binding->SegmentIndex = SegIndex;
    Binding->FieldId = FieldId;
    Spin->OnValueCommitted.AddDynamic(Binding, &USFWalkCellBinding::OnCommitted);
    CellBindings.Add(Binding);
    return Spin;
}

void USFWalkPanelWidget::ApplyCellEdit(int32 Index, int32 FieldId, float NewValue)
{
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

    W->SetSegmentAtIndex(Index, Adv, Turn, Rise, Shift);   // rebuilds the 3D preview downstream
    Refresh();                                             // refresh the derived Exit headings in the table
}

void USFWalkCellBinding::OnCommitted(float NewValue, ETextCommit::Type CommitType)
{
    if (Owner.IsValid())
    {
        Owner->ApplyCellEdit(SegmentIndex, FieldId, NewValue);
    }
}

UWidget* USFWalkPanelWidget::MakeSelectorRow(const FString& Label, const FString& Value, UButton*& OutButton)
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);

    // Label cell (header orange).
    if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeCell(Label, FLinearColor(1.0f, 0.6f, 0.2f, 1.0f))))
    {
        LabelSlot->SetPadding(FMargin(4.0f, 2.0f));
    }

    // Clickable value (gold) — click cycles the option forward; Refresh rebinds it each rebuild.
    UButton* Btn = NewObject<UButton>(this);
    UTextBlock* ValText = NewObject<UTextBlock>(this);
    ValText->SetText(FText::FromString(Value));
    ValText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)));
    ValText->SetFont(SFFont::Get(11));
    Btn->AddChild(ValText);
    if (UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(Btn))
    {
        BtnSlot->SetPadding(FMargin(4.0f, 2.0f));
    }
    OutButton = Btn;
    return Row;
}

void USFWalkPanelWidget::OnConveyanceTierCycle()
{
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W) { return; }

    const auto& AC = S->GetAutoConnectRuntimeSettings();
    const bool bPipe = W->GetConveyanceType() == ESFWalkConveyanceType::Pipe;
    AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());

    if (bPipe)
    {
        const int32 Max = S->GetHighestUnlockedPipeTier(PC);
        int32 Next = AC.PipeTierMain + 1;
        if (Next > Max) { Next = 0; }   // wrap back through 0 = Auto
        S->SetAutoConnectPipeTierMain(Next);
    }
    else
    {
        const int32 Max = S->GetHighestUnlockedBeltTier(PC);
        int32 Next = AC.BeltTierMain + 1;
        if (Next > Max) { Next = 0; }
        S->SetAutoConnectBeltTierMain(Next);
    }

    W->RerouteSpans();   // re-route every span with the new tier (path/frames unchanged)
    Refresh();
}

void USFWalkPanelWidget::OnRoutingCycle()
{
    USFSubsystem* S = GetSubsystem();
    USFWalkService* W = S ? S->GetWalkService() : nullptr;
    if (!S || !W) { return; }

    const auto& AC = S->GetAutoConnectRuntimeSettings();
    const bool bPipe = W->GetConveyanceType() == ESFWalkConveyanceType::Pipe;
    if (bPipe)
    {
        S->SetAutoConnectPipeRoutingMode((AC.PipeRoutingMode + 1) % 6);
    }
    else
    {
        S->SetAutoConnectBeltRoutingMode((AC.BeltRoutingMode + 1) % 3);
    }

    W->RerouteSpans();   // re-route every span with the new routing mode
    Refresh();
}
