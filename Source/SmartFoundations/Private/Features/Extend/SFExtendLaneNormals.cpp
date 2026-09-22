// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendLaneNormals.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Shared/Conduits/SFDistributorTopology.h"

namespace
{
    bool ValidDirection(const FVector& Value)
    {
        return !Value.ContainsNaN() && !Value.IsNearlyZero() && FMath::IsFinite(Value.SizeSquared());
    }
}

void SFExtendLaneNormals::VerifyCapture(FSFCloneHologram& Lane, const FString& ClassName, const FRotator& Rotation)
{
    FVector Local;
    Lane.bLaneStartNormalVerified = FSFDistributorTopologyResolver::GetLocalPortDirection(ClassName, FName(*Lane.LaneFromConnector), Local)
        && Rotation.RotateVector(Local).Equals(Lane.LaneStartNormal.ToFVector(), 0.001);
    Lane.bLaneEndNormalVerified = FSFDistributorTopologyResolver::GetLocalPortDirection(ClassName, FName(*Lane.LaneToConnector), Local)
        && Rotation.RotateVector(Local).Equals(Lane.LaneEndNormal.ToFVector(), 0.001);
}

void SFExtendLaneNormals::RecoverLegacy(const FSFCloneTopology& Topology, FSFCloneHologram& Lane)
{
    if (!Lane.bIsLaneSegment || Lane.SplineData.Points.Num() < 2) return;
    auto Recover = [&](const FSFConnectionRef& Ref, const FVector& Endpoint, FSFVec3& Normal, bool& Verified)
    {
        if (Verified && ValidDirection(Normal.ToFVector())) return;
        Verified = false;
        const bool Source = Ref.Target.StartsWith(TEXT("source:"));
        const FString Id = Source ? Ref.Target.Mid(7) : Ref.Target;
        const FSFCloneHologram* Owner = nullptr;
        for (const FSFCloneHologram& Candidate : Topology.ChildHolograms)
        {
            if (Candidate.Role != TEXT("distributor") && Candidate.Role != TEXT("pipe_junction")) continue;
            if ((Source ? Candidate.SourceId : Candidate.HologramId) != Id) continue;
            if (Owner) return; // ambiguous identity is not an orientation proof
            Owner = &Candidate;
        }
        FVector Local;
        if (!Owner || !FSFDistributorTopologyResolver::GetLocalPortDirection(Owner->BuildClass, FName(*Ref.Connector), Local)) return;
        const FVector Direction = Owner->Transform.Rotation.ToFRotator().RotateVector(Local);
        const FVector Center = Owner->Transform.Location.ToFVector() - (Source ? Topology.WorldOffset.ToFVector() : FVector::ZeroVector);
        // All catalogued distributor sockets are 100 cm from the root. A legacy
        // rotated clone may not establish its source's pose; retain the old recovery then.
        if (!(Center + Direction * 100.0).Equals(Endpoint, 1.0)) return;
        Normal = FSFVec3(Direction);
        Verified = true;
    };
    Recover(Lane.CloneConnections.ConveyorAny0, Lane.SplineData.Points[0].World.ToFVector(), Lane.LaneStartNormal, Lane.bLaneStartNormalVerified);
    Recover(Lane.CloneConnections.ConveyorAny1, Lane.SplineData.Points.Last().World.ToFVector(), Lane.LaneEndNormal, Lane.bLaneEndNormalVerified);
}

void SFExtendLaneNormals::RepairUnverified(FSFCloneHologram& Lane, const FVector& Start, const FVector& End)
{
    if (!Lane.bIsLaneSegment || (Lane.LaneSegmentType != TEXT("belt") && Lane.LaneSegmentType != TEXT("pipe"))) return;
    const FVector Chord = (End - Start).GetSafeNormal();
    if (!ValidDirection(Chord)) return;
    // Compatibility for poisoned pre-named-port captures (#422), per endpoint.
    // A valid verified socket normal is never replaced merely because the lane bends.
    if (!Lane.bLaneStartNormalVerified || !ValidDirection(Lane.LaneStartNormal.ToFVector())) Lane.LaneStartNormal = FSFVec3(Chord);
    if (!Lane.bLaneEndNormalVerified || !ValidDirection(Lane.LaneEndNormal.ToFVector())) Lane.LaneEndNormal = FSFVec3(-Chord);
}
