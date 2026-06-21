// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "SFWalkPanelWidget.generated.h"

class USFSubsystem;
struct FSFWalkSegmentView;

/**
 * Smart Walking — dedicated Walk widget (#356, Slice 4).
 *
 * A segment list/timeline view of the Path being walked: one row per segment (index, the four authored
 * adjusters, and the derived exit heading), the active segment highlighted. Steer the active segment and
 * advance/back-up via the buttons; Cancel tears the preview down; Commit (Slice 3) builds the run.
 *
 * Lives at /SmartFoundations/SmartFoundations/UI/Smart_WalkPanel_Widget (parented to this class), opened
 * from the Smart Panel's "Walk Path" button. See docs/Sprints/CONCEPT_SmartWalking.md.
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USFWalkPanelWidget : public UFGInteractWidget
{
    GENERATED_BODY()

public:
    /** Rebuild the segment list + summary from the current walk state. */
    void Refresh();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SummaryText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> SegmentListBox;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> AdvanceButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BackUpButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TurnLeftButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TurnRightButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RaiseButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> LowerButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CommitButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

private:
    TWeakObjectPtr<USFSubsystem> CachedSubsystem;

    USFSubsystem* GetSubsystem();

    UFUNCTION() void OnAdvance();
    UFUNCTION() void OnBackUp();
    UFUNCTION() void OnTurnLeft();
    UFUNCTION() void OnTurnRight();
    UFUNCTION() void OnRaise();
    UFUNCTION() void OnLower();
    UFUNCTION() void OnCommit();
    UFUNCTION() void OnCancel();
    UFUNCTION() void OnClose();

    /** Build one segment-list row (index | Advance | Turn | Rise | Exit heading), highlighting the active. */
    UWidget* MakeSegmentRow(const FSFWalkSegmentView& View);

    /** A small text cell for a row. */
    UWidget* MakeCell(const FString& Text, const FLinearColor& Color);

    /** Tear down the widget (remove from viewport, restore input). */
    void CloseWidget();
};
