// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "SFConveyorBeltGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorBeltGeometryTest,
    "SmartFoundations.Conveyor.CanonicalGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorBeltGeometryTest::RunTest(const FString& Parameters)
{
    // #504: a fresh 8 m Extend lane split into a -90 degree first half and a
    // zero-degree second half, leaving the latter's far endpoint 544 cm away.
    TArray<FSplinePointData> Straight;
    Straight.SetNum(2);
    Straight[0].Location = FVector::ZeroVector;
    Straight[0].ArriveTangent = FVector(1, 0, 0);
    Straight[0].LeaveTangent = FVector(400, 0, 0);
    Straight[1].Location = FVector(800, 0, 0);
    Straight[1].ArriveTangent = FVector(400, 0, 0);
    Straight[1].LeaveTangent = FVector(1, 0, 0);
    FTransform StraightTransform(FRotator(0, -90, 0), FVector(314500, -138700, 8600.001953125));
    TestTrue(TEXT("Rotated Extend lane is converted"),
        SFConveyorBeltGeometry::BakeActorRotation(StraightTransform, Straight));
    TestTrue(TEXT("Built actor rotation is exactly neutral"),
        StraightTransform.GetRotation().Equals(FQuat::Identity, 0.0));
    TestTrue(TEXT("Direction is baked into local spline"), Straight[1].Location.Equals(FVector(0, -800, 0), 0.0001));
    TestTrue(TEXT("Arrive tangent rotates without normalization"), Straight[1].ArriveTangent.Equals(FVector(0, -400, 0), 0.0001));
    TestTrue(TEXT("Leave tangent rotates without normalization"), Straight[0].LeaveTangent.Equals(FVector(0, -400, 0), 0.0001));
    TestTrue(TEXT("Far merger endpoint is preserved"),
        StraightTransform.TransformPosition(Straight[1].Location).Equals(FVector(314500, -139500, 8600.001953125), 0.0001));

    // Model the observed split's second half: local data is rebased at the cut,
    // but the new actor gets zero rotation. Canonical input must remain correct.
    const double Cut = 415.3187632560731;
    const FVector CutLocal(0, -Cut, 0);
    const FTransform SecondHalf(FQuat::Identity, StraightTransform.TransformPosition(CutLocal));
    TestTrue(TEXT("Zero-rotation split half reaches original endpoint"),
        SecondHalf.TransformPosition(Straight.Last().Location - CutLocal).Equals(
            FVector(314500, -139500, 8600.001953125), 0.0001));

    const FTransform BeforeSecondPass = StraightTransform;
    const TArray<FSplinePointData> BeforeSecondPoints = Straight;
    TestFalse(TEXT("Already-canonical input is a no-op"),
        SFConveyorBeltGeometry::BakeActorRotation(StraightTransform, Straight));
    TestTrue(TEXT("Second pass preserves transform"), StraightTransform.Equals(BeforeSecondPass, 0.0));
    TestTrue(TEXT("Second pass preserves points exactly"), Straight[1].Location.Equals(BeforeSecondPoints[1].Location, 0.0));

    // Captured internal segments / Restore may have nonzero first local points,
    // bends and vertical offsets. Sampling Hermite spans catches tangent damage
    // that endpoint-only tests miss. Also exercise both lane directions and yaw.
    TArray<FSplinePointData> Curve;
    Curve.SetNum(3);
    Curve[0].Location = FVector(80, 120, 30);
    Curve[0].ArriveTangent = FVector(-20, 50, 5);
    Curve[0].LeaveTangent = FVector(400, 50, 70);
    Curve[1].Location = FVector(600, 200, 300);
    Curve[1].ArriveTangent = FVector(350, -75, 90);
    Curve[1].LeaveTangent = FVector(50, 250, -70);
    Curve[2].Location = FVector(700, 800, 250);
    Curve[2].ArriveTangent = FVector(-50, 400, 45);
    Curve[2].LeaveTangent = FVector(1, 0, 0);
    for (const FRotator Rotation : {FRotator(0, 90, 0), FRotator(0, -37, 0), FRotator(20, 135, 0)})
    {
        const FTransform Original(Rotation, FVector(314500, -139500, 8600), FVector(1.5, 0.75, 2));
        FTransform Canonical = Original;
        TArray<FSplinePointData> Converted = Curve;
        TestTrue(TEXT("Curved copied segment is converted"), SFConveyorBeltGeometry::BakeActorRotation(Canonical, Converted));
        TestTrue(TEXT("Origin is not moved"), Canonical.GetLocation().Equals(Original.GetLocation(), 0.0));
        TestTrue(TEXT("Scale is not changed"), Canonical.GetScale3D().Equals(Original.GetScale3D(), 0.0));
        TestTrue(TEXT("Copied segment has zero actor rotation"), Canonical.GetRotation().Equals(FQuat::Identity, 0.0));
        TestEqual(TEXT("Point count is preserved"), Converted.Num(), Curve.Num());
        for (int32 Index = 0; Index < Curve.Num(); ++Index)
        {
            TestTrue(TEXT("World point and flow order preserved"), Canonical.TransformPosition(Converted[Index].Location).Equals(Original.TransformPosition(Curve[Index].Location), 0.0001));
            TestTrue(TEXT("World arrive tangent preserved"), Canonical.TransformVector(Converted[Index].ArriveTangent).Equals(Original.TransformVector(Curve[Index].ArriveTangent), 0.0001));
            TestTrue(TEXT("World leave tangent preserved"), Canonical.TransformVector(Converted[Index].LeaveTangent).Equals(Original.TransformVector(Curve[Index].LeaveTangent), 0.0001));
        }
        for (int32 Span = 0; Span + 1 < Curve.Num(); ++Span)
        {
            for (int32 Sample = 0; Sample <= 20; ++Sample)
            {
                const double Alpha = Sample / 20.0;
                const FVector OldLocal = FMath::CubicInterp(Curve[Span].Location, Curve[Span].LeaveTangent,
                    Curve[Span + 1].Location, Curve[Span + 1].ArriveTangent, Alpha);
                const FVector NewLocal = FMath::CubicInterp(Converted[Span].Location, Converted[Span].LeaveTangent,
                    Converted[Span + 1].Location, Converted[Span + 1].ArriveTangent, Alpha);
                TestTrue(TEXT("Whole world curve preserved"), Original.TransformPosition(OldLocal).Equals(Canonical.TransformPosition(NewLocal), 0.0001));
            }
        }
    }
    // Vanilla manual belts can anchor at the flow END with a nonzero first point.
    // Zero rotation means no rebase, reorder, tangent edits, or origin movement.
    FTransform Manual(FQuat::Identity, FVector(314500, -139500, 8600.001953125));
    TArray<FSplinePointData> ManualPoints = BeforeSecondPoints;
    ManualPoints[0].Location = FVector(0, 800, 0);
    ManualPoints[1].Location = FVector::ZeroVector;
    TestFalse(TEXT("Manual end-anchored belt is unchanged"), SFConveyorBeltGeometry::BakeActorRotation(Manual, ManualPoints));
    TestTrue(TEXT("Manual start is not rebased"), ManualPoints[0].Location.Equals(FVector(0, 800, 0), 0.0));

    TArray<FSplinePointData> Empty;
    FTransform Incomplete(FRotator(0, 90, 0), FVector(100, 200, 300));
    const FTransform OriginalIncomplete = Incomplete;
    TestFalse(TEXT("Missing spline does not change frame"), SFConveyorBeltGeometry::BakeActorRotation(Incomplete, Empty));
    TestTrue(TEXT("Incomplete transform is untouched"), Incomplete.Equals(OriginalIncomplete, 0.0));
    return true;
}
#endif
