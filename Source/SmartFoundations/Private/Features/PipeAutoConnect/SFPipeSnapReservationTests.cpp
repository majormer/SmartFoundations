// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "FGPipeConnectionComponent.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPipeSnapReservationTest, "SmartFoundations.PipeAutoConnect.NativeSnapReservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPipeSnapReservationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Scratch world"), World)) return false;
    auto* Junction = World->SpawnActor<AFGPipelineJunctionHologram>();
    auto* Pipe = World->SpawnActor<AFGBuildablePipeline>();
    TArray<UFGPipeConnectionComponent*> Ports;
    // Deliberately do not use component enumeration order as port identity.
    for (const TCHAR* Name : {TEXT("Connection3"), TEXT("Connection1"), TEXT("Connection0"), TEXT("Connection2")})
    {
        auto* Port = NewObject<UFGPipeConnectionComponent>(Junction, Name);
        Junction->AddInstanceComponent(Port);
        Ports.Add(Port);
    }
    auto Available = [&]()
    {
        TArray<UFGPipeConnectionComponent*> Found;
        FSFPipeConnectorFinder::GetJunctionConnectors(Junction, Found);
        TArray<FName> Names;
        for (const auto* Port : Found) Names.Add(Port->GetFName());
        return Names;
    };
    const auto* SnappedPipe = FindFProperty<FObjectPropertyBase>(Junction->GetClass(), TEXT("mSnappedPipe"));
    const auto* SnappedEndpoint = FindFProperty<FObjectPropertyBase>(Junction->GetClass(), TEXT("mSnappedConnectionComponent"));
    const auto* Index = FindFProperty<FIntProperty>(Junction->GetClass(), TEXT("mSnapConnectionIndex"));
    const auto* CachedPorts = FindFProperty<FArrayProperty>(Junction->GetClass(), TEXT("mPipeConnectionComponents"));
    TestEqual(TEXT("Unsnapped ports remain available"), Available().Num(), 4);
    SnappedPipe->SetObjectPropertyValue_InContainer(Junction, Pipe);
    TestEqual(TEXT("Insertion leaves only two branch ports"), Available().Num(), 2);
    TestTrue(TEXT("Side port preserved"), Available().Contains(TEXT("Connection3")));
    TestFalse(TEXT("Native inline port excluded"), Available().Contains(TEXT("Connection0")));
    Junction->SetActorRotation(FRotator(0, 67, 0));
    TestFalse(TEXT("Rotation does not change the reservation"), Available().Contains(TEXT("Connection1")));
    SnappedPipe->SetObjectPropertyValue_InContainer(Junction, nullptr);
    TestEqual(TEXT("Unsnap restores every port"), Available().Num(), 4);

    // Populate the native cached array in a different order from enumeration.
    FScriptArrayHelper Array(CachedPorts, CachedPorts->ContainerPtrToValuePtr<void>(Junction));
    Array.EmptyValues();
    const auto* Inner = CastFieldChecked<FObjectPropertyBase>(CachedPorts->Inner);
    Inner->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Ports[3]);
    Inner->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Ports[0]);
    Index->SetPropertyValue_InContainer(Junction, 1);
    auto* Peer = NewObject<UFGPipeConnectionComponent>(Pipe);
    SnappedEndpoint->SetObjectPropertyValue_InContainer(Junction, Peer);
    TestEqual(TEXT("Endpoint snap reserves only the selected native component"), Available().Num(), 3);
    TestFalse(TEXT("Native index resolves Connection3, not enumeration index1"), Available().Contains(TEXT("Connection3")));
    TestTrue(TEXT("Unused inline port remains usable"), Available().Contains(TEXT("Connection1")));
    SnappedEndpoint->SetObjectPropertyValue_InContainer(Junction, nullptr);
    TestEqual(TEXT("Endpoint unsnap restores all ports"), Available().Num(), 4);
    World->DestroyWorld(false);
    return true;
}
#endif
