// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Features/Upgrade/SFConveyorUpgradeOwner.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorUpgradeOwnerTest, "SmartFoundations.Upgrade.ConveyorRequestOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorUpgradeOwnerTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USceneComponent> RequesterA(NewObject<USceneComponent>()), RequesterB(NewObject<USceneComponent>());
    FSFConveyorUpgradeOwner Owner;
    FSFUpgradeExecutionParams Intent;
    Intent.RequestId = FGuid::NewGuid(); Intent.RequestSequence = 1;
    Intent.AuthorityWorldGeneration = FGuid::NewGuid();
    Intent.Family = ESFUpgradeFamily::Belt; Intent.SourceTier = 1; Intent.TargetTier = 2;
    const FSFConstructionOperationKey First{FObjectKey(RequesterA.Get()), 1};
    Owner.Remember(First, Intent);
    TestTrue(TEXT("Duplicate while pending returns existing operation"), Owner.ClassifyReplay(First, Intent).GetValue() == ESFConstructionAdmission::Existing);
    auto Changed = Intent; Changed.TargetTier = 3;
    TestTrue(TEXT("Same identity cannot authorize a different tier"), Owner.ClassifyReplay(First, Changed).GetValue() == ESFConstructionAdmission::Invalid);
    Changed = Intent; Changed.AuthorityWorldGeneration = FGuid::NewGuid();
    TestTrue(TEXT("A replay cannot acquire a different authority world"), Owner.ClassifyReplay(First, Changed).GetValue() == ESFConstructionAdmission::Invalid);
    Changed = Intent; Changed.bPreserveDismantleGroupMembership = false;
    TestTrue(TEXT("Group policy belongs to immutable intent"), Owner.ClassifyReplay(First, Changed).GetValue() == ESFConstructionAdmission::Invalid);
    Changed = Intent; Changed.bUseSpecificBuildables = true;
    TestTrue(TEXT("Selection mode cannot be changed on replay"), Owner.ClassifyReplay(First, Changed).GetValue() == ESFConstructionAdmission::Invalid);
    Changed = Intent; Changed.Radius = 1000.0f;
    TestTrue(TEXT("Radius cannot expand on replay"), Owner.ClassifyReplay(First, Changed).GetValue() == ESFConstructionAdmission::Invalid);
    TestFalse(TEXT("Another requester has an independent sequence stream"), Owner.ClassifyReplay({FObjectKey(RequesterB.Get()), 1}, Intent).IsSet());
    FSFUpgradeExecutionResult Result;
    Result.RequestId = Intent.RequestId; Result.RequestSequence = 1;
    Result.ErrorMessage = TEXT("Rejected before effects");
    Owner.Settle(First, Result);
    TestTrue(TEXT("No-effect rejection remains a replay result"), Owner.ClassifyReplay(First, Intent).GetValue() == ESFConstructionAdmission::Existing);
    auto ReplacementResult = Result; ReplacementResult.bCompleted = true;
    Owner.Settle(First, ReplacementResult);
    TestFalse(TEXT("Terminal rejection cannot become success later"), Owner.Requests.FindChecked(First).Result.bCompleted);
    for (int64 Sequence = 2; Sequence <= 18; ++Sequence)
    {
        Intent.RequestId = FGuid::NewGuid(); Intent.RequestSequence = Sequence;
        const FSFConstructionOperationKey Key{First.Requester, static_cast<uint64>(Sequence)};
        Owner.Remember(Key, Intent);
        Result.RequestId = Intent.RequestId; Result.RequestSequence = Sequence;
        Owner.Settle(Key, Result);
    }
    TestEqual(TEXT("Detailed terminal history is bounded"), Owner.Requests.Num(), 16);
    Intent.RequestId = FGuid::NewGuid(); Intent.RequestSequence = 1;
    TestTrue(TEXT("Eviction cannot authorize old delivery as fresh work"), Owner.ClassifyReplay(First, Intent).GetValue() == ESFConstructionAdmission::ReplayRejected);
    TestFalse(TEXT("New sequence can submit a new operation"), Owner.ClassifyReplay({First.Requester, 19}, Intent).IsSet());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorDeclaredSelectionTest, "SmartFoundations.Upgrade.ConveyorDeclaredSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConveyorDeclaredSelectionTest::RunTest(const FString& Parameters)
{
    auto* Belt = GetMutableDefault<AFGBuildableConveyorBelt>();
    auto* Lift = GetMutableDefault<AFGBuildableConveyorLift>();
    FSFUpgradeExecutionParams Intent;
    Intent.bUseSpecificBuildables = true;
    Intent.SpecificBuildables = {Lift};
    TestTrue(TEXT("One declared lift may be replaced"), FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift}));
    TestFalse(TEXT("Owning its connected belt does not authorize replacing that belt"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift, Belt}));
    TestFalse(TEXT("A different actor cannot substitute for the declared actor"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Belt}));
    TestFalse(TEXT("One declared actor cannot be replaced twice"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift, Lift}));
    Intent.SpecificBuildables.Add(Belt);
    TestTrue(TEXT("Explicit mixed belt/lift traversal selection is retained"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift, Belt}));
    Intent.SpecificBuildables.Empty();
    TestFalse(TEXT("Empty explicit intent cannot become a radius request"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift}));
    Intent.bUseSpecificBuildables = false;
    TestTrue(TEXT("Radius gathering supplies its own declared replacement list"),
        FSFConveyorUpgradeOwner::SelectionWithinIntent(Intent, {Lift, Belt}));
    return true;
}
#endif
