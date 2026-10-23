// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkRailAdapter.h"
#include "Features/Walk/SFWalkService.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkRailStartFrameTest,
    "SmartFoundations.Walking.RailStartFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkRailStartFrameTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Track = World->SpawnActor<AFGRailroadTrackHologram>();
    if (!TestNotNull(TEXT("Native rail fixture"), Track)) { World->DestroyWorld(false); return false; }
    auto* Data = FindFProperty<FArrayProperty>(Track->GetClass(), TEXT("mSplineData"));
    if (!Data) { World->DestroyWorld(false); return false; }
    auto& Points = *Data->ContainerPtrToValuePtr<TArray<FSplinePointData>>(Track);
    Points.SetNum(2);
    Points[0].Location = FVector(120, 240, 0);
    Points[0].LeaveTangent = FVector(0, 400, 0);
    FSFWalkSegment Segment;
    Segment.Advance = 5600.f;
    for (const float Yaw : {0.f, 45.f, 90.f, 180.f, -90.f})
    {
        Track->SetActorLocationAndRotation(FVector(1000, 2000, 3000), FRotator(0, Yaw, 0));
        const FTransform Native = Track->GetActorTransform();
        const FVector Anchor = Native.TransformPosition(Points[0].Location);
        const FVector Heading = Native.TransformVector(Points[0].LeaveTangent).GetSafeNormal();
        const FTransform Origin = FSFWalkRailAdapter::StartFrame(Track);
        TestTrue(TEXT("Rail anchor uses the first native spline point"), Origin.GetLocation().Equals(Anchor));
        TestTrue(TEXT("Rail advances along the world-space spline tangent"),
            USFWalkService::AccumulateFrame({Segment}, Origin, 0).GetLocation().Equals(Anchor + Heading * 5600.f, 0.1f));
    }
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkRailCurveLengthTest,
    "SmartFoundations.Walking.RailCurveLength",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkRailCurveLengthTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    auto* Track = World->SpawnActor<AFGRailroadTrackHologram>();
    if (!TestNotNull(TEXT("Rail fixture"), Track)) { World->DestroyWorld(false); return false; }
    auto* Spline = NewObject<USplineComponent>(Track);
    auto* SplineProperty = FindFProperty<FObjectPropertyBase>(Track->GetClass(), TEXT("mSplineComponent"));
    auto* DataProperty = FindFProperty<FArrayProperty>(Track->GetClass(), TEXT("mSplineData"));
    auto* MinProperty = FindFProperty<FFloatProperty>(Track->GetClass(), TEXT("mMinLength"));
    auto* MaxProperty = FindFProperty<FFloatProperty>(Track->GetClass(), TEXT("mMaxLength"));
    if (!SplineProperty || !DataProperty || !MinProperty || !MaxProperty)
    {
        AddError(TEXT("Native rail contract unavailable"));
        World->DestroyWorld(false);
        return false;
    }
    SplineProperty->SetObjectPropertyValue_InContainer(Track, Spline);
    FScriptArrayHelper(DataProperty, DataProperty->ContainerPtrToValuePtr<void>(Track)).Resize(2);
    MinProperty->SetPropertyValue_InContainer(Track, 1200.0f);
    MaxProperty->SetPropertyValue_InContainer(Track, 10000.0f);

    // Geometry measured from an ordinary native 90-degree replacement.
    // The Editor's routing bodies are stubs; use the retained native geometry
    // to exercise the real component length and the production acceptance gate.
    auto SetQuarterCurve = [Spline](float Scale)
    {
        const float Radius = 3565.07f * Scale;
        const float Handle = 5906.80127f * Scale;
        Spline->ClearSplinePoints(false);
        Spline->AddPoint(FSplinePoint(0, FVector::ZeroVector,
            FVector(Handle, 0, 0), FVector(Handle, 0, 0), FRotator::ZeroRotator,
            FVector::OneVector, ESplinePointType::CurveCustomTangent), false);
        Spline->AddPoint(FSplinePoint(1, FVector(Radius, Radius, 0),
            FVector(0, Handle, 0), FVector(0, Handle, 0), FRotator::ZeroRotator,
            FVector::OneVector, ESplinePointType::CurveCustomTangent), true);
    };
    SetQuarterCurve(1.0f);
    TestTrue(TEXT("Native replacement length retained"), FMath::IsNearlyEqual(Spline->GetSplineLength(), 5600.784f, 0.1f));
    TestTrue(TEXT("Measured native curve is within limits"), FSFWalkRailAdapter::Validate(Track).IsEmpty());
    SetQuarterCurve(9900.0f / 5600.784f);
    TestTrue(TEXT("99m routed curve is accepted"), FSFWalkRailAdapter::Validate(Track).IsEmpty());
    SetQuarterCurve(10010.0f / 5600.784f);
    TestTrue(TEXT("Overlong curve has a chord below 100m"), Spline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local).Size() < 10000.0f);
    TestFalse(TEXT("100.1m routed curve is rejected despite shorter chord"), FSFWalkRailAdapter::Validate(Track).IsEmpty());
    Spline->SetSplinePoints({FVector::ZeroVector, FVector(10000, 0, 0)}, ESplineCoordinateSpace::Local, false);
    Spline->SetSplinePointType(0, ESplinePointType::Linear, false);
    Spline->SetSplinePointType(1, ESplinePointType::Linear, true);
    TestTrue(TEXT("Exact native 100m straight limit is accepted"), FSFWalkRailAdapter::Validate(Track).IsEmpty());
    World->DestroyWorld(false);
    return true;
}
#endif
