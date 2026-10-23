// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkEndpointGrid.h"

namespace
{
    bool ResolveAxis(TArray<double> Values, double Tolerance, TArray<double>& Coordinates, float& Spacing)
    {
        Values.Sort();
        for (double Value : Values)
        {
            if (Coordinates.IsEmpty() || Value - Coordinates.Last() > Tolerance)
            {
                Coordinates.Add(Value);
            }
        }
        if (Coordinates.Num() < 2) { return true; }
        const double Pitch = (Coordinates.Last() - Coordinates[0]) / (Coordinates.Num() - 1);
        if (Pitch <= 2.0 * Tolerance) { return false; }
        for (int32 Index = 0; Index < Coordinates.Num(); ++Index)
        {
            if (FMath::Abs(Coordinates[Index] - Coordinates[0] - Pitch * Index) > Tolerance)
            {
                return false;
            }
        }
        Spacing = static_cast<float>(Pitch);
        return true;
    }

    int32 CoordinateIndex(double Value, const TArray<double>& Coordinates, double Tolerance)
    {
        int32 Match = INDEX_NONE;
        for (int32 Index = 0; Index < Coordinates.Num(); ++Index)
        {
            if (FMath::Abs(Value - Coordinates[Index]) <= Tolerance)
            {
                if (Match != INDEX_NONE) { return INDEX_NONE; }
                Match = Index;
            }
        }
        return Match;
    }
}

