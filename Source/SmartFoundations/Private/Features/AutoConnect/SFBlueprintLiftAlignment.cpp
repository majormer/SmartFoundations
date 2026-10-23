// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintLiftAlignment.h"

FSFBlueprintLiftAlignment::FPairing FSFBlueprintLiftAlignment::PairTails(TConstArrayView<FTail> Tails,
    double LaneTolerance, double MaximumReach)
{
    FPairing Result;
    if (!FMath::IsFinite(LaneTolerance) || LaneTolerance < 0.0
        || !FMath::IsFinite(MaximumReach) || MaximumReach <= 0.0)
    { Result.bInvalid = true; return Result; }

    struct FCandidate { int32 Output; int32 Input; double LaneError; };
    TArray<FCandidate> Candidates;
    TSet<int32> Indices;
    for (const auto& Tail : Tails)
    {
        if (!Tail.bLift || !Tail.bOpen) continue;
        if (Tail.Index < 0 || Indices.Contains(Tail.Index) || Tail.Position.ContainsNaN()
            || Tail.Normal.ContainsNaN() || Tail.Normal.IsNearlyZero() || Tail.bInput == Tail.bOutput)
        { Result.bInvalid = true; return Result; }
        Indices.Add(Tail.Index);
    }
    for (int32 O = 0; O < Tails.Num(); ++O)
    {
        const auto& Output = Tails[O];
        if (!Output.bLift || !Output.bOpen || !Output.bOutput) continue;
        const FVector OutNormal = FVector(Output.Normal.X, Output.Normal.Y, 0).GetSafeNormal();
        if (OutNormal.IsNearlyZero()) continue;
        for (int32 I = 0; I < Tails.Num(); ++I)
        {
            const auto& Input = Tails[I];
            if (!Input.bLift || !Input.bOpen || !Input.bInput || O == I) continue;
            const FVector InNormal = FVector(Input.Normal.X, Input.Normal.Y, 0).GetSafeNormal();
            const FVector Delta(Input.Position.X - Output.Position.X, Input.Position.Y - Output.Position.Y, 0);
            // These are intent filters, not substitutes for native belt angles/shape.
            // Both mouths must face the gap; a spare port behind either mouth is not a mate.
            if (InNormal.IsNearlyZero() || FVector::DotProduct(OutNormal, InNormal) >= 0.0
                || FVector::DotProduct(OutNormal, Delta) <= 0.0 || FVector::DotProduct(InNormal, -Delta) <= 0.0
                || Delta.Size() > MaximumReach || FMath::IsNearlyEqual(Input.Position.Z, Output.Position.Z, .1)) continue;
            const double OutLane = FMath::Abs(OutNormal.X * Delta.Y - OutNormal.Y * Delta.X);
            const double InLane = FMath::Abs(InNormal.X * Delta.Y - InNormal.Y * Delta.X);
            const double Error = FMath::Max(OutLane, InLane);
            if (Error <= LaneTolerance) Candidates.Add({O, I, Error});
        }
    }
    auto IsUniqueBest = [&](const FCandidate& Candidate, bool OutputSide)
    {
        for (const auto& Other : Candidates)
        {
            if (&Other == &Candidate) continue;
            if ((OutputSide ? Other.Output == Candidate.Output : Other.Input == Candidate.Input)
                && Other.LaneError <= Candidate.LaneError + .1) return false;
        }
        return true;
    };
    for (const auto& Candidate : Candidates)
    {
        const bool OutputBest = IsUniqueBest(Candidate, true);
        const bool InputBest = IsUniqueBest(Candidate, false);
        if (OutputBest && InputBest)
        {
            const auto& O = Tails[Candidate.Output]; const auto& I = Tails[Candidate.Input];
            Result.Pairs.Add({O.Index, I.Index, O.Position.Z > I.Position.Z, FMath::Abs(O.Position.Z - I.Position.Z)});
        }
    }
    // An eligible tail with viable candidates but no unique reciprocal assignment is ambiguous.
    for (const auto& Candidate : Candidates)
        if (!Result.Pairs.ContainsByPredicate([&](const FPair& P) { return P.Output == Tails[Candidate.Output].Index; })
            || !Result.Pairs.ContainsByPredicate([&](const FPair& P) { return P.Input == Tails[Candidate.Input].Index; }))
        { Result.bAmbiguous = true; Result.Pairs.Reset(); return Result; }
    Result.Pairs.Sort([](const FPair& A, const FPair& B) { return A.Output != B.Output ? A.Output < B.Output : A.Input < B.Input; });
    return Result;
}

