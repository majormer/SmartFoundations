// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
struct FSFCloneTopology;
struct FSFCloneHologram;

namespace SFExtendLaneNormals
{
    void VerifyCapture(FSFCloneHologram& Lane, const FString& ClassName, const FRotator& OwnerRotation);
    void RecoverLegacy(const FSFCloneTopology& Topology, FSFCloneHologram& Lane);
    void RepairUnverified(FSFCloneHologram& Lane, const FVector& Start, const FVector& End);
}
