// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFBlueprintPowerService.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGPowerConnectionComponent.h"

FTransform FSFBlueprintPowerService::OwnerTransformFromSocket(const FTransform& OriginalOwner,
	const FTransform& OriginalSocket, const FTransform& PreviewSocket)
{
	return OriginalSocket.GetRelativeTransform(OriginalOwner).Inverse() * PreviewSocket;
}

FSFPowerWireEndpoint FSFBlueprintPowerService::CaptureEndpoint(UFGPowerConnectionComponent* Connection)
{
	FSFPowerWireEndpoint Result;
	auto* Blueprint = Connection ? Cast<AFGBlueprintHologram>(Connection->GetOwner()) : nullptr;
	if (!Blueprint) return Result;
	const auto* OriginalPtr = Blueprint->mDuplicateConnectionToOriginalMap.Find(Connection);
	auto* Original = OriginalPtr ? Cast<UFGPowerConnectionComponent>(OriginalPtr->Get()) : nullptr;
	auto* Owner = Original ? Cast<AFGBuildablePowerPole>(Original->GetOwner()) : nullptr;
	if (!Owner) return Result;
	Result.BuildClass = Owner->GetClass();
	Result.ComponentName = Original->GetFName();
	Result.OwnerLocation = OwnerTransformFromSocket(Owner->GetActorTransform(), Original->GetComponentTransform(),
		Connection->GetComponentTransform()).GetLocation();
	return Result;
}

TArray<FSFBlueprintPowerPort> FSFBlueprintPowerService::CollectPorts(AFGBlueprintHologram* Blueprint)
{
	TArray<FSFBlueprintPowerPort> Result;
	if (!Blueprint) return Result;
	TSet<AActor*> ContentActors;
	for (const auto& Pair : Blueprint->mBuildableToNewRoot) if (IsValid(Pair.Key)) ContentActors.Add(Pair.Key);
	TMap<UFGPowerConnectionComponent*, UFGPowerConnectionComponent*> Duplicates;
	for (const auto& Pair : Blueprint->mDuplicateConnectionToOriginalMap)
		if (auto* Original = Cast<UFGPowerConnectionComponent>(Pair.Value))
			if (auto* Dup = Cast<UFGPowerConnectionComponent>(Pair.Key)) Duplicates.Add(Original, Dup);

	for (AActor* Content : ContentActors)
	{
		auto* Pole = Cast<AFGBuildablePowerPole>(Content);
		if (!Pole) continue; // consumers and arbitrary circuits are not inter-copy backbones
		TInlineComponentArray<UFGPowerConnectionComponent*> Ports(Pole);
		for (UFGPowerConnectionComponent* Port : Ports)
		{
			if (!IsValid(Port) || Port->IsHidden()) continue;
			FSFBlueprintPowerPort Entry;
			Entry.Original = Port;
			Entry.FreeSlots = FMath::Max(0, Port->GetNumFreeConnections());
			// Native blueprint-world transforms are content-fixed; never use instance names
			// or preview positions, which change between copies and as spacing changes.
			const FVector P = Pole->GetActorLocation();
			const FRotator R = Pole->GetActorRotation();
			Entry.Identity = FString::Printf(TEXT("%s|%.3f,%.3f,%.3f|%.3f,%.3f,%.3f|%s"),
				*Pole->GetClass()->GetPathName(), P.X, P.Y, P.Z, R.Pitch, R.Yaw, R.Roll, *Port->GetName());
			Entry.Preview = Duplicates.FindRef(Port);
			if (!Entry.Preview && Entry.FreeSlots > 0)
			{
				// A pole already wired INSIDE the blueprint still has usable external slots.
				// Create the same native visual-only representation as an open socket, without
				// touching its source circuit or adding anything to the blueprint's saved content.
				Entry.Preview = Blueprint->DuplicateConnectionComponent(Pole, Port);
				if (Entry.Preview) Entry.Preview->SetFlags(RF_Transient);
			}
			Result.Add(MoveTemp(Entry));
		}
	}
	Result.Sort([](const auto& A, const auto& B) { return A.Identity < B.Identity; });
	// Ambiguous coincident content must not select an arbitrary socket.
	TSet<FString> Ambiguous;
	for (int32 I = 1; I < Result.Num(); ++I)
		if (Result[I - 1].Identity == Result[I].Identity) Ambiguous.Add(Result[I].Identity);
	Result.RemoveAll([&](const auto& Entry) { return Ambiguous.Contains(Entry.Identity); });

	// Discover internal connectivity from actual cables AND hidden bridges. Circuit IDs
	// need not be initialized in the staging world, so they are not used as topology keys.
	TMap<UFGCircuitConnectionComponent*, FString> Networks;
	for (auto& Entry : Result)
	{
		if (!Networks.Contains(Entry.Original))
		{
			TArray<UFGCircuitConnectionComponent*> Pending{Entry.Original};
			while (!Pending.IsEmpty())
			{
				auto* Current = Pending.Pop();
				if (!IsValid(Current) || !ContentActors.Contains(Current->GetOwner()) || Networks.Contains(Current)) continue;
				Networks.Add(Current, Entry.Identity); // sorted first member is a stable network key
				TArray<UFGCircuitConnectionComponent*> Neighbors;
				Current->GetConnections(Neighbors);
				Current->GetHiddenConnections(Neighbors);
				Pending.Append(Neighbors);
			}
		}
		Entry.Network = Networks.FindRef(Entry.Original);
	}
	return Result;
}
