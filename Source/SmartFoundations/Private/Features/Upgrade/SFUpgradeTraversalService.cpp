// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "FGPlayerController.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerCircuit.h"
#include "Buildables/FGBuildableWire.h"

FSFTraversalResult USFUpgradeTraversalService::TraverseNetwork(
	AFGBuildable* AnchorBuildable,
	const FSFTraversalConfig& Config,
	AFGPlayerController* PlayerController)
{
	FSFTraversalResult Result;

	if (!AnchorBuildable)
	{
		Result.ErrorMessage = TEXT("No anchor buildable provided");
		return Result;
	}

	Result.AnchorBuildable = AnchorBuildable;
	Result.Family = GetUpgradeFamily(AnchorBuildable);

	if (Result.Family == ESFUpgradeFamily::None)
	{
		Result.ErrorMessage = TEXT("Anchor buildable is not upgradeable");
		return Result;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("TraversalService: Starting traversal from %s (Family: %d)"),
		*AnchorBuildable->GetName(), (int32)Result.Family);

	TSet<AFGBuildable*> VisitedSet;
	TArray<AFGBuildable*> FoundBuildables;

	// Traverse based on family type
	switch (Result.Family)
	{
	case ESFUpgradeFamily::Belt:
	case ESFUpgradeFamily::Lift:
		if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(AnchorBuildable))
		{
			TraverseConveyorNetwork(Conveyor, Config, VisitedSet, FoundBuildables);
		}
		break;

	case ESFUpgradeFamily::Pipe:
	case ESFUpgradeFamily::Pump:
		if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(AnchorBuildable))
		{
			TraversePipelineNetwork(Pipeline, Config, VisitedSet, FoundBuildables);
		}
		else if (AFGBuildablePipelinePump* Pump = Cast<AFGBuildablePipelinePump>(AnchorBuildable))
		{
			// Start from pump - get connected pipes and traverse from them
			TArray<UFGPipeConnectionComponent*> PipeConns = GetPipeConnections(Pump);
			for (UFGPipeConnectionComponent* Conn : PipeConns)
			{
				if (AFGBuildable* Connected = GetConnectedBuildable(Conn))
				{
					if (AFGBuildablePipeline* ConnectedPipe = Cast<AFGBuildablePipeline>(Connected))
					{
						TraversePipelineNetwork(ConnectedPipe, Config, VisitedSet, FoundBuildables);
					}
				}
			}
		}
		break;

	case ESFUpgradeFamily::PowerPole:
	case ESFUpgradeFamily::WallOutletSingle:
	case ESFUpgradeFamily::WallOutletDouble:
		if (AFGBuildablePowerPole* PowerPole = Cast<AFGBuildablePowerPole>(AnchorBuildable))
		{
			TraversePowerNetwork(PowerPole, Config, VisitedSet, FoundBuildables);
		}
		else if (AFGBuildableWire* Wire = Cast<AFGBuildableWire>(AnchorBuildable))
		{
			// If anchored on a wire, find the connected poles and traverse from them
			bool bFoundPole = false;
			for (int32 i = 0; i < 2; ++i) // Wires have exactly 2 connections (0 and 1)
			{
				if (UFGCircuitConnectionComponent* Conn = Wire->GetConnection(i))
				{
					if (AActor* ConnectedOwner = Conn->GetOwner())
					{
						if (AFGBuildablePowerPole* ConnectedPole = Cast<AFGBuildablePowerPole>(ConnectedOwner))
						{
							TraversePowerNetwork(ConnectedPole, Config, VisitedSet, FoundBuildables);
							bFoundPole = true;
						}
					}
				}
			}
			if (!bFoundPole)
			{
				Result.ErrorMessage = TEXT("Wire is not connected to any power poles");
				return Result;
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("TraversalService: Failed to cast %s to AFGBuildablePowerPole or AFGBuildableWire - class hierarchy: %s"),
				*AnchorBuildable->GetName(), *AnchorBuildable->GetClass()->GetSuperClass()->GetName());
			Result.ErrorMessage = FString::Printf(TEXT("Power pole/wire cast failed for %s"), *AnchorBuildable->GetClass()->GetName());
			return Result;
		}
		break;

	default:
		Result.ErrorMessage = FString::Printf(TEXT("Unsupported family type: %d"), (int32)Result.Family);
		return Result;
	}

	// Check if we hit the limit
	if (FoundBuildables.Num() >= Config.MaxTraversalCount)
	{
		Result.bHitMaxLimit = true;
		UE_LOG(LogSmartFoundations, Warning, TEXT("TraversalService: Hit max traversal limit of %d"), Config.MaxTraversalCount);
	}

	// Get subsystem for tier lookups
	USFSubsystem* Subsystem = nullptr;
	if (PlayerController)
	{
		Subsystem = USFSubsystem::Get(PlayerController->GetWorld());
	}

	// Convert found buildables to audit entries
	FVector AnchorLocation = AnchorBuildable->GetActorLocation();
	for (AFGBuildable* Buildable : FoundBuildables)
	{
		if (!IsValid(Buildable))
		{
			continue;
		}

		FSFUpgradeAuditEntry Entry;
		Entry.Buildable = Buildable;
		Entry.Family = GetUpgradeFamily(Buildable);
		Entry.CurrentTier = GetBuildableTier(Buildable);
		Entry.Location = Buildable->GetActorLocation();
		Entry.DistanceFromOrigin = FVector::Dist(Entry.Location, AnchorLocation);

		// Get max available tier based on family and player unlocks
		if (Subsystem && PlayerController)
		{
			switch (Entry.Family)
			{
			case ESFUpgradeFamily::Belt:
			case ESFUpgradeFamily::Lift:
				Entry.MaxAvailableTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
				break;
			case ESFUpgradeFamily::Pipe:
				Entry.MaxAvailableTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
				break;
			case ESFUpgradeFamily::PowerPole:
				Entry.MaxAvailableTier = Subsystem->GetHighestUnlockedPowerPoleTier(PlayerController);
				break;
			case ESFUpgradeFamily::WallOutletSingle:
				Entry.MaxAvailableTier = Subsystem->GetHighestUnlockedWallOutletTier(PlayerController, /*bDouble*/ false);
				break;
			case ESFUpgradeFamily::WallOutletDouble:
				Entry.MaxAvailableTier = Subsystem->GetHighestUnlockedWallOutletTier(PlayerController, /*bDouble*/ true);
				break;
			default:
				Entry.MaxAvailableTier = 1;
				break;
			}
		}
		else
		{
			Entry.MaxAvailableTier = 6; // Default max
		}

		Result.Entries.Add(Entry);
		Result.TotalCount++;

		// Count by tier
		int32& TierCount = Result.CountByTier.FindOrAdd(Entry.CurrentTier);
		TierCount++;

		if (Entry.IsUpgradeable())
		{
			Result.UpgradeableCount++;
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("TraversalService: Found %d buildables (%d upgradeable)"),
		Result.TotalCount, Result.UpgradeableCount);

	return Result;
}

