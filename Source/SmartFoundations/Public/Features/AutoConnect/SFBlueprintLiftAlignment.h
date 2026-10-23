// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

/** Geometry-only decisions. Native routing and placement supply the feasible pitch ranges/verdict. */
class SMARTFOUNDATIONS_API FSFBlueprintLiftAlignment
{
public:
    struct FTail
    {
        int32 Index = INDEX_NONE;
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
        bool bLift = false;
        bool bOpen = false;
        bool bInput = false;
        bool bOutput = false;
    };

    struct FPair
    {
        int32 Output = INDEX_NONE;
        int32 Input = INDEX_NONE;
        bool bOutputInLowerCopy = true;
        double LevelPitch = 0.0;
    };

    struct FPairing
    {
        TArray<FPair> Pairs;
        bool bAmbiguous = false;
        bool bInvalid = false;
    };

    struct FPitchRange
    {
        double Minimum = 0.0;
        double Maximum = 0.0;
        double Preferred = 0.0;
    };

    enum class EStatus : uint8 { NoTails, Aligned, Conflict, Invalid, NoValidatedPitch };
    struct FResult
    {
        EStatus Status = EStatus::NoTails;
        double Pitch = 0.0;
        int32 NativeChecks = 0;
    };

    /** Unique reciprocal lane matches only; never resolve equal candidates by instance/index order. */
    static FPairing PairTails(TConstArrayView<FTail> Tails, double LaneTolerance, double MaximumReach);

    /** Necessary rise bounds from native arc length/incline; never substitute the XY chord for routed travel. */
    static double MaximumVerticalReach(double NativeMaxLength, double HorizontalChord, double NativeInclineDegrees);

    /** One pitch for every range. The native callback validates the whole intended layout. */
    static FResult SolveSharedPitch(double DefaultPitch, TConstArrayView<FPitchRange> Ranges,
        TFunctionRef<bool(double)> NativeLayoutValid);
};
