// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

namespace SFPowerConnectionPolicy
{
inline bool IsWallOutlet(const FString& ClassName)
{
    return ClassName == TEXT("Build_PowerPoleWall_C") || ClassName == TEXT("Build_PowerPoleWall_Mk2_C")
        || ClassName == TEXT("Build_PowerPoleWall_Mk3_C") || ClassName == TEXT("Build_PowerPoleWallDouble_C")
        || ClassName == TEXT("Build_PowerPoleWallDouble_Mk2_C") || ClassName == TEXT("Build_PowerPoleWallDouble_Mk3_C");
}

inline int32 AvailableSlots(int32 Maximum, int32 Used, int32 Planned, int32 Reserved)
{
    return FMath::Max(0, Maximum - Used - Planned - FMath::Clamp(Reserved, 0, FMath::Max(0, Maximum - 1)));
}

/** Auto is a single serpentine chain, X/Y/X+Y retain axis-specific edges. Missing cells never bridge diagonally. */
inline TArray<TPair<FIntVector, FIntVector>> GridEdges(TArray<FIntVector> Cells, int32 Mode)
{
    TArray<TPair<FIntVector, FIntVector>> Edges;
    if (Cells.IsEmpty()) return Edges;
    int32 XSize = 1;
    for (const FIntVector& Cell : Cells) XSize = FMath::Max(XSize, Cell.X + 1);
    Cells.Sort([XSize, Mode](const FIntVector& A, const FIntVector& B)
    {
        if (A.Z != B.Z) return A.Z < B.Z;
        const int32 AX = Mode == 0 && (A.Z & 1) ? XSize - 1 - A.X : A.X;
        const int32 BX = Mode == 0 && (B.Z & 1) ? XSize - 1 - B.X : B.X;
        if (AX != BX) return AX < BX;
        return Mode == 0 && ((A.Z * XSize + AX) & 1) ? A.Y > B.Y : A.Y < B.Y;
    });
    TSet<FIntVector> Present;
    for (const FIntVector& Cell : Cells) Present.Add(Cell);
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const FIntVector& Cell = Cells[Index];
        if (Mode == 0)
        {
            if (Index == 0) continue;
            const FIntVector Delta = Cell - Cells[Index - 1];
            if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) + FMath::Abs(Delta.Z) == 1)
                Edges.Emplace(Cells[Index - 1], Cell);
        }
        else
        {
            if ((Mode == 1 || Mode == 3) && Present.Contains(Cell + FIntVector(1, 0, 0)))
                Edges.Emplace(Cell, Cell + FIntVector(1, 0, 0));
            if ((Mode == 2 || Mode == 3) && Present.Contains(Cell + FIntVector(0, 1, 0)))
                Edges.Emplace(Cell, Cell + FIntVector(0, 1, 0));
        }
    }
    return Edges;
}
}