ESFUpgradeFamily USFUpgradeTraversalService::GetUpgradeFamily(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return ESFUpgradeFamily::None;
	}

	FString ClassName = Buildable->GetClass()->GetName();

	// Conveyor belts
	if (ClassName.Contains(TEXT("ConveyorBeltMk")))
	{
		return ESFUpgradeFamily::Belt;
	}

	// Conveyor lifts
	if (ClassName.Contains(TEXT("ConveyorLiftMk")))
	{
		return ESFUpgradeFamily::Lift;
	}

	// Pipeline pumps - EXCLUDED from upgrade consideration
	// Pumps are sized for head lift requirements; upgrading them has no benefit
	// as overkill head lift is wasted. Check BEFORE the AFGBuildablePipeline cast
	// because pumps derive from AFGBuildablePipeline in some hierarchies.
	if (Cast<AFGBuildablePipelinePump>(Buildable) || ClassName.Contains(TEXT("PipelinePump")))
	{
		return ESFUpgradeFamily::None;
	}

	// Pipelines - use Cast so we catch every variant (Build_Pipeline_C, Build_PipelineMK2_C,
	// Build_Pipeline_NoIndicator_C, Build_PipelineMK2_NoIndicator_C, etc.). Issue #295/#296:
	// the previous string match missed NoIndicator variants entirely.
	if (Cast<AFGBuildablePipeline>(Buildable))
	{
		return ESFUpgradeFamily::Pipe;
	}

	// Power poles
	if (ClassName.Contains(TEXT("PowerPoleMk")))
	{
		return ESFUpgradeFamily::PowerPole;
	}

	// Wall outlets - double (check BEFORE single to avoid false match on "PowerPoleWall")
	// Class names: Build_PowerPoleWallDouble_C, Build_PowerPoleWallDouble_Mk2_C, Build_PowerPoleWallDouble_Mk3_C
	if (ClassName.Contains(TEXT("PowerPoleWallDouble")))
	{
		return ESFUpgradeFamily::WallOutletDouble;
	}

	// Wall outlets - single
	// Class names: Build_PowerPoleWall_C, Build_PowerPoleWall_Mk2_C, Build_PowerPoleWall_Mk3_C
	if (ClassName.Contains(TEXT("PowerPoleWall")))
	{
		return ESFUpgradeFamily::WallOutletSingle;
	}

	// Power towers (audit only)
	if (ClassName.Contains(TEXT("PowerTower")))
	{
		return ESFUpgradeFamily::Tower;
	}

	// Power lines/wires (used as traversal anchors, returning the PowerPole family)
	// We allow targeting a wire to upgrade the connected power grid.
	if (ClassName.Contains(TEXT("Wire")) || ClassName.Contains(TEXT("PowerLine")))
	{
		return ESFUpgradeFamily::PowerPole;
	}

	return ESFUpgradeFamily::None;
}

