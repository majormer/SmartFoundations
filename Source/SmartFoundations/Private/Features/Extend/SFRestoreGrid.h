// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "Features/Extend/SFExtendControlFrame.h"

/** Restore cell identity and placement, shared by preview and authoritative replay. */
namespace SFRestoreGrid
{
    inline FString Prefix(int32 X, int32 Y, int32 Z)
    {
        // Keep saved/base-layer identities stable; only additional layers need a third coordinate.
        return Z == 0 ? FString::Printf(TEXT("rr_%d_%d_"), X, Y)
            : FString::Printf(TEXT("rr_%d_%d_%d_"), X, Y, Z);
    }

    inline bool ParsePrefix(const FString& PrefixText, FIntVector& OutCell)
    {
        if (!PrefixText.StartsWith(TEXT("rr_")) || !PrefixText.EndsWith(TEXT("_")))
        {
            return false;
        }
        TArray<FString> Parts;
        PrefixText.Mid(3, PrefixText.Len() - 4).ParseIntoArray(Parts, TEXT("_"), false);
        FIntVector Cell = FIntVector::ZeroValue;
        if (Parts.Num() != 2 && Parts.Num() != 3)
        {
            return false;
        }
        // LexTryParseString accepts a numeric prefix followed by junk. Cell identities
        // must not alias another module, and integer overflow must not wrap an index.
        for (int32 Axis = 0; Axis < Parts.Num(); ++Axis)
        {
            if (Parts[Axis].IsEmpty()) return false;
            int32 Value = 0;
            for (TCHAR Character : Parts[Axis])
            {
                if (Character < TEXT('0') || Character > TEXT('9')) return false;
                const int32 Digit = Character - TEXT('0');
                if (Value > (MAX_int32 - Digit) / 10) return false;
                Value = Value * 10 + Digit;
            }
            Cell[Axis] = Value;
        }
        OutCell = Cell;
        return true;
    }

    inline FSFExtendCellPlacement Placement(const FRotator& Rotation, const FVector& Size,
        float RowHeight, const FSFCounterState& State, int32 X, int32 Y, int32 Z)
    {
        FSFExtendCellPlacement Result = CalculateExtendCellPlacement(
            Rotation, Size, RowHeight, State, X + 1, Y, 1, 0, Z);
        const double SignedLayer = static_cast<double>(Z) * (State.GridCounters.Z < 0 ? -1.0 : 1.0);
        // Preserve the existing XY/steps/rotation contract. Z is world vertical, as in normal
        // scaling; stack stagger follows the parent's horizontal frame, never the camera.
        Result.WorldOffset += FRotator(0, Rotation.Yaw, 0).RotateVector(FVector(
            SignedLayer * State.StaggerZX, SignedLayer * State.StaggerZY, 0));
        return Result;
    }
}
