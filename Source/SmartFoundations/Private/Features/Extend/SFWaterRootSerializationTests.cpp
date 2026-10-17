// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendService.h"
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Hologram/FGWaterPumpHologram.h"
#include "Buildables/FGBuildable.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWaterRootSerializationTest, "SmartFoundations.Extend.WaterRootSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWaterRootSerializationTest::RunTest(const FString& Parameters)
{
    UClass* Water = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/WaterPump/Build_WaterPump.Build_WaterPump_C"));
    if (!TestNotNull(TEXT("Native Water Extractor class"), Water)) return false;
    UClass* Root = USFExtendService::GetExtendRootHologramClass(Water);
    TestEqual(TEXT("Water uses resource-aware root"), Root, ASFWaterPumpChildHologram::StaticClass());
    for (const FName Name : {FName(TEXT("mSnappedExtractableResource")), FName(TEXT("mUpgradeTarget"))})
    {
        FProperty* Native = FindFProperty<FProperty>(AFGWaterPumpHologram::StaticClass(), Name);
        TestNotNull(TEXT("Native serialized resource field"), Native);
        TestTrue(TEXT("Root inherits the exact native resource field"), Native && FindFProperty<FProperty>(Root, Name) == Native);
    }
    TestEqual(TEXT("Unrelated roots keep the existing factory path"), USFExtendService::GetExtendRootHologramClass(nullptr), ASFFactoryHologram::StaticClass());
    return true;
}
#endif
