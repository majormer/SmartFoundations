// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFInterfaceSettingsWidget.h"
#include "SmartFoundations.h"
#include "UI/SFInterfacePreferences.h"
#include "UI/SFInterfaceLayoutSubsystem.h"
#include "UI/SFFontLibrary.h"
#include "UI/SFPanelStyle.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SmartInterface"
TSharedRef<SWidget> USFInterfaceSettingsWidget::RebuildWidget()
{
    static const FButtonStyle ButtonStyle = SFPanelStyle::MakeButtonStyle(false);
    auto Button = [](FText Text, TFunction<void()> Action)
    {
        return SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12, 8)).OnClicked_Lambda([Action]{Action();return FReply::Handled();})
            [SNew(STextBlock).Font(SFFont::Get(16)).ColorAndOpacity(SFPanelStyle::LightText).Text(Text)];
    };
    TSharedPtr<SWidget> HelpButton;
    auto HelpDetails = SFPanelStyle::MakeSlateHelp(LOCTEXT("SharedLayoutScope", "All task tabs share one window position across saves on this computer. Reset Panel Positions restores its default location. Reset HUD Appearance changes position, scale and theme only; building settings and presets are unaffected."), HelpButton);
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,16,0,8)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(SFFont::Get(20)).ColorAndOpacity(SFPanelStyle::Accent).Text(LOCTEXT("InterfaceLayout", "Interface layout"))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[HelpButton.ToSharedRef()]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Button(LOCTEXT("EditAfterResume", "Edit HUD layout after Resume"), [this]
        {
            if (auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
                if (auto* Player = PC->GetLocalPlayer()) if (auto* Layout = Player->GetSubsystem<USFInterfaceLayoutSubsystem>()) Layout->RequestEdit();
        })]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Button(LOCTEXT("ResetPanels", "Reset Panel Positions"), []{SFInterface::ResetPanelPositions();})]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Button(LOCTEXT("ResetHUD", "Reset HUD Appearance"), [this]{SFInterface::WriteHud(this,SFInterface::ReadHud(this,true));})]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[HelpDetails];
}
#undef LOCTEXT_NAMESPACE
