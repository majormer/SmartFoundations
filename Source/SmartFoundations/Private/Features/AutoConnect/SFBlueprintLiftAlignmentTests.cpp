// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintLiftAlignment.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Hologram/FGBlueprintHologram.h"
#include "FGFactoryConnectionComponent.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using FAlignment = FSFBlueprintLiftAlignment;
FAlignment::FTail Tail(int32 Index, const FVector& Position, const FVector& Normal, bool Output)
{
    return {Index, Position, Normal, true, true, !Output, Output};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftTailPairsTest, "SmartFoundations.AutoConnect.BlueprintLiftTailPairs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftTailPairsTest::RunTest(const FString& Parameters)
{
    TArray<FAlignment::FTail> Tails = {
        Tail(0, FVector(0,0,1200), FVector::ForwardVector, true),
        Tail(1, FVector(800,0,0), -FVector::ForwardVector, false),
        Tail(2, FVector(0,200,0), FVector::ForwardVector, true),
        Tail(3, FVector(800,200,1200), -FVector::ForwardVector, false)};
    auto Pairs = FAlignment::PairTails(Tails, 300, 5600.1);
    TestFalse(TEXT("Exact lanes are unambiguous despite nearby parallel lanes"), Pairs.bAmbiguous);
    if (!TestEqual(TEXT("Both bus lanes pair"), Pairs.Pairs.Num(), 2)) return false;
    TestTrue(TEXT("High output joins input in upper copy"), Pairs.Pairs[0].bOutputInLowerCopy);
    TestFalse(TEXT("Low output supports downward flow from upper copy"), Pairs.Pairs[1].bOutputInLowerCopy);
    TestEqual(TEXT("Native mouth elevation defines level pitch"), Pairs.Pairs[0].LevelPitch, 1200.0);
    TArray<FAlignment::FTail> Reordered = {Tails[3],Tails[1],Tails[2],Tails[0]};
    auto Stable = FAlignment::PairTails(Reordered, 300, 5600.1);
    TestEqual(TEXT("Permutation cannot change intended output"), Stable.Pairs[0].Output, Pairs.Pairs[0].Output);
    TestEqual(TEXT("Permutation cannot change intended input"), Stable.Pairs[1].Input, Pairs.Pairs[1].Input);
    auto Extra = Tails[1]; Extra.Index = 4; Tails.Add(Extra);
    auto Ambiguous = FAlignment::PairTails(Tails, 300, 5600.1);
    TestTrue(TEXT("Coincident equally viable mouths reject ambiguity"), Ambiguous.bAmbiguous);
    TestTrue(TEXT("Ambiguity cannot silently connect only part of the bus"), Ambiguous.Pairs.IsEmpty());
    Extra.bLift = false; Tails.Last() = Extra;
    TestEqual(TEXT("Bare machine/conveyor ports cannot become lift intent"), FAlignment::PairTails(Tails,300,5600.1).Pairs.Num(),2);
    Tails[1].bOpen = false; Tails[3].Normal = FVector::ForwardVector;
    TestTrue(TEXT("Closed and same-facing mouths do not pair"), FAlignment::PairTails(Tails,300,5600.1).Pairs.IsEmpty());
    Tails[1].bOpen = true; Tails[1].Index = Tails[0].Index;
    TestTrue(TEXT("Duplicate source identity fails closed"), FAlignment::PairTails(Tails,300,5600.1).bInvalid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftSharedPitchTest, "SmartFoundations.AutoConnect.BlueprintLiftSharedPitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftSharedPitchTest::RunTest(const FString& Parameters)
{
    int32 Calls = 0;
    auto NoTails = FAlignment::SolveSharedPitch(2400, {}, [&](double) { ++Calls; return true; });
    TestTrue(TEXT("No tails preserves default status"), NoTails.Status == FAlignment::EStatus::NoTails);
    TestEqual(TEXT("No tails preserves exact original pitch"), NoTails.Pitch, 2400.0);
    TestEqual(TEXT("No-tail control does not start native probing"), Calls, 0);
    TArray<FAlignment::FPitchRange> Ranges = {{1000,1400,1200},{1100,1500,1300}};
    auto Sloped = FAlignment::SolveSharedPitch(2400, Ranges, [](double P) { return P >= 1100 && P <= 1400; });
    TestTrue(TEXT("Common native-validated pitch selected"), Sloped.Status == FAlignment::EStatus::Aligned);
    TestEqual(TEXT("Different lane elevations may share sloped connections"), Sloped.Pitch,1250.0);
    auto Alternative = FAlignment::SolveSharedPitch(2400,Ranges,[](double P) { return P == 1300; });
    TestTrue(TEXT("Native rejection of preferred layout tries another common candidate"), Alternative.Status == FAlignment::EStatus::Aligned && Alternative.Pitch == 1300);
    Ranges = {{1000,1100,1050},{1200,1300,1250}};
    Calls = 0;
    auto Conflict = FAlignment::SolveSharedPitch(2400,Ranges,[&](double) { ++Calls; return true; });
    TestTrue(TEXT("Disjoint native ranges reject conflicting pitch"), Conflict.Status == FAlignment::EStatus::Conflict);
    TestEqual(TEXT("Conflict retains default layout"), Conflict.Pitch,2400.0);
    TestEqual(TEXT("Conflict cannot invent a native-valid verdict"), Calls,0);
    Ranges = {{1000,1400,1200}};
    auto Invalid = FAlignment::SolveSharedPitch(2400,Ranges,[](double) { return false; });
    TestTrue(TEXT("No native-approved layout is not a success"), Invalid.Status == FAlignment::EStatus::NoValidatedPitch);
    TestEqual(TEXT("Native rejection retains default"), Invalid.Pitch,2400.0);
    Ranges[0].Maximum = std::numeric_limits<double>::infinity();
    TestTrue(TEXT("Nonfinite limits cannot reach native probing"), FAlignment::SolveSharedPitch(2400,Ranges,[](double) { return true; }).Status == FAlignment::EStatus::Invalid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftTailSourcesTest, "SmartFoundations.AutoConnect.BlueprintLiftTailSources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftTailSourcesTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Blueprint = World->SpawnActor<AFGBlueprintHologram>();
    if (!Blueprint->GetRootComponent())
    {
        auto* Root = NewObject<USceneComponent>(Blueprint); Blueprint->AddInstanceComponent(Root);
        Blueprint->SetRootComponent(Root); Root->RegisterComponent();
    }
    auto* MapProperty = FindFProperty<FMapProperty>(Blueprint->GetClass(),TEXT("mDuplicateConnectionToOriginalMap"));
    if (!TestNotNull(TEXT("Native duplicate map"),MapProperty)) { World->DestroyWorld(false); return false; }
    using FMap = TMap<TObjectPtr<UFGConnectionComponent>,TObjectPtr<UFGConnectionComponent>>;
    auto Add = [&](bool Lift, bool Output, const FVector& Position, bool Mapped, bool Closed, bool SnapOnly)
    {
        AActor* Owner = Lift ? static_cast<AActor*>(World->SpawnActor<AFGBuildableConveyorLift>()) : World->SpawnActor<AActor>();
        auto* Original = NewObject<UFGFactoryConnectionComponent>(Owner);
        Owner->AddInstanceComponent(Original); Original->RegisterComponent();
        Original->SetDirection(SnapOnly ? EFactoryConnectionDirection::FCD_SNAP_ONLY : Output ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT);
        if (Closed)
        {
            auto* Connected = FindFProperty<FBoolProperty>(Original->GetClass(),TEXT("mHasConnectedComponent"));
            if (Connected) Connected->SetPropertyValue_InContainer(Original,true);
        }
        auto* Dup = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Dup); Dup->SetupAttachment(Blueprint->GetRootComponent());
        Dup->SetRelativeLocationAndRotation(Position, Output ? FRotator::ZeroRotator : FRotator(0,180,0)); Dup->RegisterComponent();
        if (Mapped) MapProperty->ContainerPtrToValuePtr<FMap>(Blueprint)->Add(Dup,Original);
    };
    Add(true,true,FVector(0,0,1200),true,false,false);
    Add(true,false,FVector(800,0,0),true,false,false);
    Add(false,true,FVector(0,0,1200),true,false,false);
    Add(true,true,FVector(0,100,1200),false,false,false);
    Add(true,true,FVector(0,200,1200),true,true,false);
    Add(true,false,FVector(800,300,0),true,false,true);
    TArray<FAlignment::FTail> Tails;
    FSFBlueprintSeamService::GetOpenLiftTails(Blueprint,Tails);
    TestEqual(TEXT("Only mapped, open, transport-capable native lift ends express intent"),Tails.Num(),2);
    auto Pairing = FAlignment::PairTails(Tails,300,5600.1);
    TestEqual(TEXT("Native lift identities pair"),Pairing.Pairs.Num(),1);
    Blueprint->SetActorTransform(FTransform(FRotator(0,137,0),FVector(10000,-9000,7500)));
    TArray<FAlignment::FTail> Rotated;
    FSFBlueprintSeamService::GetOpenLiftTails(Blueprint,Rotated);
    TestEqual(TEXT("Rotating/moving whole Blueprint retains tail count"),Rotated.Num(),Tails.Num());
    for (int32 I=0; I<FMath::Min(Tails.Num(),Rotated.Num()); ++I)
    {
        TestEqual(TEXT("Belt index identity is retained"),Rotated[I].Index,Tails[I].Index);
        TestTrue(TEXT("Pair geometry remains parent-local"),Rotated[I].Position.Equals(Tails[I].Position,.01));
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintLiftAnyFlowTest, "SmartFoundations.AutoConnect.BlueprintLiftAnyFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintLiftAnyFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Blueprint = World->SpawnActor<AFGBlueprintHologram>();
    if (!Blueprint->GetRootComponent())
    {
        auto* Root = NewObject<USceneComponent>(Blueprint); Blueprint->AddInstanceComponent(Root);
        Blueprint->SetRootComponent(Root); Root->RegisterComponent();
    }
    auto* MapProperty = FindFProperty<FMapProperty>(Blueprint->GetClass(),TEXT("mDuplicateConnectionToOriginalMap"));
    if (!TestNotNull(TEXT("Native duplicate map"),MapProperty)) { World->DestroyWorld(false); return false; }
    using FMap = TMap<TObjectPtr<UFGConnectionComponent>,TObjectPtr<UFGConnectionComponent>>;
    auto Add = [&](UFGFactoryConnectionComponent* Original, const FVector& Position, const FRotator& Rotation)
    {
        Original->SetDirection(EFactoryConnectionDirection::FCD_ANY);
        auto* Dup = NewObject<UFGFactoryConnectionComponent>(Blueprint);
        Blueprint->AddInstanceComponent(Dup); Dup->SetupAttachment(Blueprint->GetRootComponent());
        Dup->SetRelativeLocationAndRotation(Position,Rotation); Dup->RegisterComponent();
        MapProperty->ContainerPtrToValuePtr<FMap>(Blueprint)->Add(Dup,Original);
    };
    auto* OutputLift = World->SpawnActor<AFGBuildableConveyorLift>();
    auto* InputLift = World->SpawnActor<AFGBuildableConveyorLift>();
    // Deliberately place the output physically below the input. Logical entry/exit
    // identity must not be inferred from mouth height, name sorting or FCD_ANY alone.
    Add(OutputLift->GetConnection1(),FVector(0,0,0),FRotator::ZeroRotator);
    Add(InputLift->GetConnection0(),FVector(800,0,1200),FRotator(0,180,0));
    auto* Unknown = NewObject<UFGFactoryConnectionComponent>(OutputLift);
    OutputLift->AddInstanceComponent(Unknown); Unknown->RegisterComponent();
    Add(Unknown,FVector(0,0,1200),FRotator::ZeroRotator);
    TArray<FAlignment::FTail> Tails;
    FSFBlueprintSeamService::GetOpenLiftTails(Blueprint,Tails);
    TestEqual(TEXT("Unknown ANY connector is not a bidirectional candidate"),Tails.Num(),2);
    auto Pairs = FAlignment::PairTails(Tails,300,5600.1);
    if (TestEqual(TEXT("Logical native entry/exit pairs"),Pairs.Pairs.Num(),1))
    {
        TestFalse(TEXT("Reversed physical height retains downward flow"),Pairs.Pairs[0].bOutputInLowerCopy);
        TestEqual(TEXT("Selected mouth heights retain the required common pitch"),Pairs.Pairs[0].LevelPitch,1200.0);
    }
    World->DestroyWorld(false);
    return true;
}
#endif
