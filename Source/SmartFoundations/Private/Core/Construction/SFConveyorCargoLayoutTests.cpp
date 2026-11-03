// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Misc/AutomationTest.h"
#include "Core/Construction/SFConveyorCohort.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorCargoLayoutTest, "SmartFoundations.Construction.ConveyorCargoLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorCargoLayoutTest::RunTest(const FString& Parameters)
{
    using EStatus = ESFConveyorCohortStatus;
    using FLayout = FSFConveyorCohort::FCargoLayout;
    TArray<FSFConveyorCohort::FCargoSlot> Slots;
    FString Reason;
    auto Status = [&](const FLayout& Layout) { return FSFConveyorCohortValidator::ValidateCargoLayout(Layout, Slots, Reason); };
    FLayout Good;
    Good.AllocatedCapacity = Good.CachedCapacity = 8; Good.Lead = 6; Good.Tail = 2;
    Good.SegmentRanges = {{6, 7}, {INDEX_NONE, INDEX_NONE}, {0, 2}};
    TestTrue(TEXT("Wrapped active range with empty middle segment is valid"), Status(Good) == EStatus::CoherentStructure);
    TestEqual(TEXT("Unused preallocation is not cargo"), Slots.Num(), 5);
    TestEqual(TEXT("Lead physical slot retained"), Slots[0].PhysicalIndex, 6);
    TestEqual(TEXT("Wrap physical slot retained"), Slots[2].PhysicalIndex, 0);
    TestEqual(TEXT("Empty segment doesn't shift ownership"), Slots[2].SegmentIndex, 2);
    auto Case = Good; Case.CachedCapacity = 9;
    TestTrue(TEXT("Cached capacity cannot override backing array"), Status(Case) == EStatus::Inconsistent);
    TestTrue(TEXT("Rejected layout clears prior published slots"), Slots.IsEmpty());
    Case = Good; Case.Lead = INDEX_NONE;
    TestTrue(TEXT("One-sided empty header isn't zero cargo"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Tail = 8;
    TestTrue(TEXT("Out-of-bounds ring endpoints rejected before native getter"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[0].Last = INDEX_NONE;
    TestTrue(TEXT("One-sided empty segment rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[0].First = -2;
    TestTrue(TEXT("Negative physical segment index rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[0].Last = 8;
    TestTrue(TEXT("Segment physical bounds independently checked"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[1] = {7, 0};
    TestTrue(TEXT("Overlapping segment owners rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[2] = {1, 2};
    TestTrue(TEXT("Unclaimed active slot rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.SegmentRanges[2] = {0, 3};
    TestTrue(TEXT("Unused backing slot isn't live segment cargo"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Lead = Case.Tail = 4; Case.SegmentRanges = {{4, 4}};
    TestTrue(TEXT("Equal nonempty endpoints contain one item"), Status(Case) == EStatus::CoherentStructure);
    TestEqual(TEXT("Equal endpoints don't count all storage"), Slots.Num(), 1);
    Case.Lead = 3; Case.Tail = 2; Case.SegmentRanges = {{3, 2}};
    TestTrue(TEXT("Full wrapped ring is explicitly countable"), Status(Case) == EStatus::CoherentStructure);
    TestEqual(TEXT("Full ring has capacity active items"), Slots.Num(), 8);
    Case.Lead = Case.Tail = INDEX_NONE; Case.SegmentRanges = {{INDEX_NONE, INDEX_NONE}};
    TestTrue(TEXT("Empty allocated ring has no cargo"), Status(Case) == EStatus::CoherentStructure);
    TestTrue(TEXT("Empty ring exposes no storage slots"), Slots.IsEmpty());
    Case.SegmentRanges = {{0, 0}};
    TestTrue(TEXT("Empty ring cannot have occupied segment"), Status(Case) == EStatus::Inconsistent);
    Case = FLayout();
    TestTrue(TEXT("Zero capacity transition isn't a settled ring"), Status(Case) == EStatus::NativePending);
    Case.CachedCapacity = 8;
    TestTrue(TEXT("Native export may retain previous cached capacity"), Status(Case) == EStatus::NativePending);
    Case.Lead = Case.Tail = 0;
    TestTrue(TEXT("No backing storage cannot carry nonempty headers"), Status(Case) == EStatus::Inconsistent);
    Case.Lead = Case.Tail = INDEX_NONE;
    Case.AllocatedCapacity = Case.CachedCapacity = MAX_int32;
    TestTrue(TEXT("Huge capacity rejected before allocation or integer wrap"), Status(Case) == EStatus::Unsupported);
    return true;
}
#endif
