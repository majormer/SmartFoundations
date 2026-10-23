// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
struct FSFScalingSpec;

namespace SFHypertubeEntranceState
{
    bool IsHandled(const AFGHologram* Root);
    bool HasProxyGrid(AFGHologram* Root);
    bool Prepare(AFGHologram* Root, const FSFScalingSpec* Spec = nullptr);
    AFGHologram* FindPreparedCell(AFGHologram* Preview);
    bool IsRequestActive();
    class FRequestScope
    {
    public:
        FRequestScope();
        ~FRequestScope();
        FRequestScope(const FRequestScope&) = delete;
        FRequestScope& operator=(const FRequestScope&) = delete;
    private:
        bool bOwnsRequest = false;
    };
}
