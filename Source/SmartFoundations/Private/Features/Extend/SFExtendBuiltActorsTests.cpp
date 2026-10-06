// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "Features/Extend/SFExtendBuiltActors.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendConstructedOwnersTest, "SmartFoundations.Extend.ConstructedOwners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExtendConstructedOwnersTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    const FTransform Expected(FRotator(0, 90, 0), FVector(1000, 2000, 3000));
    auto Spawn = [&](UClass* Class, const FTransform& Transform)
    {
        AActor* Actor = World->SpawnActor<AActor>(Class);
        USceneComponent* Root = NewObject<USceneComponent>(Actor);
        Actor->AddInstanceComponent(Root);
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorTransform(Transform);
        return Actor;
    };
    AActor* Existing = Spawn(AActor::StaticClass(), Expected);
    AActor* Built = Spawn(AActor::StaticClass(), Expected);
    AActor* WrongClass = Spawn(APawn::StaticClass(), Expected);
    TArray<AActor*> Constructed = {WrongClass, Built};
    TestTrue(TEXT("Exact class in construction result wins over coincident other class"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed) == Built);
    Constructed = {WrongClass};
    TestNull(TEXT("Coincident existing world actor is never considered"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed));
    Constructed = {Built};
    Built->AddActorWorldOffset(FVector(0, 0, 400));
    TestNull(TEXT("Different floor within old radius is not a match"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed));
    Built->SetActorTransform(Expected);
    Built->SetActorRotation(FRotator(0, 180, 0));
    TestNull(TEXT("Coincident pivot with wrong rotation is not a match"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed));
    Built->SetActorTransform(Expected);
    Built->SetActorScale3D(FVector(2));
    TestNull(TEXT("Wrong scale is not a match"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed));
    Built->SetActorTransform(Expected);
    Constructed = {Built, Existing};
    TestNull(TEXT("Two constructed candidates at identical transform are ambiguous"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed));
    Constructed = {Built, Built};
    TestTrue(TEXT("Repeated array reference is not another actor"),
        SFExtendBuiltActors::Match(AActor::StaticClass(), Expected, Constructed) == Built);
    TestNull(TEXT("Unknown expected class cannot select a candidate"),
        SFExtendBuiltActors::Match(nullptr, Expected, Constructed));
    World->DestroyWorld(false);
    return true;
}
#endif