int32 USFUpgradeTraversalService::GetBuildableTier(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return 0;
	}

	FString ClassName = Buildable->GetClass()->GetName();

	// Pipeline tier detection (Issue #295/#296): pipe class names do NOT contain "Mk1".
	// Mk.1 = Build_Pipeline_C or Build_Pipeline_NoIndicator_C (no tier token)
	// Mk.2 = Build_PipelineMK2_C or Build_PipelineMK2_NoIndicator_C (contains "MK2", all caps)
	// Handle this explicitly before the generic "MkN" scan, which is case-sensitive.
	if (Cast<AFGBuildablePipeline>(Buildable))
	{
		if (ClassName.Contains(TEXT("MK2"), ESearchCase::CaseSensitive))
		{
			return 2;
		}
		return 1;
	}

	// Power pole tier detection (Issue #267): wall outlet Mk1 variants do NOT contain "Mk1".
	// Mk.1 single = Build_PowerPoleWall_C (no tier token)
	// Mk.1 double = Build_PowerPoleWallDouble_C (no tier token)
	// Mk.2/Mk.3 variants (single and double) contain "Mk2"/"Mk3" and fall through to the generic loop.
	// Handle the base tier explicitly so network-traversal upgrades don't drop Mk1 wall outlets.
	// Consistent with SFUpgradeAuditService::GetBuildableTier, which defaults unsuffixed names to 1.
	if (Cast<AFGBuildablePowerPole>(Buildable))
	{
		for (int32 Tier = 3; Tier >= 2; Tier--)
		{
			FString TierStr = FString::Printf(TEXT("Mk%d"), Tier);
			if (ClassName.Contains(TierStr))
			{
				return Tier;
			}
		}
		return 1;
	}

	// Extract tier number from class name (e.g., "Build_ConveyorBeltMk5_C" -> 5)
	for (int32 Tier = 6; Tier >= 1; Tier--)
	{
		FString TierStr = FString::Printf(TEXT("Mk%d"), Tier);
		if (ClassName.Contains(TierStr))
		{
			return Tier;
		}
	}

	return 0;
}

