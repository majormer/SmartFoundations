// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFContextHelp.h"
#include "SmartFoundations.h"
#include "UI/SFPanelStyle.h"
#include "UI/SFFontLibrary.h"
#include "Components/Button.h"

void USFContextHelp::InitializeHelp(FText Guidance)
{
    SetText(Guidance);
    SetFont(SFFont::Get(13));
    SetColorAndOpacity(SFPanelStyle::LightText);
    SetAutoWrapText(true);
    SetIsExpanded(false);
    HelpButton = NewObject<UButton>(this, FName(*(GetName() + TEXT("Button"))));
    HelpButton->SetStyle(SFPanelStyle::MakeHelpButtonStyle());
    HelpButton->SetToolTipText(Guidance);
    auto* Caption = NewObject<UTextBlock>(HelpButton);
    Caption->SetText(FText::FromString(TEXT("?")));
    Caption->SetFont(SFFont::Get(14));
    Caption->SetColorAndOpacity(SFPanelStyle::Accent);
    HelpButton->SetContent(Caption);
    HelpButton->OnClicked.AddDynamic(this, &USFContextHelp::ToggleDetails);
}
void USFContextHelp::SetIsExpanded(bool bExpanded)
{
    SetVisibility(bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
void USFContextHelp::ToggleDetails() { SetIsExpanded(!GetIsExpanded()); }
