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
    
    // Update belt cost display (for auto-connect preview)
    void UpdateBeltCosts(const TArray<struct FItemAmount>& BeltCosts, const TArray<struct FItemAmount>& DistributorCosts, class UFGInventoryComponent* PlayerInventory = nullptr, class AFGCentralStorageSubsystem* CentralStorage = nullptr);
    
    // Clear belt cost display
    void ClearBeltCosts();

    // Update pipe cost display (for auto-connect preview)
    void UpdatePipeCosts(const TArray<struct FItemAmount>& PipeCosts, class UFGInventoryComponent* PlayerInventory = nullptr, class AFGCentralStorageSubsystem* CentralStorage = nullptr);
    
    // Clear pipe cost display
    void ClearPipeCosts();

    // Update power line cost display (for auto-connect preview)
    void UpdatePowerCosts(const TArray<struct FItemAmount>& PowerCosts, class UFGInventoryComponent* PlayerInventory = nullptr, class AFGCentralStorageSubsystem* CentralStorage = nullptr);
    
    // Clear power line cost display
    void ClearPowerCosts();

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
    
    // Cached belt cost text (separate section)
    FString CurrentBeltCostText;
    
    // Cached belt availability (for color rendering)
    TArray<int32> CachedBeltAvailability;

    // Cached pipe cost text (separate section)
    FString CurrentPipeCostText;
    
    // Cached pipe cost items (for icon rendering)
    TArray<FItemAmount> CachedPipeCosts;
    
    // Cached pipe availability (for color rendering)
    TArray<int32> CachedPipeAvailability;

    // Cached power line cost text (separate section)
    FString CurrentPowerCostText;
    
    // Cached power line cost items (for icon rendering)
    TArray<FItemAmount> CachedPowerCosts;
    
    // Cached power line availability (for color rendering)
    TArray<int32> CachedPowerAvailability;

    // Cached lift height values (for conveyor/pipe lifts)
    float CachedLiftHeight = 0.0f;
    float CachedWorldHeight = 0.0f;

    // HUD scale and config cache
    float HUDScaleMultiplier = 1.5f;
    FSmart_ConfigStruct CachedConfig;
};
