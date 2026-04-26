#include "Services/SFAxisLabelProvider.h"

// ========================================
// Full Labels
// ========================================

FString USFAxisLabelProvider::GetAxisLabel(EAxis::Type Axis, bool bPlayerRelative)
{
    if (bPlayerRelative)
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("Left/Right");
            case EAxis::Y: return TEXT("Forward/Back");
            case EAxis::Z: return TEXT("Up/Down");
            default: return TEXT("?");
        }
    }
    else
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("X");
            case EAxis::Y: return TEXT("Y");
            case EAxis::Z: return TEXT("Z");
            default: return TEXT("?");
        }
    }
}

FString USFAxisLabelProvider::GetPrefixedAxisLabel(const FString& Prefix, EAxis::Type Axis, bool bPlayerRelative)
{
    return FString::Printf(TEXT("%s %s:"), *Prefix, *GetAxisLabel(Axis, bPlayerRelative));
}

// ========================================
// Short Labels
// ========================================

FString USFAxisLabelProvider::GetAxisLabelShort(EAxis::Type Axis, bool bPlayerRelative)
{
    if (bPlayerRelative)
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("L/R");
            case EAxis::Y: return TEXT("F/B");
            case EAxis::Z: return TEXT("U/D");
            default: return TEXT("?");
        }
    }
    else
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("X");
            case EAxis::Y: return TEXT("Y");
            case EAxis::Z: return TEXT("Z");
            default: return TEXT("?");
        }
    }
}

// ========================================
// Direction Labels
// ========================================

FString USFAxisLabelProvider::GetPositiveDirectionLabel(EAxis::Type Axis, bool bPlayerRelative)
{
    if (bPlayerRelative)
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("Right");
            case EAxis::Y: return TEXT("Forward");
            case EAxis::Z: return TEXT("Up");
            default: return TEXT("+?");
        }
    }
    else
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("+X");
            case EAxis::Y: return TEXT("+Y");
            case EAxis::Z: return TEXT("+Z");
            default: return TEXT("+?");
        }
    }
}

FString USFAxisLabelProvider::GetNegativeDirectionLabel(EAxis::Type Axis, bool bPlayerRelative)
{
    if (bPlayerRelative)
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("Left");
            case EAxis::Y: return TEXT("Back");
            case EAxis::Z: return TEXT("Down");
            default: return TEXT("-?");
        }
    }
    else
    {
        switch (Axis)
        {
            case EAxis::X: return TEXT("-X");
            case EAxis::Y: return TEXT("-Y");
            case EAxis::Z: return TEXT("-Z");
            default: return TEXT("-?");
        }
    }
}

FString USFAxisLabelProvider::GetDirectionLabel(EAxis::Type Axis, bool bPositive, bool bPlayerRelative)
{
    return bPositive 
        ? GetPositiveDirectionLabel(Axis, bPlayerRelative) 
        : GetNegativeDirectionLabel(Axis, bPlayerRelative);
}

// ========================================
// Mode Labels
// ========================================

FString USFAxisLabelProvider::GetControlModeName(bool bPlayerRelative)
{
    return bPlayerRelative ? TEXT("Player-Relative") : TEXT("Hologram-Relative");
}

FString USFAxisLabelProvider::GetControlModeIndicator(bool bPlayerRelative)
{
    return bPlayerRelative ? TEXT("[Player]") : TEXT("[Hologram]");
}

// ========================================
// Formatted Value Strings
// ========================================

FString USFAxisLabelProvider::FormatAxisValue(EAxis::Type Axis, int32 Value, const FString& Unit, bool bPlayerRelative)
{
    return FString::Printf(TEXT("%s: %d%s"), 
        *GetAxisLabel(Axis, bPlayerRelative), 
        Value, 
        *Unit);
}

FString USFAxisLabelProvider::FormatAxisValueShort(EAxis::Type Axis, int32 Value, bool bPlayerRelative)
{
    return FString::Printf(TEXT("%s: %d"), 
        *GetAxisLabelShort(Axis, bPlayerRelative), 
        Value);
}

// ========================================
// Grid Display Helpers
// ========================================

FString USFAxisLabelProvider::FormatGridLine(int32 X, int32 Y, int32 Z, bool bPlayerRelative)
{
    if (bPlayerRelative)
    {
        // Include mode indicator when in player-relative mode
        return FString::Printf(TEXT("Grid: %d x %d x %d [Player]"), X, Y, Z);
    }
    else
    {
        return FString::Printf(TEXT("Grid: %d x %d x %d"), X, Y, Z);
    }
}

FString USFAxisLabelProvider::FormatTriAxisLine(const FString& Prefix, int32 X, int32 Y, int32 Z, const FString& Unit, bool bPlayerRelative)
{
    const FString XLabel = GetAxisLabelShort(EAxis::X, bPlayerRelative);
    const FString YLabel = GetAxisLabelShort(EAxis::Y, bPlayerRelative);
    const FString ZLabel = GetAxisLabelShort(EAxis::Z, bPlayerRelative);
    
    return FString::Printf(TEXT("%s %s: %d%s  %s: %d%s  %s: %d%s"),
        *Prefix,
        *XLabel, X, *Unit,
        *YLabel, Y, *Unit,
        *ZLabel, Z, *Unit);
}

FString USFAxisLabelProvider::FormatDualAxisLine(const FString& Prefix, int32 X, int32 Y, const FString& Unit, bool bPlayerRelative)
{
    const FString XLabel = GetAxisLabelShort(EAxis::X, bPlayerRelative);
    const FString YLabel = GetAxisLabelShort(EAxis::Y, bPlayerRelative);
    
    return FString::Printf(TEXT("%s %s: %d%s  %s: %d%s"),
        *Prefix,
        *XLabel, X, *Unit,
        *YLabel, Y, *Unit);
}
