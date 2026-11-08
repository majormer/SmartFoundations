// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

class APlayerController;
struct FSFScalingSpec;

enum class ESFScalingPayloadStatus : uint8 { Unavailable, Fits, TooLarge };

/** Measures the actual staging RPC parameters without submitting a construction request. */
struct FSFScalingPayloadBudget
{
    static constexpr int64 MaxBytes = 45000;
    static constexpr int64 EnvelopeReserveBytes = 1024;
    ESFScalingPayloadStatus Status = ESFScalingPayloadStatus::Unavailable;
    int64 ParameterBits = 0;

    static FSFScalingPayloadBudget Measure(APlayerController* Player, const FSFScalingSpec& Spec);
};