void USFUpgradeTraversalService::TraverseConveyorNetwork(
	AFGBuildableConveyorBase* StartConveyor,
	const FSFTraversalConfig& Config,
	TSet<AFGBuildable*>& VisitedSet,
	TArray<AFGBuildable*>& OutBuildables)
{
	if (!StartConveyor || VisitedSet.Contains(StartConveyor))
	{
		return;
	}

	// Check max limit
	if (OutBuildables.Num() >= Config.MaxTraversalCount)
	{
		return;
	}

	// Mark as visited
	VisitedSet.Add(StartConveyor);

	// Only add if it's a belt or lift (not splitters, etc.)
	ESFUpgradeFamily Family = GetUpgradeFamily(StartConveyor);
	if (Family == ESFUpgradeFamily::Belt || Family == ESFUpgradeFamily::Lift)
	{
		OutBuildables.Add(StartConveyor);
	}

	// Get connections and traverse
	TArray<UFGFactoryConnectionComponent*> Connections = GetFactoryConnections(StartConveyor);
	for (UFGFactoryConnectionComponent* Conn : Connections)
	{
		if (!Conn)
		{
			continue;
		}

		UFGFactoryConnectionComponent* PartnerConn = Conn->GetConnection();
		if (!PartnerConn)
		{
			continue;
		}

		AActor* PartnerOwner = PartnerConn->GetOwner();
		if (!PartnerOwner)
		{
			continue;
		}

		AFGBuildable* PartnerBuildable = Cast<AFGBuildable>(PartnerOwner);
		if (!PartnerBuildable || VisitedSet.Contains(PartnerBuildable))
		{
			continue;
		}

		// Check if it's a conveyor we can traverse
		if (AFGBuildableConveyorBase* PartnerConveyor = Cast<AFGBuildableConveyorBase>(PartnerBuildable))
		{
			TraverseConveyorNetwork(PartnerConveyor, Config, VisitedSet, OutBuildables);
		}
		// Check if we should cross through this buildable
		else if (ShouldCrossBuildable(PartnerBuildable, Config))
		{
			// Mark as visited but don't add to output
			VisitedSet.Add(PartnerBuildable);

			// Get all connections from this intermediate and continue traversal
			TArray<UFGFactoryConnectionComponent*> IntermediateConns = GetFactoryConnections(PartnerBuildable);
			for (UFGFactoryConnectionComponent* IntConn : IntermediateConns)
			{
				if (!IntConn || IntConn == PartnerConn)
				{
					continue;
				}

				UFGFactoryConnectionComponent* NextPartnerConn = IntConn->GetConnection();
				if (!NextPartnerConn)
				{
					continue;
				}

				AActor* NextOwner = NextPartnerConn->GetOwner();
				if (AFGBuildableConveyorBase* NextConveyor = Cast<AFGBuildableConveyorBase>(NextOwner))
				{
					TraverseConveyorNetwork(NextConveyor, Config, VisitedSet, OutBuildables);
				}
			}
		}
	}
}

