// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendPipeCompatibilityReceipt.h"
#include "Components/SplineComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendPipeWorldGeometryTest,
    "SmartFoundations.Extend.PipeWorldGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendPipeWorldGeometryTest::RunTest(const FString& Parameters)
{
    for (const double Yaw : {0.0, 67.0, 90.0, -15.0})
    {
        FSFExtendPreparedPipeSpline Prepared;
        Prepared.LocalToWorld = FTransform(FRotator(0, Yaw, 0), FVector(159600, -244000, 1200));
        for (const FVector Position : {FVector(0, 0, 0), FVector(300, 100, 150), FVector(900, 200, 300)})
        {
            FSFSplinePoint Point;
            Point.Local = FSFVec3(Position);
            Point.ArriveTangent = FSFVec3(FVector(200, 80, 30));
            Point.LeaveTangent = FSFVec3(FVector(250, -50, 40));
            Prepared.Spline.Points.Add(Point);
        }
        TStrongObjectPtr<USplineComponent> Actual(NewObject<USplineComponent>());
        // The same built route rebased to a different frame, including scale.
        const FTransform BuiltFrame(FRotator(0, 31, 0), FVector(150000, -240000, 800), FVector(2, 1, 0.5));
        Actual->SetWorldTransform(BuiltFrame);
        Actual->ClearSplinePoints(false);
        for (int32 Index = 0; Index < Prepared.Spline.Points.Num(); ++Index)
        {
            const auto& Point = Prepared.Spline.Points[Index];
            Actual->AddSplinePoint(Prepared.LocalToWorld.TransformPosition(Point.Local.ToFVector()),
                ESplineCoordinateSpace::World, false);
            Actual->SetTangentsAtSplinePoint(Index,
                Prepared.LocalToWorld.TransformVector(Point.ArriveTangent.ToFVector()),
                Prepared.LocalToWorld.TransformVector(Point.LeaveTangent.ToFVector()),
                ESplineCoordinateSpace::World, false);
        }
        Actual->UpdateSpline();
        TestTrue(TEXT("Rebased stepped/rotated curve retains world geometry"), Prepared.MatchesGeometry(Actual.Get()));
        const FVector Start = Actual->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
        Actual->SetLocationAtSplinePoint(0, Start + FVector(1, 0, 0), ESplineCoordinateSpace::World);
        TestFalse(TEXT("Real endpoint displacement is rejected"), Prepared.MatchesGeometry(Actual.Get()));
        Actual->SetLocationAtSplinePoint(0, Start, ESplineCoordinateSpace::World);
        Actual->SetTangentsAtSplinePoint(1, FVector::ZeroVector, FVector::ZeroVector, ESplineCoordinateSpace::World);
        TestFalse(TEXT("Same endpoints with altered curve tangents are rejected"), Prepared.MatchesGeometry(Actual.Get()));
    }
    return true;
}
#endif
