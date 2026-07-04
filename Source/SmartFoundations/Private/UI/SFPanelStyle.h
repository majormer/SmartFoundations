// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "Components/EditableTextBox.h"

/**
 * Shared Smart! panel visual language.
 *
 * The dark-backdrop / orange-accent / light-text scheme used by the Smart Panel and Smart Restore,
 * factored out here (2026-07-03) so every runtime-styled Smart panel (Restore, Upgrade, and future
 * ones) aligns to ONE palette + button + input style instead of drifting into per-file oranges and
 * greys. Header-only: all definitions are `inline`, no .cpp / no new translation unit.
 *
 * Usage: `#include "UI/SFPanelStyle.h"` then `SFPanelStyle::Accent`, `SFPanelStyle::MakeButtonStyle(bAccent)`,
 * `SFPanelStyle::StyleInputLight(Box)`. Apply button styles via `UButton::SetStyle` and colors via
 * `SetColorAndOpacity` / `SetBrushColor` - all reachable programmatically, so no Blueprint edits.
 */
namespace SFPanelStyle
{
	// ---- Palette ------------------------------------------------------------
	/** Smart! orange - accents, section headers, active-tab fill, selection highlight. */
	inline const FLinearColor Accent(1.0f, 0.6f, 0.2f, 1.0f);
	/** Backdrop for detail/summary panes sitting on the dark panel. */
	inline const FLinearColor MutedPanel(0.05f, 0.05f, 0.08f, 0.95f);
	/** Primary readable text on the dark panel. */
	inline const FLinearColor LightText(0.9f, 0.9f, 0.9f, 1.0f);
	/** Secondary / section-label text on the dark panel. */
	inline const FLinearColor DimText(0.65f, 0.65f, 0.65f, 1.0f);
	/** Label color for text sitting ON the orange accent fill (active tab / primary button). */
	inline const FLinearColor NearBlackText(0.05f, 0.05f, 0.05f, 1.0f);

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
		Style.Normal = MakeBrush(bAccent ? FLinearColor(0.55f, 0.33f, 0.11f, 1.0f) : FLinearColor(0.13f, 0.13f, 0.17f, 1.0f));
		Style.Hovered = MakeBrush(bAccent ? FLinearColor(0.75f, 0.45f, 0.15f, 1.0f) : FLinearColor(0.24f, 0.24f, 0.30f, 1.0f));
		Style.Pressed = MakeBrush(Accent);
		Style.Disabled = MakeBrush(FLinearColor(0.09f, 0.09f, 0.11f, 1.0f));
		Style.NormalPadding = FMargin(6.0f, 3.0f);
		Style.PressedPadding = FMargin(6.0f, 4.0f, 6.0f, 2.0f);
		return Style;
	}

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
		Input->WidgetStyle.SetForegroundColor(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
		Input->WidgetStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)));
		Input->WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		Input->SynchronizeProperties();
	}
}
