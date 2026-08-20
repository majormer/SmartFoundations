// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Net/SFRainOcclusionAdditionPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFRainOcclusionAdditionPolicyTest,
    "SmartFoundations.Net.RainOcclusion.AdditionPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFRainOcclusionAdditionPolicyTest::RunTest(const FString& Parameters)
{
    using Decision = ESFRainOcclusionAdditionDecision;

    TestEqual(TEXT("First registration of a hash forwards to vanilla"),
        SFRainOcclusionAdditionPolicy::Decide(false), Decision::Forward);
    TestEqual(TEXT("Regression #523: a hash already registered is discarded, not forwarded"),
        SFRainOcclusionAdditionPolicy::Decide(true), Decision::DiscardDuplicateAdd);

    return true;
}

#endif
