// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFInventoryItemSnapshot.h"
#include "SmartFoundations.h"
#include "GameFramework/Actor.h"

bool FSFInventoryItemSnapshot::IsValidValue(const FInventoryItem& Value)
{
    if (!IsValid(Value.GetItemClass().Get())) return false;
    if (Value.HasState() && (!IsValid(Value.GetItemState().GetStruct()) || !Value.GetItemState().GetStructValueRaw())) return false;
    auto* Legacy = Value.LegacyItemStateActor.Get();
    return !Legacy || (IsValid(Legacy) && !Legacy->IsActorBeingDestroyed());
}

bool FSFInventoryItemSnapshot::Matches(const FInventoryItem& Other) const
{
    if (!IsValidValue(Item) || !IsValidValue(Other) || Item.GetItemClass() != Other.GetItemClass()
        || Item.LegacyItemStateActor != Other.LegacyItemStateActor || Item.HasState() != Other.HasState()) return false;
    // Explicitly use the dynamic container's native identity operation. Comparing
    // raw bytes or just its reflected outer struct would miss dynamic contents.
    return !Item.HasState() || Item.GetItemState().Identical(Other.GetItemState(), PPF_None);
}

void FSFInventoryItemSnapshot::AddReferencedObjects(FReferenceCollector& Collector)
{
    Collector.AddPropertyReferencesWithStructARO(FInventoryItem::StaticStruct(), &Item);
}

FInventoryStack FSFInventoryStackSnapshot::ToStack() const
{
    FInventoryStack Result;
    Result.NumItems = Count;
    if (Item) Result.Item = Item->Get();
    return Result;
}

bool FSFInventoryStackSnapshot::Matches(const FInventoryStack& Other) const
{
    return Count == Other.NumItems && (Count == 0 || (Item && Item->Matches(Other.Item)));
}

bool SFInventorySnapshots::Capture(const TArray<FInventoryStack>& Stacks, TArray<FSFInventoryStackSnapshot>& Out)
{
    Out.Reset();
    if (Stacks.Num() > 32768) return false;
    for (const auto& Stack : Stacks)
    {
        if (Stack.NumItems < 0 || (Stack.NumItems > 0 && !FSFInventoryItemSnapshot::IsValidValue(Stack.Item)))
        { Out.Reset(); return false; }
        FSFInventoryStackSnapshot Value;
        Value.Count = Stack.NumItems;
        if (Stack.NumItems > 0) Value.Item = MakeShared<FSFInventoryItemSnapshot>(Stack.Item);
        Out.Add(MoveTemp(Value));
    }
    return true;
}

bool SFInventorySnapshots::Matches(const TArray<FSFInventoryStackSnapshot>& Before, const TArray<FInventoryStack>& After)
{
    if (Before.Num() != After.Num()) return false;
    for (int32 Index = 0; Index < Before.Num(); ++Index) if (!Before[Index].Matches(After[Index])) return false;
    return true;
}

static bool MatchContents(const TArray<FSFInventoryStackSnapshot>& Expected, const TArray<FInventoryStack>& Actual, bool bAllowExtra)
{
    if (Actual.Num() > 32768) return false;
    TArray<int32> Remaining;
    for (const auto& Stack : Actual)
    {
        if (Stack.NumItems < 0 || (Stack.NumItems > 0 && !FSFInventoryItemSnapshot::IsValidValue(Stack.Item))) return false;
        Remaining.Add(Stack.NumItems);
    }
    for (const auto& Stack : Expected)
    {
        int32 Needed = Stack.Count;
        for (int32 Index = 0; Needed > 0 && Index < Actual.Num(); ++Index)
            if (Remaining[Index] > 0 && Stack.Item && Stack.Item->Matches(Actual[Index].Item))
            {
                const int32 Taken = FMath::Min(Needed, Remaining[Index]);
                Needed -= Taken; Remaining[Index] -= Taken;
            }
        if (Needed != 0) return false;
    }
    if (!bAllowExtra) for (int32 Count : Remaining) if (Count != 0) return false;
    return true;
}

bool SFInventorySnapshots::SameContents(const TArray<FSFInventoryStackSnapshot>& Expected, const TArray<FInventoryStack>& Actual)
{
    return MatchContents(Expected, Actual, false);
}

bool SFInventorySnapshots::ContainsContents(const TArray<FSFInventoryStackSnapshot>& Expected, const TArray<FInventoryStack>& Actual)
{
    return MatchContents(Expected, Actual, true);
}
