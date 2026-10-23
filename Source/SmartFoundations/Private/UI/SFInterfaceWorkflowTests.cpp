// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SmartSettingsFormWidget.h"
#include "UI/SFPanelStyle.h"
#include "UI/SFContextHelp.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Widgets/SWidget.h"
#include "UI/SFWalkPanelWidget.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Walk/SFWalkTypes.h"
#include "Components/SizeBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTabbedRestoreDraftsTest, "SmartFoundations.Interface.TabbedRestoreDrafts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFTabbedRestoreDraftsTest::RunTest(const FString&)
{
    auto* Panel = NewObject<USmartSettingsFormWidget>();
    Panel->WidgetTree = NewObject<UWidgetTree>(Panel);
    Panel->TaskContentSwitcher = NewObject<UWidgetSwitcher>(Panel);
    Panel->TaskContentSwitcher->AddChild(NewObject<UVerticalBox>(Panel));
    Panel->TaskContentSwitcher->AddChild(NewObject<UVerticalBox>(Panel));
    auto* Root = NewObject<UVerticalBox>(Panel);
    Panel->WidgetTree->RootWidget = Root;
    auto* ScalingHelp = SFPanelStyle::MakeHelp(Panel, TEXT("ScalingHelp"), FText::FromString(TEXT("Scaling")), FText::FromString(TEXT("Summary")), FText::FromString(TEXT("Details")));
    Root->AddChildToVerticalBox(SFPanelStyle::MakeHelpLabelRow(Panel, NewObject<UTextBlock>(Panel), ScalingHelp));
    Root->AddChildToVerticalBox(Panel->TaskContentSwitcher);
    Cast<UVerticalBox>(Panel->TaskContentSwitcher->GetWidgetAtIndex(0))->AddChildToVerticalBox(ScalingHelp);
    ScalingHelp->GetHelpButton()->OnClicked.Broadcast();
    Panel->PlacementFooter = NewObject<UHorizontalBox>(Panel);
    Panel->GridYInput = NewObject<USpinBox>(Panel);
    Panel->GridYInput->SetValue(3);
    Panel->PresetNameInput = NewObject<UEditableTextBox>(Panel);
    Panel->PresetNameInput->SetText(FText::FromString(TEXT("Unsaved preset draft")));
    Panel->ShowRestorePanel(true);
    TestEqual(TEXT("Restore is the second page in the same window"), Panel->TaskContentSwitcher->GetActiveWidgetIndex(), 1);
    TestTrue(TEXT("Placement actions absent on Restore"), Panel->PlacementFooter->GetVisibility() == ESlateVisibility::Collapsed);
    TestTrue(TEXT("Shared header must not expose Scaling help on Restore"), ScalingHelp->GetHelpButton()->GetVisibility() == ESlateVisibility::Collapsed);
    Panel->ShowRestorePanel(false);
    TestEqual(TEXT("Scaling page restored"), Panel->TaskContentSwitcher->GetActiveWidgetIndex(), 0);
    TestTrue(TEXT("Scaling help returns with its own task"), ScalingHelp->GetHelpButton()->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Help expansion survives switching tasks"), ScalingHelp->GetIsExpanded());
    TestEqual(TEXT("Placement draft survives Restore"), Panel->GridYInput->GetValue(), 3.f);
    TestEqual(TEXT("Author draft survives Scaling"), Panel->PresetNameInput->GetText().ToString(), FString(TEXT("Unsaved preset draft")));
    Panel->bWaitingForConfirmation = true;
    Panel->ShowRestorePanel(true);
    TestEqual(TEXT("Pending confirmation blocks a page change"), Panel->TaskContentSwitcher->GetActiveWidgetIndex(), 0);
    TestTrue(TEXT("Rejected task switch leaves current help available"), ScalingHelp->GetHelpButton()->GetVisibility() == ESlateVisibility::Visible);
    Panel->bWaitingForConfirmation = false;
    Panel->PendingPanelPreset = MakeShared<const FSFRestorePreset>();
    Panel->ShowRestorePanel(true);
    TestEqual(TEXT("Asynchronous preset handoff blocks task changes until ready"), Panel->TaskContentSwitcher->GetActiveWidgetIndex(), 0);
    Panel->PendingPanelPreset.Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFContextHelpTest, "SmartFoundations.Interface.ContextHelp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFContextHelpTest::RunTest(const FString&)
{
    auto* Help = SFPanelStyle::MakeHelp(GetTransientPackage(), TEXT("WorkflowHelpTest"), FText::FromString(TEXT("How to use")), FText::FromString(TEXT("Summary stays available")), FText::FromString(TEXT("Detailed workflow")));
    TestFalse(TEXT("Help starts collapsed and does not interrupt work"), Help->GetIsExpanded());
    auto* Button = Help->GetHelpButton();
    auto* Caption = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
    TestTrue(TEXT("Help is a real button without literal brackets"), Caption && Caption->GetText().ToString() == TEXT("?"));
    auto* Context = NewObject<UTextBlock>(); Context->SetText(FText::FromString(TEXT("Building name")));
    auto* Header = Cast<UHorizontalBox>(SFPanelStyle::MakeHelpLabelRow(GetTransientPackage(), Context, Help));
    TestTrue(TEXT("Context label and help button share one row"), Header && Header->GetChildAt(0) == Context && Header->GetChildAt(1) == Button);
    TestTrue(TEXT("Collapsed details have no layout height"), Help->GetVisibility() == ESlateVisibility::Collapsed);
    auto* Layout = NewObject<UVerticalBox>();
    Layout->AddChildToVerticalBox(Header);
    Layout->AddChildToVerticalBox(Help);
    auto NativeLayout = Layout->TakeWidget();
    NativeLayout->SlatePrepass();
    const float CollapsedHeight = NativeLayout->GetDesiredSize().Y;
    const FString Guidance = Button->GetToolTipText().ToString();
    TestTrue(TEXT("Hover guidance keeps the topic, summary and details"), Guidance.Contains(TEXT("How to use")) && Guidance.Contains(TEXT("Summary stays available")) && Guidance.Contains(TEXT("Detailed workflow")));
    Button->OnClicked.Broadcast();
    TestTrue(TEXT("Clicking the real button opens separate inline details"), Help->GetIsExpanded());
    NativeLayout->SlatePrepass();
    TestTrue(TEXT("Only expanded guidance adds body height"), NativeLayout->GetDesiredSize().Y > CollapsedHeight);
    TestEqual(TEXT("Guidance remains complete"), Help->GetText().ToString(), Guidance);
    Button->OnClicked.Broadcast();
    TestFalse(TEXT("Second click collapses the details without removing the context label"), Help->GetIsExpanded());
    NativeLayout->SlatePrepass();
    TestEqual(TEXT("Collapsing removes the entire details row"), NativeLayout->GetDesiredSize().Y, CollapsedHeight);
    TestTrue(TEXT("Button stays beside its label"), Header->GetChildAt(1) == Button);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFInterfaceOwnershipTest, "SmartFoundations.Interface.OwnershipDrafts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFInterfaceOwnershipTest::RunTest(const FString&)
{
    auto* Walk = NewObject<USFWalkPanelWidget>();
    Walk->CachedSubsystem = NewObject<USFSubsystem>();
    Walk->bApplyImmediately = false;
    const float Expected[] = {24.f, 45.f, 3.f, -2.f};
    for (int32 Field = 0; Field < 4; ++Field) Walk->ApplyCellEditLive(0, Field, Expected[Field]);
    Walk->RequestScalingReturn();
    Walk->OnKeepWalking();
    FSFWalkSegmentView View;
    View.Advance = 800; View.TurnDegrees = 0; View.Rise = 0; View.Shift = 0;
    // Exercise the actual row factory repeatedly, as task warnings/reopening do.
    for (int32 Refresh = 0; Refresh < 2; ++Refresh)
    {
        auto* Border = Cast<UBorder>(Walk->MakeSegmentRow(View, 56));
        auto* Row = Border ? Cast<UHorizontalBox>(Border->GetContent()) : nullptr;
        if (!TestNotNull(TEXT("Actual segment row"), Row)) return false;
        for (int32 Field = 0; Field < 4; ++Field)
        {
            auto* Box = Cast<USizeBox>(Row->GetChildAt(Field + 1));
            auto* Spin = Box ? Cast<USpinBox>(Box->GetContent()) : nullptr;
            if (TestNotNull(TEXT("Native draft editor"), Spin))
                TestEqual(TEXT("Visible values retain the draft that Apply will consume"), Spin->GetValue(), Expected[Field]);
        }
    }
    TestEqual(TEXT("Drawing the draft must not consume it"), Walk->PendingEdits.Num(), 4);
    TestEqual(TEXT("Drawing must not apply staged geometry to the route view"), View.Advance, 800.f);

    auto* Panel = NewObject<USmartSettingsFormWidget>();
    auto* Subsystem = NewObject<USFSubsystem>();
    Panel->CachedSubsystem = Subsystem;
    Panel->bApplyImmediately = false;
    Panel->RotationAxisComboBox = NewObject<UComboBoxString>(Panel);
    Panel->RotationAxisComboBox->AddOption(TEXT("X")); Panel->RotationAxisComboBox->AddOption(TEXT("Y"));
    Panel->RotationAxisComboBox->SetSelectedIndex(1);
    const auto Before = Subsystem->GetCounterState();
    Panel->OnRotationAxisChanged(TEXT("Y"), ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Rotation progression stays unapplied with immediate mode off"), Subsystem->GetCounterState().RotationAxis == Before.RotationAxis);
    TestTrue(TEXT("Apply reads the same staged progression axis shown in the editor"), Panel->ReadPanelCounterState().RotationAxis == ESFScaleAxis::Y);
    const bool BeforeEnabled = Subsystem->GetAutoConnectRuntimeSettings().bEnabled;
    Panel->bRestoringSpinBoxValues = true;
    Panel->OnBeltEnabledChanged(!BeforeEnabled);
    Panel->bRestoringSpinBoxValues = false;
    TestEqual(TEXT("Populating native controls is not a live user edit"), Subsystem->GetAutoConnectRuntimeSettings().bEnabled, BeforeEnabled);
    Panel->bWaitingForConfirmation = true;
    Panel->SetActiveRestoreTab(1);
    TestEqual(TEXT("Restore subtabs cannot bypass a pending confirmation"), Panel->ActiveRestoreTabIndex, 0);
    Panel->bWaitingForConfirmation = false;
    Panel->PendingPanelPreset = MakeShared<const FSFRestorePreset>();
    Panel->SetActiveRestoreTab(1);
    TestEqual(TEXT("Restore subtabs cannot bypass a pending preset handoff"), Panel->ActiveRestoreTabIndex, 0);

    // Captions are presentation. Routing and direction must not parse English words.
    Panel->BeltRoutingModeComboBox = NewObject<UComboBoxString>(Panel);
    Panel->BeltRoutingModeComboBox->AddOption(TEXT("標準"));
    Panel->BeltRoutingModeComboBox->AddOption(TEXT("曲線"));
    Panel->BeltRoutingModeComboBox->AddOption(TEXT("直線"));
    Panel->OnBeltRoutingModeChanged(TEXT("直線"), ESelectInfo::OnMouseClick);
    TestEqual(TEXT("Localized belt caption preserves the native routing index"), Subsystem->GetAutoConnectRuntimeSettings().BeltRoutingMode, 2);
    Panel->OnBeltRoutingModeChanged(TEXT("Unknown option"), ESelectInfo::OnMouseClick);
    TestEqual(TEXT("An unknown caption cannot reset a valid route"), Subsystem->GetAutoConnectRuntimeSettings().BeltRoutingMode, 2);

    Panel->PipeRoutingModeComboBox = NewObject<UComboBoxString>(Panel);
    Panel->HypertubeRoutingModeComboBox = NewObject<UComboBoxString>(Panel);
    for (const TCHAR* Caption : {TEXT("自動"), TEXT("自動2D"), TEXT("直線"), TEXT("曲線"), TEXT("自由"), TEXT("水平から垂直")})
    {
        Panel->PipeRoutingModeComboBox->AddOption(Caption);
        Panel->HypertubeRoutingModeComboBox->AddOption(Caption);
    }
    Panel->OnPipeRoutingModeChanged(TEXT("水平から垂直"), ESelectInfo::OnMouseClick);
    Panel->OnHypertubeRoutingModeChanged(TEXT("曲線"), ESelectInfo::OnMouseClick);
    TestEqual(TEXT("Localized pipe caption preserves the native routing index"), Subsystem->GetAutoConnectRuntimeSettings().PipeRoutingMode, 5);
    TestEqual(TEXT("Localized Hypertube caption preserves the native routing index"), Subsystem->GetAutoConnectRuntimeSettings().HypertubeRoutingMode, 3);

    Panel->StackableBeltDirectionComboBox = NewObject<UComboBoxString>(Panel);
    Panel->StackableBeltDirectionComboBox->AddOption(TEXT("前方"));
    Panel->StackableBeltDirectionComboBox->AddOption(TEXT("後方"));
    Panel->OnStackableBeltDirectionChanged(TEXT("後方"), ESelectInfo::OnMouseClick);
    TestEqual(TEXT("Localized belt direction keeps reverse flow"), Subsystem->GetAutoConnectRuntimeSettings().StackableBeltDirection, 1);
    Panel->PowerGridAxisComboBox = NewObject<UComboBoxString>(Panel);
    Panel->PowerGridAxisComboBox->AddOption(TEXT("自動"));
    Panel->PowerGridAxisComboBox->AddOption(TEXT("X"));
    TestEqual(TEXT("Localized automatic power selection resolves by index"), Panel->DisplayStringToPowerGridAxis(TEXT("自動")), 0);
    Panel->CachedBeltTiers = {0, 1, 3};
    Panel->CachedPipeTiers = {0, 2};
    TestEqual(TEXT("Belt tier captions round-trip without English parsing"), Panel->DisplayStringToBeltTier(Panel->BeltTierToDisplayString(3)), 3);
    TestEqual(TEXT("Pipe tier captions round-trip without English parsing"), Panel->DisplayStringToPipeTier(Panel->PipeTierToDisplayString(2)), 2);
    return true;
}
#endif
