// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFInterfaceTaskStrip.h"
#include "SmartFoundations.h"
#include "UI/SFPanelStyle.h"
#include "UI/SFInterfaceCapabilities.h"
#include "UI/SFFontLibrary.h"
#include "Subsystem/SFSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBoxSlot.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"
void USFInterfaceTaskStrip::Initialize(USFSubsystem* Subsystem, int32 ActiveTask)
{
    Owner = Subsystem;
    const auto Capabilities = Subsystem ? Subsystem->GetInterfaceCapabilities() : FSFInterfaceCapabilities();
    const TCHAR* Names[] = {TEXT("TaskPanelButton"), TEXT("TaskWalkButton"), TEXT("TaskUpgradeButton"), TEXT("TaskRestoreButton"), TEXT("TaskExtendButton")};
    TaskIds = {0,4,1,2,3};
    for (int32 Task : TaskIds)
    {
        if (!Capabilities.Tasks.IsValidIndex(Task)) continue;
        auto* Button = NewObject<UButton>(this, Names[Task]);
        auto* Text = NewObject<UTextBlock>(this);
        Text->SetText(Capabilities.Tasks[Task].Label); Text->SetFont(SFFont::Get(14)); Text->SetJustification(ETextJustify::Center);
        Button->SetContent(Text);
        FText Reason;
        Button->SetIsEnabled(Subsystem && Subsystem->CanNavigateInterfaceTask(Task, &Reason));
        Button->SetToolTipText(Reason);
        switch (Task)
        {
        case 0: Button->OnClicked.AddDynamic(this, &USFInterfaceTaskStrip::OnPanel); break;
        case 1: Button->OnClicked.AddDynamic(this, &USFInterfaceTaskStrip::OnWalking); break;
        case 2: Button->OnClicked.AddDynamic(this, &USFInterfaceTaskStrip::OnUpgrade); break;
        case 3: Button->OnClicked.AddDynamic(this, &USFInterfaceTaskStrip::OnRestore); break;
        case 4: Button->OnClicked.AddDynamic(this, &USFInterfaceTaskStrip::OnExtend); break;
        }
        UWidget* Tab = SFPanelStyle::WrapTabButton(Button);
        if (!Capabilities.Tasks[Task].bVisible) Tab->SetVisibility(ESlateVisibility::Collapsed);
        AddChildToHorizontalBox(Tab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        SFPanelStyle::StyleTab(Button, Task == ActiveTask);
    }
}
void USFInterfaceTaskStrip::OnPanel() { if (Owner.IsValid()) Owner->NavigateInterfaceTask(0); }
void USFInterfaceTaskStrip::SetActiveTask(int32 ActiveTask)
{
    const auto Capabilities = Owner.IsValid() ? Owner->GetInterfaceCapabilities() : FSFInterfaceCapabilities();
    for (int32 Index = 0; Index < GetChildrenCount(); ++Index)
        if (auto* Column = Cast<UPanelWidget>(GetChildAt(Index)))
            if (auto* Button = Cast<UButton>(Column->GetChildAt(0)))
            {
                const int32 Task = TaskIds.IsValidIndex(Index) ? TaskIds[Index] : INDEX_NONE;
                FText Reason;
                Button->SetIsEnabled(Owner.IsValid() && Owner->CanNavigateInterfaceTask(Task, &Reason));
                Button->SetToolTipText(Reason);
                Column->SetVisibility(Capabilities.Tasks.IsValidIndex(Task) && Capabilities.Tasks[Task].bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
                SFPanelStyle::StyleTab(Button, Task == ActiveTask);
            }
}
void USFInterfaceTaskStrip::OnWalking() { if (Owner.IsValid()) Owner->NavigateInterfaceTask(1); }
void USFInterfaceTaskStrip::OnUpgrade() { if (Owner.IsValid()) Owner->NavigateInterfaceTask(2); }
void USFInterfaceTaskStrip::OnRestore() { if (Owner.IsValid()) Owner->NavigateInterfaceTask(3); }
void USFInterfaceTaskStrip::OnExtend() { if (Owner.IsValid()) Owner->NavigateInterfaceTask(4); }
#undef LOCTEXT_NAMESPACE
