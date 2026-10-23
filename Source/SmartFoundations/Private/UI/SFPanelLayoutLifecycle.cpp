// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SmartUpgradePanel.h"
#include "UI/SFWalkPanelWidget.h"
#include "UI/SFInterfacePreferences.h"
#include "UI/SFInterfaceLayoutSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"

namespace
{
    bool FitTaskWindow(UUserWidget* Owner, USizeBox* Size, float Padding)
    {
        if (!Size) return false;
        const float Height = FMath::Max(480.f - Padding, float(UWidgetLayoutLibrary::GetViewportWidgetGeometry(Owner).GetLocalSize().Y - 120 - Padding));
        if (FMath::IsNearlyEqual(Size->GetHeightOverride(), Height)) return false;
        Size->SetHeightOverride(Height);
        return true; // Wait for fresh geometry before restoring/clamping.
    }
    void Track(UUserWidget* Panel)
    {
        if (auto* Player = Panel->GetOwningLocalPlayer()) if (auto* Layout = Player->GetSubsystem<USFInterfaceLayoutSubsystem>()) Layout->TrackPanel(Panel);
    }
}

void USmartSettingsFormWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!IsVisible()) return;
    TickPendingPresetLoad();
    if (IsExtendPanelShown()) RefreshExtendPanel();
    Track(this);
    if (BackgroundPanel && FitTaskWindow(this, Cast<USizeBox>(BackgroundPanel->GetContent()), 0)) return;
    if (!bIsDragging)
    {
        SFInterface::RestorePanel(this, BackgroundPanel, TEXT("Panel"), bPanelPositionRestored);
        SFInterface::ClampPanel(this, BackgroundPanel);
        UpdateRestoreDockPosition();
    }
}
void USmartSettingsFormWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    if (bIsDragging) SFInterface::SavePanel(this, BackgroundPanel, TEXT("Panel"));
    bIsDragging = false;
    Super::NativeOnMouseCaptureLost(Event);
}
void USmartUpgradePanel::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!IsVisible()) return;
    Track(this);
    if (BackgroundBorder && FitTaskWindow(this, Cast<USizeBox>(BackgroundBorder->GetContent()), 32)) return;
    if (!bIsDragging)
    {
        SFInterface::RestorePanel(this, BackgroundBorder, TEXT("Upgrade"), bPanelPositionRestored);
        SFInterface::ClampPanel(this, BackgroundBorder);
    }
    // The target layer covers the viewport even when dragging translates this
    // window. Counter only the owner's translation, not the movable frame.
    if (TargetPickLayer) TargetPickLayer->SetRenderTranslation(-GetRenderTransform().Translation);
}
void USmartUpgradePanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    if (bIsDragging) SFInterface::SavePanel(this, BackgroundBorder, TEXT("Upgrade"));
    bIsDragging = false;
    Super::NativeOnMouseCaptureLost(Event);
}
void USFWalkPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!IsVisible()) return;
    Track(this);
    if (SegmentListBox && SegmentListBox->GetChildrenCount() && FitTaskWindow(this, Cast<USizeBox>(SegmentListBox->GetChildAt(0)), 32)) return;
    if (!bDraggingPanel)
    {
        SFInterface::RestorePanel(this, WalkBackdrop, TEXT("Walking"), bPanelPositionRestored);
        SFInterface::ClampPanel(this, WalkBackdrop);
    }
    UpdatePendingSegmentReveal();
}
void USFWalkPanelWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    if (bDraggingPanel) SFInterface::SavePanel(this, WalkBackdrop, TEXT("Walking"));
    bDraggingPanel = false;
    Super::NativeOnMouseCaptureLost(Event);
}