double FSFBlueprintLiftAlignment::MaximumVerticalReach(double NativeMaxLength, double HorizontalChord, double NativeInclineDegrees)
{
    if (!FMath::IsFinite(NativeMaxLength) || !FMath::IsFinite(HorizontalChord) || !FMath::IsFinite(NativeInclineDegrees)
        || NativeMaxLength <= 0.0 || HorizontalChord < 0.0 || HorizontalChord >= NativeMaxLength
        || NativeInclineDegrees <= 0.0 || NativeInclineDegrees >= 90.0) return 0.0;
    // A bent native route travels farther horizontally than its endpoint chord.
    // Only full native arc length and its incline bound give a necessary rise limit.
    const double LengthBound = FMath::Sqrt(NativeMaxLength * NativeMaxLength - HorizontalChord * HorizontalChord);
    const double InclineBound = NativeMaxLength * FMath::Sin(FMath::DegreesToRadians(NativeInclineDegrees));
    return FMath::Min(LengthBound, InclineBound);
}

FSFBlueprintLiftAlignment::FResult FSFBlueprintLiftAlignment::SolveSharedPitch(double DefaultPitch,
    TConstArrayView<FPitchRange> Ranges, TFunctionRef<bool(double)> NativeLayoutValid)
{
    FResult Result; Result.Pitch = DefaultPitch;
    if (!FMath::IsFinite(DefaultPitch) || DefaultPitch <= 0.0) { Result.Status = EStatus::Invalid; return Result; }
    if (Ranges.IsEmpty()) return Result;
    double Minimum = 0.0, Maximum = TNumericLimits<double>::Max();
    TArray<double> Preferred;
    for (const auto& Range : Ranges)
    {
        if (!FMath::IsFinite(Range.Minimum) || !FMath::IsFinite(Range.Maximum) || !FMath::IsFinite(Range.Preferred)
            || Range.Minimum <= 0.0 || Range.Maximum < Range.Minimum || Range.Preferred <= 0.0)
        { Result.Status = EStatus::Invalid; return Result; }
        Minimum = FMath::Max(Minimum, Range.Minimum); Maximum = FMath::Min(Maximum, Range.Maximum);
        Preferred.Add(Range.Preferred);
    }
    if (Maximum < Minimum) { Result.Status = EStatus::Conflict; return Result; }
    Preferred.Sort();
    const int32 Mid = Preferred.Num() / 2;
    const double Median = Preferred.Num() % 2 ? Preferred[Mid] : Preferred[Mid - 1] + (Preferred[Mid] - Preferred[Mid - 1]) * .5;
    TArray<double> Candidates;
    auto Add = [&](double Pitch) { Candidates.AddUnique(FMath::Clamp(Pitch, Minimum, Maximum)); };
    Add(Median);
    for (double Pitch : Preferred) Add(Pitch);
    Add(DefaultPitch); Add(Minimum + (Maximum - Minimum) * .5); Add(Minimum); Add(Maximum);
    for (double Pitch : Candidates)
    {
        ++Result.NativeChecks;
        if (NativeLayoutValid(Pitch)) { Result.Status = EStatus::Aligned; Result.Pitch = Pitch; return Result; }
    }
    Result.Status = EStatus::NoValidatedPitch;
    return Result;
}
