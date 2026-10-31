// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "FGConstructDisqualifier.h"
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
