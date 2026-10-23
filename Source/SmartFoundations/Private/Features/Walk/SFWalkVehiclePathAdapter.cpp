// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkVehiclePathAdapter.h"
#include "SmartFoundations.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "Engine/World.h"
#include "FGConstructDisqualifier.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — Walking vehicle paths
//  1. [MP-CLIENT] Native routing projects authored frames to legal surfaces and validates presets.
//  2. [MP-AUTH] Before native BeginPlay, reuse an initialized staged neighbor's node.
namespace
{
    struct FPendingPathLink
    {
        TWeakObjectPtr<AFGVehiclePathSegmentHologram> Path;
        TWeakObjectPtr<AFGVehiclePathSegmentHologram> Previous;
    };
    TArray<FPendingPathLink> PendingPathLinks;
    TMap<TWeakObjectPtr<AFGVehiclePathSegmentHologram>, TWeakObjectPtr<AFGVehiclePathSegment>> ConstructedPaths;


}

FTransform FSFWalkVehiclePathAdapter::StartFrame(AFGVehiclePathSegmentHologram* Path)
{
    FVector Location = Path->GetActorLocation();
    // The native first-point arrow rotates independently of the actor. Its
    // graph direction is authoritative even before a complete spline exists.
    FVector Forward = Path->mStartPointExitDirection.GetSafeNormal();
    if (Forward.IsNearlyZero()) { Forward = Path->GetActorForwardVector(); }
    if (Path->mBuildStep == EVehiclePathBuildStep::EndPoint && Path->mPathSplineComponent
        && Path->mPathSplineComponent->GetNumberOfSplinePoints() >= 2)
    {
        // Native reversed splines are stored in graph order: the selected first
        // point is the graph end, not the free endpoint at spline index zero.
        const int32 SelectedPoint = Path->mIsSegmentReversed
            ? Path->mPathSplineComponent->GetNumberOfSplinePoints() - 1 : 0;
        Location = Path->mPathSplineComponent->GetLocationAtSplinePoint(SelectedPoint, ESplineCoordinateSpace::World);
        Forward = Path->mPathSplineComponent->GetDirectionAtSplinePoint(SelectedPoint, ESplineCoordinateSpace::World);
    }
    if (Forward.IsNearlyZero()) { Forward = FVector::ForwardVector; }
    // Walking's established position calculator advances opposite the frame's
    // forward axis. Reverse placement therefore needs the graph-forward frame.
    return FTransform((Path->mIsSegmentReversed ? Forward : -Forward).Rotation(), Location);
}

bool FSFWalkVehiclePathAdapter::Route(AFGVehiclePathSegmentHologram* Path, const FTransform& From,
    const FTransform& To, AFGVehiclePathSegmentHologram* Seed)
{
    if (!IsValid(Path) || !IsValid(Seed) || From.ContainsNaN() || To.ContainsNaN()) { return false; }
    // Standalone previews and reconstructed spans must inherit the native
    // Designer context before validation and construction of their path nodes.
    if (Path != Seed && Path->GetBlueprintDesigner() != Seed->GetBlueprintDesigner())
    {
        Path->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    }
    // The public mode descriptor is authoritative; mStraightMode alone is not
    // (see the retained native construction contract).
    const auto Mode = Seed->IsCurrentBuildMode(Seed->mBuildModeStraight)
        ? Seed->mBuildModeStraight : Seed->GetDefaultBuildGunMode();
    if (!Path->IsCurrentBuildMode(Mode)) { Path->SetBuildModeOverride(Mode); }
    Path->SetActorLocationAndRotation(From.GetLocation(), FRotator::ZeroRotator);
    Path->mBuildStep = EVehiclePathBuildStep::EndPoint;
    Path->mStartLocation = From.GetLocation();
    Path->mStartPointExitDirection = Seed->mIsSegmentReversed
        ? From.GetRotation().GetForwardVector() : -From.GetRotation().GetForwardVector();
    Path->mEndLocation = To.GetLocation();
    Path->mEndPointEntryDirection = -To.GetRotation().GetForwardVector();
    Path->mUseCustomEndRotation = true;
    Path->mIsSegmentReversed = Seed->mIsSegmentReversed;
    Path->mCurrentPathValidationVehicleClass = Seed->mCurrentPathValidationVehicleClass;
    Path->RouteHologramSpline();
    Path->UpdatePathValidationInfoFromSplineComponent();
    Path->UpdateHologramVisualState();
    Path->UpdatePathVisualizationHandle();
    Path->UpdateComponentTransforms();
    Path->SetActorHiddenInGame(false);
    return Path->mSplineData.Num() >= 2;
}

