#pragma once

#include "CoreMinimal.h"
#include "SFAxisLabelProvider.generated.h"

/**
 * Provides axis labels that adapt based on control mode.
 * 
 * In Hologram-Relative mode: Uses traditional X, Y, Z labels
 * In Player-Relative mode: Uses intuitive Left/Right, Forward/Back, Up/Down labels
 * 
 * This ensures consistent labeling across HUD, Settings Form, and any other UI
 * that displays axis-related information.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFAxisLabelProvider : public UObject
{
    GENERATED_BODY()
    
public:
    // ========================================
    // Full Labels (for forms and detailed displays)
    // ========================================
    
    /**
     * Get the full display label for an axis.
     * 
     * @param Axis The axis (X, Y, or Z)
     * @param bPlayerRelative True for player-relative labels, false for hologram-relative
     * @return "X"/"Y"/"Z" or "Left/Right"/"Forward/Back"/"Up/Down"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetAxisLabel(EAxis::Type Axis, bool bPlayerRelative);
    
    /**
     * Get the full label with a prefix (e.g., "Grid X" or "Grid Left/Right").
     * 
     * @param Prefix The prefix text (e.g., "Grid", "Spacing", "Steps")
     * @param Axis The axis (X, Y, or Z)
     * @param bPlayerRelative True for player-relative labels
     * @return Combined label like "Grid X:" or "Spacing Left/Right:"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetPrefixedAxisLabel(const FString& Prefix, EAxis::Type Axis, bool bPlayerRelative);
    
    // ========================================
    // Short Labels (for compact HUD displays)
    // ========================================
    
    /**
     * Get a short label for compact displays.
     * 
     * @param Axis The axis (X, Y, or Z)
     * @param bPlayerRelative True for player-relative labels
     * @return "X"/"Y"/"Z" or "L/R"/"F/B"/"U/D"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetAxisLabelShort(EAxis::Type Axis, bool bPlayerRelative);
    
    // ========================================
    // Direction Labels (for signed values)
    // ========================================
    
    /**
     * Get the label for the positive direction of an axis.
     * 
     * @param Axis The axis (X, Y, or Z)
     * @param bPlayerRelative True for player-relative labels
     * @return "+X"/"+Y"/"+Z" or "Right"/"Forward"/"Up"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetPositiveDirectionLabel(EAxis::Type Axis, bool bPlayerRelative);
    
    /**
     * Get the label for the negative direction of an axis.
     * 
     * @param Axis The axis (X, Y, or Z)
     * @param bPlayerRelative True for player-relative labels
     * @return "-X"/"-Y"/"-Z" or "Left"/"Back"/"Down"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetNegativeDirectionLabel(EAxis::Type Axis, bool bPlayerRelative);
    
    /**
     * Get the direction label based on sign.
     * 
     * @param Axis The axis (X, Y, or Z)
     * @param bPositive True for positive direction, false for negative
     * @param bPlayerRelative True for player-relative labels
     * @return Appropriate direction label
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetDirectionLabel(EAxis::Type Axis, bool bPositive, bool bPlayerRelative);
    
    // ========================================
    // Mode Labels
    // ========================================
    
    /**
     * Get the display name for the current control mode.
     * 
     * @param bPlayerRelative True for player-relative mode
     * @return "Player-Relative" or "Hologram-Relative"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetControlModeName(bool bPlayerRelative);
    
    /**
     * Get a short indicator for the current control mode.
     * 
     * @param bPlayerRelative True for player-relative mode
     * @return "[Player]" or "[Hologram]"
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString GetControlModeIndicator(bool bPlayerRelative);
    
    // ========================================
    // Formatted Value Strings
    // ========================================
    
    /**
     * Format a value with its axis label.
     * Example: "X: 100cm" or "Left/Right: 100cm"
     * 
     * @param Axis The axis
     * @param Value The value to display
     * @param Unit The unit suffix (e.g., "cm", "")
     * @param bPlayerRelative True for player-relative labels
     * @return Formatted string
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString FormatAxisValue(EAxis::Type Axis, int32 Value, const FString& Unit, bool bPlayerRelative);
    
    /**
     * Format a value with short axis label (for compact displays).
     * Example: "X: 100" or "L/R: 100"
     * 
     * @param Axis The axis
     * @param Value The value to display
     * @param bPlayerRelative True for player-relative labels
     * @return Formatted string
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString FormatAxisValueShort(EAxis::Type Axis, int32 Value, bool bPlayerRelative);
    
    // ========================================
    // Grid Display Helpers
    // ========================================
    
    /**
     * Format a complete grid line (e.g., "Grid: 3 x 2 x 1" or with mode indicator).
     * 
     * @param X Grid X count
     * @param Y Grid Y count
     * @param Z Grid Z count
     * @param bPlayerRelative True to add mode indicator
     * @return Formatted grid string
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString FormatGridLine(int32 X, int32 Y, int32 Z, bool bPlayerRelative);
    
    /**
     * Format a spacing/steps/stagger line with all three axes.
     * 
     * @param Prefix Line prefix (e.g., "Spacing", "Steps", "Stagger")
     * @param X X-axis value
     * @param Y Y-axis value
     * @param Z Z-axis value
     * @param Unit Unit suffix (e.g., "cm")
     * @param bPlayerRelative True for player-relative labels
     * @return Formatted line
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString FormatTriAxisLine(const FString& Prefix, int32 X, int32 Y, int32 Z, const FString& Unit, bool bPlayerRelative);
    
    /**
     * Format a spacing/steps line with two axes (X and Y only).
     * 
     * @param Prefix Line prefix
     * @param X X-axis value
     * @param Y Y-axis value
     * @param Unit Unit suffix
     * @param bPlayerRelative True for player-relative labels
     * @return Formatted line
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Labels")
    static FString FormatDualAxisLine(const FString& Prefix, int32 X, int32 Y, const FString& Unit, bool bPlayerRelative);
};
