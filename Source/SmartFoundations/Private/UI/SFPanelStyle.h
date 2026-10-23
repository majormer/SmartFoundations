// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "Components/EditableTextBox.h"
class UWidgetTree;
class USpinBox;
class UBorder;
class UButton;
class UWidget;
class USFContextHelp;
class SWidget;

/**
 * Shared Smart! panel visual language.
 *
 * The dark-backdrop / orange-accent / light-text scheme used by the Smart Panel and Smart Restore,
 * factored out here (2026-07-03) so every runtime-styled Smart panel (Restore, Upgrade, and future
 * ones) aligns to ONE palette + button + input style instead of drifting into per-file oranges and
 * greys. Native widget behavior and localized font selection remain owned by each panel.
 *
 * Usage: `#include "UI/SFPanelStyle.h"` then `SFPanelStyle::Accent`, `SFPanelStyle::MakeButtonStyle(bAccent)`,
 * `SFPanelStyle::StyleInputLight(Box)`. Apply button styles via `UButton::SetStyle` and colors via
 * `SetColorAndOpacity` / `SetBrushColor` - all reachable programmatically, so no Blueprint edits.
 */
namespace SFPanelStyle
{
	// ---- Palette ------------------------------------------------------------
	/** Smart! orange - accents, section headers, active-tab fill, selection highlight. */
	inline const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(229,148,69));
	/** Backdrop for detail/summary panes sitting on the dark panel. */
	inline const FLinearColor MutedPanel = FLinearColor::FromSRGBColor(FColor(32,32,36));
	/** Primary readable text on the dark panel. */
	inline const FLinearColor LightText(0.9f, 0.9f, 0.9f, 1.0f);
	/** Secondary / section-label text on the dark panel. */
	inline const FLinearColor DimText(0.73f, 0.73f, 0.76f, 1.0f);
	/** Label color for text sitting ON the orange accent fill (active tab / primary button). */
	inline const FLinearColor NearBlackText = FLinearColor::FromSRGBColor(FColor(24,24,27));

	// ---- Button -------------------------------------------------------------
	/**
	 * Dark Smart-Panel button style. bAccent = primary / active (orange fill, near-black label);
	 * otherwise a neutral dark fill with a light label. Replaces the BP light-grey button styles
	 * that produced grey-on-grey text (maintainer feedback 2026-07-03).
	 */
	inline FButtonStyle MakeButtonStyle(bool bAccent)
	{
		auto MakeBrush = [](const FLinearColor& Color)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(Color);
			return Brush;
		};

		FButtonStyle Style;
		Style.Normal = MakeBrush(bAccent ? Accent : FLinearColor::FromSRGBColor(FColor(46,46,52)));
		Style.Hovered = MakeBrush(bAccent ? FLinearColor::FromSRGBColor(FColor(246,174,100)) : FLinearColor::FromSRGBColor(FColor(65,65,73)));
		Style.Pressed = MakeBrush(bAccent ? Accent : FLinearColor::FromSRGBColor(FColor(76,76,84)));
		Style.Disabled = MakeBrush(bAccent ? FLinearColor::FromSRGBColor(FColor(174,174,180)) : FLinearColor::FromSRGBColor(FColor(46,46,52)));
		Style.SetDisabledForeground(bAccent ? NearBlackText : DimText);
		Style.NormalPadding = FMargin(10.0f, 6.0f);
		Style.PressedPadding = FMargin(10.0f, 7.0f, 10.0f, 5.0f);
		return Style;
	}

    /** Presentation only: preserve the native widget instances, fonts, ranges and delegates. */
    void ApplyToTree(UWidgetTree* Tree);
    void StyleSpinBox(USpinBox* Spin);
    UBorder* MakeRule(UObject* Outer, bool bAccent = false);
    UWidget* WrapTabButton(UButton* Button);
    void StyleTab(UButton* Button, bool bSelected);
    UWidget* MakeSectionHeading(UObject* Outer, FText Title, FText Scope = FText::GetEmpty());
    inline FButtonStyle MakeHelpButtonStyle()
    {
        FButtonStyle Style = MakeButtonStyle(false);
        Style.NormalPadding = FMargin(8, 3);
        Style.PressedPadding = FMargin(8, 4, 8, 2);
        return Style;
    }
    USFContextHelp* MakeHelp(UObject* Outer, FName Name, FText Title, FText Summary, FText Details);
    UWidget* MakeHelpLabelRow(UObject* Outer, UWidget* Label, USFContextHelp* Help);
    /** Matching compact help for native Slate settings: hover guidance, click to toggle details. */
    TSharedRef<SWidget> MakeSlateHelp(FText Guidance, TSharedPtr<SWidget>& OutButton);
    void ApplyControlHelp(UWidgetTree* Tree);

	// ---- Input --------------------------------------------------------------
	/**
	 * Light-fill / dark-text editable box: the BP inputs' dark fill left hint text grey-on-dark-grey.
	 * This matches the Smart Panel's light value boxes so hint text renders dark-on-light and stays
	 * legible. Calls SynchronizeProperties() to push the style onto an already-constructed BP-bound
	 * widget (no-op for a not-yet-constructed programmatic one).
	 */
	inline void StyleInputLight(UEditableTextBox* Input)
	{
		if (!Input)
		{
			return;
		}
		auto MakeBrush = [](const FLinearColor& Color)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(Color);
			return Brush;
		};
		Input->WidgetStyle.SetBackgroundImageNormal(MakeBrush(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
		Input->WidgetStyle.SetBackgroundImageHovered(MakeBrush(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
		Input->WidgetStyle.SetBackgroundImageFocused(MakeBrush(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
		Input->WidgetStyle.SetBackgroundImageReadOnly(MakeBrush(FLinearColor(0.55f, 0.55f, 0.55f, 1.0f)));
		Input->WidgetStyle.SetBackgroundColor(FSlateColor(FLinearColor::White));
		Input->WidgetStyle.SetForegroundColor(FSlateColor(NearBlackText));
		Input->WidgetStyle.SetReadOnlyForegroundColor(FSlateColor(NearBlackText));
		Input->WidgetStyle.SetFocusedForegroundColor(FSlateColor(NearBlackText));
		Input->WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(NearBlackText);
		Input->SynchronizeProperties();
	}
}
