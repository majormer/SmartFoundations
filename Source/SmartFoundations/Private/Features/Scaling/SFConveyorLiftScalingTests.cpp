// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFConveyorLiftHologramState.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGFactoryConnectionComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftFloorPlacementTest, "SmartFoundations.Scaling.LiftFloorPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftFloorPlacementTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    AFGConveyorLiftHologram* Root = World->SpawnActor<AFGConveyorLiftHologram>();
    AFGBuildableBlueprintDesigner* Designer = World->SpawnActor<AFGBuildableBlueprintDesigner>();
    AFGBuildableBlueprintDesigner* Other = World->SpawnActor<AFGBuildableBlueprintDesigner>();
    // The native base is abstract. This gate only classifies the hit owner; its
    // CDO suffices and avoids inventing a concrete construction fixture.
    AFGBuildableFoundation* Foundation = GetMutableDefault<AFGBuildableFoundation>();
    if (!Root || !Designer || !Other || !Foundation) { World->DestroyWorld(false); return false; }
    // Editor SDK setter/getter bodies are stubs. Supply the reflected selected
    // owner to the pure gate; live placement tests cover the real native methods.
    FObjectPropertyBase* Owner = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mBlueprintDesigner"));
    if (!TestNotNull(TEXT("Native Designer owner contract"), Owner)) { World->DestroyWorld(false); return false; }
    Owner->SetObjectPropertyValue_InContainer(Root, Designer);
    const FHitResult Floor(Designer, nullptr, FVector::ZeroVector, FVector::UpVector);
    auto* Selected = Cast<AFGBuildableBlueprintDesigner>(Owner->GetObjectPropertyValue_InContainer(Root));
    TestTrue(TEXT("Native owned Designer floor may replay legal nudge"), SFConveyorLiftHologramState::CanReplayFloorOwner(Floor.GetActor(), Selected, Floor.ImpactNormal));
    TestFalse(TEXT("Another Designer is not the selected floor"), SFConveyorLiftHologramState::CanReplayFloorOwner(Other, Selected, FVector::UpVector));
    TestFalse(TEXT("Designer wall is not a floor"), SFConveyorLiftHologramState::CanReplayFloorOwner(Designer, Selected, FVector::ForwardVector));
    Owner->SetObjectPropertyValue_InContainer(Root, nullptr);
    Selected = Cast<AFGBuildableBlueprintDesigner>(Owner->GetObjectPropertyValue_InContainer(Root));
    TestFalse(TEXT("Unowned Designer cannot authorize floor displacement"), SFConveyorLiftHologramState::CanReplayFloorOwner(Designer, Selected, FVector::UpVector));
    TestTrue(TEXT("Ordinary foundation remains a legal replay surface"), SFConveyorLiftHologramState::CanReplayFloorPlacement(Root, FHitResult(Foundation, nullptr, FVector::ZeroVector, FVector::UpVector)));
    TestFalse(TEXT("A missing root cannot authorize replay"), SFConveyorLiftHologramState::CanReplayFloorPlacement(nullptr, Floor));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftSelectedStateTest, "SmartFoundations.Scaling.LiftSelectedState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftSelectedStateTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Preview world"), World)) return false;
    AFGConveyorLiftHologram* Parent = World->SpawnActor<AFGConveyorLiftHologram>();
    AFGConveyorLiftHologram* Child = World->SpawnActor<AFGConveyorLiftHologram>();
    FObjectPropertyBase* BuildClass = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mBuildClass"));
    FProperty* Top = FindFProperty<FProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
    FIntProperty* Step = FindFProperty<FIntProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mActivePointIdx"));
    FBoolProperty* Reverse = FindFProperty<FBoolProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mIsReversed"));
    if (!TestNotNull(TEXT("Native parent"), Parent) || !TestNotNull(TEXT("Native child"), Child)
        || !TestNotNull(TEXT("Top transform contract"), Top) || !TestNotNull(TEXT("Placement phase contract"), Step)
        || !TestNotNull(TEXT("Flow contract"), Reverse) || !TestNotNull(TEXT("Build class contract"), BuildClass)) { World->DestroyWorld(false); return false; }
    BuildClass->SetObjectPropertyValue_InContainer(Parent, AFGBuildableConveyorLift::StaticClass());
    BuildClass->SetObjectPropertyValue_InContainer(Child, AFGBuildableConveyorLift::StaticClass());
    USceneComponent* Root = NewObject<USceneComponent>(Child);
    Child->SetRootComponent(Root); Root->RegisterComponent();
    const FTransform Pose(FRotator(0, 90, 0), FVector(4000, 7000, 900));
    Child->SetActorTransform(Pose);
    FTransform Selected(FRotator(0, 180, 0), FVector(0, 0, 1400));
    Top->CopyCompleteValue(Top->ContainerPtrToValuePtr<void>(Parent), &Selected);
    Step->SetPropertyValue_InContainer(Parent, 1);
    Reverse->SetPropertyValue_InContainer(Parent, true);
    FObjectPropertyBase* Snapped = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    if (!TestNotNull(TEXT("Native attachment array contract"), Snapped)) { World->DestroyWorld(false); return false; }
    UFGFactoryConnectionComponent* ParentBinding = NewObject<UFGFactoryConnectionComponent>(Parent);
    Snapped->SetObjectPropertyValue(Snapped->ContainerPtrToValuePtr<void>(Parent, 0), ParentBinding);
    SFConveyorLiftHologramState::Sync(Parent, Child);
    TestTrue(TEXT("Child takes endpoint height and rotation"), Top->Identical_InContainer(Parent, Child));
    TestEqual(TEXT("Native phase retained"), Step->GetPropertyValue_InContainer(Child), 1);
    TestTrue(TEXT("Reverse flow retained"), Reverse->GetPropertyValue_InContainer(Child));
    TestTrue(TEXT("Grid transform survives refresh"), Child->GetActorTransform().Equals(Pose));
    TestNull(TEXT("Copying selected geometry does not reuse the parent's attachment"),
        Snapped->GetObjectPropertyValue(Snapped->ContainerPtrToValuePtr<void>(Child, 0)));
    TestTrue(TEXT("Parent attachment remains intact"),
        Snapped->GetObjectPropertyValue(Snapped->ContainerPtrToValuePtr<void>(Parent, 0)) == ParentBinding);
    AFGConveyorLiftHologram* Late = World->SpawnActor<AFGConveyorLiftHologram>();
    SFConveyorLiftHologramState::Sync(Parent, Late);
    TestTrue(TEXT("Late child receives current selected state"), Top->Identical_InContainer(Parent, Late));
    Selected = FTransform(FRotator(0, 90, 0), FVector(0, 0, -600));
    Top->CopyCompleteValue(Top->ContainerPtrToValuePtr<void>(Parent), &Selected);
    Reverse->SetPropertyValue_InContainer(Parent, false);
    SFConveyorLiftHologramState::Sync(Parent, Child);
    TestTrue(TEXT("Existing child follows downward height and endpoint change"), Top->Identical_InContainer(Parent, Child));
    TestFalse(TEXT("Flow changes update existing child"), Reverse->GetPropertyValue_InContainer(Child));
    TestEqual(TEXT("Child sync never advances parent phase"), Step->GetPropertyValue_InContainer(Parent), 1);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftTierProfilesTest, "SmartFoundations.Scaling.LiftTierProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftTierProfilesTest::RunTest(const FString& Parameters)
{
    for (int32 Tier = 1; Tier <= 6; ++Tier)
        TestTrue(*FString::Printf(TEXT("Mk.%d scaling enabled"), Tier),
            USFBuildableSizeRegistry::GetProfileByName(FString::Printf(TEXT("Build_ConveyorLiftMk%d_C"), Tier)).bSupportsScaling);
    TestFalse(TEXT("Belt routing remains outside free-grid scaling"),
        USFBuildableSizeRegistry::GetProfileByName(TEXT("Build_ConveyorBeltMk1_C")).bSupportsScaling);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLiftPassthroughSlotsTest, "SmartFoundations.Scaling.LiftPassthroughSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLiftPassthroughSlotsTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AFGConveyorLiftHologram* Parent = World->SpawnActor<AFGConveyorLiftHologram>();
    AFGConveyorLiftHologram* Cell = World->SpawnActor<AFGConveyorLiftHologram>();
    AFGBuildablePassthrough* SourceHole = World->SpawnActor<AFGBuildablePassthrough>();
    AFGBuildablePassthrough* TargetHole = World->SpawnActor<AFGBuildablePassthrough>();
    for (AActor* Actor : {static_cast<AActor*>(Parent), static_cast<AActor*>(Cell), static_cast<AActor*>(SourceHole), static_cast<AActor*>(TargetHole)})
    {
        if (!Actor) { World->DestroyWorld(false); return false; }
        if (!Actor->GetRootComponent())
        {
            USceneComponent* Root = NewObject<USceneComponent>(Actor);
            Actor->SetRootComponent(Root); Root->RegisterComponent();
        }
    }
    Parent->SetActorLocation(FVector(1000, 1200, 200));
    const FTransform Pose(FRotator::ZeroRotator, FVector(1600, 1200, 200));
    Cell->SetActorTransform(Pose);
    SourceHole->SetActorLocation(Parent->GetActorLocation());
    TargetHole->SetActorLocation(Pose.GetLocation());
    const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
    const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!TestNotNull(TEXT("Native passthrough actor slots"), Inner)) { World->DestroyWorld(false); return false; }
    FScriptArrayHelper Source(Property, Property->ContainerPtrToValuePtr<void>(Parent));
    FScriptArrayHelper Target(Property, Property->ContainerPtrToValuePtr<void>(Cell));
    Source.Resize(2); Target.Resize(2);
    Inner->SetObjectPropertyValue(Source.GetRawPtr(0), SourceHole);
    TestFalse(TEXT("Equal array lengths cannot certify a missing selected floor hole"),
        SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Parent, Cell, Pose));
    Inner->SetObjectPropertyValue(Target.GetRawPtr(1), TargetHole);
    TestFalse(TEXT("A hole in the other endpoint slot does not preserve the selected endpoint"),
        SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Parent, Cell, Pose));
    Inner->SetObjectPropertyValue(Target.GetRawPtr(1), nullptr);
    Inner->SetObjectPropertyValue(Target.GetRawPtr(0), SourceHole);
    TestFalse(TEXT("A copy cannot retain its parent's world hole instead of its own"),
        SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Parent, Cell, Pose));
    Inner->SetObjectPropertyValue(Target.GetRawPtr(0), TargetHole);
    TestTrue(TEXT("Independently resolved matching endpoint and grid location are preserved"),
        SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Parent, Cell, Pose));
    World->DestroyWorld(false);
    return true;
}
#endif
