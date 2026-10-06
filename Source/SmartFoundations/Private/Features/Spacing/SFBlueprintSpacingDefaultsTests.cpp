// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Spacing/SFBlueprintSpacingDefaults.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintSpacingDefaultsTest,
    "SmartFoundations.Spacing.BlueprintDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBlueprintSpacingDefaultsTest::RunTest(const FString& Parameters)
{
    using SFBlueprintSpacingDefaults::ToCentimeters;
    TestEqual(TEXT("Legacy default"), ToCentimeters(1.0f), 100);
    TestEqual(TEXT("Flush tiling"), ToCentimeters(0.0f), 0);
    TestEqual(TEXT("Fractional metres"), ToCentimeters(0.25f), 25);
    TestEqual(TEXT("Negative config clamped"), ToCentimeters(-3.0f), 0);
    TestEqual(TEXT("Oversized config clamped before conversion"), ToCentimeters(MAX_flt), 10000);
    TestEqual(TEXT("Nonfinite config falls back to legacy default"), ToCentimeters(std::numeric_limits<float>::quiet_NaN()), 100);
    TestEqual(TEXT("Infinity falls back to legacy default"), ToCentimeters(std::numeric_limits<float>::infinity()), 100);
    return true;
}
#endif
