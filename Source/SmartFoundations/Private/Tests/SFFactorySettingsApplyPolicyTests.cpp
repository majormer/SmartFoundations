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

    TestEqual(TEXT("Empty inventory cannot fund a copied shard"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, 0, 0), 0);
    TestEqual(TEXT("Partial inventory caps the installed target"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, 0, 1), 1);
    TestEqual(TEXT("Existing shards do not need to be purchased again"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, 2, 1), 3);
    TestEqual(TEXT("An already satisfied target needs no player inventory"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(2, 2, 0), 2);
    TestEqual(TEXT("An overfilled machine does not increase the requested target"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(2, 3, 0), 2);
    TestEqual(TEXT("Surplus inventory does not increase the requested target"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, 0, 50), 3);
    TestEqual(TEXT("Negative inventory counts fail closed"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, -1, -1), 0);
    TestEqual(TEXT("Negative requests do not remove items"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(-1, 2, 3), 0);
    TestEqual(TEXT("Large inventory counts cannot overflow the budget"),
        FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(3, 1, MAX_int32), 3);

    // Model the native helper's add-then-remove behavior. Each queued machine must
    // re-budget against the remaining inventory, not reuse a batch's initial count.
    for (int32 ShardsPerMachine = 1; ShardsPerMachine <= 3; ++ShardsPerMachine)
    {
        for (int32 InitialInventory = 0; InitialInventory <= 12; ++InitialInventory)
        {
            int32 PlayerInventory = InitialInventory;
            int32 InstalledTotal = 0;
            for (int32 Machine = 0; Machine < 10; ++Machine)
            {
                const int32 Target = FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(
                    ShardsPerMachine, 0, PlayerInventory);
                InstalledTotal += Target;
                PlayerInventory = FMath::Max(0, PlayerInventory - Target);
                TestEqual(TEXT("Every copied machine conserves the shared item supply"),
                    PlayerInventory + InstalledTotal, InitialInventory);
                TestEqual(TEXT("Reapplying a satisfied snapshot buys no extra shards"),
                    FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(Target, Target, PlayerInventory), Target);
            }
        }
    }

    return true;
}

#endif
