// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class AFGHologram;

// Cable previews contribute cost, but the exact endpoint plan constructs the wires.
// Temporarily exclude them from the native child loop and restore them before the
// build gun's post-Construct cost query. Does not destroy or detach preview actors.
class FSFExtendWirePreviewScope
{
public:
    explicit FSFExtendWirePreviewScope(TArray<TObjectPtr<AFGHologram>>& InChildren);
    ~FSFExtendWirePreviewScope();
    FSFExtendWirePreviewScope(const FSFExtendWirePreviewScope&) = delete;
    FSFExtendWirePreviewScope& operator=(const FSFExtendWirePreviewScope&) = delete;
private:
    TArray<TObjectPtr<AFGHologram>>& Children;
    TArray<int32> Indices;
    TArray<TStrongObjectPtr<AFGHologram>> Previews;
};
