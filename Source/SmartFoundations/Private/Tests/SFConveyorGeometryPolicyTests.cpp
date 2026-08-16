// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/Upgrade/SFConveyorGeometryPolicy.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSFConveyorGeometryPolicyTest,
    "SmartFoundations.Upgrade.ConveyorGeometry.Policy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorGeometryPolicyTest::RunTest(const FString& Parameters)
{
    using Backend = ESFConveyorGeometryBackend;

    TestEqual(TEXT("Ready belt selects direct spline geometry"),
        SFConveyorGeometryPolicy::SelectBackend(true, true, true, true, 4, 1200.0f, 1198.0f),
        Backend::BeltSpline);
    TestEqual(TEXT("Ready lift retains conveyor runtime geometry"),
        SFConveyorGeometryPolicy::SelectBackend(true, true, false, false, 0, 600.0f, 0.0f),
        Backend::ConveyorRuntime);
    TestEqual(TEXT("Unready belt rejects geometry instead of falling back to runtime sampling"),
        SFConveyorGeometryPolicy::SelectBackend(true, true, true, true, 1, 1200.0f, 1198.0f),
        Backend::Reject);

    TestTrue(TEXT("Ready belt spline geometry may be sampled"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, true, 4, 1200.0f, 1198.0f));
    TestFalse(TEXT("Belt geometry waits for BeginPlay"),
        SFConveyorGeometryPolicy::CanSample(true, false, true, true, 4, 1200.0f, 1198.0f));
    TestFalse(TEXT("Missing belt spline component is rejected"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, false, 0, 1200.0f, 0.0f));
    TestFalse(TEXT("Belt spline with fewer than two points is rejected"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, true, 1, 1200.0f, 1198.0f));
    TestFalse(TEXT("Non-finite conveyor length is rejected"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, true, 4,
            std::numeric_limits<float>::quiet_NaN(), 1198.0f));
    TestFalse(TEXT("Non-finite belt spline length is rejected"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, true, 4, 1200.0f,
            std::numeric_limits<float>::infinity()));
    TestFalse(TEXT("Zero-length belt geometry is rejected"),
        SFConveyorGeometryPolicy::CanSample(true, true, true, true, 4, 0.0f, 0.0f));
    TestTrue(TEXT("Ready conveyor lift does not require belt spline state"),
        SFConveyorGeometryPolicy::CanSample(true, true, false, false, 0, 600.0f, 0.0f));
    TestFalse(TEXT("Invalid conveyor actor is rejected"),
        SFConveyorGeometryPolicy::CanSample(false, true, false, false, 0, 600.0f, 0.0f));

    FSFUpgradeExecutionParams TraversalParams;
    TraversalParams.bUseSpecificBuildables = true;
    TestTrue(TEXT("Traversal intent survives an empty filtered payload"), TraversalParams.HasSpecificBuildables());
    TestFalse(TEXT("Empty traversal payload is rejected"), TraversalParams.HasValidSpecificSelection());
    TraversalParams.SpecificBuildables.Add(nullptr);
    TestTrue(TEXT("Non-empty traversal payload is structurally valid"), TraversalParams.HasValidSpecificSelection());

    float RadiusSq = -1.0f;
    const FVector FiniteOrigin(100.0, 200.0, 300.0);
    TestTrue(TEXT("Zero radius is the explicit unlimited mode"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(FiniteOrigin, 0.0f, RadiusSq));
    TestEqual(TEXT("Unlimited mode resolves to zero squared radius"), RadiusSq, 0.0f);
    TestTrue(TEXT("UI maximum radius is accepted"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(FiniteOrigin, 1000000.0f, RadiusSq));
    TestTrue(TEXT("UI maximum radius produces a finite square"), FMath::IsFinite(RadiusSq));
    TestFalse(TEXT("Radius above the UI maximum is rejected"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(FiniteOrigin, 1000001.0f, RadiusSq));
    TestFalse(TEXT("Negative radius is rejected"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(FiniteOrigin, -1.0f, RadiusSq));
    TestFalse(TEXT("Infinite radius is rejected"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(
            FiniteOrigin, std::numeric_limits<float>::infinity(), RadiusSq));
    TestFalse(TEXT("NaN origin is rejected"),
        SFConveyorGeometryPolicy::TryResolveRadiusSquared(
            FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0), 100.0f, RadiusSq));

    int32 BeltSamples = 0;
    int32 RuntimeSamples = 0;
    const auto BeltSampler = [&BeltSamples]() { ++BeltSamples; return true; };
    const auto RuntimeSampler = [&RuntimeSamples]() { ++RuntimeSamples; return true; };
    TestTrue(TEXT("Production closest-point adapter dispatches belt spline only"),
        SFConveyorGeometryPolicy::TryFindClosestLocation(Backend::BeltSpline, BeltSampler, RuntimeSampler));
    TestEqual(TEXT("Closest-point belt adapter invokes spline sampler once"), BeltSamples, 1);
    TestEqual(TEXT("Closest-point belt adapter never invokes runtime sampler"), RuntimeSamples, 0);

    BeltSamples = 0;
    RuntimeSamples = 0;
    TestTrue(TEXT("Production fraction adapter dispatches lift runtime only"),
        SFConveyorGeometryPolicy::TrySampleLocationAtFraction(Backend::ConveyorRuntime, BeltSampler, RuntimeSampler));
    TestEqual(TEXT("Fraction lift adapter never invokes belt spline sampler"), BeltSamples, 0);
    TestEqual(TEXT("Fraction lift adapter invokes runtime sampler once"), RuntimeSamples, 1);

    BeltSamples = 0;
    RuntimeSamples = 0;
    TestFalse(TEXT("Rejected production closest-point adapter invokes no sampler"),
        SFConveyorGeometryPolicy::TryFindClosestLocation(Backend::Reject, BeltSampler, RuntimeSampler));
    TestEqual(TEXT("Rejected geometry leaves spline sampler untouched"), BeltSamples, 0);
    TestEqual(TEXT("Rejected geometry leaves runtime sampler untouched"), RuntimeSamples, 0);

    TestTrue(TEXT("Finite closest-point distance inside radius intersects"),
        SFConveyorGeometryPolicy::EvaluateIntersection(100.0f,
            [](float& OutDistanceSq) { OutDistanceSq = 25.0f; return true; }));
    TestFalse(TEXT("Non-finite closest-point distance fails closed"),
        SFConveyorGeometryPolicy::EvaluateIntersection(100.0f,
            [](float& OutDistanceSq) { OutDistanceSq = std::numeric_limits<float>::infinity(); return true; }));
    TestFalse(TEXT("Failed closest-point sample fails closed"),
        SFConveyorGeometryPolicy::EvaluateIntersection(100.0f,
            [](float& OutDistanceSq) { OutDistanceSq = 0.0f; return false; }));

    int32 FullInsideSamples = 0;
    int32 FullInsideRuntimeSamples = 0;
    TestTrue(TEXT("Fully-inside evaluation uses production fraction adapter for all five samples"),
        SFConveyorGeometryPolicy::EvaluateFullyInside(100.0f,
            [&FullInsideSamples, &FullInsideRuntimeSamples](float Fraction, float& OutDistanceSq)
            {
                return SFConveyorGeometryPolicy::TrySampleLocationAtFraction(
                    Backend::BeltSpline,
                    [&FullInsideSamples, Fraction, &OutDistanceSq]()
                    {
                        ++FullInsideSamples;
                        OutDistanceSq = Fraction * 25.0f;
                        return true;
                    },
                    [&FullInsideRuntimeSamples]()
                    {
                        ++FullInsideRuntimeSamples;
                        return true;
                    });
            }));
    TestEqual(TEXT("Fully-inside evaluation samples five fractions"), FullInsideSamples, 5);
    TestEqual(TEXT("Fully-inside belt evaluation never samples runtime geometry"), FullInsideRuntimeSamples, 0);
    TestFalse(TEXT("Fully-inside evaluation rejects a non-finite distance"),
        SFConveyorGeometryPolicy::EvaluateFullyInside(100.0f,
            [](float Fraction, float& OutDistanceSq)
            {
                OutDistanceSq = Fraction == 0.5f
                    ? std::numeric_limits<float>::quiet_NaN()
                    : 25.0f;
                return true;
            }));

    return true;
}

#endif
