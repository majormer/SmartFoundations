// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SFWalkPanelWidget.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkTypes.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerController.h"

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

    Refresh();
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
void USFWalkPanelWidget::OnCommit()    { UE_LOG(LogSmartWalkUI, Log, TEXT("[Walk] Commit not yet implemented (Slice 3).")); }
void USFWalkPanelWidget::OnCancel()    { if (USFSubsystem* S = GetSubsystem()) { S->ExitWalkMode(); } CloseWidget(); }
void USFWalkPanelWidget::OnClose()     { CloseWidget(); }

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

    if (SummaryText)
    {
        const float HeadDeg = Views.Num() > 0 ? Views.Last().ExitHeadingDeg : 0.0f;
        SummaryText->SetText(FText::FromString(FString::Printf(
            TEXT("%d segment%s  ·  head %.0f deg"),
            Views.Num(), (Views.Num() == 1) ? TEXT("") : TEXT("s"), HeadDeg)));
    }

    if (SegmentListBox)
    {
        SegmentListBox->ClearChildren();
        for (const FSFWalkSegmentView& V : Views)
        {
            if (UWidget* Row = MakeSegmentRow(V))
            {
                SegmentListBox->AddChildToVerticalBox(Row);
            }
        }
    }
}

UWidget* USFWalkPanelWidget::MakeCell(const FString& Text, const FLinearColor& Color)
{
    UTextBlock* Cell = NewObject<UTextBlock>(this);
    Cell->SetText(FText::FromString(Text));
    Cell->SetColorAndOpacity(FSlateColor(Color));
    return Cell;
}

UWidget* USFWalkPanelWidget::MakeSegmentRow(const FSFWalkSegmentView& View)
{
    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    const FLinearColor Col = View.bActive ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

    auto AddCell = [this, Row, &Col](const FString& Text)
    {
        if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(MakeCell(Text, Col)))
        {
            Slot->SetPadding(FMargin(4.0f, 2.0f));
        }
    };

    AddCell(FString::Printf(TEXT("%s#%d"), View.bActive ? TEXT(">") : TEXT(""), View.Index));
    AddCell(FString::Printf(TEXT("Adv %.1fm"), View.Advance / 100.0f));
    AddCell(View.TurnDegrees != 0.0f ? FString::Printf(TEXT("Turn %.0f"), View.TurnDegrees) : TEXT("-"));
    AddCell(View.Rise != 0.0f ? FString::Printf(TEXT("Rise %.1fm"), View.Rise / 100.0f) : TEXT("-"));
    AddCell(FString::Printf(TEXT("Exit %.0f deg"), View.ExitHeadingDeg));

    return Row;
}
