// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendPipeCompatibilityReceipt.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "FGSplineBuildableInterface.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"

bool FSFExtendPipeCompatibilityReceipt::Verify(const FSFCloneTopology& Plan,
    const TMap<FString, FObjectKey>& Built, const TMap<FString, FObjectKey>& Sources,
    const TSet<FObjectKey>& ActualOutputs, USFConstructionOperationCoordinator& Owner,
    const FSFConstructionOperationKey& Key, FString& Reason)
{
    auto Fail = [&](const TCHAR* Why) { Reason = Why; return false; };
    if (!Owner.IsInObservationPhase()) return Fail(TEXT("pipe_sibling_readback_outside_observation"));
    auto Resolve = [&](const FString& Role) -> AActor*
    {
        AActor* Actor = nullptr;
        if (Role.StartsWith(TEXT("source:")))
        {
            const auto* Identity = Sources.Find(Role.Mid(7));
            Actor = Identity ? Cast<AActor>(Identity->ResolveObjectPtr()) : nullptr;
        }
        else
        {
            const auto* Identity = Built.Find(Role);
            Actor = Identity ? Cast<AActor>(Identity->ResolveObjectPtr()) : nullptr;
            if (!Actor || !ActualOutputs.Contains(FObjectKey(Actor))) return nullptr;
        }
        return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->HasAuthority()
            && Actor->GetWorld() == Owner.GetWorld() && Owner.AdoptActor(Key, Actor) ? Actor : nullptr;
    };
    auto Port = [](AActor* Actor, const FString& Name) -> UFGPipeConnectionComponentBase*
    {
        if (!IsValid(Actor)) return nullptr;
        TArray<UFGPipeConnectionComponentBase*> Ports; Actor->GetComponents(Ports);
        UFGPipeConnectionComponentBase* Found = nullptr;
        for (auto* Candidate : Ports) if (IsValid(Candidate) && Candidate->GetName() == Name)
        { if (Found) return nullptr; Found = Candidate; }
        return Found;
    };
    TMap<UFGPipeConnectionComponentBase*, UFGPipeConnectionComponentBase*> Intended;
    TArray<AFGBuildablePipeline*> Pipes;
    for (const auto& Child : Plan.ChildHolograms)
    {
        if (Child.bPreviewOnly || !Child.bConstructible) continue;
        const bool bPipe = Child.Role == TEXT("pipe_segment") || (Child.Role == TEXT("lane_segment") && Child.LaneSegmentType == TEXT("pipe"));
        const bool bAttachment = Child.Role == TEXT("pipe_attachment");
        const bool bHole = Child.Role == TEXT("passthrough") && Child.BuildClass == TEXT("Build_FoundationPassthrough_Pipe_C");
        auto* Actor = bPipe || bAttachment || bHole || Child.Role == TEXT("distributor") ? Resolve(Child.HologramId) : nullptr;
        const bool bJunction = Cast<AFGBuildablePipelineJunction>(Actor) != nullptr;
        if (!bPipe && !bAttachment && !bHole && !bJunction) continue;
        if (!Actor || Actor->GetClass()->GetName() != Child.BuildClass
            || !Actor->GetActorTransform().Equals(Child.Transform.ToFTransform(), 0.1f)) return Fail(TEXT("pipe_sibling_identity_or_pose_mismatch"));
        Actors.Add(FObjectKey(Actor));
        if (bPipe)
        {
            auto* Pipe = Cast<AFGBuildablePipeline>(Actor);
            auto* Spline = Pipe ? Cast<IFGSplineBuildableInterface>(Pipe) : nullptr;
            auto* Component = Spline ? Spline->GetSplineComponent() : nullptr;
            if (!Component || !Child.bHasSplineData || Component->GetNumberOfSplinePoints() != Child.SplineData.Points.Num())
                return Fail(TEXT("pipe_sibling_spline_shape_mismatch"));
            for (int32 Index = 0; Index < Child.SplineData.Points.Num(); ++Index)
            {
                const auto& Point = Child.SplineData.Points[Index];
                if (!Component->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local).Equals(Point.Local.ToFVector(), 0.1f)
                    || !Component->GetArriveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local).Equals(Point.ArriveTangent.ToFVector(), 0.1f)
                    || !Component->GetLeaveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local).Equals(Point.LeaveTangent.ToFVector(), 0.1f))
                    return Fail(TEXT("pipe_sibling_spline_geometry_mismatch"));
            }
            Pipes.Add(Pipe);
            const FSFConnectionRef* Targets[] = {&Child.CloneConnections.ConveyorAny0, &Child.CloneConnections.ConveyorAny1};
            UFGPipeConnectionComponentBase* Ends[] = {Pipe->GetPipeConnection0(), Pipe->GetPipeConnection1()};
            for (int32 Index = 0; Index < 2; ++Index)
            {
                const auto& Target = *Targets[Index];
                if (Target.Target.IsEmpty() || Target.Target == TEXT("external")) continue;
                auto* Peer = Port(Resolve(Target.Target), Target.Connector);
                auto* Local = Ends[Index];
                if (!Local || !Peer || (Intended.Contains(Local) && Intended[Local] != Peer)
                    || (Intended.Contains(Peer) && Intended[Peer] != Local)) return Fail(TEXT("pipe_sibling_exact_edge_conflict"));
                Intended.Add(Local, Peer); Intended.Add(Peer, Local);
            }
        }
        if (bAttachment)
        {
            auto* Pump = Cast<AFGBuildablePipelinePump>(Actor);
            if (!Pump || !FMath::IsFinite(Child.UserFlowLimit) || !FMath::IsNearlyEqual(Pump->GetUserFlowLimit(), Child.UserFlowLimit, 0.001f))
                return Fail(TEXT("pipe_sibling_flow_setting_mismatch"));
        }
        if (bHole)
        {
            auto* Hole = Cast<AFGBuildablePassthrough>(Actor);
            if (!Hole || !Child.bHasPassthroughLinks || !FMath::IsNearlyEqual(Hole->GetSnappedBuildingThickness(), Child.Thickness, 0.01f))
                return Fail(TEXT("pipe_sibling_floor_hole_shape_mismatch"));
            for (const bool bTop : {false, true})
            {
                const auto& Face = bTop ? Child.PassthroughTop : Child.PassthroughBottom;
                auto* Expected = Face.Target.IsEmpty() ? nullptr : Port(Resolve(Face.Target), Face.Connector);
                auto* Actual = bTop ? Hole->GetTopSnappedConnection<UFGConnectionComponent>() : Hole->GetBottomSnappedConnection<UFGConnectionComponent>();
                if ((!Face.Target.IsEmpty() && !Expected) || Actual != Expected) return Fail(TEXT("pipe_sibling_floor_hole_face_mismatch"));
            }
        }
    }
    auto* Subsystem = AFGPipeSubsystem::Get(Owner.GetWorld());
    for (const auto& Pair : Intended)
        if (Pair.Key->GetConnection() != Pair.Value || Pair.Value->GetConnection() != Pair.Key
            || !Pair.Key->IsConnected() || !Pair.Value->IsConnected()) return Fail(TEXT("pipe_sibling_reciprocal_edge_mismatch"));
    for (auto* Pipe : Pipes)
        for (auto* Endpoint : {Pipe->GetPipeConnection0(), Pipe->GetPipeConnection1()})
            if (!Intended.Contains(Endpoint) && Endpoint->IsConnected()) return Fail(TEXT("pipe_sibling_unplanned_connection"));
    for (const auto& Identity : Actors)
    {
        auto* Actor = Cast<AActor>(Identity.ResolveObjectPtr());
        if (!IsValid(Actor)) return Fail(TEXT("pipe_sibling_retired_during_readback"));
        TArray<UFGPipeConnectionComponent*> Ports; Actor->GetComponents(Ports);
        for (auto* Component : Ports)
        {
            if (!IsValid(Component) || Component->GetOwner() != Actor) return Fail(TEXT("pipe_sibling_component_retired"));
            auto* Network = Subsystem ? Subsystem->FindPipeNetwork(Component->GetPipeNetworkID()) : nullptr;
            if (!IsValid(Network) || Network->IsActorBeingDestroyed() || Network->NeedFullRebuild()
                || !Component->GetFluidIntegrant() || Network->NumFluidIntegrants() <= 0) return Fail(TEXT("pipe_sibling_native_network_pending"));
            if (auto* Peer = Cast<UFGPipeConnectionComponent>(Component->GetConnection()))
                if (Peer->GetPipeNetworkID() != Component->GetPipeNetworkID()) return Fail(TEXT("pipe_sibling_network_edge_mismatch"));
        }
    }
    return true;
}
