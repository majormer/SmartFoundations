// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Extend/SFExtendControlFrame.h"

#include "Features/Extend/SFExtendCloneTopology.h"

FSFExtendControlFrame FSFExtendControlFrame::FromState(const FSFCounterState& State)
{
    FSFExtendControlFrame Frame;
    Frame.ChainSign = State.GridCounters.X < 0 ? -1.0f : 1.0f;
    Frame.RowsSign = State.GridCounters.Y < 0 ? -1.0f : 1.0f;
    return Frame;
}

float CalculateExtendEffectiveRowHeight(const FVector& BuildingSize, const FSFCloneTopology* Topology)
{
    float EffectiveRowHeight = BuildingSize.Y;
    if (!Topology || Topology->ChildHolograms.IsEmpty())
    {
        return EffectiveRowHeight;
    }

    const FVector FactoryCenter = Topology->ParentTransform.Location.ToFVector();
    const FRotator InverseRotation = Topology->ParentTransform.Rotation.ToFRotator().GetInverse();
    float MinLocalY = 0.0f;
    float MaxLocalY = 0.0f;

    for (const FSFCloneHologram& Hologram : Topology->ChildHolograms)
    {
        if (Hologram.bIsLaneSegment)
        {
            continue;
        }

        const FVector LocalPosition = InverseRotation.RotateVector(
            Hologram.Transform.Location.ToFVector() - FactoryCenter);
        const float HalfWidth = Hologram.Role == TEXT("distributor") ? 200.0f : 0.0f;
        MinLocalY = FMath::Min(MinLocalY, LocalPosition.Y - HalfWidth);
        MaxLocalY = FMath::Max(MaxLocalY, LocalPosition.Y + HalfWidth);
    }

    return FMath::Max(EffectiveRowHeight, MaxLocalY - MinLocalY);
}

FSFExtendCellPlacement CalculateExtendCellPlacement(
    const FRotator& BaseRotation,
    const FVector& BuildingSize,
    float EffectiveRowHeight,
    const FSFCounterState& State,
    int32 ChainIndex,
    int32 RowIndex,
    int32 OriginChainIndex,
    int32 OriginRowIndex)
{
    const FSFExtendControlFrame Frame = FSFExtendControlFrame::FromState(State);
    const float ChainDistance = FMath::Max(1.0f, BuildingSize.X + static_cast<float>(State.SpacingX));
    const float RowDistance = FMath::Max(1.0f, EffectiveRowHeight + static_cast<float>(State.SpacingY));
    const bool bRotationActive = !FMath::IsNearlyZero(State.RotationZ);
    const bool bRowsProgressRotation = State.RotationAxis == ESFScaleAxis::Y;
    const float StepRadians = FMath::Abs(FMath::DegreesToRadians(State.RotationZ));
    const float Radius = bRotationActive && StepRadians > KINDA_SMALL_NUMBER
        ? ChainDistance / StepRadians
        : 0.0f;
    const float RotationSign = State.RotationZ >= 0.0f ? 1.0f : -1.0f;

    auto CalculateLocalCell = [&](int32 CellChain, int32 CellRow, FVector& OutLocation, float& OutYaw)
    {
        OutLocation = FVector::ZeroVector;
        OutYaw = 0.0f;

        if (!bRotationActive)
        {
            OutLocation.X = Frame.ChainSign * ChainDistance * static_cast<float>(CellChain);
            OutLocation.Y = Frame.RowsSign * RowDistance * static_cast<float>(CellRow);
            OutLocation.Z = static_cast<float>(State.StepsX * CellChain + State.StepsY * CellRow);
            return;
        }

        if (!bRowsProgressRotation)
        {
            const float AngleDegrees = static_cast<float>(CellChain) * State.RotationZ;
            const float AbsoluteAngle = FMath::Abs(FMath::DegreesToRadians(AngleDegrees));
            OutLocation.X = Frame.ChainSign * Radius * FMath::Sin(AbsoluteAngle);
            OutLocation.Y = RotationSign * (Radius - Radius * FMath::Cos(AbsoluteAngle))
                + Frame.RowsSign * RowDistance * static_cast<float>(CellRow);
            OutLocation.Z = static_cast<float>(State.StepsX * CellChain + State.StepsY * CellRow);
            OutYaw = AngleDegrees * Frame.ChainSign;
            return;
        }

        const float AngleDegrees = static_cast<float>(CellRow) * State.RotationZ;
        const float AbsoluteAngle = FMath::Abs(FMath::DegreesToRadians(AngleDegrees));
        const FVector RowAnchor(
            RotationSign * (Radius - Radius * FMath::Cos(AbsoluteAngle)),
            Frame.RowsSign * Radius * FMath::Sin(AbsoluteAngle),
            static_cast<float>(State.StepsY * CellRow));
        const FVector ChainOffset = FRotator(0.0f, AngleDegrees, 0.0f).RotateVector(
            FVector(Frame.ChainSign * ChainDistance * static_cast<float>(CellChain), 0.0f, 0.0f));

        OutLocation = RowAnchor + ChainOffset;
        OutLocation.Z += static_cast<float>(State.StepsX * CellChain);
        OutYaw = AngleDegrees;
    };

    FVector TargetLocal;
    FVector OriginLocal;
    float TargetYaw = 0.0f;
    float OriginYaw = 0.0f;
    CalculateLocalCell(ChainIndex, RowIndex, TargetLocal, TargetYaw);
    CalculateLocalCell(OriginChainIndex, OriginRowIndex, OriginLocal, OriginYaw);

    const FVector LocalDelta = TargetLocal - OriginLocal;
    FSFExtendCellPlacement Placement;
    Placement.WorldOffset = BaseRotation.RotateVector(FVector(LocalDelta.X, LocalDelta.Y, 0.0f));
    Placement.WorldOffset.Z += LocalDelta.Z;
    Placement.RotationOffset = FRotator(0.0f, TargetYaw - OriginYaw, 0.0f);
    return Placement;
}
