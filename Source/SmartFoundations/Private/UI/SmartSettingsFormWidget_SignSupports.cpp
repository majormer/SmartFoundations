// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidgetImpl.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBoxSlot.h"
#include "Holograms/Core/SFSignSupportMode.h"

void USmartSettingsFormWidget::RefreshSignSupportControl()
{
	if (!WidgetTree || !ContentContainer) return;
	const FName ButtonName(TEXT("SignSupportModeButton"));
	const FName LabelName(TEXT("SignSupportModeLabel"));
	UButton* Button = Cast<UButton>(GetWidgetFromName(ButtonName));
	UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(LabelName));
	UWidget* Row = GetWidgetFromName(TEXT("SignSupportRow"));
	UWidget* Header = GetWidgetFromName(TEXT("SignSupportSectionHeader"));
	if (!Button || !Label || !Row || !Header) return;
	Button->OnClicked.AddUniqueDynamic(this, &USmartSettingsFormWidget::OnSignSupportButtonClicked);
	AFGHologram* Hologram = CachedSubsystem.IsValid() ? CachedSubsystem->GetActiveHologram() : nullptr;
	const bool bAvailable = SFSignSupportMode::IsAvailable(Hologram);
	Row->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Header->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bAvailable) Label->SetText(SFSignSupportMode::Get(Hologram) == SFSignSupportMode::EMode::None
		? NSLOCTEXT("SmartFoundations", "SignSupportValue_None", "None")
		: NSLOCTEXT("SmartFoundations", "SignSupportValue_Auto", "Auto"));
}

void USmartSettingsFormWidget::OnSignSupportButtonClicked()
{
	if (CachedSubsystem.IsValid()) SFSignSupportMode::Toggle(CachedSubsystem->GetActiveHologram());
	RefreshSignSupportControl();
}
