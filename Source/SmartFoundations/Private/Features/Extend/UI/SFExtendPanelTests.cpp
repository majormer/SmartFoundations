// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidget.h"
#include "Features/Extend/SFExtendService.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPanelWorkflowTest, "SmartFoundations.Interface.ExtendSourceSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendPanelWorkflowTest::RunTest(const FString&)
{
    auto* Panel = NewObject<USmartSettingsFormWidget>();
    Panel->WidgetTree = NewObject<UWidgetTree>(Panel);
    auto* Canvas = NewObject<UCanvasPanel>(Panel);
    Panel->WidgetTree->RootWidget = Canvas;
    auto* Body = NewObject<UVerticalBox>(Panel); Canvas->AddChild(Body);
    Panel->TaskContentSwitcher = NewObject<UWidgetSwitcher>(Panel);
    Panel->TaskContentSwitcher->AddChild(NewObject<UVerticalBox>(Panel));
    Panel->TaskContentSwitcher->AddChild(NewObject<UVerticalBox>(Panel));
    Panel->GridYInput = NewObject<USpinBox>(Panel); Panel->GridYInput->SetValue(3);
    Panel->BuildExtendControls(Body);
    Panel->ShowExtendPanel(true);
    TestTrue(TEXT("Extend has an explicit entry without a source"), Panel->IsExtendPanelShown());
    TestEqual(TEXT("Opening the tab keeps placement draft"), Panel->GridYInput->GetValue(), 3.f);
    Panel->OnSelectExtendSource();
    TestTrue(TEXT("Select Source arms one world click"), Panel->bSelectingExtendSource);
    TestTrue(TEXT("World hit layer exists only while armed"), Panel->ExtendPickLayer->GetVisibility() == ESlateVisibility::Visible);
    const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
    TestTrue(TEXT("Escape is consumed by target mode"), Panel->NativeOnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
    TestFalse(TEXT("Escape cancels selection without closing the tab"), Panel->bSelectingExtendSource);
    TestTrue(TEXT("Extend remains open after Escape"), Panel->IsExtendPanelShown());
    TestFalse(TEXT("Cancelled targeting cannot keep instructing a world click"), Panel->ExtendSourceStatus->GetText().ToString().Contains(TEXT("Click one matching")));
    TestEqual(TEXT("Cancelling selection preserves draft"), Panel->GridYInput->GetValue(), 3.f);
    Panel->OnSelectExtendSource(); Panel->ShowRestorePanel(true);
    TestFalse(TEXT("Switching tasks cannot leave a click-through layer armed"), Panel->bSelectingExtendSource);
    TestTrue(TEXT("World hit layer collapses on task switch"), Panel->ExtendPickLayer->GetVisibility() == ESlateVisibility::Collapsed);
    TestFalse(TEXT("Restore hides Extend content"), Panel->IsExtendPanelShown());
    Panel->bWaitingForConfirmation = true; Panel->ShowExtendPanel(true);
    TestFalse(TEXT("A pending destructive-work warning blocks task switches"), Panel->IsExtendPanelShown());
    auto* Service = NewObject<USFExtendService>();
    TestFalse(TEXT("Null source cannot select or invoke construction"), Service->SelectPanelSource(nullptr, nullptr));
    return true;
}
#endif
