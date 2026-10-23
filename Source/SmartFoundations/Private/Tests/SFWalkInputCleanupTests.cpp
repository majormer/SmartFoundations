// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFSubsystem.h"
#include "UI/SFWalkPanelWidget.h"
#include "Services/SFHudService.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkInputCleanupTest,
    "SmartFoundations.Walking.InputCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFWalkInputCleanupTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::EditorPreview);
    Context.SetCurrentWorld(World);
    Context.GameViewport = NewObject<UGameViewportClient>(GEngine);
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    LocalPlayer->PlayerController = Controller;
    Controller->Player = LocalPlayer;
    World->AddController(Controller);
    auto* Subsystem = NewObject<USFSubsystem>(World);
    auto* Hud = NewObject<USFHudService>(Subsystem);
    Hud->Initialize(Subsystem);
    const auto DrawHud = [&]()
    {
        // This fixture has no native game viewport. Exercise suppression reconciliation
        // without attempting to create an onscreen widget for its synthetic controller.
        World->RemoveController(Controller);
        Hud->DrawCounterToHUD(nullptr, nullptr);
        World->AddController(Controller);
    };

    for (bool Active : {true, false})
    {
        auto* Panel = NewObject<USFWalkPanelWidget>(World);
        Panel->SetOwningPlayer(Controller);
        TestEqual(TEXT("Fixture resolves its local controller"), Panel->GetOwningPlayer(), Controller);
        Panel->SetVisibility(ESlateVisibility::Visible);
        Subsystem->WalkPanelWidget = Panel;
        Subsystem->bWalkModeActive = Active;
        Controller->bShowMouseCursor = true;
        Controller->SetInputMode(FInputModeUIOnly());
        Hud->SetHUDSuppressed(true); // Task-strip navigation suppresses before Walking closes.
        Subsystem->ExitWalkMode();
        DrawHud();
        TestFalse(TEXT("Next draw clears stale task suppression after Walking exit"), Hud->IsHUDSuppressed());
        TestFalse(TEXT("Exit releases visible panel cursor"), Controller->bShowMouseCursor);
        TestTrue(TEXT("Exit restores game input"), Controller->GetCurrentInputModeDebugString() == FInputModeGameOnly().GetDebugDisplayName());
        TestFalse(TEXT("Exit drops panel reference"), Subsystem->WalkPanelWidget.IsValid());
    }

    auto* HiddenPanel = NewObject<USFWalkPanelWidget>(World);
    HiddenPanel->SetOwningPlayer(Controller);
    HiddenPanel->SetVisibility(ESlateVisibility::Collapsed);
    Subsystem->WalkPanelWidget = HiddenPanel;
    Hud->SetHUDSuppressed(true);
    Hud->SetLayoutEditorActive(true);
    DrawHud();
    TestFalse(TEXT("Collapsed Walking draft cannot suppress the ordinary HUD"), Hud->IsHUDSuppressed());
    TestTrue(TEXT("Task reconciliation preserves independent layout editor ownership"), Hud->IsLayoutEditorActive());
    Controller->bShowMouseCursor = true;
    Controller->SetInputMode(FInputModeUIOnly());
    Subsystem->ExitWalkMode();
    TestTrue(TEXT("Hidden panel cleanup preserves another UI cursor"), Controller->bShowMouseCursor);
    TestTrue(TEXT("Hidden panel cleanup preserves another input mode"), Controller->GetCurrentInputModeDebugString() == FInputModeUIOnly().GetDebugDisplayName());
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
