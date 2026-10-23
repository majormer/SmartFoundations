// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartUpgradePanel.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUpgradeTargetSelectionTest,
    "SmartFoundations.Interface.UpgradeTargetSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFUpgradeTargetSelectionTest::RunTest(const FString& Parameters)
{
    // Identity-only callback test: class defaults supply stable buildable
    // identities without spawning factory actors or requiring a saved world.
    auto* Panel = NewObject<USmartUpgradePanel>();
    Panel->ActiveTab = ESmartUpgradeTab::Traversal;
    auto* Current = GetMutableDefault<AFGBuildableConveyorBelt>();
    auto* Previous = GetMutableDefault<AFGBuildablePipeline>();
    Panel->TraversalAnchor = Current;
    Panel->SharedStatusText = NewObject<UTextBlock>(Panel);
    Panel->SharedStatusText->SetText(FText::FromString(TEXT("Current selection retained")));
    Panel->CachedTraversalResult.TotalCount = 17;

    FSFTraversalResult Delayed;
    Delayed.AnchorBuildable = Previous;
    Delayed.ErrorMessage = TEXT("Old target response");
    Panel->OnClientTraversalResult(Delayed);
    TestEqual(TEXT("Old target cannot replace cached results"), Panel->CachedTraversalResult.TotalCount, 17);
    TestEqual(TEXT("Old target cannot replace current status"), Panel->SharedStatusText->GetText().ToString(), FString(TEXT("Current selection retained")));
    TestTrue(TEXT("Old target cannot replace selected anchor"), Panel->TraversalAnchor.Get() == Current);

    Panel->TraversalAnchor.Reset();
    Panel->OnClientTraversalResult(Delayed);
    TestEqual(TEXT("A missing current target rejects delayed responses"), Panel->CachedTraversalResult.TotalCount, 17);

    Panel->TraversalAnchor = Current;
    FSFTraversalResult CurrentError;
    CurrentError.AnchorBuildable = Current;
    CurrentError.ErrorMessage = TEXT("Current target error");
    Panel->OnClientTraversalResult(CurrentError);
    TestEqual(TEXT("Matching target errors still reach the UI"), Panel->CachedTraversalResult.ErrorMessage, CurrentError.ErrorMessage);
    TestTrue(TEXT("Matching target error is displayed"), Panel->SharedStatusText->GetText().ToString().Contains(CurrentError.ErrorMessage));
    return true;
}
#endif
