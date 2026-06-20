// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFUpgradeExecutionService - batch connection repair (save/fix connection refs across the
 * destroy/respawn upgrade, + connection-manifest capture/validate). Split out of
 * SFUpgradeExecutionService.cpp (slice UP, pure impl-split, one class across .cpp) to keep each
 * file <2k. The 3 file-scope static connector-finders ride along here (their only users are in
 * this block). No behavior change.
 */

#include "Features/Upgrade/SFUpgradeExecutionServiceImpl.h"

void USFUpgradeExecutionService::SaveBatchConnectionPairs()
{
	// Pre-scan all pending conveyors to find inter-connected pairs
	// We need to save this BEFORE any upgrades because the old actors will be destroyed

	TSet<AFGBuildableConveyorBase*> PendingConveyorSet;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Buildable))
		{
			PendingConveyorSet.Add(Conveyor);
		}
	}

	for (AFGBuildableConveyorBase* Conveyor : PendingConveyorSet)
	{
		if (!IsValid(Conveyor))
		{
			continue;
		}

		// Check Connection0
		if (UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0())
		{
			if (UFGFactoryConnectionComponent* Partner = Conn0->GetConnection())
			{
				if (AFGBuildableConveyorBase* PartnerConveyor = Cast<AFGBuildableConveyorBase>(Partner->GetOwner()))
				{
					// Is the partner also being upgraded?
					if (PendingConveyorSet.Contains(PartnerConveyor))
					{
						// Determine which connection index on the partner
						int32 PartnerConnIndex = (Partner == PartnerConveyor->GetConnection0()) ? 0 : 1;
						SavedConnectionPairs.Add(
							TPair<AFGBuildableConveyorBase*, int32>(Conveyor, 0),
							TPair<AFGBuildableConveyorBase*, int32>(PartnerConveyor, PartnerConnIndex));
						UE_LOG(LogSmartUpgrade, Verbose, TEXT("SaveBatchConnectionPairs: %s.Conn0 -> %s.Conn%d"),
							*Conveyor->GetName(), *PartnerConveyor->GetName(), PartnerConnIndex);
					}
				}
			}
		}

		// Check Connection1
		if (UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1())
		{
			if (UFGFactoryConnectionComponent* Partner = Conn1->GetConnection())
			{
				if (AFGBuildableConveyorBase* PartnerConveyor = Cast<AFGBuildableConveyorBase>(Partner->GetOwner()))
				{
					// Is the partner also being upgraded?
					if (PendingConveyorSet.Contains(PartnerConveyor))
					{
						// Determine which connection index on the partner
						int32 PartnerConnIndex = (Partner == PartnerConveyor->GetConnection0()) ? 0 : 1;
						SavedConnectionPairs.Add(
							TPair<AFGBuildableConveyorBase*, int32>(Conveyor, 1),
							TPair<AFGBuildableConveyorBase*, int32>(PartnerConveyor, PartnerConnIndex));
						UE_LOG(LogSmartUpgrade, Verbose, TEXT("SaveBatchConnectionPairs: %s.Conn1 -> %s.Conn%d"),
							*Conveyor->GetName(), *PartnerConveyor->GetName(), PartnerConnIndex);
					}
				}
			}
		}
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("SaveBatchConnectionPairs: Saved %d inter-connected pairs"), SavedConnectionPairs.Num());
}

