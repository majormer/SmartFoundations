// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UI/SFInterfacePreferences.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPanelLayoutBoundsTest, "SmartFoundations.Interface.LayoutBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPanelLayoutBoundsTest::RunTest(const FString&)
{
    TestEqual(TEXT("Offscreen saved position is reachable after smaller resolution"), SFInterface::ClampPosition({1800,1000},{400,600},{1280,720}), FVector2D(880,120));
    TestEqual(TEXT("Negative position is corrected"), SFInterface::ClampPosition({-200,-100},{400,600},{1280,720}), FVector2D::ZeroVector);
    TestEqual(TEXT("Oversized panel keeps its header at the visible origin"), SFInterface::ClampPosition({600,500},{1800,1200},{1280,720}), FVector2D::ZeroVector);
    TestEqual(TEXT("Valid local position stays unchanged"), SFInterface::ClampPosition({40,180},{400,300},{1920,1080}), FVector2D(40,180));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudAppearanceValidationTest, "SmartFoundations.Interface.AppearanceValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFHudAppearanceValidationTest::RunTest(const FString&)
{
    SFInterface::FHudAppearance Value;
    Value.Scale = .1f;
    TestFalse(TEXT("Unreadable scale cannot be committed"), SFInterface::WriteHud(nullptr, Value));
    Value.Scale = 1; Value.Theme = 6;
    TestFalse(TEXT("Unknown theme cannot be committed"), SFInterface::WriteHud(nullptr, Value));
    Value.Theme = 0;
    TestFalse(TEXT("No local settings context cannot be reported saved"), SFInterface::WriteHud(nullptr, Value));
    return true;
}
#endif
