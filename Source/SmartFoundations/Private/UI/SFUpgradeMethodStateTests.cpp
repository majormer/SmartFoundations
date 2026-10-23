// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartUpgradePanel.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ComboBoxString.h"
#include "Buildables/FGBuildableConveyorBelt.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUpgradeMethodStateTest,
    "SmartFoundations.Interface.UpgradeMethodState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFUpgradeMethodStateTest::RunTest(const FString& Parameters)
{
    auto* Panel = NewObject<USmartUpgradePanel>();
    Panel->bMethodInitialized = true;
    Panel->SharedStatusText = NewObject<UTextBlock>(Panel);
    Panel->StatusText = NewObject<UTextBlock>(Panel);
    Panel->SharedCancelButton = NewObject<UButton>(Panel);
    Panel->SelectedFamily = ESFUpgradeFamily::Pipe;
    Panel->SelectedTier = 1;
    Panel->CachedTargetTier = 2;
    Panel->StatusText->SetText(FText::FromString(TEXT("Radius selection")));
    Panel->SwitchToTab(ESmartUpgradeTab::Radius);
    TestEqual(TEXT("Reclicking active method retains source tier"), Panel->SelectedTier, 1);
    TestEqual(TEXT("Reclicking active method retains target tier"), Panel->CachedTargetTier, 2);
    Panel->SetRadiusScanPending(true);
    TestTrue(TEXT("Radius exposes actual scan cancellation"), Panel->SharedCancelButton->GetVisibility() == ESlateVisibility::Visible);
    Panel->SwitchToTab(ESmartUpgradeTab::Traversal);
    TestEqual(TEXT("Radius draft stored independently"), Panel->MethodDrafts[0].TargetTier, 2);
    TestTrue(TEXT("Network does not expose Radius cancellation"), Panel->SharedCancelButton->GetVisibility() == ESlateVisibility::Collapsed);
    TestTrue(TEXT("Wrong-method detail cleared"), Panel->StatusText->GetText().IsEmpty());
    Panel->SharedStatusText->SetText(FText::FromString(TEXT("Network result retained")));
    FSFUpgradeAuditResult Audit;
    Panel->UpdateAuditUI(Audit);
    TestEqual(TEXT("Background Radius completion cannot overwrite Network feedback"), Panel->SharedStatusText->GetText().ToString(), FString(TEXT("Network result retained")));
    TestFalse(TEXT("Completion clears Radius cancellation"), Panel->bRadiusScanPending);

    Panel->TraversalResultsContainer = NewObject<UVerticalBox>(Panel);
    Panel->TraversalTargetTierComboBox = NewObject<UComboBoxString>(Panel);
    FSFTraversalResult Network;
    Network.Family = ESFUpgradeFamily::Belt;
    Network.TotalCount = 1;
    Network.CountByTier.Add(1, 1);
    FSFUpgradeAuditEntry Entry;
    Entry.Buildable = GetMutableDefault<AFGBuildableConveyorBelt>();
    Entry.CurrentTier = 1;
    Entry.MaxAvailableTier = 6;
    Network.Entries.Add(Entry);
    Panel->UpdateTraversalUI(Network);
    Panel->OnTraversalRowSelected(1);
    Panel->TraversalTargetTierComboBox->SetSelectedOption(TEXT("Mk.3"));
    Panel->CachedTargetTier = 3;
    Panel->SwitchToTab(ESmartUpgradeTab::Radius);
    Panel->SwitchToTab(ESmartUpgradeTab::Traversal);
    TestEqual(TEXT("Network source row restored"), Panel->SelectedTier, 1);
    TestEqual(TEXT("Network chosen target restored instead of default max"), Panel->CachedTargetTier, 3);
    Panel->SetNetworkTarget(GetMutableDefault<AFGBuildableConveyorBelt>());
    TestTrue(TEXT("Different target invalidates saved Network row"), Panel->MethodDrafts[1].Family == ESFUpgradeFamily::None);
    return true;
}
#endif
