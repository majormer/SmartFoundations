// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Holograms/Logistics/SFPipelineJunctionChildHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPipeEmptySearchCleanupTest, "SmartFoundations.PipeAutoConnect.EmptySearchCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPipeEmptySearchCleanupTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated empty world"), World)) return false;
    ASFPipelineJunctionChildHologram* Junction = World->SpawnActor<ASFPipelineJunctionChildHologram>();
    ASFPipelineJunctionChildHologram* ControlJunction = World->SpawnActor<ASFPipelineJunctionChildHologram>();
    ASFPipelineHologram* A = World->SpawnActor<ASFPipelineHologram>();
    ASFPipelineHologram* B = World->SpawnActor<ASFPipelineHologram>();
    ASFPipelineHologram* Control = World->SpawnActor<ASFPipelineHologram>();
    // The editor SDK's AddChild is a stub. Seed its native list explicitly so cleanup
    // must remove real entries; no construction or physical connection is simulated.
    FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
    FScriptArrayHelper Children(Property, Property->ContainerPtrToValuePtr<void>(Junction));
    auto AddChild = [&](AFGHologram* Child)
    {
        const int32 Index = Children.AddValue();
        CastFieldChecked<FObjectPropertyBase>(Property->Inner)->SetObjectPropertyValue(Children.GetRawPtr(Index), Child);
    };
    AddChild(A); AddChild(B);
    FSFPipeAutoConnectManager Manager;
    Manager.Initialize(NewObject<USFSubsystem>(World), NewObject<USFAutoConnectService>(World));
    Manager.BuildingPipeChildren.Add(Junction, A);
    Manager.BuildingPipeChildrenB.Add(Junction, B);
    Manager.BuildingPipeChildren.Add(ControlJunction, Control);
    Manager.ProcessPipeJunctions(Junction);
    TestFalse(TEXT("Empty search clears side A's old pipe"), Manager.BuildingPipeChildren.Contains(Junction));
    TestFalse(TEXT("Empty search clears side B's old pipe"), Manager.BuildingPipeChildrenB.Contains(Junction));
    TestEqual(TEXT("Both stale pipes removed from construction children"), Junction->GetHologramChildren().Num(), 0);
    TestTrue(TEXT("Side A preview retired"), A->IsActorBeingDestroyed());
    TestTrue(TEXT("Side B preview retired"), B->IsActorBeingDestroyed());
    TestTrue(TEXT("Another junction's preview survives"), Manager.BuildingPipeChildren.FindRef(ControlJunction).Get() == Control);
    TestFalse(TEXT("Unrelated preview is not retired"), Control->IsActorBeingDestroyed());
    Manager.BuildingPipeChildren.Empty(); Manager.BuildingPipeChildrenB.Empty();
    World->DestroyWorld(false);
    return true;
}
#endif
