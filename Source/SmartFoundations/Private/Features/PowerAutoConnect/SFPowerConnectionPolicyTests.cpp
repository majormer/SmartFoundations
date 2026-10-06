// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/PowerAutoConnect/SFPowerConnectionPolicy.h"
#include "Features/PowerAutoConnect/SFPowerBuildingTarget.h"
#include "Features/PowerAutoConnect/Net/SFPowerWireEndpoint.h"
#include "Features/Scaling/SFWallOutletPlacement.h"
#include "Misc/AutomationTest.h"
#include "Hologram/FGHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Buildables/FGBuildable.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPowerPortBudgetTest, "SmartFoundations.Power.PortBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPowerPortBudgetTest::RunTest(const FString& Parameters)
{
    using SFPowerConnectionPolicy::AvailableSlots;
    TestEqual(TEXT("Mk1 generator face 2/4"), AvailableSlots(4, 2, 0, 0), 2);
    TestEqual(TEXT("Mk1 opposite face 3/4"), AvailableSlots(4, 3, 0, 0), 1);
    TestEqual(TEXT("Grid and building reservations share one face budget"), AvailableSlots(4, 2, 1, 1), 0);
    TestEqual(TEXT("Full face does not borrow from opposite face"), AvailableSlots(4, 4, 0, 0), 0);
    TestEqual(TEXT("No-tech building has one native slot"), AvailableSlots(1, 0, 0, 0), 1);
    TestEqual(TEXT("MAM-unlocked building has two native slots"), AvailableSlots(2, 1, 0, 0), 1);
    TestEqual(TEXT("Existing plus planned saturates unlocked building"), AvailableSlots(2, 1, 1, 0), 0);
    TestEqual(TEXT("Overbooked port stays unavailable"), AvailableSlots(4, 5, 0, 0), 0);
    TestTrue(TEXT("Known double Mk3 accepted"), SFPowerConnectionPolicy::IsWallOutlet(TEXT("Build_PowerPoleWallDouble_Mk3_C")));
    TestFalse(TEXT("Unknown subclass not opted in by substring"), SFPowerConnectionPolicy::IsWallOutlet(TEXT("Build_PowerPoleWallModded_C")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPowerWallGridTest, "SmartFoundations.Power.WallGridTopology",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPowerWallGridTest::RunTest(const FString& Parameters)
{
    for (const FIntVector Size : {FIntVector(1, 4, 1), FIntVector(1, 1, 4), FIntVector(3, 4, 2), FIntVector(2, 3, 3)})
    {
        TArray<FIntVector> Cells;
        for (int32 Z = 0; Z < Size.Z; ++Z)
            for (int32 Y = 0; Y < Size.Y; ++Y)
                for (int32 X = 0; X < Size.X; ++X) Cells.Add(FIntVector(X, Y, Z));
        const auto Edges = SFPowerConnectionPolicy::GridEdges(Cells, 0);
        TestEqual(TEXT("Auto connects every cell exactly once as a chain"), Edges.Num(), Cells.Num() - 1);
        TMap<FIntVector, int32> Degree;
        for (const auto& Edge : Edges)
        {
            ++Degree.FindOrAdd(Edge.Key); ++Degree.FindOrAdd(Edge.Value);
            const FIntVector Delta = Edge.Value - Edge.Key;
            TestEqual(TEXT("No diagonal/jump edges"), FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) + FMath::Abs(Delta.Z), 1);
        }
        for (const auto& Entry : Degree) TestTrue(TEXT("Backbone leaves Mk1 building slots"), Entry.Value <= 2);
        TestEqual(TEXT("X mode count"), SFPowerConnectionPolicy::GridEdges(Cells, 1).Num(), (Size.X - 1) * Size.Y * Size.Z);
        TestEqual(TEXT("Y mode count"), SFPowerConnectionPolicy::GridEdges(Cells, 2).Num(), Size.X * (Size.Y - 1) * Size.Z);
    }
    TestEqual(TEXT("Missing cell never produces long jump"),
        SFPowerConnectionPolicy::GridEdges({FIntVector(0, 0, 0), FIntVector(0, 2, 0)}, 0).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPowerExactEndpointTest, "SmartFoundations.Power.ExactEndpoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPowerExactEndpointTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    UClass* BuildClass = LoadClass<AFGBuildable>(nullptr,
        TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWallDouble/Build_PowerPoleWallDouble.Build_PowerPoleWallDouble_C"));
    if (!TestNotNull(TEXT("Double outlet class"), BuildClass)) { World->DestroyWorld(false); return false; }
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // Deferred actors avoid native BeginPlay and Blueprint side effects in this identity-only test.
    Spawn.bDeferConstruction = true;
    AFGBuildable* First = World->SpawnActor<AFGBuildable>(BuildClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    AFGBuildable* Neighbor = World->SpawnActor<AFGBuildable>(BuildClass, FVector(100, 0, 0), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("First outlet"), First) || !TestNotNull(TEXT("Neighbor outlet"), Neighbor)) { World->DestroyWorld(false); return false; }
    for (AFGBuildable* Actor : {First, Neighbor})
    {
        USceneComponent* Root = NewObject<USceneComponent>(Actor);
        Actor->SetRootComponent(Root); Root->RegisterComponent();
        Actor->SetActorLocation(Actor == First ? FVector::ZeroVector : FVector(100, 0, 0));
        for (const TCHAR* Name : {TEXT("Face1"), TEXT("Face2")})
        {
            UFGPowerConnectionComponent* Port = NewObject<UFGPowerConnectionComponent>(Actor, FName(Name));
            Actor->AddInstanceComponent(Port);
        }
    }
    FSFPowerWireEndpoint Endpoint;
    Endpoint.BuildClass = BuildClass;
    Endpoint.ComponentName = TEXT("Face2");
    TArray<AActor*> Children{Neighbor};
    auto* Resolved = Endpoint.Resolve(First, Children);
    TestTrue(TEXT("Named opposite face on exact owner"), Resolved && Resolved->GetOwner() == First && Resolved->GetFName() == TEXT("Face2"));
    Endpoint.ComponentName = TEXT("MissingFace");
    TestNull(TEXT("Missing face never selects first port"), Endpoint.Resolve(First, Children));
    Endpoint.ComponentName = TEXT("Face2");
    Endpoint.OwnerLocation = FVector(50, 0, 0);
    TestNull(TEXT("No nearest-neighbor fallback"), Endpoint.Resolve(First, Children));
    Endpoint.bExisting = true; Endpoint.ExistingActor = Neighbor;
    TestTrue(TEXT("Existing actor identity overrides proximity"), Endpoint.Resolve(First, {}) && Endpoint.Resolve(First, {})->GetOwner() == Neighbor);
    Endpoint.ExistingActor = nullptr;
    TestNull(TEXT("Missing existing actor never resolves a new neighbor"), Endpoint.Resolve(First, Children));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPowerWallWirePlacementTest, "SmartFoundations.Power.WallWirePlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPowerWallWirePlacementTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Preview world"), World)) return false;
    AFGHologram* Wire = World->SpawnActor<AFGHologram>();
    if (!TestNotNull(TEXT("Wire preview"), Wire)) { World->DestroyWorld(false); return false; }
    USceneComponent* Root = NewObject<USceneComponent>(Wire);
    Wire->SetRootComponent(Root); Root->RegisterComponent();
    Wire->Tags.Add(TEXT("SF_ExactPowerPlan"));
    Wire->SetActorLocation(FVector(100, 200, 300));
    TArray<AFGHologram*> Children{Wire};
    FSFWallOutletPlacement::PostPlacement(Children, [&]() { Wire->SetActorLocation(FVector::ZeroVector); });
    TestTrue(TEXT("Native wall update cannot collapse exact wire geometry"), Wire->GetActorLocation().Equals(FVector(100, 200, 300)));
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPowerBuildingTargetTest, "SmartFoundations.Power.BuildingTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPowerBuildingTargetTest::RunTest(const FString& Parameters)
{
    using namespace SFPowerBuildingTarget;
    TestTrue(TEXT("Unconnected light without MAM unlock is eligible"), IsEligible(false, false, 1));
    TestTrue(TEXT("Unconnected light with MAM unlock is eligible"), IsEligible(false, false, 2));
    TestFalse(TEXT("Connected consumer cannot reserve a pole slot without a preview"), IsEligible(false, true, 1));
    TestFalse(TEXT("Hidden connector cannot consume a slot"), IsEligible(true, false, 2));
    TestFalse(TEXT("Full native connector cannot consume a slot"), IsEligible(false, false, 0));
    // Native Mk1 port is +700 Z; ceiling-light port is -600 Y, -60 Z from its pivot.
    const FVector PoleOrigin(0, 0, 0), LightOrigin(0, 2000, 2400);
    const FVector PolePort = PoleOrigin + FVector(0, 0, 700);
    const FVector LightPort = LightOrigin + FVector(0, -600, -60);
    TestTrue(TEXT("Offset ceiling socket is inside 25m, for preview AND construction"), IsWithinRange(PolePort, LightPort, 2500));
    TestFalse(TEXT("Actor-origin range check would incorrectly discard that committed wire"), IsWithinRange(PoleOrigin, LightOrigin, 2500));
    TestFalse(TEXT("Range zero disables building wires"), IsWithinRange(PolePort, LightPort, 0));
    TestFalse(TEXT("User range cannot exceed ordinary wire maximum"), IsWithinRange(FVector::ZeroVector, FVector(10001, 0, 0), 20000));
    return true;
}
#endif
