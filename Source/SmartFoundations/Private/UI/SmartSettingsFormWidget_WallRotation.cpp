// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidgetImpl.h"
#include "Shared/SFSupportGridFrame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Equipment/FGBuildGun.h"
#include "FGCharacterPlayer.h"

void USmartSettingsFormWidget::RefreshWallRotationControl()
{
    if (!WidgetTree || !ContentContainer) return;
    const FName Name(TEXT("WallRotationInput"));
    UComboBoxString* Input = Cast<UComboBoxString>(GetWidgetFromName(Name));
    UHorizontalBox* Row = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("WallRotationRow")));
    if (!Input || !Row) return;
    if (Input->GetOptionCount() == 0)
    {
        for (int32 Angle = 0; Angle < 360; Angle += 45)
            Input->AddOption(FString::FromInt(Angle) + TEXT("°"));
    }
    Input->OnSelectionChanged.AddUniqueDynamic(this, &USmartSettingsFormWidget::OnWallRotationChanged);
    AFGHologram* Holo = CachedSubsystem.IsValid() ? CachedSubsystem->GetActiveHologram() : nullptr;
    const bool bAvailable = Holo && SFSupportGridFrame::UsesParentRotation(Holo->GetBuildClass())
        && !CachedSubsystem->IsExtendModeActive();
    Row->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bAvailable)
    {
        Input->SetIsEnabled(Holo->GetRotationStep() == 45);
        const int32 Angle = ((-Holo->GetScrollRotateValue() % 360) + 360) % 360;
        Input->SetSelectedOption(FString::FromInt(Angle) + TEXT("°"));
    }
}

void USmartSettingsFormWidget::OnWallRotationChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType == ESelectInfo::Direct || !CachedSubsystem.IsValid()) return;
    AFGHologram* Holo = CachedSubsystem->GetActiveHologram();
    AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(GetOwningPlayerPawn());
    if (!Holo || !SFSupportGridFrame::UsesParentRotation(Holo->GetBuildClass())
        || CachedSubsystem->IsExtendModeActive() || Holo->GetRotationStep() != 45
        || !Player || !Player->GetBuildGun()) return;
    int32 Angle = 0;
    if (!LexTryParseString(Angle, *SelectedItem.LeftChop(1)) || Angle < 0 || Angle >= 360 || Angle % 45 != 0) return;
    const int32 Delta = FMath::RoundToInt(FRotator::NormalizeAxis(-Angle - Holo->GetScrollRotateValue()) / 45.0f);
    // Native scrolling ignores locked holograms. Preserve the existing mount and nudge,
    // let the native path accept the angle while unlocked, then restore the same anchor.
    // No fresh aim or attachment is introduced by editing the angle in the Panel.
    const bool bWasLocked = Holo->IsHologramLocked();
    const FTransform Before = Holo->GetActorTransform();
    const FVector LockLocation = Holo->GetHologramLockLocation();
    const FVector NudgeOffset = Holo->GetNudgeOffset();
    const int32 ScrollBefore = Holo->GetScrollRotateValue();
    if (bWasLocked) Holo->LockHologramPosition(false);
    {
        SFSupportGridFrame::FScopedNativeRotation NativeRotation(Holo);
        for (int32 Step = 0; Step < FMath::Abs(Delta); ++Step)
            Player->GetBuildGun()->Scroll(Delta > 0 ? 1 : -1);
    }
    if (bWasLocked)
    {
        const int32 AcceptedDelta = Holo->GetScrollRotateValue() - ScrollBefore;
        const FQuat Rotation = Before.GetRotation() * FRotator(0, 0, -AcceptedDelta).Quaternion();
        Holo->SetActorLocationAndRotation(LockLocation, Rotation);
        Holo->LockHologramPosition(true);
        Holo->SetNudgeOffset(NudgeOffset);
        Holo->SetActorLocation(Before.GetLocation());
    }
    CachedSubsystem->RegenerateChildHologramGrid();
    RefreshWallRotationControl();
}
