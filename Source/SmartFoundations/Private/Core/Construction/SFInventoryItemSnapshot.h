// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "FGInventoryComponent.h"
#include "UObject/GCObject.h"

/** An independently copied native item value. Dynamic storage is copied, while
 * referenced UObjects retain their identity (they are not cloned). FGCObject keeps
 * those references alive across deferred operation phases. Cached stack capacity
 * is deliberately not semantic identity; see the native conveyor research. */
class FSFInventoryItemSnapshot final : public FGCObject
{
public:
    explicit FSFInventoryItemSnapshot(const FInventoryItem& Source) : Item(Source) {}
    const FInventoryItem& Get() const { return Item; }
    bool Matches(const FInventoryItem& Other) const;
    static bool IsValidValue(const FInventoryItem& Value);
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override { return TEXT("SmartConstructionItemSnapshot"); }
private:
    FInventoryItem Item;
};

struct FSFInventoryStackSnapshot
{
    TSharedPtr<FSFInventoryItemSnapshot> Item;
    int32 Count = 0;
    FInventoryStack ToStack() const;
    bool Matches(const FInventoryStack& Other) const;
};

namespace SFInventorySnapshots
{
    bool Capture(const TArray<FInventoryStack>& Stacks, TArray<FSFInventoryStackSnapshot>& Out);
    bool Matches(const TArray<FSFInventoryStackSnapshot>& Before, const TArray<FInventoryStack>& After);
    /** Contents conservation independent of native destination slot packing. */
    bool SameContents(const TArray<FSFInventoryStackSnapshot>& Expected, const TArray<FInventoryStack>& Actual);
    bool ContainsContents(const TArray<FSFInventoryStackSnapshot>& Expected, const TArray<FInventoryStack>& Actual);
}
