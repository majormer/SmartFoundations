// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Shared/Conduits/SFNativeRailGeometry.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "FGConstructDisqualifier.h"
#include "Hologram/HologramHelpers.h"

FString FSFNativeRailGeometry::Validate(AFGRailroadTrackHologram* Track)
{
    if (!IsValid(Track) || Track->mSplineData.Num() < 2) { return TEXT("Rail segment has no native spline"); }
    if (!IsValid(Track->mSplineComponent)) { return TEXT("Rail segment has no native spline component"); }
    // Check the routed curve, not its chord or the requested Walking advance.
    // This also guards every reconstructed authority span before payment.
    const float Length = Track->mSplineComponent->GetSplineLength();
    if (!FMath::IsFinite(Length)) { return TEXT("Rail segment has invalid spline length"); }
    if (Length > Track->mMaxLength)
    {
        const FString Reason = UFGConstructDisqualifier::GetDisqualifyingText(UFGCDTrackTooLong::StaticClass()).ToString();
        return Reason.IsEmpty() ? NSLOCTEXT("SmartFoundations", "WalkRailTooLong", "Rail segment exceeds the native maximum length").ToString() : Reason;
    }
    if (Length < Track->mMinLength)
    {
        const FString Reason = UFGConstructDisqualifier::GetDisqualifyingText(UFGCDTrackTooShort::StaticClass()).ToString();
        return Reason.IsEmpty() ? NSLOCTEXT("SmartFoundations", "WalkRailTooShort", "Rail segment is shorter than the native minimum length").ToString() : Reason;
    }
    Track->ResetConstructDisqualifiers();
    Track->ValidateRailroadTrack();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
    Track->GetConstructDisqualifiers(Disqualifiers);
    Track->ResetConstructDisqualifiers();
    for (const auto& Disqualifier : Disqualifiers)
    {
        return UFGConstructDisqualifier::GetDisqualifyingText(Disqualifier).ToString();
    }
    return FString();
}


bool FSFNativeRailGeometry::Route(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To, AFGRailroadTrackHologram* Seed)
{
    if (!IsValid(Track) || !IsValid(Seed) || From.ContainsNaN() || To.ContainsNaN()) { return false; }
    Track->SetActorLocationAndRotation(From.GetLocation(), FRotator::ZeroRotator);
    Track->mStraightMode = Seed->mStraightMode;
    // Rails finalize from endpoint selection; they have no adjustable support pole.
    Track->mBuildStep = ESplineHologramBuildStep::SHBS_PlacePoleOrSnapEnding;
    Track->mActivePointIdx = 1;
    Track->mSnappedStartLocation = From.GetLocation();
    Track->mSnappedStartTangent = -From.GetRotation().GetForwardVector();
    Track->mHitLocation = To.GetLocation();
    Track->mHitTangent = -To.GetRotation().GetForwardVector();
    Track->mUseCustomEndRotation = true;
    // Match native GenerateAndUpdateSpline's endpoint routing. AutoRouteSpline
    // uses a different bend builder and does not produce the normal placed rail curve.
    Track->mSplineData.SetNum(2);
    auto& Start = Track->mSplineData[0];
    auto& End = Track->mSplineData[1];
    Start.Location = FVector::ZeroVector;
    Start.ArriveTangent = Start.LeaveTangent = Track->mSnappedStartTangent;
    End.Location = Track->GetActorTransform().InverseTransformPosition(To.GetLocation());
    End.ArriveTangent = End.LeaveTangent = Track->mHitTangent;
    FSplineUtils::CalcAutoCurveSpline(Start, End);
    Track->mFloorData.Reset();
    Track->mFloorData.Add(FVector::ZeroVector);
    Track->mFloorData.Add(Track->GetActorTransform().InverseTransformPosition(To.GetLocation()));
    Track->UpdateSplineComponent();
    Track->UpdateClearanceData();
    Track->UpdateComponentTransforms();
    // Spawned native holograms remain hidden until normal aim placement runs.
    // The routed preview owns its placement; expose the completed native preview.
    Track->SetActorHiddenInGame(false);
    return Track->mSplineData.Num() >= 2;
}

bool FSFNativeRailGeometry::RefreshPreview(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To)
{
    if (!IsValid(Track)) { return false; }
    const USplineComponent* Spline = Track->mSplineComponent;
    const int32 Last = Spline ? Spline->GetNumberOfSplinePoints() - 1 : -1;
    const bool Matches = Last >= 1
        && Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Equals(From.GetLocation(), 0.1f)
        && Spline->GetLocationAtSplinePoint(Last, ESplineCoordinateSpace::World).Equals(To.GetLocation(), 0.1f)
        && Spline->GetDirectionAtSplinePoint(0, ESplineCoordinateSpace::World).Equals(-From.GetRotation().GetForwardVector(), 0.001f)
        && Spline->GetDirectionAtSplinePoint(Last, ESplineCoordinateSpace::World).Equals(-To.GetRotation().GetForwardVector(), 0.001f);
    // Native aiming can rewrite the held first span even while its location is
    // locked. Reconcile only that changed preview, when its endpoints change.
    return !Matches && Route(Track, From, To, Track);
}

