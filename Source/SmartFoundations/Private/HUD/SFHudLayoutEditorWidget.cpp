// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "HUD/SFHudLayoutEditorWidget.h"
#include "SmartFoundations.h"
#include "HUD/SFHudWidget.h"
#include "UI/SFInterfaceLayoutSubsystem.h"
#include "UI/SFFontLibrary.h"
#include "UI/SFPanelStyle.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFHudService.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SmartInterface"
namespace
{
    class SHudLayoutHandle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SHudLayoutHandle) {}
            SLATE_ARGUMENT(FSimpleDelegate, Begin)
            SLATE_ARGUMENT(TFunction<void(FVector2D)>, Drag)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()
        void Construct(const FArguments& Args) { Begin = Args._Begin; Drag = Args._Drag; ChildSlot[Args._Content.Widget]; }
        virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
            Start = Event.GetScreenSpacePosition(); Begin.ExecuteIfBound(); return FReply::Handled().CaptureMouse(SharedThis(this));
        }
        virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent& Event) override
        {
            if (!HasMouseCapture()) return FReply::Unhandled();
            if (Drag) Drag(Event.GetScreenSpacePosition() - Start); return FReply::Handled();
        }
        virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& Event) override
        {
            return Event.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
        }
    private:
        FVector2D Start;
        FSimpleDelegate Begin;
        TFunction<void(FVector2D)> Drag;
    };
    const FButtonStyle& LayoutButtonStyle()
    {
        static const FButtonStyle Style = SFPanelStyle::MakeButtonStyle(false); return Style;
    }
}

