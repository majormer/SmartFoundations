// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Net/SFConstructPayloadGuard.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFConstructPayloadGuardBoundaryTest,
    "SmartFoundations.Net.ConstructPayloadGuard.Boundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructPayloadGuardBoundaryTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("59,999 bytes fits"), SFConstructPayloadGuard::ShouldReject(59999));
    TestFalse(TEXT("60,000-byte ceiling fits"), SFConstructPayloadGuard::ShouldReject(60000));
    TestTrue(TEXT("60,001 bytes is refused"), SFConstructPayloadGuard::ShouldReject(60001));
    TestTrue(TEXT("65,535 bytes is refused"), SFConstructPayloadGuard::ShouldReject(65535));
    TestTrue(TEXT("65,536 bytes is refused"), SFConstructPayloadGuard::ShouldReject(65536));
    return true;
}

#endif
