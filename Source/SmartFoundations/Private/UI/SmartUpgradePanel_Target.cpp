// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SmartUpgradePanelImpl.h"
#include "Input/SFPanelInput.h"
#include "Core/SFNativeHit.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Equipment/FGBuildGun.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Buildables/FGBuildable.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
UWidget* USmartUpgradePanel::GenerateTierItem(FString Item)
{
    auto* Text = NewObject<UTextBlock>(this);
    Text->SetFont(SFFont::Get(14));
    Text->SetText(FText::FromString(Item));
    Text->SetColorAndOpacity(SFPanelStyle::NearBlackText);
    return Text;
}

void USmartUpgradePanel::InitializeTargetSelection()
{
    if (auto* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        TargetPickLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TargetPickLayer"));
        TargetPickLayer->SetBrushColor(FLinearColor::Transparent);
        TargetPickLayer->SetCursor(EMouseCursor::Crosshairs);
        auto* LayerSlot = Canvas->AddChildToCanvas(TargetPickLayer);
        LayerSlot->SetAnchors(FAnchors(0,0,1,1)); LayerSlot->SetOffsets(FMargin(0)); LayerSlot->SetZOrder(-10);
        TargetPickLayer->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (TraversalAnchorText)
        if (auto* Column = Cast<UVerticalBox>(TraversalAnchorText->GetParent()))
        {
            auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
            const auto Children = Column->GetAllChildren(); Column->ClearChildren();
            TraversalAnchorText->SetAutoWrapText(true);
            Row->AddChildToHorizontalBox(TraversalAnchorText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            SelectTargetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectTargetButton"));
            SelectTargetLabel = WidgetTree->ConstructWidget<UTextBlock>(); SelectTargetLabel->SetFont(SFFont::Get(14));
            SelectTargetLabel->SetText(LOCTEXT("Upgrade_SelectTarget", "Select Target")); SelectTargetLabel->SetColorAndOpacity(SFPanelStyle::LightText);
            SelectTargetButton->SetContent(SelectTargetLabel); SelectTargetButton->SetStyle(SFPanelStyle::MakeButtonStyle(false));
            SelectTargetButton->SetIsEnabled(TargetPickLayer != nullptr);
            SelectTargetButton->OnClicked.AddDynamic(this, &USmartUpgradePanel::OnSelectTargetClicked);
            Row->AddChildToHorizontalBox(SelectTargetButton)->SetPadding(FMargin(12,0,0,0));
            for (UWidget* Child : Children) Column->AddChildToVerticalBox(Child == TraversalAnchorText ? static_cast<UWidget*>(Row) : Child)->SetPadding(FMargin(0,4));
        }
    // Capture native intent before the modal UI takes over the pointer. Matching
    // tiers still have an exact native hit even when no replacement is offered.
    if (auto* Character = Cast<AFGCharacterPlayer>(GetOwningPlayerPawn()))
        if (auto* Gun = Character->GetBuildGun())
        {
            auto* Target = Cast<AFGBuildable>(SFNativeHit::ResolveActor(Gun->GetHitResult()));
            if (Target && USFUpgradeTraversalService::GetUpgradeFamily(Target) != ESFUpgradeFamily::None) SetNetworkTarget(Target);
        }
    if (!TraversalAnchor.IsValid() && TraversalAnchorText)
        TraversalAnchorText->SetText(LOCTEXT("Upgrade_SelectTargetHint", "No network target. Use Select Target."));
}

void USmartUpgradePanel::OnSelectTargetClicked()
{
    if (bSelectingNetworkTarget) { CancelTargetSelection(); return; }
    if (!TargetPickLayer || ActiveTab != ESmartUpgradeTab::Traversal) return;
    bSelectingNetworkTarget = true;
    TargetSelectionPreviousStatus = SharedStatusText ? SharedStatusText->GetText() : FText::GetEmpty();
    TargetPickLayer->SetVisibility(ESlateVisibility::Visible);
    if (SelectTargetLabel) SelectTargetLabel->SetText(LOCTEXT("Upgrade_CancelTarget", "Cancel Selection"));
    if (SharedStatusText) SharedStatusText->SetText(LOCTEXT("Upgrade_PickTargetHint", "Click one target in the world. Panel controls stay active. Escape cancels selection."));
}

void USmartUpgradePanel::CancelTargetSelection()
{
    if (!bSelectingNetworkTarget) return;
    bSelectingNetworkTarget = false;
    if (TargetPickLayer) TargetPickLayer->SetVisibility(ESlateVisibility::Collapsed);
    if (SelectTargetLabel) SelectTargetLabel->SetText(LOCTEXT("Upgrade_SelectTarget", "Select Target"));
    if (SharedStatusText) SharedStatusText->SetText(TargetSelectionPreviousStatus);
}

bool USmartUpgradePanel::PickNetworkTarget(const FVector2D& AbsolutePosition)
{
    if (!bSelectingNetworkTarget || !IsVisible() || !IsInViewport() || !TargetPickLayer
        || !TargetPickLayer->GetCachedGeometry().IsUnderLocation(AbsolutePosition)
        || (BackgroundBorder && BackgroundBorder->GetCachedGeometry().IsUnderLocation(AbsolutePosition))) return false;
    // Single shot, including a miss. Never forward this click to gameplay input.
    CancelTargetSelection();
    FVector2D PixelPosition, ViewportPosition;
    USlateBlueprintLibrary::AbsoluteToViewport(this, AbsolutePosition, PixelPosition, ViewportPosition);
    FVector Start, Direction;
    APlayerController* PC = GetOwningPlayer();
    AFGBuildable* Target = nullptr;
    if (PC && PC->DeprojectScreenPositionToWorld(PixelPosition.X, PixelPosition.Y, Start, Direction))
    {
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(GetOwningPlayerPawn());
        if (auto* Subsystem = USFSubsystem::Get(this))
            if (AFGHologram* Hologram = Subsystem->GetActiveHologram()) Query.AddIgnoredActor(Hologram);
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * 5000.f, ECC_WorldStatic, Query))
            Target = Cast<AFGBuildable>(SFNativeHit::ResolveActor(Hit));
    }
    if (Target && USFUpgradeTraversalService::GetUpgradeFamily(Target) != ESFUpgradeFamily::None)
    {
        SetNetworkTarget(Target);
        if (SharedStatusText) SharedStatusText->SetText(LOCTEXT("Upgrade_TargetSelected", "Network target selected. Press Scan to inspect its network."));
        return true;
    }
    if (SharedStatusText) SharedStatusText->SetText(LOCTEXT("Upgrade_TargetMiss", "No supported target at that point. Previous target retained; use Select Target to try again."));
    return false;
}