USFHudLayoutEditorWidget::USFHudLayoutEditorWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    mUseKeyboard = true; mUseMouse = true; mHideCursorDuringCapture = false; mSupportsStacking = true;
    mDisableBuildGunActions = true; mDisablePlayerActions = true; mDisablePlayerEquipmentManagement = true;
    mInputToPassThrough.Add(TEXT("Smart.EditLayout"));
    mDesiredHorizontalAlignment = HAlign_Fill; mDesiredVerticalAlignment = VAlign_Fill;
    mDesiredAlignmentSize.SizeRule = ESlateSizeRule::Fill; mDesiredAlignmentSize.Value = 1;
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
void USFHudLayoutEditorWidget::Setup(USFInterfaceLayoutSubsystem* Owner, FSimpleDelegate Host)
{
    LayoutOwner = Owner; CloseHost = Host; Draft = SFInterface::ReadHud(this); mInteractObject = Owner;
    if (auto* Smart = USFSubsystem::Get(this))
    {
        HiddenHud = Smart->GetHudService();
        if (HiddenHud.IsValid()) HiddenHud->SetLayoutEditorActive(true);
    }
    Preview = CreateWidget<USFHudWidget>(GetOwningPlayer());
    Themes = {MakeShared<FText>(LOCTEXT("ThemeDefault", "Default")), MakeShared<FText>(LOCTEXT("ThemeDark", "Dark")),
        MakeShared<FText>(LOCTEXT("ThemeClassic", "Classic")), MakeShared<FText>(LOCTEXT("ThemeContrast", "High Contrast")),
        MakeShared<FText>(LOCTEXT("ThemeMinimal", "Minimal")), MakeShared<FText>(LOCTEXT("ThemeMono", "Monochrome"))};
}
FVector2D USFHudLayoutEditorWidget::ScreenSize() const
{
    return UWidgetLayoutLibrary::GetViewportSize(GetWorld()) / FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(GetWorld()));
}
FVector2D USFHudLayoutEditorWidget::FrameSize() const
{
    const FVector2D Size = Preview ? Preview->GetDesiredSize() : FVector2D::ZeroVector;
    return Size.X > 1 && Size.Y > 1 ? Size : FVector2D(320, 150) * Draft.Scale;
}
FVector2D USFHudLayoutEditorWidget::FramePosition() const
{
    FVector2D Position = SFInterface::ClampPosition(Draft.Position * ScreenSize(), FrameSize() + FVector2D(0,28), ScreenSize());
    // Reserve a reachable title bar above the HUD, rather than covering its first line.
    Position.Y = FMath::Clamp(Position.Y, 28., FMath::Max(28., ScreenSize().Y - FrameSize().Y - 28));
    return Position;
}
TSharedRef<SWidget> USFHudLayoutEditorWidget::RebuildWidget()
{
    if (!Preview) return Super::RebuildWidget();
    SAssignNew(Canvas, SCanvas).Visibility(EVisibility::SelfHitTestInvisible);
    auto TitleHandle = SNew(SHudLayoutHandle)
            .Begin(FSimpleDelegate::CreateWeakLambda(this, [this]{ DragStartPosition = FramePosition(); }))
            .Drag([this](FVector2D Delta){ const auto Size = ScreenSize(); if (Size.X > 1 && Size.Y > 1) Draft.Position = SFInterface::ClampPosition(DragStartPosition + Delta / FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(this)), FrameSize(), Size) / Size; })
            [SNew(SBorder).BorderBackgroundColor(SFPanelStyle::MutedPanel).Padding(FMargin(8, 2))[SNew(STextBlock).Font(SFFont::Get(13)).ColorAndOpacity(SFPanelStyle::Accent).Text(LOCTEXT("Unlocked", "Smart! HUD — unlocked"))]];
    auto ResizeHandle = SNew(SHudLayoutHandle)
            .Begin(FSimpleDelegate::CreateWeakLambda(this, [this]{ DragStartScale = Draft.Scale; DragStartSize = FrameSize(); }))
            .Drag([this](FVector2D Delta){ const FVector2D Logical = Delta / FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(this)); Draft.Scale = FMath::Clamp(DragStartScale * (1 + FVector2D::DotProduct(Logical, DragStartSize) / FMath::Max(1., DragStartSize.SizeSquared())), .5, 3.); })
            [SNew(SBorder).BorderBackgroundColor(SFPanelStyle::Accent).Padding(2)[SNew(STextBlock).Font(SFFont::Get(14)).ColorAndOpacity(SFPanelStyle::NearBlackText).Text(FText::FromString(TEXT("+")))]];
    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(28)[TitleHandle]]
        + SVerticalBox::Slot().AutoHeight()[Preview->TakeWidget()]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(SBox).WidthOverride(28).HeightOverride(28)[ResizeHandle]];
    auto Frame = SNew(SOverlay)
        + SOverlay::Slot()[Body];
    Canvas->AddSlot().Position_Lambda([this]{ return FramePosition() - FVector2D(0, 28); }).Size_Lambda([this]{ return FrameSize() + FVector2D(0, 56); })[Frame];

    TSharedPtr<SWidget> HelpButton;
    auto HelpDetails = SFPanelStyle::MakeSlateHelp(LOCTEXT("HudEditWorkflow", "HUD appearance always applies to all saves, independently of the Camera scope selector. Changes are previews until Done / Lock. Cancel restores your previous appearance. Reset is also a preview until saved. Building settings and Panel positions are unaffected."), HelpButton);
    auto Settings = SNew(SVerticalBox);
    Settings->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(SFFont::Get(18)).ColorAndOpacity(SFPanelStyle::Accent).Text(LOCTEXT("Title", "HUD appearance"))]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[HelpButton.ToSharedRef()]];
    Settings->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[SNew(STextBlock).Font(SFFont::Get(12)).ColorAndOpacity(SFPanelStyle::DimText).AutoWrapText(true).Text(LOCTEXT("Scope", "All saves · Drag the title bar. Resize at the lower corner."))];
    Settings->AddSlot().AutoHeight().Padding(0,0,0,12)[HelpDetails];
    Settings->AddSlot().AutoHeight().Padding(0, 4)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(SFFont::Get(14)).ColorAndOpacity(SFPanelStyle::LightText).Text(LOCTEXT("Scale", "Scale"))]
        + SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).Font(SFFont::Get(14)).MinValue(.5f).MaxValue(3.f).MinSliderValue(.5f).MaxSliderValue(3.f).Delta(.1f).MinFractionalDigits(1).MaxFractionalDigits(1).Value_Lambda([this]{return Draft.Scale;}).OnValueChanged_Lambda([this](float Value){Draft.Scale=Value;})]];
    Settings->AddSlot().AutoHeight().Padding(0, 4)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(SFFont::Get(14)).ColorAndOpacity(SFPanelStyle::LightText).Text(LOCTEXT("Theme", "Theme"))]
        + SHorizontalBox::Slot().FillWidth(1)[SNew(SComboBox<TSharedPtr<FText>>).OptionsSource(&Themes)
            .OnGenerateWidget_Lambda([](TSharedPtr<FText> Text){return SNew(STextBlock).Font(SFFont::Get(14)).Text(*Text);})
            .OnSelectionChanged_Lambda([this](TSharedPtr<FText> Text, ESelectInfo::Type){const int32 Index=Themes.Find(Text);if(Index!=INDEX_NONE)Draft.Theme=Index;})
            [SNew(STextBlock).Font(SFFont::Get(14)).Text_Lambda([this]{return *Themes[Draft.Theme];})]]];
    auto Button = [this](FText Text, TFunction<void()> Action)
    {
        return SNew(SButton).ButtonStyle(&LayoutButtonStyle()).ContentPadding(FMargin(10, 6))
            .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[SNew(STextBlock).Font(SFFont::Get(14)).ColorAndOpacity(SFPanelStyle::LightText).AutoWrapText(true).Justification(ETextJustify::Center).Text(Text)];
    };
    Settings->AddSlot().AutoHeight().Padding(0, 12, 0, 8)[Button(LOCTEXT("ResetHUD", "Reset HUD Appearance"), [this]{Draft=SFInterface::ReadHud(this,true);})];
    // The shared host owns the only Done/Cancel pair for HUD and camera drafts.
    if (!CloseHost.IsBound())
    {
    Settings->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,6,0)[Button(LOCTEXT("Cancel", "Cancel HUD changes"), [this]{Close(false);})]
        + SHorizontalBox::Slot().FillWidth(1)[Button(LOCTEXT("Done", "Done / Lock"), [this]{Close(true);})]];
    }
    Settings->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).Font(SFFont::Get(12)).ColorAndOpacity(FLinearColor(1,.4f,.4f,1)).AutoWrapText(true).Text_Lambda([this]{return Error;})];
    return SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot()[Canvas.ToSharedRef()]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(16)[SNew(SBox).WidthOverride_Lambda([this]{return FMath::Min(360.,FMath::Max(160.,ScreenSize().X-32));})
            [SNew(SBorder).BorderBackgroundColor(SFPanelStyle::MutedPanel).Padding(16)[Settings]]];
}
void USFHudLayoutEditorWidget::PreviewAppearance(const SFInterface::FHudAppearance& Value)
{
    if (bFinished || Value.Position.ContainsNaN() || !FMath::IsFinite(Value.Scale) || Value.Theme < 0 || Value.Theme >= Themes.Num()) return;
    Draft = Value;
    Draft.Scale = FMath::Clamp(Draft.Scale, .5f, 3.f);
    Draft.Position.X = FMath::Clamp(Draft.Position.X, 0., 1.);
    Draft.Position.Y = FMath::Clamp(Draft.Position.Y, 0., 1.);
}
void USFHudLayoutEditorWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!Preview || bFinished) return;
    Preview->SetHudScale(Draft.Scale); Preview->SetTheme(Draft.Theme);
    RefreshTime += DeltaTime;
    if (RefreshTime < .1f) return;
    RefreshTime = 0;
    TArray<FString> Lines;
    if (auto* Smart = USFSubsystem::Get(this); Smart && Smart->GetActiveHologram() && Smart->GetHudService())
    {
        Preview->SetPlayerRelative(Smart->IsPlayerRelativeEnabled());
        const auto Text = Smart->GetHudService()->BuildCounterDisplayLines();
        (Text.Key + TEXT("\n") + Text.Value).ParseIntoArrayLines(Lines, true);
    }
    if (Lines.IsEmpty()) Lines = {LOCTEXT("Sample", "Smart! HUD preview").ToString(), FText::Format(NSLOCTEXT("SmartFoundations", "HUD_Grid", "Grid: {0}x{1}x{2} ({3})"), 1, 3, 1, 3).ToString(), LOCTEXT("SampleSpacing", "Spacing: 8.0 m").ToString()};
    Preview->UpdateContent(Lines, nullptr);
}
bool USFHudLayoutEditorWidget::Finish(bool bCommit)
{
    if (bFinished) return true;
    if (bCommit)
    {
        const auto Size = ScreenSize();
        if (Size.X > 1 && Size.Y > 1) Draft.Position = FramePosition() / Size;
        if (!SFInterface::WriteHud(this, Draft)) {Error=LOCTEXT("SaveFailed", "Could not save HUD appearance. Try again or cancel.");return false;}
    }
    bFinished = true;
    if (HiddenHud.IsValid()) HiddenHud->SetLayoutEditorActive(false);
    return true;
}
void USFHudLayoutEditorWidget::Close(bool bCommit)
{
    const FSimpleDelegate Host = CloseHost;
    if (LayoutOwner && LayoutOwner->EndEdit(bCommit)) Host.ExecuteIfBound();
}
void USFHudLayoutEditorWidget::OnEscapePressed_Implementation() { Close(false); }
void USFHudLayoutEditorWidget::NativeDestruct()
{
    if (!bFinished) Finish(false);
    Super::NativeDestruct();
}
void USFHudLayoutEditorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren); Canvas.Reset();
}
#undef LOCTEXT_NAMESPACE
