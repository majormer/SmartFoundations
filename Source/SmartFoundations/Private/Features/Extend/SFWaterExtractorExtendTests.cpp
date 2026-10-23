// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Buildables/FGBuildableWaterPump.h"
#include "Holograms/Adapters/SFWaterExtractorAdapter.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWaterExtractorExtendTest, "SmartFoundations.Extend.WaterExtractorDestinations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFWaterExtractorExtendTest::RunTest(const FString& Parameters)
{
    UClass* WaterClass = LoadClass<AFGBuildableWaterPump>(nullptr,
        TEXT("/Game/FactoryGame/Buildable/Factory/WaterPump/Build_WaterPump.Build_WaterPump_C"));
    if (!TestNotNull(TEXT("Native Water Extractor class"), WaterClass)) return false;
    TestTrue(TEXT("Vanilla Water Extractor supported"), ASFWaterPumpChildHologram::IsVanillaWaterExtractor(WaterClass));
    TestFalse(TEXT("Abstract extractor is not an eligible source"), ASFWaterPumpChildHologram::IsVanillaWaterExtractor(AFGBuildableWaterPump::StaticClass()));
    TestFalse(TEXT("Missing destination is invalid"), ASFWaterPumpChildHologram::ValidateWaterPosition(nullptr));

    // FactoryGame's editor SDK implements AFGWaterVolume::EncompassesPoint as a false-returning
    // stub. Test the shared destination gate with controlled environment results, not fake claims
    // of native water/depth physics. Actual terrain/water acceptance belongs to the game.
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ASFFactoryHologram* Parent = World->SpawnActor<ASFFactoryHologram>();
    auto EnsureRoot = [](AActor* Actor)
    {
        if (!Actor->GetRootComponent())
        {
            USceneComponent* Root = NewObject<USceneComponent>(Actor);
            Actor->SetRootComponent(Root);
            Root->RegisterComponent();
        }
    };
    EnsureRoot(Parent);
    FObjectPropertyBase* BuildClass = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mBuildClass"));
    BuildClass->SetObjectPropertyValue_InContainer(Parent, WaterClass);
    Parent->SetActorLocation(FVector(0, 0, -1));
    FSFWaterExtractorAdapter Adapter(Parent);
    TestTrue(TEXT("Swapped parent retains horizontal controls"), Adapter.SupportsFeature(ESFFeature::ScaleX));
    TestFalse(TEXT("Vertical extractor grids remain disabled"), Adapter.SupportsFeature(ESFFeature::ScaleZ));
    ASFWaterPumpChildHologram* Child = ASFWaterPumpChildHologram::SpawnPreview(Parent, TEXT("WaterCell"), FVector(4000, 0, -1));
    if (!TestNotNull(TEXT("Extend/Restore use specialized water child"), Child)) { World->DestroyWorld(false); return false; }
    EnsureRoot(Child);
    Child->SetActorLocation(FVector(4000, 0, -1));
    // AddChild is also a no-op in the editor SDK. Install the fixture graph explicitly;
    // this test validates the gate, not native attachment/construction behavior.
    FArrayProperty* ChildrenProperty = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
    FScriptArrayHelper Children(ChildrenProperty, ChildrenProperty->ContainerPtrToValuePtr<void>(Parent));
    const int32 ChildIndex = Children.AddValue();
    CastFieldChecked<FObjectPropertyBase>(ChildrenProperty->Inner)->SetObjectPropertyValue(Children.GetRawPtr(ChildIndex), Child);
    bool ParentAllowed = true, ChildAllowed = true;
    TSet<const AFGHologram*> Checked;
    auto Validate = [&](const AFGHologram* Cell)
    {
        Checked.Add(Cell);
        return Cell == Parent ? ParentAllowed : ChildAllowed;
    };
    TestTrue(TEXT("All independently validated destinations accepted"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    TestTrue(TEXT("Both source-position parent and destination child were queried"), Checked.Contains(Parent) && Checked.Contains(Child));
    ChildAllowed = false;
    TestFalse(TEXT("Valid parent cannot authorize an invalid destination"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    ChildAllowed = true;
    ParentAllowed = false;
    TestFalse(TEXT("Invalid parent cannot borrow child validity"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    ParentAllowed = true;
    TestTrue(TEXT("Corrected destination recovers without a sticky rejection"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    Child->SetActorLocation(FVector(4000, 0, -101));
    TestFalse(TEXT("Imported vertical grid cannot bypass horizontal-only controls"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    Child->SetActorLocation(FVector(4000, 0, -1));
    Child->SetActorRotation(FRotator(30, 0, 0));
    TestFalse(TEXT("Tilted copies cannot bypass native orientation"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    Child->SetActorRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Restoring the legal pose recovers"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent, Validate));
    TestFalse(TEXT("Production path still requires actual environmental validity"), ASFWaterPumpChildHologram::ValidateWaterTree(Parent));
    BuildClass->SetObjectPropertyValue_InContainer(Parent, nullptr);
    Child->SetChildBuildClass(nullptr);
    World->DestroyWorld(false);
    return true;
}
#endif
