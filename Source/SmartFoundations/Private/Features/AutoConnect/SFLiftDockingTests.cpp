// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Misc/AutomationTest.h"
#include "Features/AutoConnect/Preview/SFLiftDocking.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftDockingTest, "SmartFoundations.AutoConnect.DirectLiftDocking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftDockingTest::RunTest(const FString& Parameters)
{
    const FVector Mouth(0, 300, 500), Normal(0, 1, 0), Peer(0, 600, 500);
    auto Matches = [&](FVector Position, FVector Facing, FVector Endpoint)
    { return SFLiftDocking::Matches(Position, Facing, 200, Endpoint, -Normal, 100); };
    TestTrue(TEXT("Native opposite-facing clearance arms dock"), Matches(Mouth, Normal, Peer));
    TestTrue(TEXT("Native coordinate rounding is tolerated"), Matches(Mouth + FVector(0.01, 0, 0), Normal, Peer));
    TestFalse(TEXT("Coincident markers are not the native dock"), Matches(Peer, Normal, Peer));
    TestFalse(TEXT("A short belt-sized gap is not a direct dock"), Matches(Mouth + FVector(0, 50, 0), Normal, Peer));
    TestFalse(TEXT("Misaligned shafts are not direct"), Matches(Mouth + FVector(1, 0, 0), Normal, Peer));
    TestFalse(TEXT("Wrong height is not direct"), Matches(Mouth + FVector(0, 0, 1), Normal, Peer));
    TestFalse(TEXT("Same-facing connectors are not direct"), Matches(Mouth, -Normal, Peer));
    return true;
}
#endif