void USFUpgradeExecutionService::FixBatchConnectionReferences()
{
	if (SavedConnectionPairs.Num() == 0)
	{
		UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchConnectionReferences: No inter-connected pairs to fix"));
		return;
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchConnectionReferences: Processing %d saved connection pairs"),
		SavedConnectionPairs.Num());

	int32 FixedCount = 0;
	TSet<TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*>> AlreadyConnected;

	// For each saved connection pair, reconnect the new conveyors
	for (const auto& Pair : SavedConnectionPairs)
	{
		AFGBuildableConveyorBase* OldConveyor = Pair.Key.Key;
		int32 ConnectionIndex = Pair.Key.Value;
		AFGBuildableConveyorBase* OldPartner = Pair.Value.Key;
		int32 PartnerConnectionIndex = Pair.Value.Value;

		// Find the new versions
		AFGBuildableConveyorBase** NewConveyorPtr = OldToNewConveyorMap.Find(OldConveyor);
		AFGBuildableConveyorBase** NewPartnerPtr = OldToNewConveyorMap.Find(OldPartner);

		if (!NewConveyorPtr || !*NewConveyorPtr || !IsValid(*NewConveyorPtr))
		{
			continue;
		}
		if (!NewPartnerPtr || !*NewPartnerPtr || !IsValid(*NewPartnerPtr))
		{
			continue;
		}

		AFGBuildableConveyorBase* NewConveyor = *NewConveyorPtr;
		AFGBuildableConveyorBase* NewPartner = *NewPartnerPtr;

		// Skip if we've already connected these two conveyors (avoid double-connecting)
		TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*> ConveyorPair(NewConveyor, NewPartner);
		TPair<AFGBuildableConveyorBase*, AFGBuildableConveyorBase*> ReversePair(NewPartner, NewConveyor);
		if (AlreadyConnected.Contains(ConveyorPair) || AlreadyConnected.Contains(ReversePair))
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchConnectionReferences: Skipping %s <-> %s (already connected)"),
				*NewConveyor->GetName(), *NewPartner->GetName());
			continue;
		}

		// Get the connection to fix
		UFGFactoryConnectionComponent* ConnToFix = (ConnectionIndex == 0)
			? NewConveyor->GetConnection0()
			: NewConveyor->GetConnection1();

		if (!ConnToFix)
		{
			continue;
		}

		// Get the EXACT connection on the partner (not just any available)
		UFGFactoryConnectionComponent* NewPartnerConn = (PartnerConnectionIndex == 0)
			? NewPartner->GetConnection0()
			: NewPartner->GetConnection1();

		if (NewPartnerConn)
		{
			// Clear any existing (broken) connections and reconnect after every replacement exists.
			// This batch-level repair runs before the queued vanilla chain rebuild, so chain
			// stabilization sees the final physical graph.
			if (ConnToFix->IsConnected())
			{
				ConnToFix->ClearConnection();
			}
			if (NewPartnerConn->IsConnected())
			{
				NewPartnerConn->ClearConnection();
			}
			ConnToFix->SetConnection(NewPartnerConn);
			FixedCount++;
			AlreadyConnected.Add(ConveyorPair);
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchConnectionReferences: Connected %s.Conn%d -> %s.Conn%d"),
				*NewConveyor->GetName(), ConnectionIndex, *NewPartner->GetName(), PartnerConnectionIndex);
		}
		else
		{
			UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchConnectionReferences: Connection%d not found on %s"),
				PartnerConnectionIndex, *NewPartner->GetName());
		}
	}

	if (FixedCount > 0)
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("FixBatchConnectionReferences: Fixed %d inter-connected references"), FixedCount);

		// NOTE: Do NOT call RemoveConveyor/AddConveyor here.
		// The vanilla hologram upgrade path (TryUpgrade → ConfigureComponents → AddConveyor)
		// handles chain formation during Construct(). Manual chain rebuilds corrupt bucket
		// indices, causing crashes in RemoveAndSplitConveyorBucket when belts are later
		// dismantled (Array index out of bounds: -1). See RESEARCH_MassUpgrade_ChainActorSafety.md.
	}
}

// =============================================================================
// Option A (Pipe extension) + Option B (Expected-vs-Actual validation)
// =============================================================================

