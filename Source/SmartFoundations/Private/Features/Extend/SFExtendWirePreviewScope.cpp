// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendWirePreviewScope.h"
#include "Holograms/Power/SFWireHologram.h"

FSFExtendWirePreviewScope::FSFExtendWirePreviewScope(TArray<TObjectPtr<AFGHologram>>& InChildren)
    : Children(InChildren)
{
    for (int32 Index = Children.Num() - 1; Index >= 0; --Index)
    {
        AFGHologram* Child = Children[Index];
        if (IsValid(Child) && Child->IsA<ASFWireHologram>() && Child->Tags.Contains(TEXT("SF_ExtendWirePlan")))
        {
            Indices.Add(Index);
            Previews.Emplace(Child);
            Children.RemoveAt(Index, 1, EAllowShrinking::No);
        }
    }
}

FSFExtendWirePreviewScope::~FSFExtendWirePreviewScope()
{
    for (int32 Index = Previews.Num() - 1; Index >= 0; --Index)
    {
        AFGHologram* Child = Previews[Index].Get();
        if (IsValid(Child) && !Children.Contains(Child))
            Children.Insert(Child, FMath::Min(Indices[Index], Children.Num()));
    }
}
