// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
class AFGHologram;
struct FSFScalingSpec;

/** Request-scoped prepayment admission; live connections remain owned by ConstructionLinks. */
class FSFConveyorBeltPlan
{
public:
    class FRequestScope
    {
    public:
        FRequestScope();
        ~FRequestScope();
        FRequestScope(const FRequestScope&) = delete;
        FRequestScope& operator=(const FRequestScope&) = delete;
    private:
        bool bEntered = false;
    };
    static bool HasBelts(const FSFScalingSpec& Spec);
    static bool Prepare(AFGHologram* Root, const FSFScalingSpec& Spec, bool bReconstruct);
    static bool IsPrepared(const AFGHologram* Root);
    static bool IsComputingQuote(const AFGHologram* Root);
    static bool RefreshCost(AFGHologram* Root);
    static bool Revalidate(AFGHologram* Root);
    static bool GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost);
private:
    static void EndRequest();
};
