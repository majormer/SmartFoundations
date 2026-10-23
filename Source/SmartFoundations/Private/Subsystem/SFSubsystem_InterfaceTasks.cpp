// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFSubsystemImpl.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SFInterfaceCapabilities.h"
#include "UI/SmartUpgradePanel.h"
#include "UI/SFInterfaceTaskStrip.h"
#include "Blueprint/WidgetTree.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "Features/Walk/SFWalkService.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "UI/SFWalkPanelWidget.h"
#include "Services/SFHudService.h"
#include "Framework/Application/SlateApplication.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
bool USFSubsystem::IsInterfaceTaskPanelVisible() const
{
    for (const UUserWidget* Panel : {static_cast<const UUserWidget*>(SettingsFormWidget.Get()),
        static_cast<const UUserWidget*>(WalkPanelWidget.Get()), static_cast<const UUserWidget*>(UpgradePanelWidget.Get())})
    {
        if (IsValid(Panel) && Panel->IsInViewport() && Panel->IsVisible()) return true;
    }
    return false;
}

bool USFSubsystem::IsWalkingOnlyContext() const
{
    return ActiveHologram.IsValid() && (ActiveHologram->IsA<AFGRailroadTrackHologram>() || ActiveHologram->IsA<AFGVehiclePathSegmentHologram>());
}

FSFInterfaceCapabilities USFSubsystem::GetInterfaceCapabilities() const
{
    FSFInterfaceCapabilities Result;
    const bool bRouteOnly = IsWalkingOnlyContext();
    const bool bWalking = IsWalkModeActive();
    const bool bPlacement = ActiveHologram.IsValid() && !bRouteOnly && !IsUpgradeCapableContext();
    const bool bConduit = ActiveHologram.IsValid() && (ActiveHologram->IsA<AFGConveyorBeltHologram>() || ActiveHologram->IsA<AFGPipelineHologram>());
    const bool bCanWalk = bWalking || IsCurrentHologramWalkable();
    bool bCanUpgrade = IsUpgradeCapableContext();
    if (ActiveHologram.IsValid() && ActiveHologram->GetBuildClass())
        bCanUpgrade |= USFUpgradeAuditService::GetUpgradeFamily(ActiveHologram->GetBuildClass()->GetDefaultObject<AFGBuildable>()) != ESFUpgradeFamily::None;
    if (bWalking && WalkService)
        bCanUpgrade |= WalkService->GetConveyanceType() == ESFWalkConveyanceType::Belt || WalkService->GetConveyanceType() == ESFWalkConveyanceType::Pipe;
    const bool bCanExtend = ExtendService && ExtendService->CanOfferPanel(ActiveHologram.Get());
    Result.DefaultTask = bRouteOnly ? 1 : IsUpgradeCapableContext() ? 2 : 0;
    Result.Tasks = {
        {0, LOCTEXT("Task_Panel", "Scaling"), bPlacement, bPlacement, !bRouteOnly, LOCTEXT("Task_PlacementUnavailable", "Select a buildable with Smart placement controls.")},
        {1, LOCTEXT("Task_Walking", "Walking"), bCanWalk || bConduit, bCanWalk, true, bConduit ? LOCTEXT("Task_WalkSupport", "Equip the matching stackable support to start or resume Walking. To resume a bus, close the Panel, aim at an open end of the existing run with that support equipped, then reopen the Panel and choose Walking. Do not build the starting support.") : LOCTEXT("Task_WalkUnavailable", "This buildable does not support Walking.")},
        {2, LOCTEXT("Task_Upgrade", "Upgrade"), bCanUpgrade, bCanUpgrade, true, LOCTEXT("Task_UpgradeUnavailable", "This buildable has no supported tier replacement.")},
        {3, LOCTEXT("Task_Restore", "Restore"), bPlacement, bPlacement, true, LOCTEXT("Task_RestoreNeedsPlacement", "Select a buildable with Smart placement controls to use presets and Modules.")},
        {4, LOCTEXT("Task_Extend", "Extend"), bCanExtend, bCanExtend, bCanExtend, LOCTEXT("Task_ExtendUnavailable", "Equip a compatible factory building or Water Extractor to copy an existing module.")}
    };
    if (bWalking && !bRouteOnly)
        for (int32 Task : {0,3})
        {
            Result.Tasks[Task].bAvailable = bPlacement;
            Result.Tasks[Task].Reason = LOCTEXT("Task_ScalingReturn", "Returning to Scaling ends the unbuilt walk. You will be asked to confirm.");
        }
    return Result;
}

