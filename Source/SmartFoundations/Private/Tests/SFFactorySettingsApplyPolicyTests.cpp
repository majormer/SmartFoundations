// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Construction/SFFactorySettingsApplyPolicy.h"
#include "Core/Construction/SFFactorySettingsSnapshot.h"
#include <limits>
#include "Misc/AutomationTest.h"
#include "Resources/FGPowerShardDescriptor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFFactorySettingsApplyPolicyTest,
    "SmartFoundations.Construction.FactorySettings.ApplyPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFactorySettingsApplyPolicyTest::RunTest(const FString& Parameters)
{
    using Decision = ESFFactorySettingsApplyDecision;

    FSFFactorySettingsSnapshot Snapshot;
    TestFalse(TEXT("Empty snapshot has no settings"), Snapshot.HasAnySettings());
    TestFalse(TEXT("Empty snapshot needs no inventory transfer"), Snapshot.NeedsInventoryTransfer());
    Snapshot.bHasRecipe = true;
    TestTrue(TEXT("Recipe-only snapshot has settings"), Snapshot.HasAnySettings());
    TestFalse(TEXT("Recipe-only snapshot needs no inventory transfer"), Snapshot.NeedsInventoryTransfer());
    Snapshot.bHasPotential = true;
    TestTrue(TEXT("Potential snapshot needs inventory transfer"), Snapshot.NeedsInventoryTransfer());
    Snapshot.bHasPotential = false;
    Snapshot.bHasProductionBoost = true;
    TestTrue(TEXT("Production-boost snapshot needs inventory transfer"), Snapshot.NeedsInventoryTransfer());

    TestEqual(TEXT("Destroyed factory aborts"),
        FSFFactorySettingsApplyPolicy::Decide(false, false, true, false, false, 1, 25), Decision::Abort);
    TestEqual(TEXT("Recipe-only contract waits for BeginPlay"),
        FSFFactorySettingsApplyPolicy::Decide(true, false, false, false, false, 1, 25), Decision::Retry);
    TestEqual(TEXT("Recipe-only contract does not wait for inventories after BeginPlay"),
        FSFFactorySettingsApplyPolicy::Decide(true, true, false, false, false, 1, 25), Decision::Apply);
    TestEqual(TEXT("Missing target inventory retries before the bound"),
        FSFFactorySettingsApplyPolicy::Decide(true, true, true, false, true, 24, 25), Decision::Retry);
    TestEqual(TEXT("Missing player inventory retries before the bound"),
        FSFFactorySettingsApplyPolicy::Decide(true, true, true, true, false, 24, 25), Decision::Retry);
    TestEqual(TEXT("Missing readiness gives up at the bound"),
        FSFFactorySettingsApplyPolicy::Decide(true, true, true, false, true, 25, 25), Decision::GiveUp);
    TestEqual(TEXT("Ready inventory-backed contract applies"),
        FSFFactorySettingsApplyPolicy::Decide(true, true, true, true, true, 1, 25), Decision::Apply);

    float PendingValue = 0.0f;
    TestTrue(TEXT("Finite pending value resolves"),
        FSFFactorySettingsApplyPolicy::TryResolvePendingValue(2.5f, 2.0f, PendingValue));
    TestEqual(TEXT("Pending value is capped by current maximum"), PendingValue, 2.0f);
    TestFalse(TEXT("NaN requested value is rejected"),
        FSFFactorySettingsApplyPolicy::TryResolvePendingValue(std::numeric_limits<float>::quiet_NaN(), 2.5f, PendingValue));
    TestFalse(TEXT("NaN current maximum is rejected"),
        FSFFactorySettingsApplyPolicy::TryResolvePendingValue(2.0f, std::numeric_limits<float>::quiet_NaN(), PendingValue));
    TestFalse(TEXT("Unusable current maximum is rejected"),
        FSFFactorySettingsApplyPolicy::TryResolvePendingValue(2.0f, 1.0f, PendingValue));

    TestFalse(TEXT("Incompatible recipe is rejected"),
        FSFFactorySettingsApplyPolicy::IsRecipeApplicationAllowed(true, false));
    TestTrue(TEXT("Compatible present recipe is accepted"),
        FSFFactorySettingsApplyPolicy::IsRecipeApplicationAllowed(true, true));
    TestFalse(TEXT("Missing recipe is rejected even when compatibility input is true"),
        FSFFactorySettingsApplyPolicy::IsRecipeApplicationAllowed(false, true));

    TestTrue(TEXT("Overclock descriptor may enter overclock slots"),
        FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(EPowerShardType::PST_Overclock, EPowerShardType::PST_Overclock));
    TestFalse(TEXT("Production boost descriptor may not enter overclock slots"),
        FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(EPowerShardType::PST_Overclock, EPowerShardType::PST_ProductionBoost));
    TestFalse(TEXT("None descriptor type is rejected"),
        FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(EPowerShardType::PST_ProductionBoost, EPowerShardType::PST_None));

    TestEqual(TEXT("Unreturned displaced items preserve the partial remainder"),
        FSFFactorySettingsApplyPolicy::GetUnreturnedItemCount(5, 2), 3);
    TestEqual(TEXT("Fully returned displaced stack leaves no world drop"),
        FSFFactorySettingsApplyPolicy::GetUnreturnedItemCount(5, 5), 0);
    TestEqual(TEXT("Overreported inventory add cannot produce a negative remainder"),
        FSFFactorySettingsApplyPolicy::GetUnreturnedItemCount(5, 7), 0);

    return true;
}

#endif
