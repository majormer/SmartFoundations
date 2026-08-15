// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

enum class ESFConveyorGeometryBackend : uint8
{
    Reject,
    BeltSpline,
    ConveyorRuntime
};

/** Pure readiness, dispatch, and finite-radius policy shared by Upgrade audit/execution. */
namespace SFConveyorGeometryPolicy
{
    inline constexpr float MaxRadiusCm = 1000000.0f; // Smart Upgrade UI maximum: 10,000 m.

    inline bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    }

    inline bool TryResolveRadiusSquared(const FVector& Origin, const float Radius, float& OutRadiusSq)
    {
        OutRadiusSq = 0.0f;
        if (!IsFiniteVector(Origin)
            || !FMath::IsFinite(Radius)
            || Radius < 0.0f
            || Radius > MaxRadiusCm)
        {
            return false;
        }

        if (Radius == 0.0f)
        {
            return true;
        }

        OutRadiusSq = Radius * Radius;
        return FMath::IsFinite(OutRadiusSq) && OutRadiusSq > 0.0f;
    }

    inline bool IsDistanceWithinRadius(const float DistanceSq, const float RadiusSq)
    {
        return FMath::IsFinite(DistanceSq)
            && DistanceSq >= 0.0f
            && FMath::IsFinite(RadiusSq)
            && RadiusSq > 0.0f
            && DistanceSq <= RadiusSq;
    }

    inline ESFConveyorGeometryBackend SelectBackend(
        const bool bActorValid,
        const bool bActorHasBegunPlay,
        const bool bIsBelt,
        const bool bSplineComponentValid,
        const int32 SplinePointCount,
        const float ConveyorLength,
        const float SplineLength)
    {
        if (!bActorValid || !bActorHasBegunPlay || !FMath::IsFinite(ConveyorLength) || ConveyorLength <= 0.0f)
        {
            return ESFConveyorGeometryBackend::Reject;
        }

        if (!bIsBelt)
        {
            return ESFConveyorGeometryBackend::ConveyorRuntime;
        }

        return bSplineComponentValid
            && SplinePointCount >= 2
            && FMath::IsFinite(SplineLength)
            && SplineLength > 0.0f
            ? ESFConveyorGeometryBackend::BeltSpline
            : ESFConveyorGeometryBackend::Reject;
    }

    inline bool CanSample(
        const bool bActorValid,
        const bool bActorHasBegunPlay,
        const bool bIsBelt,
        const bool bSplineComponentValid,
        const int32 SplinePointCount,
        const float ConveyorLength,
        const float SplineLength)
    {
        return SelectBackend(
            bActorValid,
            bActorHasBegunPlay,
            bIsBelt,
            bSplineComponentValid,
            SplinePointCount,
            ConveyorLength,
            SplineLength) != ESFConveyorGeometryBackend::Reject;
    }

    template<typename TBeltSplineSampler, typename TConveyorRuntimeSampler>
    inline bool DispatchSample(
        const ESFConveyorGeometryBackend Backend,
        TBeltSplineSampler BeltSplineSampler,
        TConveyorRuntimeSampler ConveyorRuntimeSampler)
    {
        if (Backend == ESFConveyorGeometryBackend::BeltSpline)
        {
            return BeltSplineSampler();
        }
        if (Backend == ESFConveyorGeometryBackend::ConveyorRuntime)
        {
            return ConveyorRuntimeSampler();
        }
        return false;
    }

    template<typename TBeltSplineSampler, typename TConveyorRuntimeSampler>
    inline bool TryFindClosestLocation(
        const ESFConveyorGeometryBackend Backend,
        TBeltSplineSampler BeltSplineSampler,
        TConveyorRuntimeSampler ConveyorRuntimeSampler)
    {
        return DispatchSample(Backend, BeltSplineSampler, ConveyorRuntimeSampler);
    }

    template<typename TBeltSplineSampler, typename TConveyorRuntimeSampler>
    inline bool TrySampleLocationAtFraction(
        const ESFConveyorGeometryBackend Backend,
        TBeltSplineSampler BeltSplineSampler,
        TConveyorRuntimeSampler ConveyorRuntimeSampler)
    {
        return DispatchSample(Backend, BeltSplineSampler, ConveyorRuntimeSampler);
    }

    template<typename TDistanceSampler>
    inline bool EvaluateIntersection(const float RadiusSq, TDistanceSampler DistanceSampler)
    {
        if (!FMath::IsFinite(RadiusSq) || RadiusSq <= 0.0f)
        {
            return false;
        }

        float DistanceSq = 0.0f;
        return DistanceSampler(DistanceSq) && IsDistanceWithinRadius(DistanceSq, RadiusSq);
    }

    template<typename TDistanceSampler>
    inline bool EvaluateFullyInside(const float RadiusSq, TDistanceSampler DistanceSampler)
    {
        if (!FMath::IsFinite(RadiusSq) || RadiusSq <= 0.0f)
        {
            return false;
        }

        constexpr float SampleFractions[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        for (const float Fraction : SampleFractions)
        {
            float DistanceSq = 0.0f;
            if (!DistanceSampler(Fraction, DistanceSq)
                || !IsDistanceWithinRadius(DistanceSq, RadiusSq))
            {
                return false;
            }
        }
        return true;
    }
}