static UFGFactoryConnectionComponent* FindFactoryConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGFactoryConnectionComponent*> Comps;
	Buildable->GetComponents<UFGFactoryConnectionComponent>(Comps);
	for (UFGFactoryConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

static UFGPipeConnectionComponent* FindPipeConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGPipeConnectionComponent*> Comps;
	Buildable->GetComponents<UFGPipeConnectionComponent>(Comps);
	for (UFGPipeConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

static UFGCircuitConnectionComponent* FindPowerConnectorByName(AFGBuildable* Buildable, FName Name)
{
	if (!IsValid(Buildable) || Name.IsNone()) return nullptr;
	TArray<UFGCircuitConnectionComponent*> Comps;
	Buildable->GetComponents<UFGCircuitConnectionComponent>(Comps);
	for (UFGCircuitConnectionComponent* C : Comps)
	{
		if (C && C->GetFName() == Name) return C;
	}
	return nullptr;
}

void USFUpgradeExecutionService::SaveBatchPipeConnectionPairs()
{
	// Pre-scan pipelines in the batch to find inter-connected pipe-to-pipe pairs.
	// Mirrors SaveBatchConnectionPairs but for AFGBuildablePipeline.

	TSet<AFGBuildablePipeline*> PendingPipeSet;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable))
		{
			PendingPipeSet.Add(Pipe);
		}
	}

	for (AFGBuildablePipeline* Pipe : PendingPipeSet)
	{
		if (!IsValid(Pipe)) continue;

		UFGPipeConnectionComponent* Conn0 = Pipe->GetPipeConnection0();
		UFGPipeConnectionComponent* Conn1 = Pipe->GetPipeConnection1();

		auto TrySave = [&](UFGPipeConnectionComponent* Conn, int32 ConnIndex)
		{
			if (!Conn) return;
			UFGPipeConnectionComponentBase* PartnerBase = Conn->GetConnection();
			if (!PartnerBase) return;

			AActor* PartnerOwner = PartnerBase->GetOwner();
			AFGBuildablePipeline* PartnerPipe = Cast<AFGBuildablePipeline>(PartnerOwner);
			if (!PartnerPipe || !PendingPipeSet.Contains(PartnerPipe)) return;

			// Determine partner's connection index (0 or 1)
			int32 PartnerConnIndex = -1;
			if (PartnerBase == PartnerPipe->GetPipeConnection0()) PartnerConnIndex = 0;
			else if (PartnerBase == PartnerPipe->GetPipeConnection1()) PartnerConnIndex = 1;
			if (PartnerConnIndex < 0) return;

			SavedPipeConnectionPairs.Add(
				TPair<AFGBuildablePipeline*, int32>(Pipe, ConnIndex),
				TPair<AFGBuildablePipeline*, int32>(PartnerPipe, PartnerConnIndex));

			UE_LOG(LogSmartUpgrade, Verbose, TEXT("SaveBatchPipeConnectionPairs: %s.PipeConn%d -> %s.PipeConn%d"),
				*Pipe->GetName(), ConnIndex, *PartnerPipe->GetName(), PartnerConnIndex);
		};

		TrySave(Conn0, 0);
		TrySave(Conn1, 1);
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("SaveBatchPipeConnectionPairs: Saved %d inter-connected pipe pairs"), SavedPipeConnectionPairs.Num());
}

