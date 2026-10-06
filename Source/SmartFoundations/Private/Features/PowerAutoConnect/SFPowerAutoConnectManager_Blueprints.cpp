// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFBlueprintPowerService.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Subsystem/SFSubsystem.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Buildables/FGBuildableWire.h"
#include "Constants/SFAssetPaths.h"

// SP/MP DIVERGENCE MAP: local preview only. Exact sockets cross in the conduit plan;
// the blueprint Construct hook materializes wires after all content on either authority path.
void FSFPowerAutoConnectManager::ProcessBlueprintPower(AFGHologram* Parent)
{
	auto* Blueprint = Cast<AFGBlueprintHologram>(Parent);
	if (!Blueprint || !Subsystem) return;
	const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
	if (Subsystem->IsSmartDisabledForCurrentAction() || !Settings.bBlueprintSeamAutoConnectEnabled || !Settings.bConnectPower)
	{
		ClearPowerLinePreviews();
		return;
	}
	TMap<FIntVector, AFGBlueprintHologram*> Grid;
	Grid.Add(FIntVector::ZeroValue, Blueprint);
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		FIntVector Cell;
		if (auto* Copy = Cast<AFGBlueprintHologram>(Child))
			if (Copy->ActorHasTag(TEXT("SF_GridChild")) && USFGridCoordComponent::TryGetCell(Copy, Cell)) Grid.Add(Cell, Copy);
	}
	TArray<FSFBlueprintPowerNode> Nodes;
	TArray<UFGPowerConnectionComponent*> Ports;
	if (Grid.Num() > 1)
	{
		for (const auto& Cell : Grid)
		{
			if (Cell.Value->GetBlueprintDesigner() != Parent->GetBlueprintDesigner()) continue;
			for (const auto& Port : FSFBlueprintPowerService::CollectPorts(Cell.Value))
			{
				if (!IsValid(Port.Preview)) continue;
				Nodes.Add({Cell.Key, Port.Identity, Port.Network, Port.Preview->GetComponentLocation(), Port.FreeSlots});
				Ports.Add(Port.Preview);
			}
		}
	}
	const UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
	const double MaxLength = WireClass ? FMath::Min(10000.0f, WireClass->GetDefaultObject<AFGBuildableWire>()->mMaxLength) : 0.0;
	TArray<TPair<UFGPowerConnectionComponent*, UFGPowerConnectionComponent*>> Desired;
	for (const auto& Span : FSFBlueprintPowerService::PlanSpans(Nodes, Settings.PowerGridAxis, MaxLength))
		Desired.Emplace(Ports[Span.Key], Ports[Span.Value]);
	UpdateExactPreviews(Parent, Desired);
}
