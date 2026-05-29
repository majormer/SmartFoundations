#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Config/Smart_ConfigStruct.h"
#include "SFHudService.generated.h"

// Forward declarations

class USFSubsystem;
class AHUD;
class USFHudWidget;

UCLASS()
class SMARTFOUNDATIONS_API USFHudService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(USFSubsystem* InSubsystem);

    // HUD lifecycle
    void InitializeWidgets();
    void EnsureHUDBinding();
    void CleanupWidgets();

    // Build formatted HUD display lines
    TPair<FString, FString> BuildCounterDisplayLines() const;

    // Update HUD text (owned by this service)
    void UpdateWidgetDisplay(const FString& FirstLine, const FString& SecondLine);
    
    // Update lift height display (for conveyor lifts and pipe lifts)
    void UpdateLiftHeight(float LiftHeight, float WorldHeight);
    
    // Clear lift height display
    void ClearLiftHeight();

    // Draw callback bound to HUD PostRender
    void DrawCounterToHUD(AHUD* HUD, class UCanvas* Canvas);

    // Temporarily suppress HUD drawing (e.g., when settings form is open)
    void SetHUDSuppressed(bool bSuppressed) { bHUDSuppressed = bSuppressed; }
    bool IsHUDSuppressed() const { return bHUDSuppressed; }

    // Reset all cached HUD state (called when hologram changes)
    void ResetState();

private:
    // Runtime suppression flag (settings form, etc.)
    bool bHUDSuppressed = false;
    // Owner
    TWeakObjectPtr<USFSubsystem> Subsystem;

    // Cached HUD binding (kept for tick callback)
    TWeakObjectPtr<AHUD> CachedHUD;

    // UMG widget (Issue #179 — replaces Canvas drawing for crisp text)
    UPROPERTY()
    USFHudWidget* HudWidget = nullptr;

    void CreateHudWidget();
    void DestroyHudWidget();

    // Cached current counter text (multi-line)
    FString CurrentCounterText;
    
    // Cached lift height values (for conveyor/pipe lifts)
    float CachedLiftHeight = 0.0f;
    float CachedWorldHeight = 0.0f;

    // HUD scale and config cache
    float HUDScaleMultiplier = 1.5f;
    FSmart_ConfigStruct CachedConfig;
};
