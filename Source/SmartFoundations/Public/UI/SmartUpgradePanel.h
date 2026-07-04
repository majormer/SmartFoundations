// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "Services/SFChainActorService.h"
#include "SmartUpgradePanel.generated.h"

/** Which tab is currently active in the Upgrade Panel */
UENUM()
enum class ESmartUpgradeTab : uint8
{
	Radius,
	Traversal
	// [Track E] Triage tab removed - the chain-repair Detect/Repair tooling is gone (stop silent world-repair).
};

// Forward declarations
class USFSubsystem;

/**
 * Smart! Upgrade Panel Widget
 * Provides visual interface for Mass Upgrade feature
 */
UCLASS(BlueprintType, Blueprintable, Category = "SmartFoundations")
class SMARTFOUNDATIONS_API USmartUpgradePanel : public UFGInteractWidget
{
	GENERATED_BODY()

public:
	/** Close the panel and restore game input */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void ClosePanel();

protected:
	/** Handle keyboard input */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Handle mouse input for dragging */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Native construct to bind close button click */
	virtual void NativeConstruct() override;

	/** [UPGRADE-MP] Unbind the static traversal-result delegate */
	virtual void NativeDestruct() override;

	/** Refresh audit results */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void RefreshAudit();

	/** Cancel in-progress audit */
	UFUNCTION(BlueprintCallable, Category = "SmartFoundations|Upgrade")
	void CancelAudit();

protected:
	/** Background border that wraps all content */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UBorder> BackgroundBorder;

	/** Status text bound from Blueprint */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> StatusText;

	/** Context header text bound from Blueprint */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ContextHeaderText;

	/** Refresh button bound from Blueprint */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> RefreshButton;

	/** Cancel button bound from Blueprint */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> CancelButton;

	/** Header close button (the X in the panel header). SharedCloseButton is the bottom-row close. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> CloseButton;

	// ========== NEW TABBED UI WIDGETS ==========

	/** Tab buttons */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> RadiusTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> TraversalTabButton;

	// [Track E] The Triage tab's tooling/handlers were removed. The widgets remain in the Blueprint
	// (a clean editor delete left dangling variable GUIDs that fail cook), so we keep the tab
	// button + content bound here only to COLLAPSE them at construct - the tab is hidden, never wired.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> TriageTabButton;

	/** Tab content containers (visibility toggled) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> RadiusContent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> TraversalContent;

	// [Track E] Collapsed at construct (see TriageTabButton note); not wired to any handler.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> TriageContent;

	/** Radius tab widgets */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> RadiusSliderSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> RadiusScanButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> EntireMapButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> RadiusAuditResultsContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> RadiusTargetTierComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> RadiusCostDetailsText;

	/** Traversal tab widgets */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> TraversalAnchorText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> TraversalScanButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> TraversalResultsContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> TraversalTargetTierComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> TraversalCostDetailsText;

	/** Cross-attachment options */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UCheckBox> CrossSplittersCheckBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UCheckBox> CrossStorageCheckBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UCheckBox> CrossTrainPlatformsCheckBox;

	/** Shared bottom widgets */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> SharedStatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> SharedUpgradeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> SharedUpgradeButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> SharedCancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> SharedCloseButton;

	/** Update UI with audit results */
	UFUNCTION()
	void UpdateAuditUI(const FSFUpgradeAuditResult& Result);

	/** Update audit progress */
	UFUNCTION()
	void UpdateAuditProgress(float ProgressPercent, int32 ScannedCount);

	/** Handle row selection - find closest instance */
	void OnRowSelected(ESFUpgradeFamily Family, int32 Tier);

	/** Get cardinal direction string (N, NNE, NE, etc.) */
	static FString GetCardinalDirection(const FVector& FromLocation, const FVector& ToLocation);

	/** Handle upgrade progress updates */
	UFUNCTION()
	void OnUpgradeProgress(float ProgressPercent, int32 SuccessCount, int32 TotalCount);

	/** Handle upgrade completion */
	UFUNCTION()
	void OnUpgradeCompleted(const FSFUpgradeExecutionResult& Result);

private:
	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnRefreshButtonClicked();

	UFUNCTION()
	void OnCancelButtonClicked();

	/** Handle radius value change */
	UFUNCTION()
	void OnRadiusValueChanged(float NewValue);

	/** Handle "Entire Map" button click - scans all buildables with no radius limit */
	UFUNCTION()
	void OnEntireMapButtonClicked();

	/** Handle upgrade button click */
	UFUNCTION()
	void OnUpgradeButtonClicked();

	/** Handle target tier selection change */
	UFUNCTION()
	void OnTargetTierChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** Populate target tier dropdown based on selected family */
	void PopulateTargetTierDropdown();

