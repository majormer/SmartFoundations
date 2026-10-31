// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "ItemAmount.h"
class AActor;
class AFGHologram;
struct FSFScalingSpec;
struct FNetConstructionID;
class FSFLiftFloorHoleGridRequest
{
public:
    class FScope
    {
    public:
        FScope();
        ~FScope();
        FScope(const FScope&) = delete;
        FScope& operator=(const FScope&) = delete;
    private:
        bool bEntered = false;
    };
    static bool IsHandled(const AFGHologram* Root);
    static bool Prepare(AFGHologram* Root, const FSFScalingSpec* Spec = nullptr);
    static bool IsPrepared(const AFGHologram* Root);
    static AFGHologram* FindCell(AFGHologram* Preview);
    static bool ClaimCell(AFGHologram* Cell, const FNetConstructionID& Id);
    static void ObserveCell(AFGHologram* Cell, AActor* Built);
    static bool GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost);
private:
    static void EndRequest();
};