void USFUpgradeTraversalService::TraversePipelineNetwork(
	AFGBuildablePipeline* StartPipeline,
	const FSFTraversalConfig& Config,
	TSet<AFGBuildable*>& VisitedSet,
	TArray<AFGBuildable*>& OutBuildables)
{
	if (!StartPipeline || VisitedSet.Contains(StartPipeline))
	{
		return;
	}

	// Check max limit
	if (OutBuildables.Num() >= Config.MaxTraversalCount)
	{
		return;
	}

	// Mark as visited
	VisitedSet.Add(StartPipeline);

	// Add to output
	OutBuildables.Add(StartPipeline);

	// Get pipe connections and traverse
	TArray<UFGPipeConnectionComponent*> Connections = GetPipeConnections(StartPipeline);
	for (UFGPipeConnectionComponent* Conn : Connections)
	{
		if (!Conn)
		{
			continue;
		}

		UFGPipeConnectionComponent* PartnerConn = Cast<UFGPipeConnectionComponent>(Conn->GetConnection());
		if (!PartnerConn)
		{
			continue;
		}

		AActor* PartnerOwner = PartnerConn->GetOwner();
		if (!PartnerOwner)
		{
			continue;
		}

		AFGBuildable* PartnerBuildable = Cast<AFGBuildable>(PartnerOwner);
		if (!PartnerBuildable || VisitedSet.Contains(PartnerBuildable))
		{
			continue;
		}

		// Check if it's a pipeline we can traverse
		if (AFGBuildablePipeline* PartnerPipeline = Cast<AFGBuildablePipeline>(PartnerBuildable))
		{
			TraversePipelineNetwork(PartnerPipeline, Config, VisitedSet, OutBuildables);
		}
		// Check if we should cross through pumps
		else if (Config.bCrossPumps && Cast<AFGBuildablePipelinePump>(PartnerBuildable))
		{
			// Mark pump as visited
			VisitedSet.Add(PartnerBuildable);

			// Get all pipe connections from pump and continue
			TArray<UFGPipeConnectionComponent*> PumpConns = GetPipeConnections(PartnerBuildable);
			for (UFGPipeConnectionComponent* PumpConn : PumpConns)
			{
				if (!PumpConn || PumpConn == PartnerConn)
				{
					continue;
				}

				UFGPipeConnectionComponent* NextPartnerConn = Cast<UFGPipeConnectionComponent>(PumpConn->GetConnection());
				if (!NextPartnerConn)
				{
					continue;
				}

				AActor* NextOwner = NextPartnerConn->GetOwner();
				if (AFGBuildablePipeline* NextPipeline = Cast<AFGBuildablePipeline>(NextOwner))
				{
					TraversePipelineNetwork(NextPipeline, Config, VisitedSet, OutBuildables);
				}
			}
		}
	}
}

void USFUpgradeTraversalService::TraversePowerNetwork(
	AFGBuildablePowerPole* StartPole,
	const FSFTraversalConfig& Config,
	TSet<AFGBuildable*>& VisitedSet,
	TArray<AFGBuildable*>& OutBuildables)
{
	if (!StartPole || VisitedSet.Contains(StartPole))
	{
		return;
	}

	// Check max limit
	if (OutBuildables.Num() >= Config.MaxTraversalCount)
	{
		return;
	}

	// Mark as visited
	VisitedSet.Add(StartPole);

	// Only add if it's a power pole/outlet
	ESFUpgradeFamily Family = GetUpgradeFamily(StartPole);
	if (Family == ESFUpgradeFamily::PowerPole || 
		Family == ESFUpgradeFamily::WallOutletSingle || 
		Family == ESFUpgradeFamily::WallOutletDouble)
	{
		OutBuildables.Add(StartPole);
	}

	// Get power connection components
	const TArray<UFGPowerConnectionComponent*>& PowerConnections = StartPole->GetPowerConnections();
	if (PowerConnections.Num() == 0)
	{
		return;
	}

	// For each power connection, get wires and traverse
	for (UFGPowerConnectionComponent* PowerConn : PowerConnections)
	{
		if (!PowerConn)
		{
			continue;
		}

		TArray<class AFGBuildableWire*> Wires;
		PowerConn->GetWires(Wires);

		for (AFGBuildableWire* Wire : Wires)
		{
			if (!Wire)
			{
				continue;
			}

			// Get the other end of the wire (wires have 2 connections: index 0 and 1)
			UFGCircuitConnectionComponent* Conn0 = Wire->GetConnection(0);
			UFGCircuitConnectionComponent* Conn1 = Wire->GetConnection(1);
			UFGCircuitConnectionComponent* OtherConn = (Conn0 == PowerConn) ? Conn1 : Conn0;

			if (!OtherConn || OtherConn == PowerConn)
			{
				continue;
			}

			AActor* ConnectedOwner = OtherConn->GetOwner();
			if (!ConnectedOwner)
			{
				continue;
			}

			// Check if it's another power pole
			if (AFGBuildablePowerPole* ConnectedPole = Cast<AFGBuildablePowerPole>(ConnectedOwner))
			{
				if (!VisitedSet.Contains(ConnectedPole))
				{
					TraversePowerNetwork(ConnectedPole, Config, VisitedSet, OutBuildables);
				}
			}
		}
	}
}