	/** Tab switching */
	UFUNCTION()
	void OnRadiusTabClicked();

	UFUNCTION()
	void OnTraversalTabClicked();

	// [Track E] OnTriageTabClicked removed.

	/** Handle traversal scan button click */
	UFUNCTION()
	void OnTraversalScanClicked();

	// [Track E] OnTriageDetectClicked / OnTriageRepairClicked removed (chain-repair tooling gone).

	void SwitchToTab(ESmartUpgradeTab NewTab);

	/** Update traversal UI with results */
	void UpdateTraversalUI(const FSFTraversalResult& Result);

	/**
	 * [#456] Network-scan source-tier row clicked. Tier > 0 = upgrade ONLY that tier;
	 * Tier == 0 = the "All tiers below target" sweep row (legacy behavior, default selection).
	 * Lightweight counterpart to OnRowSelected - reads CachedTraversalResult, not CachedAuditResult.
	 */
	void OnTraversalRowSelected(int32 Tier);

	/** [#456] Run the network walk from a resolved anchor (config read + SP/MP dispatch). Shared by
	 *  the Scan button and the post-upgrade refresh. */
	void RunTraversalScanFromAnchor(AFGBuildable* AnchorBuildable);

	/** [#456] Re-run the network scan after an upgrade so the tier rows/costs reflect the new state.
	 *  Re-acquires the anchor (the old one may have been replaced by its own upgrade) from the still-
	 *  valid scan entries, else by proximity to the remembered anchor location.
	 *  @return true if a valid anchor was found and a scan was kicked; false if no seed was available. */
	bool RefreshTraversalScan();

	/** [#456] Client-side deferred refresh: on a network client the upgraded actors have not
	 *  replicated yet when the result RPC arrives, so re-acquiring an anchor immediately finds nothing
	 *  (the auto-refresh silently no-ops - MP field report). Retry on a short timer until a fresh
	 *  result lands (OnClientTraversalResult clears the timer) or the attempts are exhausted. */
	void BeginDeferredTraversalRefresh();

	/** [UPGRADE-MP] Server-walked traversal result arrived on this client */
	void OnClientTraversalResult(const FSFTraversalResult& Result);

	/** Current tab state */
	ESmartUpgradeTab ActiveTab = ESmartUpgradeTab::Radius;

	/** Traversal anchor buildable (set when player aims at something) */
	UPROPERTY()
	TWeakObjectPtr<AFGBuildable> TraversalAnchor;

	/** [#456] World location of the last traversal anchor - remembered so the post-upgrade refresh
	 *  can re-acquire a network seed even when the anchor belt was replaced by its own upgrade. */
	FVector TraversalAnchorLocation = FVector::ZeroVector;

	/** [#456] Deferred client-refresh retry timer + attempt counter (MP post-upgrade replication race). */
	FTimerHandle TraversalRefreshTimerHandle;
	int32 TraversalRefreshAttempts = 0;

	/** Cached traversal result */
	FSFTraversalResult CachedTraversalResult;

	/** Traversal configuration */
	FSFTraversalConfig TraversalConfig;

	/** Calculate and display estimated cost for current selection */
	void UpdateCostDisplay();

	/** Get the selected target tier from dropdown (returns 0 if none selected) */
	int32 GetSelectedTargetTier() const;

	/** Calculate upgrade costs for a family/tier combination
	 * @param Family - Upgrade family (Belt, Lift, Pipe, etc.)
	 * @param SourceTier - Current tier of items
	 * @param TargetTier - Target tier to upgrade to
	 * @param ItemCount - Number of items to upgrade
	 * @param OutNetCost - Output: Net cost (target cost - source refund)
	 * @return true if calculation succeeded
	 */
	bool CalculateUpgradeCost(ESFUpgradeFamily Family, int32 SourceTier, int32 TargetTier, int32 ItemCount, TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutNetCost) const;

	/** Current radius in meters for area-based operations */
	float CurrentRadiusMeters = 100.0f;

	/** Cached target tier for the currently selected family */
	int32 CachedTargetTier = 0;

	/** Dragging state */
	bool bIsDragging = false;
	FVector2D DragOffset;
	FVector2D InitialPanelPosition;

	/** Cached audit result for row selection */
	FSFUpgradeAuditResult CachedAuditResult;

	/** Currently selected row (for highlighting) */
	ESFUpgradeFamily SelectedFamily = ESFUpgradeFamily::None;
	int32 SelectedTier = 0;

	/** Row data for click detection */
	struct FRowData
	{
		ESFUpgradeFamily Family;
		int32 Tier;
		FString DisplayName;
	};
	TMap<UWidget*, FRowData> RowDataMap;
};
