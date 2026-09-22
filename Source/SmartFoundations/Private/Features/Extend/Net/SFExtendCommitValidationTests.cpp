// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGResourceDescriptor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendCommitCostTest, "SmartFoundations.Extend.Commit.CostAgreement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendCommitCostTest::RunTest(const FString& Parameters)
{
    using SFExtendCommitValidation::SameCost;
    UClass* A = UFGItemDescriptor::StaticClass();
    UClass* B = UFGResourceDescriptor::StaticClass();
    TestTrue(TEXT("Empty quote"), SameCost({}, {}));
    TestTrue(TEXT("Ordering and split rows do not change price"),
        SameCost({{A, 2}, {B, 3}, {A, 4}}, {{B, 3}, {A, 6}}));
    TestFalse(TEXT("Missing resource rejected"), SameCost({{A, 6}, {B, 3}}, {{A, 6}}));
    TestFalse(TEXT("Extra resource rejected"), SameCost({{A, 6}}, {{A, 6}, {B, 3}}));
    TestFalse(TEXT("Changed quantity rejected"), SameCost({{A, 6}}, {{A, 7}}));
    TestFalse(TEXT("Same quantity of different resource rejected"), SameCost({{A, 6}}, {{B, 6}}));
    TestTrue(TEXT("Zero rows are irrelevant"), SameCost({{A, 0}, {nullptr, 0}}, {}));
    TestFalse(TEXT("Positive cost must have a descriptor"), SameCost({{nullptr, 1}}, {{nullptr, 1}}));
    TestFalse(TEXT("Negative preview quantity rejected even if matched"), SameCost({{A, -1}}, {{A, -1}}));
    TestFalse(TEXT("Negative authoritative quantity rejected"), SameCost({}, {{A, -1}}));
    TestFalse(TEXT("Negative rows cannot cancel a charge"), SameCost({{A, 5}, {A, -5}}, {}));
    TestTrue(TEXT("Aggregation is wider than int32"),
        SameCost({{A, MAX_int32}, {A, MAX_int32}}, {{A, MAX_int32 - 1}, {A, MAX_int32}, {A, 1}}));
    TestFalse(TEXT("Large sum cannot wrap to a small charge"),
        SameCost({{A, MAX_int32}, {A, MAX_int32}, {A, 3}}, {{A, 1}}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendCommitScopeTest, "SmartFoundations.Extend.Commit.RequestIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendCommitScopeTest::RunTest(const FString& Parameters)
{
    using namespace SFExtendCommitValidation;
    AFGHologram* RootA = GetMutableDefault<AFGHologram>();
    AFGHologram* RootB = GetMutableDefault<AFGFactoryHologram>();
    TestFalse(TEXT("Aiming has no active request"), IsRequestActive());
    SetPrepared(RootA, true);
    TestNull(TEXT("Aiming cannot leave prepared state"), FindPrepared(RootA));
    {
        FRequestScope Outer;
        TestTrue(TEXT("Native build enters request"), IsRequestActive());
        TestNull(TEXT("New root is not prepared"), FindPrepared(RootA));
        SetPrepared(RootA, false);
        const bool* Failed = FindPrepared(RootA);
        TestTrue(TEXT("Failed preparation is distinct from no preparation"), Failed && !*Failed);
        TestNull(TEXT("Different root does not inherit preparation"), FindPrepared(RootB));
        {
            FRequestScope Inner;
            TestTrue(TEXT("Nested native request shares recursion guard"), FindPrepared(RootA) == Failed);
            SetPrepared(RootA, true);
            SetPrepared(RootB, false);
        }
        const bool* A = FindPrepared(RootA);
        const bool* B = FindPrepared(RootB);
        TestTrue(TEXT("Nested success remains visible to Construct"), A && *A);
        TestTrue(TEXT("Roots retain independent outcomes"), B && !*B);
    }
    TestFalse(TEXT("Request ends at native return"), IsRequestActive());
    TestNull(TEXT("No preparation leaks into aiming"), FindPrepared(RootA));
    {
        FRequestScope Next;
        TestNull(TEXT("Later build must revalidate same root"), FindPrepared(RootA));
        TestNull(TEXT("Later player cannot inherit another root"), FindPrepared(RootB));
    }
    return true;
}
#endif
