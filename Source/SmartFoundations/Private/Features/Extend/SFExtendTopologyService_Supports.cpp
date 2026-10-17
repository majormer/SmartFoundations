// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Extend/SFExtendTopologyService.h"
#include "Features/Extend/SFExtendRouteSupports.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Holograms/Logistics/SFSelectedPoleState.h"
#include "EngineUtils.h"

void USFExtendTopologyService::DiscoverRouteSupports()
{
	if (!CachedTopology.SourceBuilding.IsValid()) return;
	// Native snap anchors do not participate in GetConnection()'s transport graph.
	// Index the discovered route's endpoints, then require an actual snap-position match.
	// The one-centimeter tolerance covers native spline position quantization, not a search radius.
	struct FEndpoint
	{
		FVector Position;
		TArray<TWeakObjectPtr<AFGBuildable>>* Supports;
		bool bPipe;
		TWeakObjectPtr<AFGBuildable> Candidate;
		bool bAmbiguous = false;
	};
	TArray<FEndpoint> Endpoints;
	TMultiMap<FIntVector, int32> Cells;
	auto Cell = [](const FVector& Position)
	{
		return FIntVector(FMath::FloorToInt(Position.X), FMath::FloorToInt(Position.Y), FMath::FloorToInt(Position.Z));
	};
	auto Add = [&](USceneComponent* Connection, TArray<TWeakObjectPtr<AFGBuildable>>& Supports, bool bPipe)
	{
		if (!IsValid(Connection)) return;
		const FVector Position = Connection->GetComponentLocation();
		if (Position.ContainsNaN()) return;
		Cells.Add(Cell(Position), Endpoints.Num());
		Endpoints.Add({Position, &Supports, bPipe, nullptr, false});
	};
	for (TArray<FSFConnectionChainNode>* Group : {&CachedTopology.InputChains, &CachedTopology.OutputChains})
		for (FSFConnectionChainNode& Chain : *Group)
			for (const auto& Route : Chain.Conveyors)
				if (Route.IsValid())
				{
					Add(Route->GetConnection0(), Chain.SupportPoles, false);
					Add(Route->GetConnection1(), Chain.SupportPoles, false);
				}
	for (TArray<FSFPipeConnectionChainNode>* Group : {&CachedTopology.PipeInputChains, &CachedTopology.PipeOutputChains})
		for (FSFPipeConnectionChainNode& Chain : *Group)
			for (const auto& Route : Chain.Pipelines)
				if (Route.IsValid())
				{
					TInlineComponentArray<UFGPipeConnectionComponent*> Connections;
					Route->GetComponents(Connections);
					for (UFGPipeConnectionComponent* Connection : Connections) Add(Connection, Chain.SupportPoles, true);
				}
	if (Endpoints.IsEmpty()) return;
	for (TActorIterator<AFGBuildable> It(CachedTopology.SourceBuilding->GetWorld()); It; ++It)
	{
		AFGBuildable* Pole = *It;
		UFGConnectionComponent* Anchor = SFExtendRouteSupports::GetAnchor(Pole);
		if (!Anchor) continue;
		FSFSelectedPoleState State;
		if (!SFExtendRouteSupports::IsFixedSupport(Pole->GetClass()->GetFName()) && !SFSelectedPoleState::Capture(Pole, State)) continue;
		const bool bPipe = SFExtendRouteSupports::IsPipe(Pole->GetClass()->GetFName());
		const FVector Snap = Anchor->GetComponentLocation();
		if (Snap.ContainsNaN()) continue;
		const FIntVector Center = Cell(Snap);
		for (int32 X = -1; X <= 1; ++X)
			for (int32 Y = -1; Y <= 1; ++Y)
				for (int32 Z = -1; Z <= 1; ++Z)
				{
					TArray<int32> Matches;
					Cells.MultiFind(Center + FIntVector(X, Y, Z), Matches);
					for (int32 Index : Matches)
					{
						FEndpoint& Endpoint = Endpoints[Index];
						if (Endpoint.bPipe == bPipe && FVector::DistSquared(Snap, Endpoint.Position) <= 1.0f)
						{
							if (Endpoint.Candidate.IsValid() && Endpoint.Candidate.Get() != Pole) Endpoint.bAmbiguous = true;
							Endpoint.Candidate = Pole;
						}
					}
				}
	}
	// Coincident supports have no persistent native ownership edge to disambiguate them.
	// Do not turn an arbitrary actor enumeration order into module ownership.
	for (const FEndpoint& Endpoint : Endpoints)
		if (!Endpoint.bAmbiguous && Endpoint.Candidate.IsValid()) Endpoint.Supports->AddUnique(Endpoint.Candidate);
}
