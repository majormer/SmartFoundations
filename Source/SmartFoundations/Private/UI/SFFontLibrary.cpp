// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "UI/SFFontLibrary.h"
#include "SmartFoundations.h"
#include "SFLogMacros.h"
#include "Engine/Font.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "Components/EditableTextBox.h"

namespace SFFont
{
    // The in-game RUNTIME multi-script UI font. Base-game content, always present at runtime,
    // so a soft load by path is sufficient (it is not packaged into the mod). This is the game's
    // composite "description" font: a default Latin typeface plus Noto sub-typefaces
    // (Arabic/Persian, Thai, CJK, Bengali, Hebrew, ...) giving full script coverage WITH runtime
    // HarfBuzz shaping.
    //
    // NOTE: the stylized FactoryFont is an OFFLINE font (baked glyph atlas, no shaping), so it
    // physically cannot render Arabic/Persian/Thai (they show as tofu) — which is exactly the set
    // of languages this work re-enabled. The game itself uses this DescriptionText font for its
    // localized UI text, so we match it.
    static const TCHAR* GUIFontPath = TEXT("/Game/FactoryGame/Interface/Font/DescriptionText.DescriptionText");

    static UFont* ResolveUIFont()
    {
        static TWeakObjectPtr<UFont> Cached;
        if (UFont* Existing = Cached.Get())
        {
            return Existing;
        }
        UFont* Loaded = LoadObject<UFont>(nullptr, GUIFontPath);
        if (!Loaded)
        {
            UE_LOG(LogSmartUI, Verbose,
                TEXT("SFFont: could not load in-game font %s — falling back to engine default; non-Latin text may not render."),
                GUIFontPath);
        }
        Cached = Loaded;
        return Loaded;
    }

    FSlateFontInfo Get(int32 Size)
    {
        FSlateFontInfo Info;
        Info.Size = Size;

        if (UFont* UIFont = ResolveUIFont())
        {
            Info.FontObject = UIFont;
            Info.TypefaceFontName = NAME_None;  // use the composite's default typeface + script fallback
        }
        else
        {
            // Keep text visible if the game font is unavailable (e.g. unexpected content layout).
            Info.FontObject = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
            Info.TypefaceFontName = TEXT("Regular");
        }

        return Info;
    }

    void ApplyToWidgetTree(::UWidgetTree* Tree)
    {
        if (!Tree)
        {
            return;
        }
        Tree->ForEachWidget([](UWidget* W)
        {
            if (UTextBlock* Text = Cast<UTextBlock>(W))
            {
                // Preserve the designer's size; swap only the font family to FactoryFont.
                Text->SetFont(Get(Text->GetFont().Size));
            }
            else if (USpinBox* Spin = Cast<USpinBox>(W))
            {
                Spin->SetFont(Get(Spin->GetFont().Size));
            }
            else if (UEditableTextBox* Edit = Cast<UEditableTextBox>(W))
            {
                // Preset-name field: localized/user input needs the multi-script font too.
                // FEditableTextBoxStyle keeps its font in TextStyle.Font (SetFont sets that).
                Edit->WidgetStyle.SetFont(Get(Edit->WidgetStyle.TextStyle.Font.Size));
            }
            // NOTE: UComboBoxString font cannot be set here — its InitFont() is protected and the
            // Font property is construction-only. The dropdown font is set on the blueprint asset
            // (each ComboBoxString's Font property) instead, so it bakes in at construction.
        });
    }
}
