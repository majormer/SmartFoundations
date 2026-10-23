// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SmartUpgradePanelImpl.h"

void USmartUpgradePanel::CaptureMethodDraft()
{
    auto& Draft = MethodDrafts[ActiveTab == ESmartUpgradeTab::Radius ? 0 : 1];
    Draft.Family = SelectedFamily;
    Draft.SourceTier = SelectedTier;
    Draft.TargetTier = CachedTargetTier;
}

void USmartUpgradePanel::RestoreMethodDraft()
{
    const auto Draft = MethodDrafts[ActiveTab == ESmartUpgradeTab::Radius ? 0 : 1];
    bool bRowExists = false;
    for (const auto& Pair : RowDataMap)
        bRowExists |= Pair.Value.Family == Draft.Family && Pair.Value.Tier == Draft.SourceTier;
    if (!bRowExists || Draft.Family == ESFUpgradeFamily::None) return;
    if (ActiveTab == ESmartUpgradeTab::Radius) OnRowSelected(Draft.Family, Draft.SourceTier);
    else OnTraversalRowSelected(Draft.SourceTier);
    if (SelectedFamily != Draft.Family || SelectedTier != Draft.SourceTier) return;
    UComboBoxString* Combo = ActiveTab == ESmartUpgradeTab::Radius ? RadiusTargetTierComboBox.Get() : TraversalTargetTierComboBox.Get();
    const FString Option = FString::Printf(TEXT("Mk.%d"), Draft.TargetTier);
    // Never resurrect a target removed by native unlock/family policy.
    if (Combo && Combo->FindOptionIndex(Option) != INDEX_NONE)
    {
        Combo->SetSelectedOption(Option);
        CachedTargetTier = Draft.TargetTier;
        UpdateCostDisplay();
    }
}

void USmartUpgradePanel::SetRadiusScanPending(bool bPending)
{
    bRadiusScanPending = bPending;
    if (SharedCancelButton)
    {
        const bool bCanCancel = bPending && ActiveTab == ESmartUpgradeTab::Radius;
        SharedCancelButton->SetVisibility(bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        SharedCancelButton->SetIsEnabled(bCanCancel);
    }
}
