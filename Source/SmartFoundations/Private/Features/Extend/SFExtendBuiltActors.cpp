// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendBuiltActors.h"
#include "Features/Extend/SFExtendService.h"
#include "Data/SFHologramDataRegistry.h"
#include "Buildables/FGBuildable.h"
#include "Hologram/FGHologram.h"

AActor* SFExtendBuiltActors::Match(UClass* BuildClass, const FTransform& Transform,
    const TArray<AActor*>& Constructed)
{
    if (!BuildClass || Transform.ContainsNaN()) return nullptr;
    AActor* Match = nullptr;
    for (AActor* Candidate : Constructed)
    {
        if (!IsValid(Candidate) || Candidate->GetClass() != BuildClass) continue;
        const FTransform Actual = Candidate->GetActorTransform();
        if (Actual.ContainsNaN() || !Actual.GetLocation().Equals(Transform.GetLocation(), 1.0)
            || !Actual.GetRotation().Equals(Transform.GetRotation(), 0.0001)
            || !Actual.GetScale3D().Equals(Transform.GetScale3D(), 0.0001)) continue;
        if (Match && Match != Candidate) return nullptr;
        Match = Candidate;
    }
    return Match;
}

int32 SFExtendBuiltActors::RegisterChildren(USFExtendService* Service, AFGHologram* Parent,
    const TArray<AActor*>& Constructed)
{
    if (!Service || !IsValid(Parent)) return 0;
    int32 Count = 0;
    TSet<AActor*> Claimed;
    for (AFGHologram* Child : Parent->GetHologramChildren())
    {
        const FSFHologramData* Data = USFHologramDataRegistry::GetData(Child);
        if (!IsValid(Child) || !Data || Data->JsonCloneId.IsEmpty()) continue;
        if (AActor* Existing = Service->GetBuiltActorByCloneId(Data->JsonCloneId)) Claimed.Add(Existing);
    }
    for (AFGHologram* Child : Parent->GetHologramChildren())
    {
        const FSFHologramData* Data = USFHologramDataRegistry::GetData(Child);
        if (!IsValid(Child) || !Data || Data->JsonCloneId.IsEmpty()
            || Service->GetBuiltActorByCloneId(Data->JsonCloneId)) continue;
        AActor* Candidate = Match(Child->GetBuildClass(), Child->GetActorTransform(), Constructed);
        if (!Candidate || Claimed.Contains(Candidate)) continue;
        Service->RegisterJsonBuiltActor(Data->JsonCloneId, Candidate);
        Claimed.Add(Candidate);
        ++Count;
    }
    return Count;
}