void USFUpgradeExecutionService::FixBatchPipeConnectionReferences()
{
	if (SavedPipeConnectionPairs.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchPipeConnectionReferences: Processing %d saved pipe pairs"),
		SavedPipeConnectionPairs.Num());

	int32 FixedCount = 0;
	TSet<TPair<AFGBuildablePipeline*, AFGBuildablePipeline*>> AlreadyConnected;

	for (const auto& Pair : SavedPipeConnectionPairs)
	{
		AFGBuildablePipeline* OldPipe = Pair.Key.Key;
		int32 ConnIdx = Pair.Key.Value;
		AFGBuildablePipeline* OldPartner = Pair.Value.Key;
		int32 PartnerConnIdx = Pair.Value.Value;

		AFGBuildable** NewPipePtr = OldToNewBuildableMap.Find(OldPipe);
		AFGBuildable** NewPartnerPtr = OldToNewBuildableMap.Find(OldPartner);
		if (!NewPipePtr || !*NewPipePtr || !IsValid(*NewPipePtr)) continue;
		if (!NewPartnerPtr || !*NewPartnerPtr || !IsValid(*NewPartnerPtr)) continue;

		AFGBuildablePipeline* NewPipe = Cast<AFGBuildablePipeline>(*NewPipePtr);
		AFGBuildablePipeline* NewPartner = Cast<AFGBuildablePipeline>(*NewPartnerPtr);
		if (!NewPipe || !NewPartner) continue;

		TPair<AFGBuildablePipeline*, AFGBuildablePipeline*> Fwd(NewPipe, NewPartner);
		TPair<AFGBuildablePipeline*, AFGBuildablePipeline*> Rev(NewPartner, NewPipe);
		if (AlreadyConnected.Contains(Fwd) || AlreadyConnected.Contains(Rev)) continue;

		UFGPipeConnectionComponent* LocalConn = (ConnIdx == 0) ? NewPipe->GetPipeConnection0() : NewPipe->GetPipeConnection1();
		UFGPipeConnectionComponent* PartnerConn = (PartnerConnIdx == 0) ? NewPartner->GetPipeConnection0() : NewPartner->GetPipeConnection1();
		if (!LocalConn || !PartnerConn) continue;

		if (LocalConn->IsConnected()) LocalConn->ClearConnection();
		if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
		LocalConn->SetConnection(PartnerConn);
		FixedCount++;
		AlreadyConnected.Add(Fwd);

		UE_LOG(LogSmartUpgrade, Verbose, TEXT("FixBatchPipeConnectionReferences: Connected %s.PipeConn%d -> %s.PipeConn%d"),
			*NewPipe->GetName(), ConnIdx, *NewPartner->GetName(), PartnerConnIdx);
	}

	if (FixedCount > 0)
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose, TEXT("FixBatchPipeConnectionReferences: Fixed %d inter-connected pipe references"), FixedCount);
	}
}

void USFUpgradeExecutionService::CaptureExpectedConnectionManifests()
{
	// Walk every pending buildable and snapshot its full connection surface BEFORE any destruction.
	// Partner identity = (old raw ptr, partner connector FName). Pointer is used as identity only
	// (map key / lookup into OldToNewBuildableMap); never dereferenced after destruction.
	//
	// This manifest covers power, building-side partners (splitters/factories), and any edge
	// that the pair-based Save/Fix helpers don't explicitly handle — the safety net for Option A.

	int32 TotalEdges = 0;
	for (AFGBuildable* Buildable : PendingUpgrades)
	{
		if (!IsValid(Buildable)) continue;

		TArray<FConnectionEdge>& Edges = ExpectedConnectionEdges.FindOrAdd(Buildable);

		// Factory (belt/lift) connections
		{
			TArray<UFGFactoryConnectionComponent*> Comps;
			Buildable->GetComponents<UFGFactoryConnectionComponent>(Comps);
			for (UFGFactoryConnectionComponent* C : Comps)
			{
				if (!C || !C->IsConnected()) continue;
				UFGFactoryConnectionComponent* Partner = C->GetConnection();
				if (!Partner) continue;
				AActor* PartnerOwner = Partner->GetOwner();
				if (!PartnerOwner) continue;

				FConnectionEdge Edge;
				Edge.LocalConnectorName = C->GetFName();
				Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
				Edge.PartnerConnectorName = Partner->GetFName();
				Edge.Kind = EConnectionEdgeKind::Factory;
				Edges.Add(Edge);
				TotalEdges++;
			}
		}

		// Pipe connections
		{
			TArray<UFGPipeConnectionComponent*> Comps;
			Buildable->GetComponents<UFGPipeConnectionComponent>(Comps);
			for (UFGPipeConnectionComponent* C : Comps)
			{
				if (!C || !C->IsConnected()) continue;
				UFGPipeConnectionComponentBase* Partner = C->GetConnection();
				if (!Partner) continue;
				AActor* PartnerOwner = Partner->GetOwner();
				if (!PartnerOwner) continue;

				FConnectionEdge Edge;
				Edge.LocalConnectorName = C->GetFName();
				Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
				Edge.PartnerConnectorName = Partner->GetFName();
				Edge.Kind = EConnectionEdgeKind::Pipe;
				Edges.Add(Edge);
				TotalEdges++;
			}
		}

		// Power connections (each pole/outlet can have multiple wires per connector)
		// Use UFGCircuitConnectionComponent base class so wall outlets and all pole variants are covered.
		{
			TArray<UFGCircuitConnectionComponent*> Comps;
			Buildable->GetComponents<UFGCircuitConnectionComponent>(Comps);
			for (UFGCircuitConnectionComponent* C : Comps)
			{
				if (!C) continue;
				TArray<UFGCircuitConnectionComponent*> Partners;
				C->GetConnections(Partners);
				for (UFGCircuitConnectionComponent* Partner : Partners)
				{
					if (!Partner) continue;
					AActor* PartnerOwner = Partner->GetOwner();
					if (!PartnerOwner) continue;

					FConnectionEdge Edge;
					Edge.LocalConnectorName = C->GetFName();
					Edge.PartnerOldPtr = Cast<AFGBuildable>(PartnerOwner);
					Edge.PartnerConnectorName = Partner->GetFName();
					Edge.Kind = EConnectionEdgeKind::Power;
					Edges.Add(Edge);
					TotalEdges++;
				}
			}
		}
	}

	UE_LOG(LogSmartUpgrade, Verbose, TEXT("CaptureExpectedConnectionManifests: captured %d edges across %d buildables"),
		TotalEdges, ExpectedConnectionEdges.Num());
}

