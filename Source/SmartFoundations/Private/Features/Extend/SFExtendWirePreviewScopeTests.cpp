// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendWirePreviewScope.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendWirePreviewScopeTest, "SmartFoundations.Extend.Power.PreviewConstructionScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendWirePreviewScopeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ASFWireHologram* First = World->SpawnActor<ASFWireHologram>();
    ASFWireHologram* Last = World->SpawnActor<ASFWireHologram>();
    ASFWireHologram* OtherWire = World->SpawnActor<ASFWireHologram>();
    ASFFactoryHologram* Factory = World->SpawnActor<ASFFactoryHologram>();
    if (!First || !Last || !OtherWire || !Factory) { World->DestroyWorld(false); return false; }
    First->Tags.Add(TEXT("SF_ExtendWirePlan"));
    Last->Tags.Add(TEXT("SF_ExtendWirePlan"));
    Factory->Tags.Add(TEXT("SF_ExtendWirePlan")); // tag alone cannot exclude an unrelated class
    OtherWire->Tags.Add(TEXT("SF_ExtendChild")); // do not intercept a different wiring contract
    First->SetWireEndpoints(FVector::ZeroVector, FVector(1000, 0, 0));
    const float QuotedLength = First->GetWireLength();
    TArray<TObjectPtr<AFGHologram>> Children = {First, Factory, OtherWire, Last};
    const TArray<TObjectPtr<AFGHologram>> Original = Children;
    {
        FSFExtendWirePreviewScope Scope(Children);
        TestEqual(TEXT("Native loop sees only constructible children"), Children.Num(), 2);
        TestFalse(TEXT("First priced wire cannot spawn a raw duplicate"), Children.Contains(First));
        TestFalse(TEXT("Last priced wire cannot spawn a raw duplicate"), Children.Contains(Last));
        TestTrue(TEXT("Factory remains even if tagged"), Children.Contains(Factory));
        TestTrue(TEXT("Other wire construction remains untouched"), Children.Contains(OtherWire));
        TestTrue(TEXT("Excluded preview stays alive"), IsValid(First));
        {
            FSFExtendWirePreviewScope Nested(Children);
            TestEqual(TEXT("Nested wrappers do not remove more children"), Children.Num(), 2);
        }
        TestEqual(TEXT("Inner return cannot reinsert the outer previews"), Children.Num(), 2);
    }
    TestTrue(TEXT("Original child order restored before post-Construct price query"), Children == Original);
    TestEqual(TEXT("Cable length used by GetCost is preserved"), First->GetWireLength(), QuotedLength);
    {
        FSFExtendWirePreviewScope Scope(Children);
        Children.Add(First); // tolerate another hook restoring one preview
    }
    TestEqual(TEXT("No duplicate child after nested handler changes"), Children.Num(), Original.Num());
    World->DestroyWorld(false);
    return true;
}
#endif
