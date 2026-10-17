// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCounterResetMirrorTest,
    "SmartFoundations.Controls.CounterResetMirror",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCounterResetMirrorTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    USFSubsystem* Subsystem = NewObject<USFSubsystem>(World);
    FSFCounterState Expanded;
    Expanded.GridCounters = FIntVector(6, 5, 1);
    Subsystem->UpdateCounterState(Expanded);
    TestEqual(TEXT("Legacy reader sees expanded grid"), Subsystem->GetGridCounters(), Expanded.GridCounters);
    Subsystem->ResetCounters();
    TestEqual(TEXT("Authoritative state resets"), Subsystem->GetCounterState().GridCounters, FIntVector(1, 1, 1));
    TestEqual(TEXT("Camera and other legacy readers reset too"), Subsystem->GetGridCounters(), FIntVector(1, 1, 1));

    // The startup/fallback branch must have the same reset contract.
    FObjectPropertyBase* Service = FindFProperty<FObjectPropertyBase>(USFSubsystem::StaticClass(), TEXT("GridStateService"));
    if (!TestNotNull(TEXT("Grid service property"), Service)) { World->DestroyWorld(false); return false; }
    Service->SetObjectPropertyValue_InContainer(Subsystem, nullptr);
    Subsystem->UpdateCounterState(Expanded);
    Subsystem->ResetCounters();
    TestEqual(TEXT("Fallback authoritative state resets"), Subsystem->GetCounterState().GridCounters, FIntVector(1, 1, 1));
    TestEqual(TEXT("Fallback legacy reader resets"), Subsystem->GetGridCounters(), FIntVector(1, 1, 1));
    World->DestroyWorld(false);
    return true;
}
#endif
