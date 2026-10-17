// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkVehiclePathAdapter.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "Features/Walk/SFWalkService.h"
#include "UObject/UnrealType.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkVehiclePathReversedFrameTest,
    "SmartFoundations.Walking.VehiclePathReversedFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkVehiclePathReversedFrameTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Path = World->SpawnActor<AFGVehiclePathSegmentHologram>();
    if (!TestNotNull(TEXT("Native path fixture"), Path)) { World->DestroyWorld(false); return false; }
    const FVector Free(1000, 0, 0), Selected(4200, 0, 0);
    auto* SplineProperty = FindFProperty<FObjectPropertyBase>(Path->GetClass(), TEXT("mPathSplineComponent"));
    auto* ReverseProperty = FindFProperty<FBoolProperty>(Path->GetClass(), TEXT("mIsSegmentReversed"));
    auto* StepProperty = FindFProperty<FProperty>(Path->GetClass(), TEXT("mBuildStep"));
    auto* StartProperty = FindFProperty<FStructProperty>(Path->GetClass(), TEXT("mStartPointExitDirection"));
    auto* EndProperty = FindFProperty<FStructProperty>(Path->GetClass(), TEXT("mEndPointEntryDirection"));
    if (!SplineProperty || !ReverseProperty || !StepProperty || !StartProperty || !EndProperty)
    { World->DestroyWorld(false); return false; }
    auto* Spline = Cast<USplineComponent>(SplineProperty->GetObjectPropertyValue_InContainer(Path));
    if (!TestNotNull(TEXT("Native spline"), Spline)) { World->DestroyWorld(false); return false; }
    Spline->SetSplinePoints({Free, Selected}, ESplineCoordinateSpace::World, false);
    Spline->SetSplinePointType(0, ESplinePointType::Linear, false);
    Spline->SetSplinePointType(1, ESplinePointType::Linear, true);
    *StepProperty->ContainerPtrToValuePtr<EVehiclePathBuildStep>(Path) = EVehiclePathBuildStep::EndPoint;
    ReverseProperty->SetPropertyValue_InContainer(Path, true);
    const FTransform Origin = FSFWalkVehiclePathAdapter::StartFrame(Path);
    TestTrue(TEXT("Reversed entry retains the selected node, not the free end"), Origin.GetLocation().Equals(Selected));
    TestTrue(TEXT("Walking frame faces backward along the route"), Origin.GetRotation().GetForwardVector().Equals(FVector::ForwardVector));
    FSFWalkSegment Segment;
    Segment.Advance = 3200.0f;
    const FTransform Destination = USFWalkService::AccumulateFrame({Segment}, Origin, 0);
    TestTrue(TEXT("Walking actually advances backward from the selected node"), Destination.GetLocation().Equals(Free, 0.01f));
    // SDK routing bodies are stubs. Verify the state supplied to the native
    // router here; Shipping acceptance proves its actual spline and topology.
    FSFWalkVehiclePathAdapter::Route(Path, Origin, Destination, Path);
    TestTrue(TEXT("Selected-node direction retains graph flow"), StartProperty->ContainerPtrToValuePtr<FVector>(Path)->Equals(FVector::ForwardVector));
    TestTrue(TEXT("Reversed free end matches native 180-degree selection"), EndProperty->ContainerPtrToValuePtr<FVector>(Path)->Equals(-FVector::ForwardVector));
    ReverseProperty->SetPropertyValue_InContainer(Path, false);
    // Route moved the actor (and its spline component). Reset this independent
    // forward fixture to its original world-space points before checking it.
    Spline->SetSplinePoints({Free, Selected}, ESplineCoordinateSpace::World, false);
    Spline->SetSplinePointType(0, ESplinePointType::Linear, false);
    Spline->SetSplinePointType(1, ESplinePointType::Linear, true);
    const FTransform ForwardOrigin = FSFWalkVehiclePathAdapter::StartFrame(Path);
    const FTransform ForwardEnd = USFWalkService::AccumulateFrame({Segment}, ForwardOrigin, 0);
    TestTrue(TEXT("Forward entry retains its graph start"), ForwardOrigin.GetLocation().Equals(Free));
    TestTrue(TEXT("Forward Walking still advances toward the graph end"), ForwardEnd.GetLocation().Equals(Selected, 0.01f));
    FSFWalkVehiclePathAdapter::Route(Path, ForwardOrigin, ForwardEnd, Path);
    TestTrue(TEXT("Forward free-end direction remains unchanged"), EndProperty->ContainerPtrToValuePtr<FVector>(Path)->Equals(FVector::ForwardVector));
    World->DestroyWorld(false);
    return true;
}
#endif
