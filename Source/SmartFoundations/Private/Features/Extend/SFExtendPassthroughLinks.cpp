// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendPassthroughLinks.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGPipeConnectionComponent.h"

namespace
{
    bool CellPrefix(const FString& Id, FString& Out)
    {
        // IDs are role-suffixed; both live sc_X_Y_Z_ and persisted rr_X_Y[_Z]_
        // prefixes stay intact. Legacy scN_ is also preserved without reinterpretation.
        for (const TCHAR* Role : {TEXT("passthrough_"), TEXT("pipe_segment_"), TEXT("lane_segment_")})
        {
            const int32 Index = Id.Find(Role);
            if (Index != INDEX_NONE) { Out = Id.Left(Index); return true; }
        }
        return false;
    }
}

TArray<TPair<int32, int32>> SFExtendPassthroughLinks::Plan(const TArray<FSFPassthroughFace>& Faces,
    const TArray<FSFPassthroughEndpoint>& Endpoints)
{
    TArray<TPair<int32, int32>> Proposed, Result;
    TMap<FString, int32> Claims;
    for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); ++FaceIndex)
    {
        const FSFPassthroughFace& Face = Faces[FaceIndex];
        FString FaceCell;
        if (Face.bOccupied || Face.Location.ContainsNaN() || !CellPrefix(Face.HoleId, FaceCell)) continue;
        if (Face.bCaptured && (Face.CapturedEndpoint.Target.IsEmpty() || Face.CapturedEndpoint.Connector.IsEmpty())) continue;
        int32 Match = INDEX_NONE;
        bool Ambiguous = false;
        for (int32 Index = 0; Index < Endpoints.Num(); ++Index)
        {
            const FSFPassthroughEndpoint& Endpoint = Endpoints[Index];
            FString EndpointCell;
            if (!CellPrefix(Endpoint.Identity.Target, EndpointCell) || FaceCell != EndpointCell
                || Endpoint.Location.ContainsNaN() || !Endpoint.Location.Equals(Face.Location, 1.0)) continue;
            if (Face.bCaptured && (Endpoint.Identity.Target != Face.CapturedEndpoint.Target
                || Endpoint.Identity.Connector != Face.CapturedEndpoint.Connector)) continue;
            if (Match != INDEX_NONE) { Ambiguous = true; break; }
            Match = Index;
        }
        if (Match == INDEX_NONE || Ambiguous) continue;
        Proposed.Emplace(FaceIndex, Match);
        const FSFConnectionRef& Ref = Endpoints[Match].Identity;
        ++Claims.FindOrAdd(Ref.Target + TEXT(".") + Ref.Connector);
    }
    // A coincident pair of holes must not both claim the same endpoint.
    for (const auto& Pair : Proposed)
    {
        const FSFConnectionRef& Ref = Endpoints[Pair.Value].Identity;
        if (Claims.FindChecked(Ref.Target + TEXT(".") + Ref.Connector) == 1) Result.Add(Pair);
    }
    return Result;
}

int32 SFExtendPassthroughLinks::Apply(const FSFCloneTopology& Topology, const TMap<FString, AActor*>& BuiltActors)
{
    TArray<FSFPassthroughFace> Faces;
    TArray<AFGBuildablePassthrough*> Holes;
    TArray<FSFPassthroughEndpoint> Endpoints;
    TArray<UFGConnectionComponent*> Connections;
    TMap<AActor*, int32> Registrations;
    for (const auto& Pair : BuiltActors) if (IsValid(Pair.Value)) ++Registrations.FindOrAdd(Pair.Value);
    for (const FSFCloneHologram& Holo : Topology.ChildHolograms)
    {
        AActor* const* Found = BuiltActors.Find(Holo.HologramId);
        AActor* Actor = Found ? *Found : nullptr;
        // [MP-AUTH] Shared SP/server materialization; never mutate a source or client preview.
        if (!IsValid(Actor) || !Actor->HasAuthority() || !Holo.bConstructible || Holo.bPreviewOnly
            || Actor->GetClass()->GetName() != Holo.BuildClass || Registrations.FindRef(Actor) != 1) continue;
        if (Holo.Role == TEXT("passthrough") && Holo.BuildClass == TEXT("Build_FoundationPassthrough_Pipe_C"))
        {
            AFGBuildablePassthrough* Hole = Cast<AFGBuildablePassthrough>(Actor);
            if (!Hole) continue;
            const float Thickness = Hole->GetSnappedBuildingThickness();
            if (!FMath::IsFinite(Thickness) || Thickness <= 0) continue;
            for (const bool Top : {false, true})
            {
                FSFPassthroughFace Face;
                Face.HoleId = Holo.HologramId;
                Face.bTop = Top;
                Face.Location = Hole->GetActorTransform().TransformPosition(FVector(0, 0, (Top ? 0.5 : -0.5) * Thickness));
                Face.bCaptured = Holo.bHasPassthroughLinks;
                Face.CapturedEndpoint = Top ? Holo.PassthroughTop : Holo.PassthroughBottom;
                Face.bOccupied = (Top ? Hole->GetTopSnappedConnection<UFGConnectionComponent>()
                    : Hole->GetBottomSnappedConnection<UFGConnectionComponent>()) != nullptr;
                Faces.Add(Face);
                Holes.Add(Hole);
            }
        }
        else if (Holo.Role == TEXT("pipe_segment") || (Holo.bIsLaneSegment && Holo.LaneSegmentType == TEXT("pipe")))
        {
            AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Actor);
            if (!Pipe) continue;
            for (UFGPipeConnectionComponentBase* Port : {Pipe->GetPipeConnection0(), Pipe->GetPipeConnection1()})
            {
                if (!IsValid(Port)) continue;
                FSFPassthroughEndpoint Endpoint;
                Endpoint.Identity = FSFConnectionRef(Holo.HologramId, Port->GetName());
                Endpoint.Location = Port->GetComponentLocation();
                Endpoints.Add(Endpoint);
                Connections.Add(Port);
            }
        }
    }
    int32 Count = 0;
    for (const auto& Pair : Plan(Faces, Endpoints))
    {
        AFGBuildablePassthrough* Hole = Holes[Pair.Key];
        UFGConnectionComponent* Connection = Connections[Pair.Value];
        const bool Top = Faces[Pair.Key].bTop;
        if (!IsValid(Hole) || !IsValid(Connection)) continue;
        if (Top ? Hole->GetTopSnappedConnection<UFGConnectionComponent>() != nullptr
                : Hole->GetBottomSnappedConnection<UFGConnectionComponent>() != nullptr) continue;
        if (Top) Hole->SetTopSnappedConnection(Connection);
        else Hole->SetBottomSnappedConnection(Connection);
        ++Count;
    }
    return Count;
}