void USFUpgradeExecutionService::ValidateAndRepairConnections()
{
	if (ExpectedConnectionEdges.Num() == 0)
	{
		return;
	}

	int32 Validated = 0;
	int32 Repaired = 0;
	int32 Broken = 0;

	for (auto& ManifestEntry : ExpectedConnectionEdges)
	{
		AFGBuildable* OldLocal = ManifestEntry.Key;
		const TArray<FConnectionEdge>& Edges = ManifestEntry.Value;

		// Translate old local buildable to new via the map
		AFGBuildable** NewLocalPtr = OldToNewBuildableMap.Find(OldLocal);
		if (!NewLocalPtr || !*NewLocalPtr || !IsValid(*NewLocalPtr))
		{
			// Local was skipped / failed to upgrade. Skip its edges (they belong to a buildable
			// that was never replaced; original connections should still be intact).
			continue;
		}
		AFGBuildable* NewLocal = *NewLocalPtr;

		for (const FConnectionEdge& Edge : Edges)
		{
			// Resolve partner: if partner was also in our batch, translate via map; otherwise
			// use the original pointer if still valid (non-upgraded partner).
			AFGBuildable* PartnerNew = nullptr;
			if (AFGBuildable** PartnerMapped = OldToNewBuildableMap.Find(Edge.PartnerOldPtr))
			{
				if (*PartnerMapped && IsValid(*PartnerMapped))
				{
					PartnerNew = *PartnerMapped;
				}
			}
			else if (IsValid(Edge.PartnerOldPtr))
			{
				PartnerNew = Edge.PartnerOldPtr;
			}

			if (!PartnerNew)
			{
				// Partner could not be resolved — this is a broken edge we cannot repair.
				Broken++;
				UE_LOG(LogSmartUpgrade, Verbose,
					TEXT("ValidateAndRepairConnections: %s.%s -> <unresolvable partner> (kind=%d)"),
					*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(), (int32)Edge.Kind);
				continue;
			}

			// Kind-specific validation and repair
			switch (Edge.Kind)
			{
				case EConnectionEdgeKind::Factory:
				{
					UFGFactoryConnectionComponent* LocalConn = FindFactoryConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGFactoryConnectionComponent* PartnerConn = FindFactoryConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartUpgrade, Verbose,
							TEXT("ValidateAndRepairConnections: factory edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					if (LocalConn->GetConnection() == PartnerConn)
					{
						// Edge intact
						break;
					}

					// Missing — attempt repair
					if (LocalConn->IsConnected()) LocalConn->ClearConnection();
					if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
					LocalConn->SetConnection(PartnerConn);
					if (LocalConn->GetConnection() == PartnerConn)
					{
						Repaired++;
						UE_LOG(LogSmartUpgrade, VeryVerbose,
							TEXT("ValidateAndRepairConnections: REPAIRED factory %s.%s <-> %s.%s"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					else
					{
						Broken++;
						UE_LOG(LogSmartUpgrade, Verbose,
							TEXT("ValidateAndRepairConnections: BROKEN factory %s.%s <-> %s.%s (repair failed)"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					break;
				}

				case EConnectionEdgeKind::Pipe:
				{
					UFGPipeConnectionComponent* LocalConn = FindPipeConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGPipeConnectionComponent* PartnerConn = FindPipeConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartUpgrade, Verbose,
							TEXT("ValidateAndRepairConnections: pipe edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					if (LocalConn->GetConnection() == PartnerConn)
					{
						break;
					}

					if (LocalConn->IsConnected()) LocalConn->ClearConnection();
					if (PartnerConn->IsConnected()) PartnerConn->ClearConnection();
					LocalConn->SetConnection(PartnerConn);
					if (LocalConn->GetConnection() == PartnerConn)
					{
						Repaired++;
						UE_LOG(LogSmartUpgrade, VeryVerbose,
							TEXT("ValidateAndRepairConnections: REPAIRED pipe %s.%s <-> %s.%s"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					else
					{
						Broken++;
						UE_LOG(LogSmartUpgrade, Verbose,
							TEXT("ValidateAndRepairConnections: BROKEN pipe %s.%s <-> %s.%s (repair failed)"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					}
					break;
				}

				case EConnectionEdgeKind::Power:
				{
					UFGCircuitConnectionComponent* LocalConn = FindPowerConnectorByName(NewLocal, Edge.LocalConnectorName);
					UFGCircuitConnectionComponent* PartnerConn = FindPowerConnectorByName(PartnerNew, Edge.PartnerConnectorName);
					if (!LocalConn || !PartnerConn)
					{
						Broken++;
						UE_LOG(LogSmartUpgrade, Verbose,
							TEXT("ValidateAndRepairConnections: power edge %s.%s -> %s.%s connector not found on new actor"),
							*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
							*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
						break;
					}

					Validated++;
					// Power connections are many-to-many (each connector holds N wires).
					// Verify the specific partner is among LocalConn's connections.
					TArray<UFGCircuitConnectionComponent*> CurrentPartners;
					LocalConn->GetConnections(CurrentPartners);
					const bool bAlreadyConnected = CurrentPartners.Contains(PartnerConn);
					if (bAlreadyConnected)
					{
						break;
					}

					// Power wires are AFGBuildableWire actors owned by the buildable subsystem;
					// we cannot cleanly re-create them here. Flag as broken so the operator knows.
					// In practice, vanilla's pole Upgrade_Implementation transfers wires, so this
					// branch should be rare. Log for diagnostic purposes.
					Broken++;
					UE_LOG(LogSmartUpgrade, Verbose,
						TEXT("ValidateAndRepairConnections: BROKEN power %s.%s <-> %s.%s (wire-based repair not implemented; vanilla Upgrade should have transferred)"),
						*NewLocal->GetName(), *Edge.LocalConnectorName.ToString(),
						*PartnerNew->GetName(), *Edge.PartnerConnectorName.ToString());
					break;
				}
			}
		}
	}

	WorkingResult.ValidatedConnectionCount = Validated;
	WorkingResult.RepairedConnectionCount = Repaired;
	WorkingResult.BrokenConnectionCount = Broken;

	UE_LOG(LogSmartUpgrade, VeryVerbose,
		TEXT("ValidateAndRepairConnections: validated=%d repaired=%d broken=%d"),
		Validated, Repaired, Broken);
}