AFGBuildable* USFUpgradeTraversalService::GetConnectedBuildable(UFGFactoryConnectionComponent* Connection)
{
	if (!Connection)
	{
		return nullptr;
	}

	UFGFactoryConnectionComponent* Partner = Connection->GetConnection();
	if (!Partner)
	{
		return nullptr;
	}

	return Cast<AFGBuildable>(Partner->GetOwner());
}

AFGBuildable* USFUpgradeTraversalService::GetConnectedBuildable(UFGPipeConnectionComponent* Connection)
{
	if (!Connection)
	{
		return nullptr;
	}

	UFGPipeConnectionComponent* Partner = Cast<UFGPipeConnectionComponent>(Connection->GetConnection());
	if (!Partner)
	{
		return nullptr;
	}

	return Cast<AFGBuildable>(Partner->GetOwner());
}

bool USFUpgradeTraversalService::ShouldCrossBuildable(AFGBuildable* Buildable, const FSFTraversalConfig& Config)
{
	if (!Buildable)
	{
		return false;
	}

	FString ClassName = Buildable->GetClass()->GetName();

	// Splitters and mergers
	if (Config.bCrossSplitters)
	{
		if (ClassName.Contains(TEXT("Splitter")) || ClassName.Contains(TEXT("Merger")))
		{
			return true;
		}
	}

	// Storage containers
	if (Config.bCrossStorage)
	{
		if (Cast<AFGBuildableStorage>(Buildable))
		{
			return true;
		}
	}

	// Train cargo platforms
	if (Config.bCrossTrainPlatforms)
	{
		if (Cast<AFGBuildableTrainPlatformCargo>(Buildable))
		{
			return true;
		}
	}

	// Floor holes / passthroughs (for lifts)
	if (Config.bCrossFloorHoles)
	{
		if (Cast<AFGBuildablePassthrough>(Buildable))
		{
			return true;
		}
	}

	return false;
}

TArray<UFGFactoryConnectionComponent*> USFUpgradeTraversalService::GetFactoryConnections(AFGBuildable* Buildable)
{
	TArray<UFGFactoryConnectionComponent*> Result;

	if (!Buildable)
	{
		return Result;
	}

	// For conveyors, use GetConnection0/GetConnection1
	if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Buildable))
	{
		if (UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0())
		{
			Result.Add(Conn0);
		}
		if (UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1())
		{
			Result.Add(Conn1);
		}
		return Result;
	}

	// For factories (splitters, etc.), get all components
	TArray<UActorComponent*> Components;
	Buildable->GetComponents(UFGFactoryConnectionComponent::StaticClass(), Components);
	for (UActorComponent* Component : Components)
	{
		if (UFGFactoryConnectionComponent* FactoryConn = Cast<UFGFactoryConnectionComponent>(Component))
		{
			Result.Add(FactoryConn);
		}
	}

	return Result;
}

TArray<UFGPipeConnectionComponent*> USFUpgradeTraversalService::GetPipeConnections(AFGBuildable* Buildable)
{
	TArray<UFGPipeConnectionComponent*> Result;

	if (!Buildable)
	{
		return Result;
	}

	// Get all pipe connection components
	TArray<UActorComponent*> Components;
	Buildable->GetComponents(UFGPipeConnectionComponent::StaticClass(), Components);
	for (UActorComponent* Component : Components)
	{
		if (UFGPipeConnectionComponent* PipeConn = Cast<UFGPipeConnectionComponent>(Component))
		{
			Result.Add(PipeConn);
		}
	}

	return Result;
}
