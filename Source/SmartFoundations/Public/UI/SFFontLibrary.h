// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 * Centralized Smart! UI font.
 *
 * Smart UI text historically used the engine default font (/Engine/EngineFonts/Roboto),
 * which has no Arabic, Persian, or Thai glyphs — so those (in-game supported) languages
 * rendered as garbage/tofu. This helper returns text styled with the in-game RUNTIME
 * multi-script font (/Game/FactoryGame/Interface/Font/DescriptionText), matching the base
 * game's localized UI and covering every language Satisfactory ships (Noto Arabic/Thai/CJK
 * fallbacks with runtime shaping). The stylized FactoryFont is offline/baked and cannot shape
 * Arabic/Persian/Thai, so it is intentionally NOT used. Falls back to the engine default if the
 * game font cannot be loaded, so text is never invisible.
 *
 * All Smart UI (HUD, Smart Upgrade panel, settings/config widgets) should route font
 * creation through here instead of grabbing the default UMG TextBlock font.
 */
namespace SFFont
{
    /** FSlateFontInfo for Smart UI text at the given point size, using the in-game FactoryFont. */
    SMARTFOUNDATIONS_API FSlateFontInfo Get(int32 Size);

    /**
     * Stamp the in-game FactoryFont onto every text widget (TextBlock, SpinBox) in a widget
     * tree, preserving each widget's current size. Call from a UserWidget's NativeConstruct so
     * designer-placed and localized labels render every supported script (Arabic/Persian/Thai/CJK)
     * instead of the engine default. Runtime-created text must still use Get() directly.
     */
    SMARTFOUNDATIONS_API void ApplyToWidgetTree(class UWidgetTree* Tree);
}