FSFWalkEndpointGrid SFWalkEndpointGrid::Resolve(const TArray<FSFWalkEndpointSample>& Samples,
    int32 SelectedIdentity, const FVector& Lateral, const FVector& Vertical, float ToleranceCm)
{
    FSFWalkEndpointGrid Grid;
    if (Samples.Num() > 4096) { Grid.Result = ESFWalkEndpointGridResult::TooManyEndpoints; return Grid; }
    if (!FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0f || Lateral.ContainsNaN() || Vertical.ContainsNaN()
        || !Lateral.IsNormalized() || !Vertical.IsNormalized()
        || FMath::Abs(FVector::DotProduct(Lateral, Vertical)) > UE_KINDA_SMALL_NUMBER)
    { Grid.Result = ESFWalkEndpointGridResult::InvalidFrame; return Grid; }

    const FSFWalkEndpointSample* Selected = nullptr;
    TSet<int32> Identities;
    for (const FSFWalkEndpointSample& Sample : Samples)
    {
        if (Sample.Identity == INDEX_NONE || Identities.Contains(Sample.Identity))
        { Grid.Result = ESFWalkEndpointGridResult::AmbiguousIdentity; return Grid; }
        Identities.Add(Sample.Identity);
        if (Sample.Identity == SelectedIdentity) { Selected = &Sample; }
    }
    if (!Selected || !Selected->bEligible || Selected->bOccupied || Selected->Position.ContainsNaN()
        || Selected->Outward.ContainsNaN() || !Selected->Outward.IsNormalized()) { return Grid; }
    const FVector Forward = Selected->Outward;
    if (FMath::Abs(FVector::DotProduct(Forward, Lateral)) > UE_KINDA_SMALL_NUMBER
        || FMath::Abs(FVector::DotProduct(Forward, Vertical)) > UE_KINDA_SMALL_NUMBER)
    { Grid.Result = ESFWalkEndpointGridResult::InvalidFrame; return Grid; }

    TArray<const FSFWalkEndpointSample*> Participants;
    TArray<double> LaneValues, StackValues;
    for (const FSFWalkEndpointSample& Sample : Samples)
    {
        if (Sample.Position.ContainsNaN() || Sample.Outward.ContainsNaN())
        { Grid.Result = ESFWalkEndpointGridResult::AmbiguousGeometry; return Grid; }
        const FVector Offset = Sample.Position - Selected->Position;
        // Other endpoint planes/headings are not members of this cross-section.
        if (FMath::Abs(FVector::DotProduct(Offset, Forward)) > ToleranceCm
            || FVector::DotProduct(Sample.Outward, Forward) < 0.9999f) { continue; }
        // Only exposed ends participate. Occupied neighbours can be the original bus
        // beside newly added lanes whose open starting ends are being walked backward.
        // Interior holes still fail the rectangle check below.
        if (!Sample.bEligible || Sample.bOccupied) { continue; }
        Participants.Add(&Sample);
        LaneValues.Add(FVector::DotProduct(Offset, Lateral));
        StackValues.Add(FVector::DotProduct(Offset, Vertical));
    }
    for (const auto* Participant : Participants)
    {
        for (const auto& Sample : Samples)
        {
            if (Sample.Identity != Participant->Identity && Sample.Position.Equals(Participant->Position, ToleranceCm)
                && FVector::DotProduct(Sample.Outward, Forward) >= 0.9999f)
            { Grid.Result = ESFWalkEndpointGridResult::AmbiguousGeometry; return Grid; }
        }
    }
    TArray<double> LaneCoordinates, StackCoordinates;
    if (!ResolveAxis(LaneValues, ToleranceCm, LaneCoordinates, Grid.LaneSpacing)
        || !ResolveAxis(StackValues, ToleranceCm, StackCoordinates, Grid.StackSpacing))
    { Grid.Result = ESFWalkEndpointGridResult::AmbiguousGeometry; return Grid; }
    // An occupied/excluded connector inside the free rectangle is a missing lane,
    // not a larger pitch. Neighbours outside the rectangle belong to another run.
    for (const auto& Sample : Samples)
    {
        if (Sample.bEligible && !Sample.bOccupied) { continue; }
        const FVector Offset = Sample.Position - Selected->Position;
        if (FMath::Abs(FVector::DotProduct(Offset, Forward)) > ToleranceCm
            || FVector::DotProduct(Sample.Outward, Forward) < 0.9999f) { continue; }
        const double Lane = FVector::DotProduct(Offset, Lateral);
        const double Stack = FVector::DotProduct(Offset, Vertical);
        if (Lane >= LaneCoordinates[0] - ToleranceCm && Lane <= LaneCoordinates.Last() + ToleranceCm
            && Stack >= StackCoordinates[0] - ToleranceCm && Stack <= StackCoordinates.Last() + ToleranceCm)
        { Grid.Result = ESFWalkEndpointGridResult::IncompleteRectangle; return Grid; }
    }
    Grid.Lanes = LaneCoordinates.Num();
    Grid.Stacks = StackCoordinates.Num();
    const int32 CellCount = Grid.Lanes * Grid.Stacks;
    if (CellCount != Participants.Num())
    { Grid.Result = ESFWalkEndpointGridResult::IncompleteRectangle; return Grid; }
    Grid.Identities.Init(INDEX_NONE, CellCount);
    Grid.ReverseFlow.Init(false, CellCount);
    for (const FSFWalkEndpointSample* Sample : Participants)
    {
        const FVector Offset = Sample->Position - Selected->Position;
        const int32 Lane = CoordinateIndex(FVector::DotProduct(Offset, Lateral), LaneCoordinates, ToleranceCm);
        const int32 Stack = CoordinateIndex(FVector::DotProduct(Offset, Vertical), StackCoordinates, ToleranceCm);
        if (Lane == INDEX_NONE || Stack == INDEX_NONE)
        { Grid.Result = ESFWalkEndpointGridResult::AmbiguousGeometry; return Grid; }
        const int32 Cell = Lane * Grid.Stacks + Stack;
        if (Grid.Identities[Cell] != INDEX_NONE)
        { Grid.Result = ESFWalkEndpointGridResult::AmbiguousGeometry; return Grid; }
        Grid.Identities[Cell] = Sample->Identity;
        Grid.ReverseFlow[Cell] = Sample->bReverseFlow;
        if (Sample == Selected) { Grid.SelectedCell = Cell; }
    }
    Grid.Origin = Selected->Position + Lateral * LaneCoordinates[0] + Vertical * StackCoordinates[0];
    Grid.Result = ESFWalkEndpointGridResult::Ready;
    return Grid;
}