bool USFSubsystem::CanNavigateInterfaceTask(int32 Task, FText* Reason) const
{
    if (SettingsFormWidget.IsValid() && SettingsFormWidget->IsVisible() && SettingsFormWidget->HasPendingPresetLoad())
    {
        if (Reason) *Reason = LOCTEXT("Task_PresetLoading", "The build gun is switching to the preset's building. Please wait.");
        return false;
    }
    if (SettingsFormWidget.IsValid() && SettingsFormWidget->IsVisible() && SettingsFormWidget->HasPendingConfirmation())
    {
        if (Reason) *Reason = LOCTEXT("Task_ConfirmationPending", "Finish or cancel the current confirmation before changing tasks.");
        return false;
    }
    const auto Capabilities = GetInterfaceCapabilities();
    if (!Capabilities.Tasks.IsValidIndex(Task)) return false;
    const auto& Capability = Capabilities.Tasks[Task];
    if (Reason) *Reason = Capability.bAvailable && Task != 0 && Task != 3 ? FText::GetEmpty() : Capability.Reason;
    return Capability.bAvailable && Capability.bVisible;
}

void USFSubsystem::NavigateInterfaceTask(int32 Task)
{
    if (!CanNavigateInterfaceTask(Task)) return;
    if (IsWalkModeActive() && (Task == 0 || Task == 3 || Task == 4))
    {
        NavigateInterfaceTask(1);
        if (WalkPanelWidget.IsValid()) WalkPanelWidget->RequestScalingReturn(Task);
        return;
    }
    AFGPlayerController* PC = GetLastController();
    if (!PC) return;
    // Commit only the control's own focus-loss behavior, never a task's Apply.
    FSlateApplication::Get().ReleaseAllPointerCapture();
    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    ClearLatchedTransformMode(TEXT("interface task changed"));
    if (auto* Upgrade = Cast<USmartUpgradePanel>(UpgradePanelWidget.Get())) Upgrade->CancelTargetSelection();
    if (SettingsFormWidget.IsValid()) SettingsFormWidget->CancelExtendSourceSelection();
    if (SettingsFormWidget.IsValid() && !SettingsFormWidget->IsForCurrentHologram())
    {
        // Obsolete UI state must never be reverted into a different placement.
        SettingsFormWidget->RemoveFromParent(); SettingsFormWidget.Reset();
    }
    UUserWidget* Previous = nullptr;
    for (UUserWidget* Widget : {static_cast<UUserWidget*>(SettingsFormWidget.Get()), UpgradePanelWidget.Get(), static_cast<UUserWidget*>(WalkPanelWidget.Get())})
        if (Widget && Widget->IsInViewport())
        {
            if (Widget->IsVisible()) Previous = Widget;
            Widget->SetVisibility(ESlateVisibility::Collapsed);
        }

    UUserWidget* Target = nullptr;
    if (Task == 0 || Task == 3 || Task == 4)
    {
        if (!SettingsFormWidget.IsValid() || !SettingsFormWidget->IsInViewport()) OpenSettingsPanel();
        Target = SettingsFormWidget.Get();
        if (SettingsFormWidget.IsValid()) { SettingsFormWidget->ShowRestorePanel(Task == 3); SettingsFormWidget->ShowExtendPanel(Task == 4); }
    }
    else if (Task == 2)
    {
        if (!UpgradePanelWidget.IsValid() || !UpgradePanelWidget->IsInViewport()) OnToggleUpgradePanel();
        Target = UpgradePanelWidget.Get();
    }
    else
    {
        if (!IsWalkModeActive()) EnterWalkMode();
        if (IsWalkModeActive())
        {
            if (!WalkPanelWidget.IsValid() || !WalkPanelWidget->IsInViewport()) OpenWalkPanel();
            Target = WalkPanelWidget.Get();
        }
        // Native start rejection must leave a usable modal, not a hidden panel.
        if (!Target) Target = SettingsFormWidget.Get();
    }
    if (!Target) Target = Previous;
    if (!Target)
    {
        PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly());
        if (HudService) HudService->SetHUDSuppressed(false);
        return;
    }
    Target->SetVisibility(ESlateVisibility::Visible);
    if (Target->WidgetTree) Target->WidgetTree->ForEachWidget([Task](UWidget* Widget)
    {
        if (auto* Strip = Cast<USFInterfaceTaskStrip>(Widget)) Strip->SetActiveTask(Task);
    });
    Target->SetIsFocusable(true);
    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Target->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode);
    Target->SetKeyboardFocus();
    if (HudService) HudService->SetHUDSuppressed(true);
}

void USFSubsystem::CloseInactiveInterfaceTasks(UUserWidget* Closing)
{
    // Explicit Close affects its own task only. Suspended drafts stay collapsed;
    // reverting a hidden Scaling panel here would overwrite a live Walking path.
    if (auto* Upgrade = Cast<USmartUpgradePanel>(UpgradePanelWidget.Get()))
        if (Upgrade != Closing) Upgrade->CancelTargetSelection();
}
#undef LOCTEXT_NAMESPACE
