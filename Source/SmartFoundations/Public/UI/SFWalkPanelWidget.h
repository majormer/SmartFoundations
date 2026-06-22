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
class USFWalkPanelWidget;
class USpinBox;
class UHorizontalBox;

/**
 * Tiny per-cell binding for the editable table. A USpinBox's OnValueCommitted delegate carries only the new value,
 * not which cell fired — so each editable cell gets one of these (segment index + field id) bound to it, and it
 * forwards the edit to the panel. Field ids: 0 = Advance(m), 1 = Turn(deg), 2 = Rise(m), 3 = Shift(m).
 */
UCLASS()
class USFWalkCellBinding : public UObject
{
    GENERATED_BODY()

public:
    TWeakObjectPtr<USFWalkPanelWidget> Owner;
    int32 SegmentIndex = 0;
    int32 FieldId = 0;

    UFUNCTION() void OnCommitted(float NewValue, ETextCommit::Type CommitType);
};

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

    /** Apply one edited table cell to the walk: sets the field on segment Index and rebuilds downstream live.
     *  Called by USFWalkCellBinding::OnCommitted. FieldId: 0=Advance(m) 1=Turn(deg) 2=Rise(m) 3=Shift(m). */
    void ApplyCellEdit(int32 Index, int32 FieldId, float NewValue);

protected:
    virtual void NativeConstruct() override;

    /** Modal (UIOnly) panel blocks the game K toggle, so handle K/Escape here to hide it. */
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
    UFUNCTION() void OnApply();   // footer Apply — push any staged (typed) edits, then re-route + redisplay
    UFUNCTION() void OnApplyImmediatelyChanged(bool bIsChecked);  // toggle: live edits vs stage-then-Apply

    /** #356 interactive selectors — clicking cycles the walk's conveyance tier / routing mode forward: writes the
     *  live auto-connect setting (belt or pipe, per GetConveyanceType) then re-routes every span via RerouteSpans. */
    UFUNCTION() void OnConveyanceTierCycle();
    UFUNCTION() void OnRoutingCycle();

    /** Build one segment-list row (index | Advance | Turn | Rise | Exit heading), highlighting the active. */
    UWidget* MakeSegmentRow(const FSFWalkSegmentView& View);

    /** Build a "[Label   <Value>]" selector row; OutButton receives the clickable value button so Refresh can bind it. */
    UWidget* MakeSelectorRow(const FString& Label, const FString& Value, class UButton*& OutButton);

    /** A small text cell for a row (FontSize defaults to the dense table size). */
    UWidget* MakeCell(const FString& Text, const FLinearColor& Color, int32 FontSize = 10);

    /** The table header row (column titles), pinned above the scroll area. */
    UWidget* MakeHeaderRow();

    /** An editable numeric cell bound to (SegIndex, FieldId); registers a USFWalkCellBinding kept in CellBindings. */
    USpinBox* MakeEditCell(int32 SegIndex, int32 FieldId, float DisplayValue, float MinV, float MaxV, float Delta);

    /** Wrap Content in a fixed-width SizeBox and add it to Row, so the table columns line up across rows. */
    void AddFixedCell(UHorizontalBox* Row, UWidget* Content, float Width);

    /** Editable-cell bindings, recreated each Refresh (kept alive here so the SpinBox delegates stay valid). */
    UPROPERTY()
    TArray<TObjectPtr<USFWalkCellBinding>> CellBindings;

    /** When true, a committed cell applies + rebuilds live; when false, edits stage in PendingEdits until Apply. */
    bool bApplyImmediately = true;

    /** Staged cell edits (key = {segment index, field id}, value = the typed display value) when not applying live. */
    TMap<FIntPoint, float> PendingEdits;

    /** Tear down the widget (remove from viewport, restore input). */
    void CloseWidget();
};
