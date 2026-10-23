// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Misc/AutomationTest.h"
#include "Subsystem/SFSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftModeSettingsTest, "SmartFoundations.AutoConnect.LiftModeSettings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftModeSettingsTest::RunTest(const FString& Parameters)
{
    FSmart_ConfigStruct Config;
    USFSubsystem::FAutoConnectRuntimeSettings Baseline;
    Baseline.InitFromConfig(Config);
    TestEqual(TEXT("Existing saves default to Belts Only"), Baseline.LiftUsageMode, 0);
    for (int32 Mode = 0; Mode <= 2; ++Mode)
    {
        Config.LiftUsageMode = Mode;
        USFSubsystem::FAutoConnectRuntimeSettings Settings;
        Settings.InitFromConfig(Config);
        TestEqual(TEXT("Each configured mode reaches runtime"), Settings.LiftUsageMode, Mode);
        TestEqual(TEXT("Mode-only changes invalidate config equality"), Settings.EqualsConfigDerived(Baseline), Mode == 0);
        TestEqual(TEXT("Mode does not alter the routing style"), Settings.BeltRoutingMode, Baseline.BeltRoutingMode);
    }
    Config.LiftUsageMode = -20;
    Baseline.InitFromConfig(Config);
    TestEqual(TEXT("Negative config cannot escape the mode range"), Baseline.LiftUsageMode, 0);
    Config.LiftUsageMode = 99;
    Baseline.InitFromConfig(Config);
    TestEqual(TEXT("Large config cannot escape the mode range"), Baseline.LiftUsageMode, 2);
    return true;
}
#endif
