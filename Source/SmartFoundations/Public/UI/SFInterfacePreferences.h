// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

class UUserWidget;
class UWidget;
class UTextBlock;
class UWorld;
struct FPointerEvent;

/** Local presentation preferences. No replicated state or world-save data. */
namespace SFInterface
{
    struct FHudAppearance
    {
        FVector2D Position{0.02, 0.25};
        float Scale = 1;
        int32 Theme = 0;
    };
    SMARTFOUNDATIONS_API FHudAppearance ReadHud(UObject* Context, bool bDefaults = false);
    SMARTFOUNDATIONS_API bool WriteHud(UObject* Context, const FHudAppearance& Value);
    SMARTFOUNDATIONS_API FVector2D ClampPosition(FVector2D Position, FVector2D Size, FVector2D Viewport);
    SMARTFOUNDATIONS_API void RestorePanel(UUserWidget* Owner, UWidget* Frame, FName Id, bool& bRestored);
    SMARTFOUNDATIONS_API void SavePanel(UUserWidget* Owner, UWidget* Frame, FName Id);
    SMARTFOUNDATIONS_API void ClampPanel(UUserWidget* Owner, UWidget* Frame);
    SMARTFOUNDATIONS_API void ResetPanelPositions();
    SMARTFOUNDATIONS_API bool IsHeaderHit(UWidget* Header, const FPointerEvent& Event);
}
