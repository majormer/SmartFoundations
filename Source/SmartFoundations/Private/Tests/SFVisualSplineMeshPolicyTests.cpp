// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Net/SFVisualSplineMeshPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFVisualSplineMeshPolicyNetModeTest,
    "SmartFoundations.Net.VisualSplineMeshPolicy.NetModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFVisualSplineMeshPolicyNetModeTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Standalone generates visual spline meshes"), SFVisualSplineMeshPolicy::ShouldGenerate(NM_Standalone));
    TestTrue(TEXT("Listen server generates visual spline meshes"), SFVisualSplineMeshPolicy::ShouldGenerate(NM_ListenServer));
    TestTrue(TEXT("Network client generates visual spline meshes"), SFVisualSplineMeshPolicy::ShouldGenerate(NM_Client));
    TestFalse(TEXT("Dedicated server skips visual spline meshes"), SFVisualSplineMeshPolicy::ShouldGenerate(NM_DedicatedServer));
    return true;
}

#endif
