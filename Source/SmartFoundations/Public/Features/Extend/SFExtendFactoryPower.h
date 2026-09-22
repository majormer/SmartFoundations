// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

struct FSFExtendFactoryPower;
struct FSFSourceFactory;
struct FSFCloneTopology;

namespace SFExtendFactoryPower
{
    FSFExtendFactoryPower Capture(AActor* Factory);
    bool IsRequested(const FSFExtendFactoryPower& Power, bool Enabled, bool Poleless, bool Unlocked);
    void AddSourceWire(FSFCloneTopology& Clone, const FSFSourceFactory& Source);
    void AddRestoreWires(FSFCloneTopology& Clone, const TMap<FIntVector, FTransform>& Cells);
}
