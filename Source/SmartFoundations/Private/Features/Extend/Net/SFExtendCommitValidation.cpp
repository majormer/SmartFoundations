// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Hologram/FGHologram.h"

namespace
{
    thread_local SFExtendCommitValidation::FRequestScope* ActiveRequest = nullptr;
}

SFExtendCommitValidation::FRequestScope::FRequestScope() : Previous(ActiveRequest)
{
    if (!ActiveRequest) ActiveRequest = this;
}
SFExtendCommitValidation::FRequestScope::~FRequestScope() { ActiveRequest = Previous; }
bool SFExtendCommitValidation::IsRequestActive() { return ActiveRequest != nullptr; }
const bool* SFExtendCommitValidation::FindPrepared(AFGHologram* Root)
{
    return ActiveRequest ? ActiveRequest->Prepared.Find(Root) : nullptr;
}
void SFExtendCommitValidation::SetPrepared(AFGHologram* Root, bool Valid)
{
    if (ActiveRequest) ActiveRequest->Prepared.Add(Root, Valid);
}

bool SFExtendCommitValidation::SameCost(const TArray<FItemAmount>& Preview, const TArray<FItemAmount>& Authoritative)
{
    auto Aggregate = [](const TArray<FItemAmount>& Cost, TMap<UClass*, int64>& Out)
    {
        for (const FItemAmount& Item : Cost)
        {
            if (Item.Amount < 0 || (!Item.ItemClass && Item.Amount != 0)) return false;
            if (Item.Amount != 0) Out.FindOrAdd(Item.ItemClass.Get()) += static_cast<int64>(Item.Amount);
        }
        return true;
    };
    TMap<UClass*, int64> A, B;
    if (!Aggregate(Preview, A) || !Aggregate(Authoritative, B) || A.Num() != B.Num()) return false;
    for (const auto& Entry : A)
    {
        const int64* Amount = B.Find(Entry.Key);
        if (!Amount || *Amount != Entry.Value) return false;
    }
    return true;
}
