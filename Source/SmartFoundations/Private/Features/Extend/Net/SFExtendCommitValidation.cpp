// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Features/Extend/Net/SFExtendAuthorityScope.h"
#include "Subsystem/SFSubsystem.h"
#include "Hologram/FGHologram.h"

namespace
{
    thread_local SFExtendCommitValidation::FRequestScope* ActiveRequest = nullptr;
}

SFExtendCommitValidation::FRequestScope::FRequestScope() : Previous(ActiveRequest)
{
    if (!ActiveRequest) ActiveRequest = this;
}
SFExtendCommitValidation::FRequestScope::~FRequestScope()
{
    Authority.Reset();
    ActiveRequest = Previous;
}
bool SFExtendCommitValidation::IsRequestActive() { return ActiveRequest != nullptr; }
const bool* SFExtendCommitValidation::FindPrepared(AFGHologram* Root)
{
    return ActiveRequest ? ActiveRequest->Prepared.Find(Root) : nullptr;
}
void SFExtendCommitValidation::SetPrepared(AFGHologram* Root, bool Valid)
{
    if (ActiveRequest) ActiveRequest->Prepared.Add(Root, Valid);
}

void SFExtendCommitValidation::IsolateAuthority(USFSubsystem* Subsystem, AFGHologram* Root)
{
    if (!ActiveRequest || !Root || !Root->HasAuthority() || Root->GetParentHologram()
        || FSFExtendAuthorityScope::IsActive(Subsystem)
        || !FSFExtendAuthorityScope::ShouldIsolate(Subsystem, Root->GetConstructionInstigator(), Root->GetBuildClass())) return;
    FSFExtendCommitSpec Commit;
    if (Subsystem->PeekExtendCommitForInstigator(Root->GetConstructionInstigator(), Root->GetBuildClass(), Commit))
        ActiveRequest->Authority = MakeUnique<FSFExtendAuthorityScope>(Subsystem, Root);
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