bool FSFWalkVehiclePathAdapter::RefreshPreview(AFGVehiclePathSegmentHologram* Path, const FTransform& From, const FTransform& To)
{
    if (!IsValid(Path)) { return false; }
    // Native aiming can regenerate the held preview through more than one entry
    // point. Reconcile only a changed seed, not every standalone span every frame.
    const USplineComponent* Spline = Path->mPathSplineComponent;
    const int32 LastPoint = Spline ? Spline->GetNumberOfSplinePoints() - 1 : -1;
    const int32 FromPoint = Path->mIsSegmentReversed ? LastPoint : 0;
    const int32 ToPoint = Path->mIsSegmentReversed ? 0 : LastPoint;
    const bool Matches = Spline && Spline->GetNumberOfSplinePoints() >= 2
        && Path->mBuildStep == EVehiclePathBuildStep::EndPoint
        && Path->mStartLocation.Equals(From.GetLocation(), 1.0f)
        && Path->mEndLocation.Equals(To.GetLocation(), 1.0f)
        && FVector::DistSquaredXY(Spline->GetLocationAtSplinePoint(FromPoint, ESplineCoordinateSpace::World), From.GetLocation()) < 1.0f
        && FVector::DistSquaredXY(Spline->GetLocationAtSplinePoint(ToPoint, ESplineCoordinateSpace::World), To.GetLocation()) < 1.0f;
    if (Matches) { return false; }
    Route(Path, From, To, Path);
    return true;
}

FString FSFWalkVehiclePathAdapter::Validate(AFGVehiclePathSegmentHologram* Path)
{
    if (!IsValid(Path) || Path->mSplineData.Num() < 2) { return NSLOCTEXT("SmartFoundations", "Walk_VehiclePathMissingRoute", "Vehicle path has no native routed spline").ToString(); }
    Path->ResetConstructDisqualifiers();
    Path->CheckValidPlacement();
    TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
    Path->GetConstructDisqualifiers(Disqualifiers);
    Path->ResetConstructDisqualifiers();
    for (const auto& Disqualifier : Disqualifiers)
    {
        return UFGConstructDisqualifier::GetDisqualifyingText(Disqualifier).ToString();
    }
    return FString();
}

float FSFWalkVehiclePathAdapter::MaximumAdvance(AFGVehiclePathSegmentHologram* Path)
{
    return Path ? Path->mMaxPathLength : 0.0f;
}

void FSFWalkVehiclePathAdapter::StageConstruction(AFGVehiclePathSegmentHologram* Path, AFGVehiclePathSegmentHologram* Previous)
{
    PendingPathLinks.RemoveAll([](const FPendingPathLink& Link) { return !Link.Path.IsValid(); });
    for (auto It = ConstructedPaths.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); }
    }
    PendingPathLinks.Add({Path, Previous});
}

void FSFWalkVehiclePathAdapter::ClearConstruction(const TArray<AFGVehiclePathSegmentHologram*>& Paths)
{
    PendingPathLinks.RemoveAll([&Paths](const FPendingPathLink& Link)
    {
        return !Link.Path.IsValid() || Paths.Contains(Link.Path.Get()) || Paths.Contains(Link.Previous.Get());
    });
    for (auto* Path : Paths) { ConstructedPaths.Remove(Path); }
}

