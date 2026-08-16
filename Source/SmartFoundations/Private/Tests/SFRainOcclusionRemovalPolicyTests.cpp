// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Net/SFRainOcclusionRemovalPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFRainOcclusionRemovalPolicyTest,
    "SmartFoundations.Net.RainOcclusion.RemovalPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFRainOcclusionRemovalPolicyTest::RunTest(const FString& Parameters)
{
    using Decision = ESFRainOcclusionRemovalDecision;
    using Path = ESFRainOcclusionRemovalPath;

    TestEqual(TEXT("Box path forwards a valid lookup"),
        SFRainOcclusionRemovalPolicy::DecideForPath(Path::BoxSprite, true, true, 23, 24), Decision::Forward);
    TestEqual(TEXT("Mesh path forwards a valid lookup"),
        SFRainOcclusionRemovalPolicy::DecideForPath(Path::MeshShape, true, true, 23, 24), Decision::Forward);
    TestEqual(TEXT("Box path rejects regression values 25 and 24"),
        SFRainOcclusionRemovalPolicy::DecideForPath(Path::BoxSprite, true, true, 25, 24), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Mesh sibling policy rejects out-of-range values 25 and 24"),
        SFRainOcclusionRemovalPolicy::DecideForPath(Path::MeshShape, true, true, 25, 24), Decision::DiscardInconsistentLookup);

    TestEqual(TEXT("Valid first instance forwards to vanilla"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 0, 24), Decision::Forward);
    TestEqual(TEXT("Valid last instance forwards to vanilla"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 23, 24), Decision::Forward);
    TestEqual(TEXT("Regression values 25 and 24 fail closed"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 25, 24), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Index equal to instance count fails closed"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 24, 24), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Negative lookup index fails closed"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, -1, 24), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Empty owner cannot remove index zero"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 0, 0), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Missing owner fails closed"),
        SFRainOcclusionRemovalPolicy::Decide(true, false, 0, 24), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Missing lookup fails closed without assigning a cause"),
        SFRainOcclusionRemovalPolicy::Decide(false, false, 0, 0), Decision::DiscardInconsistentLookup);
    TestEqual(TEXT("Negative owner instance count fails closed"),
        SFRainOcclusionRemovalPolicy::Decide(true, true, 0, -1), Decision::DiscardInconsistentLookup);

    return true;
}

#endif
