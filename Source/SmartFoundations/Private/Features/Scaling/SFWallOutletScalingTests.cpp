// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGPowerPoleWallHologram.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Features/Scaling/SFWallOutletPlacement.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWallOutletScalingTest, "SmartFoundations.Scaling.WallOutlets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFWallOutletScalingTest::RunTest(const FString& Parameters)
{
    // Explicit vanilla allow-list: do not implicitly enable unknown/modded subclasses.
    for (const FString Family : {FString(TEXT("PowerPoleWall")), FString(TEXT("PowerPoleWallDouble"))})
    {
        for (const FString Tier : {FString(), FString(TEXT("_Mk2")), FString(TEXT("_Mk3"))})
        {
            const FString Asset = TEXT("Build_") + Family + Tier;
            const FString Name = Asset + TEXT("_C");
            const FString Path = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/%s/%s.%s"),
                *Family, *Asset, *Name);
            UClass* BuildClass = LoadClass<AFGBuildablePowerPole>(nullptr, *Path);
            TestNotNull(*FString::Printf(TEXT("Vanilla outlet exists: %s"), *Name), BuildClass);
            if (!BuildClass) continue;

            TestTrue(*FString::Printf(TEXT("Explicit profile: %s"), *Name),
                USFBuildableSizeRegistry::HasProfile(BuildClass));
            const FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildClass);
            TestTrue(*FString::Printf(TEXT("Scaling enabled: %s"), *Name), Profile.bSupportsScaling);
            TestTrue(*FString::Printf(TEXT("One-metre grid pitch: %s"), *Name),
                Profile.DefaultSize.Equals(FVector(100.0), 0.001));
            TestTrue(*FString::Printf(TEXT("Wall pivot retained: %s"), *Name), Profile.AnchorOffset.IsZero());
            TestFalse(*FString::Printf(TEXT("No yaw-dependent dimension swap: %s"), *Name),
                Profile.bSwapXYOnRotation);
        }
    }

    for (const TCHAR* Asset : {TEXT("Holo_PowerSocket"), TEXT("Holo_PowerSocketDouble")})
    {
        const FString Path = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/%s.%s_C"), Asset, Asset);
        TestNotNull(*FString::Printf(TEXT("Native wall-pole hologram family: %s"), Asset),
            LoadClass<AFGPowerPoleWallHologram>(nullptr, *Path));
    }
    TestFalse(TEXT("Power wires remain non-scalable"),
        USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_PowerLine_C")).bSupportsScaling);
    TestTrue(TEXT("Ordinary power poles remain scalable"),
        USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_PowerPoleMk1_C")).bSupportsScaling);
    // Regeneration must retain the shipped pipe-support fixes, not stale CSV defaults.
    const TPair<const TCHAR*, FVector> ExistingSupports[] = {
        {TEXT("Build_PipelineSupport_C"), FVector(100, 200, 200)},
        {TEXT("Build_PipelineSupportWall_C"), FVector(200, 200, 200)},
        {TEXT("Build_PipelineSupportWallHole_C"), FVector(100, 100, 100)}
    };
    for (const auto& Support : ExistingSupports)
    {
        const FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfileByName(Support.Key);
        TestTrue(*FString::Printf(TEXT("Existing support stays scalable: %s"), Support.Key), Profile.bSupportsScaling);
        TestTrue(*FString::Printf(TEXT("Existing support pitch retained: %s"), Support.Key),
            Profile.DefaultSize.Equals(Support.Value, 0.001));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWallOutletPlacementTest, "SmartFoundations.Scaling.WallOutletPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFWallOutletPlacementTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;

    TArray<AFGHologram*> Children;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        AFGHologram* Child = World->SpawnActor<AFGHologram>();
        if (!TestNotNull(TEXT("Test hologram"), Child))
        {
            World->DestroyWorld(false);
            return false;
        }
        USceneComponent* Root = NewObject<USceneComponent>(Child);
        Child->SetRootComponent(Root);
        Root->RegisterComponent();
        if (Index < 3) Child->Tags.Add(TEXT("SF_GridChild"));
        Children.Add(Child);
    }

    // The editor SDK stubs native placement. Reproduce the confirmed Shipping write
    // using real actor transforms, including an untagged native companion control.
    for (const FVector Direction : {FVector(0, 1, 0), FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 0, 1)})
    {
        const FVector Origin(191000, -75600, 22000);
        const FVector Connector = Origin + FVector(0, 80, 0);
        TArray<FTransform> Expected;
        for (int32 Index = 0; Index < Children.Num(); ++Index)
        {
            Expected.Add(FTransform(FRotator(0, 90, 0), Origin + Direction * (100 * (Index + 1))));
            Children[Index]->SetActorTransform(Expected.Last());
        }
        int32 NativeCalls = 0;
        FSFWallOutletPlacement::PostPlacement(Children, [&]()
        {
            ++NativeCalls;
            for (AFGHologram* Child : Children) Child->SetActorLocation(Connector);
        });
        TestEqual(TEXT("Vanilla placement runs exactly once"), NativeCalls, 1);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            TestTrue(TEXT("Grid copy retains its authored transform"),
                Children[Index]->GetActorTransform().Equals(Expected[Index], 0.001));
        }
        TestTrue(TEXT("Untagged companion retains vanilla connector placement"),
            Children[3]->GetActorLocation().Equals(Connector, 0.001));
    }

    int32 EmptyCalls = 0;
    FSFWallOutletPlacement::PostPlacement({}, [&]() { ++EmptyCalls; });
    TestEqual(TEXT("Unscaled placement still runs vanilla"), EmptyCalls, 1);
    World->DestroyWorld(false);
    return true;
}
#endif