void FSFWalkVehiclePathAdapter::InstallConstructionHook()
{
    static bool Installed = false;
    if (Installed) { return; }
    Installed = true;
    SUBSCRIBE_METHOD_VIRTUAL(AFGVehiclePathSegmentHologram::PreConfigureActor, GetMutableDefault<AFGVehiclePathSegmentHologram>(),
        [](auto& scope, AFGVehiclePathSegmentHologram* Path, AFGBuildable* Buildable)
        {
            const FPendingPathLink* Link = PendingPathLinks.FindByPredicate([Path](const FPendingPathLink& Item) { return Item.Path.Get() == Path; });
            if (!Link) { scope(Path, Buildable); return; }
            // Native PreConfigureActor only stores node pointers. Missing nodes are
            // created in BeginPlay, and native child construction can finish before
            // its parent. Record the association now; resolve either ready neighbor
            // at the last safe point before this segment registers its connections.
            scope(Path, Buildable);
            if (auto* Built = Cast<AFGVehiclePathSegment>(Buildable)) { ConstructedPaths.Add(Path, Built); }
        });

    SUBSCRIBE_METHOD_VIRTUAL(AFGVehiclePathSegment::BeginPlay, GetMutableDefault<AFGVehiclePathSegment>(),
        [](auto& scope, AFGVehiclePathSegment* Built)
        {
            AFGVehiclePathSegmentHologram* Path = nullptr;
            if (Built && Built->HasAuthority() && Built->GetWorld() && Built->GetWorld()->IsGameWorld())
            {
                for (const auto& Pair : ConstructedPaths)
                {
                    if (Pair.Value.Get() == Built) { Path = Pair.Key.Get(); break; }
                }
            }
            if (!Path || !Path->mPathSplineComponent) { scope(Built); return; }

            AFGVehiclePathNode* Start = Built->GetStartNode();
            AFGVehiclePathNode* End = Built->GetEndNode();
            const auto ReuseEndpoint = [&](AFGVehiclePathNode* Node, bool bPhysicalStart)
            {
                const bool bGraphStart = bPhysicalStart != Path->mIsSegmentReversed;
                AFGVehiclePathNode*& Slot = bGraphStart ? Start : End;
                if (Slot || !IsValid(Node) || Node->GetWorld() != Built->GetWorld()
                    || Node->GetBlueprintDesigner() != Built->GetBlueprintDesigner()) { return; }
                const int32 Point = bGraphStart ? 0 : Path->mPathSplineComponent->GetNumberOfSplinePoints() - 1;
                if (Point < 0 || FVector::DistSquared(Node->GetActorLocation(),
                    Path->mPathSplineComponent->GetLocationAtSplinePoint(Point, ESplineCoordinateSpace::World)) >= 1.0f
                    || !Path->CheckNodeCompatibility(Node, !bGraphStart)) { return; }
                Slot = Node;
            };

            for (const FPendingPathLink& Link : PendingPathLinks)
            {
                if (Link.Path.Get() == Path && Link.Previous.IsValid())
                {
                    const auto* Previous = ConstructedPaths.Find(Link.Previous);
                    if (Previous && Previous->IsValid())
                    {
                        ReuseEndpoint(Link.Previous->mIsSegmentReversed
                            ? Previous->Get()->GetStartNode() : Previous->Get()->GetEndNode(), true);
                    }
                }
                else if (Link.Previous.Get() == Path && Link.Path.IsValid())
                {
                    const auto* Next = ConstructedPaths.Find(Link.Path);
                    if (Next && Next->IsValid())
                    {
                        ReuseEndpoint(Link.Path->mIsSegmentReversed
                            ? Next->Get()->GetEndNode() : Next->Get()->GetStartNode(), false);
                    }
                }
            }
            if (Start != Built->GetStartNode() || End != Built->GetEndNode())
            {
                // Public pre-BeginPlay API; preserve native supplied endpoints and let
                // native BeginPlay create/register every remaining missing endpoint.
                Built->SetNodeConnections(Start, End);
            }
            scope(Built);
        });
}
