// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SFSpecConstructionOwnership.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSpecConstructionOwnershipTest, "SmartFoundations.Net.SpecConstructionOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFSpecConstructionOwnershipTest::RunTest(const FString& Parameters)
{
    using namespace SFSpecConstructionOwnership;
    TestTrue(TEXT("Standalone and listen-host ordinary grids retain local power"),
        CanUseLocalPowerPlan(true, true, true, false, false));
    TestFalse(TEXT("Dedi mirror may be active but is not the local player's grid"),
        CanUseLocalPowerPlan(true, false, true, false, false));
    TestFalse(TEXT("Prepared staged Extend wins even while server aim-mode flag is false"),
        CanUseLocalPowerPlan(true, false, true, false, true));
    TestFalse(TEXT("Any staged intent must be consumed before local power can intercept"),
        CanUseLocalPowerPlan(true, true, true, false, true));
    TestFalse(TEXT("Extend and Restore use their own priced cable plan"),
        CanUseLocalPowerPlan(true, true, true, true, false));
    TestFalse(TEXT("An unrelated active hologram cannot supply a local plan"),
        CanUseLocalPowerPlan(true, true, false, false, false));
    TestFalse(TEXT("Children cannot run a root's local power plan"),
        CanUseLocalPowerPlan(false, true, true, false, false));

    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    ASFFactoryHologram* Root = World->SpawnActor<ASFFactoryHologram>();
    ASFFactoryHologram* Child = World->SpawnActor<ASFFactoryHologram>();
    if (!Root || !Child) { World->DestroyWorld(false); return false; }
    // The distributed Editor SDK's AddChild body is a stub. Install its reflected
    // parent relationship explicitly; otherwise this fixture is still a standalone root.
    FObjectPropertyBase* ParentProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
    if (!TestNotNull(TEXT("Native reflected parent field"), ParentProperty)) { World->DestroyWorld(false); return false; }
    ParentProperty->SetObjectPropertyValue_InContainer(Child, Root);
    TestTrue(TEXT("Fixture has the native parent relationship"), Child->GetParentHologram() == Root);
    TestTrue(TEXT("Parent is eligible for its staged request"), CanOwnStagedRequest(Root));
    TestFalse(TEXT("Same-class child cannot consume or reconstruct its parent's request"), CanOwnStagedRequest(Child));
    TestFalse(TEXT("Missing hologram cannot own a request"), CanOwnStagedRequest(nullptr));
    World->DestroyWorld(false);
    return true;
}
#endif
