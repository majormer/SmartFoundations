// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFPanelStyle.h"
#include "SmartFoundations.h"
#include "UI/SFContextHelp.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "UI/SFFontLibrary.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ComboBoxString.h"
#include "Widgets/Input/SButton.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Layout/SBox.h"

namespace
{
    // Keep a disabled native control disabled for hit testing, focus and actions.
    // Only its value's paint path bypasses Slate's second gray wash; the native
    // disabled brush still distinguishes the control from an available one.
    class SSFReadableValue : public SBox
    {
    public:
        SLATE_BEGIN_ARGS(SSFReadableValue) {} SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
        void Construct(const FArguments& Args)
        {
            SBox::Construct(SBox::FArguments()[Args._Content.Widget]);
        }
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip,
            FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
        {
            FWidgetStyle ValueStyle = Style;
            ValueStyle.SetForegroundColor(SFPanelStyle::NearBlackText);
            return SBox::OnPaint(Args, Geometry, Clip, Elements, Layer, ValueStyle, true);
        }
    };
}

UWidget* SFPanelStyle::WrapTabButton(UButton* Button)
{
    if (!Button) return nullptr;
    auto* Column = NewObject<UVerticalBox>(Button->GetOuter(), FName(Button->GetName() + TEXT("_Tab")));
    Button->RemoveFromParent();
    Column->AddChildToVerticalBox(Button);
    auto* Rule = MakeRule(Column, true);
    Rule->SetBrushColor(FLinearColor::Transparent);
    Column->AddChildToVerticalBox(Rule);
    return Column;
}

void SFPanelStyle::StyleTab(UButton* Button, bool bSelected)
{
    if (!Button) return;
    auto Style = MakeButtonStyle(false);
    Style.Normal.TintColor = FLinearColor::Transparent;
    Style.Hovered.TintColor = MutedPanel;
    Style.Pressed.TintColor = MutedPanel;
    Style.Disabled.TintColor = FLinearColor::Transparent;
    Button->SetStyle(Style); Button->SetBackgroundColor(FLinearColor::White);
    if (auto* Text = Cast<UTextBlock>(Button->GetChildAt(0))) Text->SetColorAndOpacity(bSelected ? Accent : LightText);
    if (auto* Column = Cast<UVerticalBox>(Button->GetParent()); Column && Column->GetChildrenCount() == 2)
        if (auto* Rule = Cast<UBorder>(Column->GetChildAt(1))) Rule->SetBrushColor(bSelected ? Accent : FLinearColor::Transparent);
}

