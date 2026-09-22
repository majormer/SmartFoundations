// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Features/Extend/SFExtendCloneTopology.h"

class AActor;

/** A floor-hole face and the external pipe endpoints in this construction transaction. */
struct FSFPassthroughFace
{
    FString HoleId;
    FVector Location = FVector::ZeroVector;
    FSFConnectionRef CapturedEndpoint;
    bool bCaptured = false;
    bool bOccupied = false;
    bool bTop = false;
};

struct FSFPassthroughEndpoint
{
    FSFConnectionRef Identity;
    FVector Location = FVector::ZeroVector;
};

namespace SFExtendPassthroughLinks
{
    /** Select unique, same-cell, three-dimensional matches without mutating anything. */
    TArray<TPair<int32, int32>> Plan(const TArray<FSFPassthroughFace>& Faces,
        const TArray<FSFPassthroughEndpoint>& Endpoints);
    /** Capture only newly built, exactly registered pipeline holes and pipe endpoints. */
    int32 Apply(const FSFCloneTopology& Topology, const TMap<FString, AActor*>& BuiltActors);
}
