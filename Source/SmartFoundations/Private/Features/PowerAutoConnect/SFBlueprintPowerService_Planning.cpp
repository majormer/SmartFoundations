// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFBlueprintPowerService.h"
#include "Features/PowerAutoConnect/SFPowerConnectionPolicy.h"

TArray<TPair<int32, int32>> FSFBlueprintPowerService::PlanSpans(const TArray<FSFBlueprintPowerNode>& Nodes, int32 GridMode, double MaxLength)
{
	TArray<TPair<int32, int32>> Result;
	if (!FMath::IsFinite(MaxLength) || MaxLength <= 1.0) return Result;
	TMap<FIntVector, TMap<FString, int32>> Cells;
	TMap<FString, TArray<int32>> Networks;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const auto& Node = Nodes[I];
		Cells.FindOrAdd(Node.Cell).Add(Node.Identity, I);
		if (Node.Cell == FIntVector::ZeroValue) Networks.FindOrAdd(Node.Network).Add(I);
	}
	TArray<FString> NetworkKeys;
	Networks.GetKeys(NetworkKeys);
	NetworkKeys.Sort();
	for (auto& Network : Networks)
		Network.Value.Sort([&](int32 A, int32 B)
		{
			if (Nodes[A].FreeSlots != Nodes[B].FreeSlots) return Nodes[A].FreeSlots > Nodes[B].FreeSlots;
			return Nodes[A].Identity < Nodes[B].Identity;
		});
	TArray<FIntVector> GridCells;
	Cells.GetKeys(GridCells);
	TArray<int32> Used;
	Used.Init(0, Nodes.Num());
	for (const auto& Edge : SFPowerConnectionPolicy::GridEdges(GridCells, GridMode))
	{
		for (const FString& Network : NetworkKeys)
		{
			// One cable per internal circuit per grid edge. Independently powered sections
			// remain independent; multiple already-connected poles do not add duplicate cables.
			for (int32 Template : Networks[Network])
			{
				const FString& Identity = Nodes[Template].Identity;
				const int32* A = Cells[Edge.Key].Find(Identity);
				const int32* B = Cells[Edge.Value].Find(Identity);
				if (!A || !B || Nodes[*A].Network != Network || Nodes[*B].Network != Network
					|| Nodes[*A].FreeSlots <= Used[*A] || Nodes[*B].FreeSlots <= Used[*B]) continue;
				const double Distance = FVector::Dist(Nodes[*A].Position, Nodes[*B].Position);
				// Geometry never selects a different counterpart. An overlong edge is dormant.
				if (FMath::IsFinite(Distance) && Distance > 1.0 && Distance <= MaxLength)
				{
					Result.Emplace(*A, *B);
					++Used[*A]; ++Used[*B];
				}
				break;
			}
		}
	}
	return Result;
}
