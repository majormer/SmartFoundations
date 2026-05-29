// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 * Centralized Smart! UI font.
 *
 * Smart UI text historically used the engine default font (/Engine/EngineFonts/Roboto),
 * which has no Arabic, Persian, or Thai glyphs — so those (in-game supported) languages
 * rendered as garbage/tofu. This helper returns text styled with the in-game multi-script
 * font (/Game/FactoryGame/Interface/Font/FactoryFont), matching the base game and covering
 * every language Satisfactory ships. Falls back to the engine default if the game font
 * cannot be loaded, so text is never invisible.
 *
 * All Smart UI (HUD, Smart Upgrade panel, settings/config widgets) should route font
 * creation through here instead of grabbing the default UMG TextBlock font.
 */
namespace SFFont
{
    /** FSlateFontInfo for Smart UI text at the given point size, using the in-game FactoryFont. */
    SMARTFOUNDATIONS_API FSlateFontInfo Get(int32 Size);
}
