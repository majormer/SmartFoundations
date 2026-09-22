// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Upgrade/SFUpgradeTierPolicy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUpgradeTierPolicyTest, "SmartFoundations.Upgrade.TierSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFUpgradeTierPolicyTest::RunTest(const FString& Parameters)
{
	using namespace SFUpgradeTierPolicy;
	for (ESFUpgradeFamily Family : {ESFUpgradeFamily::PowerPole, ESFUpgradeFamily::WallOutletSingle, ESFUpgradeFamily::WallOutletDouble})
	{
		const bool bDown = AllowsDowngrade(Family);
		TestFalse(TEXT("Power families remain upgrade-only"), bDown);
		TestFalse(TEXT("Maximum-tier power row has no targets"), HasAlternativeTier(3, 3, bDown));
		TestFalse(TEXT("Power downgrade request rejected"), IsValidRequest(3, 1, false, bDown));
		TestFalse(TEXT("All tiers cannot downgrade a power port"), Matches(3, 0, 2, bDown));
		TestTrue(TEXT("All tiers still upgrades a lower power port"), Matches(1, 0, 2, bDown));
	}
	for (ESFUpgradeFamily Family : {ESFUpgradeFamily::Belt, ESFUpgradeFamily::Lift, ESFUpgradeFamily::Pipe})
		TestTrue(TEXT("Logistics downgrade remains enabled"), AllowsDowngrade(Family));
	TestTrue(TEXT("Maximum-tier rows still permit downgrades"), HasAlternativeTier(6, 6));
	TestTrue(TEXT("Existing higher tier can return to unlocked Mk1"), HasAlternativeTier(3, 1));
	TestFalse(TEXT("Mk1-only research has no alternate for Mk1"), HasAlternativeTier(1, 1));
	TestFalse(TEXT("No unlocked recipe"), HasAlternativeTier(2, 0));
	TestTrue(TEXT("Radius downgrade"), IsValidRequest(6, 1, false));
	TestTrue(TEXT("Radius upgrade"), IsValidRequest(1, 6, false));
	TestFalse(TEXT("Radius needs a source row"), IsValidRequest(0, 2, false));
	TestTrue(TEXT("Network can normalize to Mk1"), IsValidRequest(0, 1, true));
	TestFalse(TEXT("Same tier is not a replacement"), IsValidRequest(2, 2, true));
	TestFalse(TEXT("Invalid target"), IsValidRequest(2, -1, true));
	TestFalse(TEXT("Invalid source"), IsValidRequest(-1, 2, true));
	for (int32 Target = 1; Target <= 6; ++Target)
	{
		for (int32 Source = 0; Source <= 6; ++Source)
		{
			for (int32 Current = 1; Current <= 6; ++Current)
			{
				const bool bExpected = Current != Target && (Source == 0 || Source == Current);
				TestEqual(TEXT("Quote, request and cohort selection share tier semantics"),
					Matches(Current, Source, Target), bExpected);
			}
		}
	}
	TestFalse(TEXT("Unknown actor tier excluded"), Matches(0, 0, 2));
	return true;
}
#endif
