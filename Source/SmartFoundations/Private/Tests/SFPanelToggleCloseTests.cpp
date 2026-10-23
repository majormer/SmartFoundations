// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SmartUpgradePanel.h"
#include "UI/SFWalkPanelWidget.h"
#include "Input/SFPanelInput.h"
#include "Subsystem/SFSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPanelToggleCloseTest,
    "SmartFoundations.Interface.PanelToggleClose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPanelToggleCloseTest::RunTest(const FString& Parameters)
{
    auto* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
    FSlateApplication::Get().ClearKeyboardFocus();
    const FKeyEvent Toggle(EKeys::K, FModifierKeysState(), 0, false, 0, 0);
    const FKeyEvent Repeat(EKeys::K, FModifierKeysState(), 0, true, 0, 0);
    const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
    TestTrue(TEXT("Default Panel key resolves without a player mapping fixture"), SFPanelInput::IsToggleKey(nullptr, Toggle));
    TestFalse(TEXT("Holding the toggle cannot repeatedly close a panel"), SFPanelInput::IsToggleKey(nullptr, Repeat));
    for (int32 CloseMethod = 0; CloseMethod < 3; ++CloseMethod)
    {
        auto* Panel = NewObject<USmartSettingsFormWidget>(World);
        Panel->PendingPanelPreset = MakeShared<const FSFRestorePreset>();
        if (CloseMethod == 0)
            TestTrue(TEXT("Scaling/Restore/Extend receives toggle in UI preview"), Panel->NativeOnPreviewKeyDown(FGeometry(), Toggle).IsEventHandled());
        else if (CloseMethod == 1)
            TestTrue(TEXT("Escape closes through the same cancellation path"), Panel->NativeOnKeyDown(FGeometry(), Escape).IsEventHandled());
        else Panel->OnCloseButtonClicked();
        TestFalse(TEXT("All close methods cancel the pending handoff"), Panel->PendingPanelPreset.IsValid());
    }
    auto* Upgrade = NewObject<USmartUpgradePanel>(World);
    TestTrue(TEXT("Upgrade receives toggle in UI preview"), Upgrade->NativeOnPreviewKeyDown(FGeometry(), Toggle).IsEventHandled());
    auto* Walk = NewObject<USFWalkPanelWidget>(World);
    Walk->CachedSubsystem = NewObject<USFSubsystem>(World);
    TestTrue(TEXT("Walking receives toggle in UI preview"), Walk->NativeOnPreviewKeyDown(FGeometry(), Toggle).IsEventHandled());
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
