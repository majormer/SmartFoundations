// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"

class AFGHologram;
class USFSubsystem;
class FSFExtendAuthorityScope;

namespace SFExtendCommitValidation
{
    // Scope the actual native construction request, never the per-frame aiming checks.
    class FRequestScope
    {
    public:
        FRequestScope();
        ~FRequestScope();
        FRequestScope(const FRequestScope&) = delete;
        FRequestScope& operator=(const FRequestScope&) = delete;
        TMap<TWeakObjectPtr<AFGHologram>, bool> Prepared;
    private:
        FRequestScope* Previous;
        TUniquePtr<FSFExtendAuthorityScope> Authority;
        friend void IsolateAuthority(USFSubsystem*, AFGHologram*);
    };
    bool IsRequestActive();
    const bool* FindPrepared(AFGHologram* Root);
    void SetPrepared(AFGHologram* Root, bool Valid);
    void IsolateAuthority(USFSubsystem* Subsystem, AFGHologram* Root);
    bool SameCost(const TArray<FItemAmount>& Preview, const TArray<FItemAmount>& Authoritative);
}