void USmartUpgradePanel::SetNetworkTarget(AFGBuildable* Target)
{
    if (!IsValid(Target)) return;
    if (TraversalAnchor.Get() != Target)
    {
        USFUpgradeTraversalService::OnClientTraversalResultReceived.RemoveAll(this);
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TraversalRefreshTimerHandle);
        CachedTraversalResult = FSFTraversalResult();
        if (TraversalResultsContainer)
        {
            for (auto It = RowDataMap.CreateIterator(); It; ++It)
                for (UWidget* Parent = It.Key(); Parent; Parent = Parent->GetParent())
                    if (Parent == TraversalResultsContainer) { It.RemoveCurrent(); break; }
            TraversalResultsContainer->ClearChildren();
        }
        if (ActiveTab == ESmartUpgradeTab::Traversal)
        {
            SelectedFamily = ESFUpgradeFamily::None; SelectedTier = 0; CachedTargetTier = 0;
            if (SharedUpgradeButton) SharedUpgradeButton->SetIsEnabled(false);
            if (TraversalTargetTierComboBox) TraversalTargetTierComboBox->SetVisibility(ESlateVisibility::Collapsed);
        }
        MethodDrafts[1] = FMethodDraft();
        if (ActiveTab == ESmartUpgradeTab::Traversal && StatusText) StatusText->SetText(FText::GetEmpty());
        if (TraversalCostDetailsText) TraversalCostDetailsText->SetText(LOCTEXT("Upgrade_TargetNeedsScan", "Scan this target to review its network and materials."));
    }
    TraversalAnchor = Target; TraversalAnchorLocation = Target->GetActorLocation();
    if (TraversalAnchorText)
        TraversalAnchorText->SetText(FText::Format(LOCTEXT("Upgrade_TargetName", "Target: {0} (Mk.{1})"), Target->mDisplayName, FText::AsNumber(USFUpgradeTraversalService::GetBuildableTier(Target))));
}

FReply USmartUpgradePanel::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (SFPanelInput::IsToggleKey(GetOwningPlayer(), Event))
    {
        ClosePanel();
        return FReply::Handled();
    }
    if (bSelectingNetworkTarget && Event.GetKey() == EKeys::Escape)
    { CancelTargetSelection(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
#undef LOCTEXT_NAMESPACE
