// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Features/AutoConnect/SFBlueprintLiftAlignment.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGFactoryConnectionComponent.h"
#include "FGConstructDisqualifier.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftPlacementScopeTest, "SmartFoundations.AutoConnect.BlueprintLiftPlacementScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftPlacementScopeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Copy = World->SpawnActor<AFGBlueprintHologram>();
    auto* Child = World->SpawnActor<AFGBlueprintHologram>();
    auto* ChildrenProperty = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
    if (!ChildrenProperty)
    { AddError(TEXT("native placement state schema missing")); World->DestroyWorld(false); return false; }
    using FChildren = TArray<TObjectPtr<AFGHologram>>;
    auto* Children = ChildrenProperty->ContainerPtrToValuePtr<FChildren>(Copy);
    Children->Add(Child);
    FSFBlueprintLiftSeams::ValidateCopy(Copy);
    // SDK native validation bodies are stubs: no placement pass is asserted here.
    // This covers rejection-safe child ownership and scope cleanup only. Native
    // reasons are not reflected; their actual restoration requires runtime proof.
    TestEqual(TEXT("temporary native validation restores owned children"), Children->Num(), 1);
    TestTrue(TEXT("the exact child reference survives"), Children->Num() == 1 && (*Children)[0] == Child);
    TestFalse(TEXT("validation permission does not leak to the copy"), FSFBlueprintLiftSeams::IsNativeValidation(Copy));
    TestFalse(TEXT("validation permission does not leak to another hologram"), FSFBlueprintLiftSeams::IsNativeValidation(Child));
    TestFalse(TEXT("missing copy is rejected"), FSFBlueprintLiftSeams::ValidateCopy(nullptr));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftOriginalEndpointTest, "SmartFoundations.AutoConnect.BlueprintLiftOriginalEndpointIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftOriginalEndpointTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Blueprint = World->SpawnActor<AFGBlueprintHologram>();
    auto* A = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* B = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* MapProperty = FindFProperty<FMapProperty>(Blueprint->GetClass(), TEXT("mDuplicateConnectionToOriginalMap"));
    if (!MapProperty)
    { AddError(TEXT("native duplicate identity map missing")); World->DestroyWorld(false); return false; }
    using FMap = TMap<TObjectPtr<UFGConnectionComponent>, TObjectPtr<UFGConnectionComponent>>;
    auto* Map = MapProperty->ContainerPtrToValuePtr<FMap>(Blueprint);
    TArray<UFGFactoryConnectionComponent*> Originals{A->GetConnection1(), B->GetConnection1()};
    for (auto* Original : Originals)
    {
        auto* Duplicate = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Duplicate); Duplicate->RegisterComponent();
        Map->Add(Duplicate, Original);
    }
    TArray<UFGFactoryConnectionComponent*> Duplicates;
    FSFBlueprintSeamService::GetDuplicatedBeltConnectors(Blueprint, Duplicates);
    TestEqual(TEXT("same-class/same-port-name mouths remain distinct identities"), Duplicates.Num(), 2);
    for (int32 I = 0; I < Duplicates.Num(); ++I)
        TestTrue(TEXT("index resolves its exact mapped owner, without proximity/name aliasing"),
            FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, I) == Map->FindRef(Duplicates[I]));
    TestNull(TEXT("negative index cannot select another mouth"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, -1));
    TestNull(TEXT("out-of-range index cannot select another mouth"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, Duplicates.Num()));
    TestNull(TEXT("missing Blueprint is rejected"), FSFBlueprintSeamService::GetOriginalBeltConnector(nullptr, 0));
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftReachBoundsTest, "SmartFoundations.AutoConnect.BlueprintLiftNativeReachBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftReachBoundsTest::RunTest(const FString& Parameters)
{
    const double Length = 5600.1, Incline = 35.0;
    const double Rise = FSFBlueprintLiftAlignment::MaximumVerticalReach(Length, 1000.0, Incline);
    TestTrue(TEXT("bent routes are not restricted to horizontal chord times tangent"),
        Rise > 1000.0 * FMath::Tan(FMath::DegreesToRadians(Incline)));
    TestTrue(TEXT("full native arc and incline provide the necessary bound"),
        FMath::IsNearlyEqual(Rise, Length * FMath::Sin(FMath::DegreesToRadians(Incline)), .0001));
    const double NearLimit = FSFBlueprintLiftAlignment::MaximumVerticalReach(Length, Length * .9, Incline);
    TestTrue(TEXT("the 3D chord still cannot exceed the native maximum arc length"),
        FMath::IsNearlyEqual(FMath::Square(NearLimit) + FMath::Square(Length * .9), FMath::Square(Length), .01));
    TestEqual(TEXT("overlength horizontal span has no necessary vertical allowance"),
        FSFBlueprintLiftAlignment::MaximumVerticalReach(Length, Length + 1.0, Incline), 0.0);
    TestEqual(TEXT("invalid native limit is rejected"), FSFBlueprintLiftAlignment::MaximumVerticalReach(-Length, 1000.0, Incline), 0.0);
    TestEqual(TEXT("invalid native incline is rejected"), FSFBlueprintLiftAlignment::MaximumVerticalReach(Length, 1000.0, 90.0), 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftTailIndexOrderTest, "SmartFoundations.AutoConnect.BlueprintLiftTailIndexOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftTailIndexOrderTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* A = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* B = World->SpawnActor<AFGBuildableConveyorLift>();
    auto Root = [&](AActor* Actor, const FVector& Location)
    {
        if (!Actor->GetRootComponent())
        {
            auto* Component = NewObject<USceneComponent>(Actor);
            Actor->AddInstanceComponent(Component); Actor->SetRootComponent(Component); Component->RegisterComponent();
        }
        Actor->SetActorLocation(Location);
    };
    Root(A, FVector(1000, 0, 0)); Root(B, FVector(2000, 0, 0));
    TestEqual(TEXT("fixture uses same-name mouths on different exact owners"), A->GetConnection1()->GetFName(), B->GetConnection1()->GetFName());
    auto Add = [&](AFGBlueprintHologram* Blueprint, UFGFactoryConnectionComponent* Original, bool LiftTail)
    {
        auto* Property = FindFProperty<FMapProperty>(Blueprint->GetClass(), TEXT("mDuplicateConnectionToOriginalMap"));
        if (!Property) return false;
        auto* Duplicate = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Duplicate); Duplicate->RegisterComponent();
        if (LiftTail) Duplicate->ComponentTags.Add(TEXT("SF_BlueprintLiftTail"));
        using FMap = TMap<TObjectPtr<UFGConnectionComponent>, TObjectPtr<UFGConnectionComponent>>;
        Property->ContainerPtrToValuePtr<FMap>(Blueprint)->Add(Duplicate, Original);
        return true;
    };
    auto* Forward = World->SpawnActor<AFGBlueprintHologram>();
    auto* Reverse = World->SpawnActor<AFGBlueprintHologram>();
    for (auto* Blueprint : {Forward, Reverse}) Add(Blueprint, A->GetConnection0(), false);
    Add(Forward, A->GetConnection1(), true); Add(Forward, B->GetConnection1(), true);
    Add(Reverse, B->GetConnection1(), true); Add(Reverse, A->GetConnection1(), true);
    for (auto* Blueprint : {Forward, Reverse})
    {
        TestTrue(TEXT("ordinary native index remains unchanged"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 0) == A->GetConnection0());
        TestTrue(TEXT("lift-tail identity is independent of component insertion order"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 1) == A->GetConnection1());
        TestTrue(TEXT("same-named second owner keeps its own endpoint"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 2) == B->GetConnection1());
    }
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftSelectedMouthOrderTest, "SmartFoundations.AutoConnect.BlueprintLiftSelectedMouthOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftSelectedMouthOrderTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* A = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* B = World->SpawnActor<AFGBuildableConveyorLift>();
    auto Root = [&](AActor* Actor, const FVector& Location)
    {
        if (!Actor->GetRootComponent())
        {
            auto* Component = NewObject<USceneComponent>(Actor);
            Actor->AddInstanceComponent(Component); Actor->SetRootComponent(Component); Component->RegisterComponent();
        }
        Actor->SetActorLocation(Location);
    };
    Root(A, FVector(1000, 0, 0)); Root(B, FVector(1000, 0, 0));
    A->GetConnection1()->SetRelativeLocation(FVector(0, 0, 100));
    B->GetConnection1()->SetRelativeLocation(FVector(0, 0, 200));
    TestEqual(TEXT("fixture uses same-name mouths on different exact owners"), A->GetConnection1()->GetFName(), B->GetConnection1()->GetFName());
    auto Add = [&](AFGBlueprintHologram* Blueprint, UFGFactoryConnectionComponent* Original, bool LiftTail)
    {
        auto* Property = FindFProperty<FMapProperty>(Blueprint->GetClass(), TEXT("mDuplicateConnectionToOriginalMap"));
        if (!Property) return false;
        auto* Duplicate = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Duplicate); Duplicate->RegisterComponent();
        if (LiftTail) Duplicate->ComponentTags.Add(TEXT("SF_BlueprintLiftTail"));
        using FMap = TMap<TObjectPtr<UFGConnectionComponent>, TObjectPtr<UFGConnectionComponent>>;
        Property->ContainerPtrToValuePtr<FMap>(Blueprint)->Add(Duplicate, Original);
        return true;
    };
    auto* Forward = World->SpawnActor<AFGBlueprintHologram>();
    auto* Reverse = World->SpawnActor<AFGBlueprintHologram>();
    for (auto* Blueprint : {Forward, Reverse}) Add(Blueprint, A->GetConnection0(), false);
    Add(Forward, A->GetConnection1(), true); Add(Forward, B->GetConnection1(), true);
    Add(Reverse, B->GetConnection1(), true); Add(Reverse, A->GetConnection1(), true);
    for (auto* Blueprint : {Forward, Reverse})
    {
        TestTrue(TEXT("ordinary native index remains unchanged"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 0) == A->GetConnection0());
        TestTrue(TEXT("lift-tail identity is independent of component insertion order"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 1) == A->GetConnection1());
        TestTrue(TEXT("same-named second owner keeps its own endpoint"), FSFBlueprintSeamService::GetOriginalBeltConnector(Blueprint, 2) == B->GetConnection1());
    }
    World->DestroyWorld(false);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftNativePopulationTest, "SmartFoundations.AutoConnect.BlueprintLiftNativePopulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftNativePopulationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Blueprint = World->SpawnActor<AFGBlueprintHologram>();
    auto* Lift = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* Property = FindFProperty<FMapProperty>(Blueprint->GetClass(), TEXT("mDuplicateConnectionToOriginalMap"));
    if (!Property) { AddError(TEXT("native Blueprint map unavailable")); World->DestroyWorld(false); return false; }
    using FMap = TMap<TObjectPtr<UFGConnectionComponent>, TObjectPtr<UFGConnectionComponent>>;
    auto* Map = Property->ContainerPtrToValuePtr<FMap>(Blueprint);
    auto Add = [&](UFGFactoryConnectionComponent* Original, bool Temporary)
    {
        auto* Duplicate = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Duplicate); Duplicate->RegisterComponent();
        if (Temporary) Duplicate->ComponentTags.Add(TEXT("SF_BlueprintLiftTail"));
        Map->Add(Duplicate, Original);
        return Duplicate;
    };
    auto* Native = Add(Lift->GetConnection0(), false);
    Add(Lift->GetConnection0(), true); Add(Lift->GetConnection1(), true);
    const auto Before = Lift->GetConnection1()->GetComponentTransform();
    TestEqual(TEXT("Only temporary lift mouths retire"), FSFBlueprintSeamService::ReleaseLiftTailConnectors(Blueprint), 2);
    TArray<UFGFactoryConnectionComponent*> Remaining;
    FSFBlueprintSeamService::GetDuplicatedBeltConnectors(Blueprint, Remaining);
    TestTrue(TEXT("The ordinary native connector and map survive"), Remaining.Num() == 1 && Remaining[0] == Native && Map->Num() == 1);
    TestTrue(TEXT("Original content port stays untouched"), Before.Equals(Lift->GetConnection1()->GetComponentTransform()));
    TestEqual(TEXT("Repeated cleanup changes nothing"), FSFBlueprintSeamService::ReleaseLiftTailConnectors(Blueprint), 0);
    TestEqual(TEXT("Missing Blueprint cleanup is safe"), FSFBlueprintSeamService::ReleaseLiftTailConnectors(nullptr), 0);
    World->DestroyWorld(false);
    return true;
}

#endif
