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
class UComboBoxString;

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
    UFUNCTION() void OnChanged(float NewValue);   // live (drag / arrow / wheel) — applies without rebuilding the table
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

    /** Live variant bound to USpinBox OnValueChanged (drag / arrow / wheel): applies the edit + updates the 3D preview
     *  via SetSegmentAtIndex, but does NOT rebuild the table (a full Refresh would destroy the spinbox being dragged). */
    void ApplyCellEditLive(int32 Index, int32 FieldId, float NewValue);

protected:
    virtual void NativeConstruct() override;

    /** Modal (UIOnly) panel blocks the game K toggle, so handle K/Escape here to hide it. */
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    /** Right-click-drag the panel to reposition it (render-translate the stable SegmentListBox; persists across refreshes). */
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    /** The panel's outer Border (dark backdrop) — set up in the Blueprint with the Smart Panel's brush, wrapping
     *  SegmentListBox. Drag-to-move render-translates THIS so the backdrop + content move together. */
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> WalkBackdrop;

    /** Content mount: Refresh() builds the UI (header, dropdowns, editable table, footer) into this VBox each rebuild.
     *  The old BP title/summary/steer-button widgets were removed — driven by scroll/keys + the table + a runtime "X". */
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> SegmentListBox;

private:
    TWeakObjectPtr<USFSubsystem> CachedSubsystem;

    USFSubsystem* GetSubsystem();

    UFUNCTION() void OnClose();
    UFUNCTION() void OnBackToScaling();   // footer "‹ Scaling" — exit walk + reopen the Smart Panel (grid scaling)
    UFUNCTION() void OnApply();   // footer Apply — push any staged (typed) edits, then re-route + redisplay
    UFUNCTION() void OnApplyImmediatelyChanged(bool bIsChecked);  // toggle: live edits vs stage-then-Apply

    /** #356 setting dropdowns — write the live auto-connect tier/direction/routing setting + RerouteSpans (per GetConveyanceType). */
    UFUNCTION() void OnTierComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnDirectionComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnRoutingComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnIndicatorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);   // pipe Clean/Normal style (unlock-gated)

    /** OnGenerateWidget for the setting combos — black SF-font item text (runtime combos can't InitFont, which is protected). */
    UFUNCTION() UWidget* MakeComboItemWidget(FString Item);

    /** Build one segment-list row (index | Advance | Turn | Rise | Exit heading), highlighting the active. */
    UWidget* MakeSegmentRow(const FSFWalkSegmentView& View);

    /** Build a "[Label  <dropdown>]" combo row; OutCombo receives the UComboBoxString so Refresh can bind it. */
    UWidget* MakeComboRow(const FText& Label, const TArray<FString>& Options, int32 SelectedIndex, TObjectPtr<class UComboBoxString>& OutCombo);

    /** A small text cell for a row (FontSize defaults to the dense table size). */
    UWidget* MakeCell(const FString& Text, const FLinearColor& Color, int32 FontSize = 10);

    /** Overload for already-localized text (LOCTEXT) — static labels/headers/buttons. */
    UWidget* MakeCell(const FText& Text, const FLinearColor& Color, int32 FontSize = 10);

    /** The table header row (column titles), pinned above the scroll area. */
    UWidget* MakeHeaderRow();

    /** An editable numeric cell bound to (SegIndex, FieldId); registers a USFWalkCellBinding kept in CellBindings. */
    USpinBox* MakeEditCell(int32 SegIndex, int32 FieldId, float DisplayValue, float MinV, float MaxV, float Delta);

    /** Wrap Content in a fixed-width SizeBox and add it to Row, so the table columns line up across rows. */
    void AddFixedCell(UHorizontalBox* Row, UWidget* Content, float Width);

    /** Editable-cell bindings, recreated each Refresh (kept alive here so the SpinBox delegates stay valid). */
    UPROPERTY()
    TArray<TObjectPtr<USFWalkCellBinding>> CellBindings;

    /** True only while MakeEditCell programmatically SetValue()s a spinbox, so the bound live OnValueChanged doesn't
     *  apply the initial value as if the user dragged it (mirrors the Smart Panel's programmatic-set guard). */
    bool bSuppressLiveEdit = false;

    /** #356 setting dropdowns (rebuilt each Refresh), wired to the live auto-connect settings. */
    UPROPERTY() TObjectPtr<UComboBoxString> TierCombo;
    UPROPERTY() TObjectPtr<UComboBoxString> DirectionCombo;
    UPROPERTY() TObjectPtr<UComboBoxString> RoutingCombo;
    UPROPERTY() TObjectPtr<UComboBoxString> IndicatorCombo;   // pipe-only "Pipe Style" (Normal/Clean); shown only when clean pipes are unlocked

    /** When true, a committed cell applies + rebuilds live; when false, edits stage in PendingEdits until Apply. */
    bool bApplyImmediately = true;

    /** Staged cell edits (key = {segment index, field id}, value = the typed display value) when not applying live. */
    TMap<FIntPoint, float> PendingEdits;

    /** Right-click drag-to-move state. PanelOffset is the accumulated render-translation applied to SegmentListBox. */
    FVector2D PanelOffset = FVector2D::ZeroVector;
    bool bDraggingPanel = false;
    FVector2D DragMouseStart = FVector2D::ZeroVector;
    FVector2D DragOffsetStart = FVector2D::ZeroVector;

    /** Tear down the widget (remove from viewport, restore input). */
    void CloseWidget();
};
