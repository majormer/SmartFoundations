// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Walk/SFWalkRailAdapter.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "FGConstructDisqualifier.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — Walking rails
//  1. [MP-CLIENT] Standalone preview uses native rail routing/shape validation.
//  2. [MP-AUTH] Native ConfigureComponents links constructed endpoints, never preview connectors.
namespace
{
    struct FPendingRailLink
    {
        TWeakObjectPtr<const AFGRailroadTrackHologram> Track;
        TWeakObjectPtr<const AFGRailroadTrackHologram> Previous;
    };
    TArray<FPendingRailLink> PendingLinks;
    TMap<TWeakObjectPtr<const AFGRailroadTrackHologram>, TWeakObjectPtr<AFGBuildableRailroadTrack>> ConstructedTracks;
}

FTransform FSFWalkRailAdapter::StartFrame(AFGRailroadTrackHologram* Track)
{
    FVector Location = Track->GetActorLocation();
    FVector Forward = Track->GetActorForwardVector();
    if (Track->mSplineData.Num() >= 2)
    {
        Location = Track->GetActorTransform().TransformPosition(Track->mSplineData[0].Location);
        Forward = Track->GetActorTransform().TransformVector(Track->mSplineData[0].LeaveTangent).GetSafeNormal();
    }
    if (Forward.IsNearlyZero()) { Forward = FVector::ForwardVector; }
    // Walking's established grid step advances along negative frame X.
    return FTransform((-Forward).Rotation(), Location);
}

FTransform FSFWalkRailAdapter::CrossSectionPose(const FTransform& Frame, int32 Lane, int32 Stack)
{
    FSFPositionCalculator Calculator;
    FSFCounterState Counters;
    const FVector Location = Calculator.CalculateChildPosition(0, Lane, Stack, Frame.GetLocation(), Frame.Rotator(),
        FVector(0.0f, 800.0f, 400.0f), Counters);
    return FTransform(Frame.GetRotation(), Location);
}

bool FSFWalkRailAdapter::Route(AFGRailroadTrackHologram* Track, const FTransform& From, const FTransform& To, AFGRailroadTrackHologram* Seed)
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
    Track->AutoRouteSpline(From.GetLocation(), -From.GetRotation().GetForwardVector(), To.GetLocation(), To.GetRotation().GetForwardVector());
    Track->mFloorData.Reset();
    Track->mFloorData.Add(FVector::ZeroVector);
    Track->mFloorData.Add(Track->GetActorTransform().InverseTransformPosition(To.GetLocation()));
    Track->UpdateSplineComponent();
    Track->UpdateClearanceData();
    Track->UpdateComponentTransforms();
    // Spawned native holograms remain hidden until normal aim placement runs.
    // Walking owns their routed placement; expose the completed native preview.
    Track->SetActorHiddenInGame(false);
    return Track->mSplineData.Num() >= 2;
}

FString FSFWalkRailAdapter::Validate(AFGRailroadTrackHologram* Track)
{
    if (!IsValid(Track) || Track->mSplineData.Num() < 2) { return TEXT("Rail segment has no native spline"); }
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

float FSFWalkRailAdapter::MaximumAdvance(AFGRailroadTrackHologram* Track)
{
    return Track ? Track->mMaxLength : 0.0f;
}

void FSFWalkRailAdapter::StageConstruction(AFGRailroadTrackHologram* Track, AFGRailroadTrackHologram* Previous)
{
    PendingLinks.RemoveAll([](const FPendingRailLink& Link) { return !Link.Track.IsValid(); });
    for (auto It = ConstructedTracks.CreateIterator(); It; ++It) { if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); } }
    PendingLinks.Add({Track, Previous});
}

void FSFWalkRailAdapter::ClearConstruction(const TArray<AFGRailroadTrackHologram*>& Tracks)
{
    PendingLinks.RemoveAll([&Tracks](const FPendingRailLink& Link)
    {
        return !Link.Track.IsValid() || Tracks.Contains(Link.Track.Get()) || Tracks.Contains(Link.Previous.Get());
    });
    for (const auto* Track : Tracks) { ConstructedTracks.Remove(Track); }
}

void FSFWalkRailAdapter::ConfigureConnections(const AFGRailroadTrackHologram* Track, AFGBuildable* Buildable)
{
    if (AFGBuildableRailroadTrack* Built = Cast<AFGBuildableRailroadTrack>(Buildable))
    {
        ConstructedTracks.Add(Track, Built);
        for (int32 Index = PendingLinks.Num() - 1; Index >= 0; --Index)
        {
            const auto& Link = PendingLinks[Index];
            const auto* Current = ConstructedTracks.Find(Link.Track);
            const auto* Previous = ConstructedTracks.Find(Link.Previous);
            if (!Link.Previous.IsValid()) { PendingLinks.RemoveAt(Index); continue; }
            if (Current && Previous && Current->IsValid() && Previous->IsValid())
            {
                UFGRailroadTrackConnectionComponent* A = Previous->Get()->GetConnection(1);
                UFGRailroadTrackConnectionComponent* B = Current->Get()->GetConnection(0);
                if (A && B && A->CanConnectTo(B)) { A->AddConnection(B); }
                PendingLinks.RemoveAt(Index);
            }
        }
    }
}

void FSFWalkRailAdapter::InstallConstructionHook()
{
    static bool Installed = false;
    if (Installed) { return; }
    Installed = true;
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGRailroadTrackHologram::ConfigureComponents, GetMutableDefault<AFGRailroadTrackHologram>(),
        [](const AFGRailroadTrackHologram* Track, AFGBuildable* Buildable)
        {
            if (PendingLinks.ContainsByPredicate([Track](const FPendingRailLink& Link) { return Link.Track.Get() == Track || Link.Previous.Get() == Track; }))
            {
                FSFWalkRailAdapter::ConfigureConnections(Track, Buildable);
            }
        });
}
