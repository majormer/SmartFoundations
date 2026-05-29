// Copyright Coffee Stain Studios. All Rights Reserved.

#include "UI/SFFontLibrary.h"
#include "SmartFoundations.h"
#include "Engine/Font.h"

namespace SFFont
{
    // The in-game multi-script UI font. Base-game content, always present at runtime, so a
    // soft load by path is sufficient (it is not packaged into the mod). Covers Latin, CJK,
    // Arabic, Persian, Thai, etc. — an offline (baked) font, so it has a single face and we
    // leave TypefaceFontName as NAME_None.
    static const TCHAR* GFactoryFontPath = TEXT("/Game/FactoryGame/Interface/Font/FactoryFont.FactoryFont");

    static UFont* ResolveFactoryFont()
    {
        static TWeakObjectPtr<UFont> Cached;
        if (UFont* Existing = Cached.Get())
        {
            return Existing;
        }
        UFont* Loaded = LoadObject<UFont>(nullptr, GFactoryFontPath);
        if (!Loaded)
        {
            UE_LOG(LogSmartFoundations, Warning,
                TEXT("SFFont: could not load in-game font %s — falling back to engine default; non-Latin text may not render."),
                GFactoryFontPath);
        }
        Cached = Loaded;
        return Loaded;
    }

    FSlateFontInfo Get(int32 Size)
    {
        FSlateFontInfo Info;
        Info.Size = Size;

        if (UFont* Factory = ResolveFactoryFont())
        {
            Info.FontObject = Factory;
            Info.TypefaceFontName = NAME_None;  // FactoryFont is an offline single-face font
        }
        else
        {
            // Keep text visible if the game font is unavailable (e.g. unexpected content layout).
            Info.FontObject = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
            Info.TypefaceFontName = TEXT("Regular");
        }

        return Info;
    }
}