UWidget* SFPanelStyle::MakeSectionHeading(UObject* Outer, FText Title, FText Scope)
{
    auto* Row = NewObject<UHorizontalBox>(Outer);
    auto* Label = NewObject<UTextBlock>(Outer, MakeUniqueObjectName(Outer, UTextBlock::StaticClass(), TEXT("SectionTitle"))); Label->SetFont(SFFont::Get(16));
    Label->SetText(Title); Label->SetColorAndOpacity(LightText); Label->SetAutoWrapText(true);
    Row->AddChildToHorizontalBox(Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Context = NewObject<UTextBlock>(Outer, MakeUniqueObjectName(Outer, UTextBlock::StaticClass(), TEXT("SectionScope"))); Context->SetFont(SFFont::Get(12));
    Context->SetText(Scope); Context->SetColorAndOpacity(DimText);
    Context->SetAutoWrapText(true);
    auto* ScopeSlot = Row->AddChildToHorizontalBox(Context);
    ScopeSlot->SetVerticalAlignment(VAlign_Center);
    ScopeSlot->SetPadding(FMargin(12, 0, 0, 0));
    if (!Scope.IsEmpty()) ScopeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    return Row;
}

UBorder* SFPanelStyle::MakeRule(UObject* Outer, bool bAccent)
{
    auto* Rule = NewObject<UBorder>(Outer);
    Rule->SetBrushColor(bAccent ? Accent : FLinearColor::FromSRGBColor(FColor(64,64,70)));
    Rule->SetPadding(FMargin(0));
    auto* Height = NewObject<USizeBox>(Outer); Height->SetHeightOverride(bAccent ? 2 : 1); Rule->SetContent(Height);
    return Rule;
}

void SFPanelStyle::StyleSpinBox(USpinBox* Spin)
{
    if (!Spin) return;
    FSpinBoxStyle Style = Spin->GetWidgetStyle();
    auto Brush = [](FLinearColor Color)
    {
        FSlateBrush Value; Value.DrawAs = ESlateBrushDrawType::Box; Value.TintColor = Color; return Value;
    };
    Style.SetBackgroundBrush(Brush(FLinearColor(.86f, .85f, .83f, 1)));
    Style.SetHoveredBackgroundBrush(Brush(FLinearColor(.96f, .94f, .91f, 1)));
    Style.SetActiveBackgroundBrush(Brush(FLinearColor::White));
    Style.SetInactiveFillBrush(Brush(FLinearColor(.86f, .85f, .83f, 1)));
    Style.SetActiveFillBrush(Brush(FLinearColor(.96f, .94f, .91f, 1)));
    Style.SetTextPadding(FMargin(6, 3));
    Spin->SetWidgetStyle(Style);
    Spin->SetForegroundColor(FSlateColor(NearBlackText));
    Spin->SetJustification(ETextJustify::Right);
}

void SFPanelStyle::ApplyToTree(UWidgetTree* Tree)
{
    if (!Tree) return;
    Tree->ForEachWidget([](UWidget* Widget)
    {
        // Inline help owns its smaller button and guidance typography.
        if (Widget->IsA<USFContextHelp>() || Widget->GetTypedOuter<USFContextHelp>()) return;
        const FString Name = Widget->GetName();
        if (auto* Text = Cast<UTextBlock>(Widget))
        {
            auto Font = Text->GetFont(); Font.Size = (Name == TEXT("TitleText") || Name == TEXT("RestorePresentationTitle") || Name == TEXT("ContextHeaderText")) ? 22 : Name.StartsWith(TEXT("SectionTitle")) ? 16 : Name.StartsWith(TEXT("SectionScope")) ? 12 : 14; Text->SetFont(Font);
        }
        if (auto* Spin = Cast<USpinBox>(Widget)) StyleSpinBox(Spin);
        if (auto* Combo = Cast<UComboBoxString>(Widget))
        {
            auto Brush = [](FColor Color)
            {
                FSlateBrush Value; Value.DrawAs = ESlateBrushDrawType::Box;
                Value.TintColor = FLinearColor::FromSRGBColor(Color); return Value;
            };
            auto Style = Combo->GetWidgetStyle();
            auto& Button = Style.ComboButtonStyle.ButtonStyle;
            Button.SetNormal(Brush(FColor(240,238,234)));
            Button.SetHovered(Brush(FColor(255,246,233)));
            Button.SetPressed(Brush(FColor(255,255,255)));
            // A real disabled brush avoids Slate's automatic gray wash; the
            // native enabled predicate still owns interaction and eligibility.
            Button.SetDisabled(Brush(FColor(182,182,188)));
            Button.SetNormalForeground(NearBlackText);
            Button.SetHoveredForeground(NearBlackText);
            Button.SetPressedForeground(NearBlackText);
            Button.SetDisabledForeground(NearBlackText);
            Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor::UseForeground();
            Style.ComboButtonStyle.MenuBorderBrush = Brush(FColor(240,238,234));
            Combo->SetWidgetStyle(Style);
            // UComboBoxString only invalidates layout on SetWidgetStyle. An
            // already-built native button also caches its disabled brush and
            // foreground attributes; notify that button without rebuilding it.
            TFunction<void(TSharedRef<SWidget>)> RefreshButton = [&](TSharedRef<SWidget> SlateWidget)
            {
                if (SlateWidget->GetType() == TEXT("SButton"))
                {
                    auto ButtonWidget = StaticCastSharedRef<SButton>(SlateWidget);
                    ButtonWidget->SetButtonStyle(&Combo->GetWidgetStyle().ComboButtonStyle.ButtonStyle);
                    if (ButtonWidget->GetContent()->GetType() != TEXT("SSFReadableValue"))
                        ButtonWidget->SetContent(SNew(SSFReadableValue)[ButtonWidget->GetContent()]);
                    return;
                }
                FChildren* Children = SlateWidget->GetChildren();
                for (int32 Index = 0; Index < Children->Num(); ++Index) RefreshButton(Children->GetChildAt(Index));
            };
            RefreshButton(Combo->TakeWidget());
            auto Item = Combo->GetItemStyle();
            Item.SetTextColor(NearBlackText).SetSelectedTextColor(NearBlackText);
            Item.SetEvenRowBackgroundBrush(Brush(FColor(240,238,234)));
            Item.SetOddRowBackgroundBrush(Brush(FColor(240,238,234)));
            Item.SetEvenRowBackgroundHoveredBrush(Brush(FColor(255,224,187)));
            Item.SetOddRowBackgroundHoveredBrush(Brush(FColor(255,224,187)));
            Item.SetActiveBrush(Brush(FColor(229,148,69)));
            Item.SetActiveHoveredBrush(Brush(FColor(246,174,100)));
            Item.SetInactiveBrush(Brush(FColor(229,148,69)));
            Item.SetInactiveHoveredBrush(Brush(FColor(246,174,100)));
            Combo->SetItemStyle(Item);
        }
        if (auto* Input = Cast<UEditableTextBox>(Widget)) {Input->WidgetStyle.SetFont(SFFont::Get(14)); StyleInputLight(Input);}
        if (auto* Button = Cast<UButton>(Widget))
        {
            if (Button->GetParent() && Button->GetParent()->GetName().EndsWith(TEXT("_Tab"))) return;
            const bool Primary = Name == TEXT("ApplyBtn") || Name == TEXT("SharedUpgradeButton") || Name == TEXT("LoadToPanelBtn") || Name == TEXT("ModuleApplyBtn") || Button->GetStyle().Normal.TintColor.GetSpecifiedColor().Equals(Accent);
            Button->SetStyle(MakeButtonStyle(Primary));
            Button->SetBackgroundColor(FLinearColor::White);
            if (auto* Text = Cast<UTextBlock>(Button->GetChildAt(0)))
                Text->SetColorAndOpacity(Primary ? NearBlackText : Name.Contains(TEXT("Delete")) ? FLinearColor(1, .55f, .55f, 1) : LightText);
            if (Primary)
            {
                // Keep the native disabled action unavailable, but its dark
                // label legible on the distinct disabled primary brush.
                auto NativeWidget = Button->TakeWidget();
                if (NativeWidget->GetType() == TEXT("SButton"))
                {
                    auto NativeButton = StaticCastSharedRef<SButton>(NativeWidget);
                    if (NativeButton->GetContent()->GetType() != TEXT("SSFReadableValue"))
                        NativeButton->SetContent(SNew(SSFReadableValue)[NativeButton->GetContent()]);
                }
            }
        }
        if (auto* Border = Cast<UBorder>(Widget); Border && (Name == TEXT("BackgroundPanel") || Name == TEXT("BackgroundBorder") || Name == TEXT("WalkBackdrop") || Name == TEXT("RestoreSidePanel")))
        {
            FSlateBrush Solid; Solid.DrawAs = ESlateBrushDrawType::Box; Solid.TintColor = FLinearColor::White;
            Border->SetBrush(Solid); Border->SetBrushColor(FLinearColor::FromSRGBColor(FColor(21,21,24)));
            Border->SetRenderOpacity(1);
        }
    });
    // Align contextual value columns without replacing the native dropdowns.
    TArray<UHorizontalBox*> Rows;
    Tree->ForEachWidget([&](UWidget* Widget)
    {
        if (auto* Row = Cast<UHorizontalBox>(Widget); Row && Row->GetName().EndsWith(TEXT("Row")) && Row->GetName() != TEXT("RotationZRow"))
            for (UWidget* Child : Row->GetAllChildren()) if (Cast<UComboBoxString>(Child)) {Rows.Add(Row); break;}
    });
    for (UHorizontalBox* Row : Rows)
    {
        const auto Children = Row->GetAllChildren(); Row->ClearChildren();
        for (UWidget* Child : Children)
        {
            UWidget* Display = Child;
            if (Cast<UComboBoxString>(Child))
            {
                auto* Width = NewObject<USizeBox>(Row); Width->SetWidthOverride(230); Width->SetContent(Child); Display = Width;
            }
            auto* RowSlot = Row->AddChildToHorizontalBox(Display); RowSlot->SetVerticalAlignment(VAlign_Center); RowSlot->SetPadding(FMargin(0,4));
            if (Cast<UTextBlock>(Child)) RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }
}
