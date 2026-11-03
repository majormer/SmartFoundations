// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Extend/SFExtendDistributorSettingsBatch.h"
#include "Curves/CurveFloat.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendDistributorSettingsBatchTest,
    "SmartFoundations.Extend.DistributorSettingsMergedCopies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendDistributorSettingsBatchTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UCurveFloat> First(NewObject<UCurveFloat>());
    TStrongObjectPtr<UCurveFloat> Second(NewObject<UCurveFloat>());
    const FObjectKey FirstKey(First.Get()), SecondKey(Second.Get());
    FSFCapturedDistributorSettings Settings;
    Settings.bCaptured = true;
    Settings.Kind = 1;
    Settings.ClassPath = TEXT("/Script/FactoryGame.FGBuildableSplitterSmart");
    Settings.ConnectionDirections.AddDefaulted();
    Settings.ConnectionDirections[0].PortName = TEXT("Output0");
    Settings.ConnectionDirections[0].Direction = 1;
    Settings.Rules.AddDefaulted();
    Settings.Rules[0].ItemClass = TEXT("/Script/FactoryGame.FGItemDescriptor");
    Settings.Rules[0].OutputIndex = 0;
    TMap<FObjectKey, FSFCapturedDistributorSettings> Batch;
    TestTrue(TEXT("Merged plan records the first returned distributor"),
        SFExtendDistributorSettingsBatch::Add(Batch, FirstKey, Settings));
    TestTrue(TEXT("Merged plan records another returned distributor"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Settings));
    TestTrue(TEXT("Visiting the same scaled copy again is idempotent"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Settings));
    TestEqual(TEXT("Merged plus per-copy traversal applies each actor once"), Batch.Num(), 2);
    auto Conflict = Settings;
    Conflict.ConnectionDirections[0].Direction = 0;
    TestFalse(TEXT("Contradictory port directions cannot overwrite the first plan"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Conflict));
    Conflict = Settings;
    Conflict.Rules[0].OutputIndex = 1;
    TestFalse(TEXT("Contradictory splitter rules cannot overwrite the first plan"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Conflict));
    Conflict = Settings;
    Conflict.Priorities.Add(1);
    TestFalse(TEXT("Different priority data cannot masquerade as an identical entry"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Conflict));
    TestTrue(TEXT("Rejected duplicates leave the accepted settings unchanged"),
        SFExtendDistributorSettingsBatch::Add(Batch, SecondKey, Settings));
    return true;
}
#endif
