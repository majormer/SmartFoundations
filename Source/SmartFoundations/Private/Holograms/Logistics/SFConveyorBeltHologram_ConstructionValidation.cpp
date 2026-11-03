// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "FGConstructDisqualifier.h"
#include "Components/SplineComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "UObject/UnrealType.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"

UFGFactoryConnectionComponent* ASFConveyorBeltHologram::GetSelectedFactoryPeer(int32 Index) const
{
    const auto* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorBeltHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    return Property && Property->ArrayDim == 2 && Index >= 0 && Index < 2
        ? Cast<UFGFactoryConnectionComponent>(Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, Index))) : nullptr;
}

bool ASFConveyorBeltHologram::ValidateNativeConstruction()
{
    // A fresh native validation pass, independent of visual-only child exemptions.
    ResetConstructDisqualifiers();
    AFGConveyorBeltHologram::CheckValidPlacement();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons; GetConstructDisqualifiers(Reasons);
    for (auto Reason : Reasons)
        if (Reason && !UFGConstructDisqualifier::GetIsSoftDisqualifier(Reason)) return false;
    return true;
}

TArray<FItemAmount> ASFConveyorBeltHologram::GetNativeConstructionCost() const
{
    return AFGConveyorBeltHologram::GetCost(false);
}

TSubclassOf<UFGConstructDisqualifier> ASFConveyorBeltHologram::GetNativeSplineDisqualifier()
{
    if (!GetSplineComponent() || GetSplineComponent()->GetNumberOfSplinePoints() < 2)
        return UFGCDConveyorInvalidShape::StaticClass();
    if (GetSplineComponent()->GetSplineLength() > GetMaxSplineLength())
        return UFGCDConveyorTooLong::StaticClass();

    TArray<TSubclassOf<UFGConstructDisqualifier>> Previous;
    GetConstructDisqualifiers(Previous);
    ResetConstructDisqualifiers();
    // Call native geometry validation directly: ordinary child placement intentionally
    // skips it, and full placement also checks unrelated floor/clearance conditions.
    ValidateConveyorBelt();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;
    GetConstructDisqualifiers(Reasons);
    ResetConstructDisqualifiers();
    for (auto Reason : Previous) AddConstructDisqualifier(Reason);
    for (auto Reason : Reasons)
        if (Reason == UFGCDConveyorTooLong::StaticClass()
            || Reason == UFGCDConveyorTooSteep::StaticClass()
            || Reason == UFGCDConveyorInvalidShape::StaticClass()) return Reason;
    return nullptr;
}
