// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/Net/SFExtendTopologyQuery.h"
#include "SmartFoundations.h"
#include "Features/Extend/Net/SFExtendSourceSnapshot.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendDetectionService.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "UObject/StrongObjectPtr.h"

FSFExtendTopology SFExtendTopologyQuery::Capture(USFSubsystem* Subsystem, AFGBuildable* SourceBuilding)
{
    FSFExtendTopology Reply;
    Reply.SourceBuilding = SourceBuilding;
    if (!IsValid(Subsystem) || !IsValid(SourceBuilding)) return Reply;

    // [MP-AUTH] A remote player's aim query must not replace the listen host's live
    // topology cache. Query-owned services walk the same graph as commit reconstruction.
    TStrongObjectPtr<USFExtendDetectionService> Detection(NewObject<USFExtendDetectionService>(Subsystem));
    Detection->Initialize(Subsystem);
    TStrongObjectPtr<USFExtendTopologyService> Topology(NewObject<USFExtendTopologyService>(Subsystem));
    Topology->Initialize(Subsystem, Detection.Get());
    if (Topology->WalkTopology(SourceBuilding))
    {
        Reply = Topology->GetCurrentTopology();
        Reply.SourceSnapshot = FSFSourceTopology::CaptureFromTopology(Reply);
        // [MP-AUTH] Capture while source connections and optimized conveyor actors
        // still exist on authority. A client cannot recover these from object refs.
        auto AddOwner = [&](AFGBuildable* Actor)
        {
            if (!IsValid(Actor)) return;
            const FString Id = Actor->GetName();
            if (Reply.SourceOwners.ContainsByPredicate([&](const FSFExtendSourceOwner& Owner) { return Owner.Id == Id; })) return;
            FSFExtendSourceOwner& Owner = Reply.SourceOwners.AddDefaulted_GetRef();
            Owner.Id = Id;
            Owner.Actor = Actor;
        };
        AddOwner(SourceBuilding);
        for (const TArray<FSFConnectionChainNode>* Group : {&Reply.InputChains, &Reply.OutputChains})
            for (const FSFConnectionChainNode& Chain : *Group)
            {
                AddOwner(Chain.Distributor.Get());
                for (const auto& Conveyor : Chain.Conveyors) AddOwner(Conveyor.Get());
            }
        for (const TArray<FSFPipeConnectionChainNode>* Group : {&Reply.PipeInputChains, &Reply.PipeOutputChains})
            for (const FSFPipeConnectionChainNode& Chain : *Group)
            {
                AddOwner(Chain.Junction.Get());
                for (const auto& Pipe : Chain.Pipelines) AddOwner(Pipe.Get());
                for (const auto& Actor : Chain.PipeAttachments) AddOwner(Actor.Get());
                for (const auto& Actor : Chain.Passthroughs) AddOwner(Actor.Get());
                for (const auto& Actor : Chain.SupportPoles) AddOwner(Actor.Get());
            }
        for (const FSFPowerChainNode& Node : Reply.PowerPoles) AddOwner(Node.PowerPole.Get());
        for (const auto& Actor : Reply.PipePassthroughs) AddOwner(Actor.Get());
        for (const auto& Actor : Reply.WallPassthroughs) AddOwner(Actor.Get());
        if (Reply.SourceOwners.Num() <= 128 && SFExtendSourceSnapshot::FitsReply(Reply.SourceSnapshot))
            Reply.bHasSourceSnapshot = true;
        else
        {
            Reply.Reset();
            Reply.SourceBuilding = SourceBuilding;
            UE_LOG(LogSmartExtend, Warning, TEXT("Extend source preview exceeds the topology reply budget for %s"), *GetNameSafe(SourceBuilding));
        }
    }
    else
    {
        // The client caches an unsupported source briefly to avoid per-tick request spam.
        Reply.Reset();
        Reply.SourceBuilding = SourceBuilding;
        Reply.bIsValid = false;
    }
    return Reply;
}
