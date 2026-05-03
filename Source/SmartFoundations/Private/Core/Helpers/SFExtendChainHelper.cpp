#include "Core/Helpers/SFExtendChainHelper.h"
#include "SmartFoundations.h"
#include "Data/SFHologramData.h"
#include "Features/Extend/SFExtendService.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildable.h"

bool FSFExtendChainHelper::IsExtendChainMember(const FSFHologramData* HoloData)
{
	return HoloData && HoloData->ExtendChainId >= 0 && HoloData->ExtendChainIndex >= 0;
}

FSFExtendChainHelper::FChainConnectionTargets FSFExtendChainHelper::ResolveChainConnections(
	const FSFHologramData* HoloData,
	USFExtendService* ExtendService,
	const FString& ConveyorTypeName)
{
	FChainConnectionTargets Result;

	if (!HoloData || !ExtendService)
	{
		return Result;
	}

	if (!IsExtendChainMember(HoloData))
	{
		return Result;
	}

	const int32 ChainId = HoloData->ExtendChainId;
	const int32 ChainIndex = HoloData->ExtendChainIndex;
	const int32 ChainLength = HoloData->ExtendChainLength;
	const bool bIsInputChain = HoloData->bIsInputChain;

	// ============================================================
	// REVERSE BUILD ORDER CONNECTION STRATEGY
	// ============================================================
	// Conveyors are built from HIGHEST index to LOWEST (factory→distributor)
	// This allows each conveyor to set BOTH Conn0 AND Conn1:
	// - Conn1 → already-built NEXT conveyor (higher index)
	// - Conn0 → will be set by PREVIOUS conveyor when it builds
	//
	// For chain formation, we need at least ONE bidirectional connection
	// at BeginPlay() time. By building in reverse, each conveyor can connect
	// its Conn1 to the already-built next conveyor's Conn0.
	// ============================================================

	// === CONN1: Connect to NEXT conveyor (higher index, already built) or endpoint ===
	if (ChainIndex < ChainLength - 1)
	{
		// Not the last conveyor - Conn1 connects to next conveyor's Conn0 (already built)
		AFGBuildableConveyorBase* NextConveyor = ExtendService->GetBuiltConveyor(ChainId, ChainIndex + 1);
		if (NextConveyor)
		{
			Result.Conn1Target = NextConveyor->GetConnection0();
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain REVERSE: %s[%d] Conn1 → %s[%d].Conn0 (%s)"),
				*ConveyorTypeName, ChainIndex, *ConveyorTypeName, ChainIndex + 1, *NextConveyor->GetName());
		}
		else
		{
			SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain REVERSE: %s[%d] - NextConveyor[%d] not found!"),
				*ConveyorTypeName, ChainIndex, ChainIndex + 1);
		}
	}
	else
	{
		// Last conveyor in chain (highest index, built FIRST in reverse order)
		if (bIsInputChain)
		{
			// INPUT chain: last conveyor's Conn1 → Factory INPUT
			// Factory connection lookup is complex - leave for post-build wiring
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain REVERSE: %s[%d] is LAST (INPUT) - Conn1→Factory deferred to post-build"),
				*ConveyorTypeName, ChainIndex);
		}
		else
		{
			// OUTPUT chain: last conveyor's Conn1 → Distributor (merger) INPUT
			AFGBuildable* Distributor = ExtendService->GetBuiltDistributor(ChainId);
			FName ConnectorName = ExtendService->GetDistributorConnectorName(ChainId);

			if (Distributor)
			{
				bool bFoundByName = false;
				Result.Conn1Target = FindDistributorConnector(
					Distributor, ConnectorName, EFactoryConnectionDirection::FCD_INPUT, bFoundByName);

				if (Result.Conn1Target)
				{
					if (bFoundByName)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain REVERSE: %s[%d] Conn1 → Distributor.%s (by name)"),
							*ConveyorTypeName, ChainIndex, *Result.Conn1Target->GetName());
					}
					else
					{
						SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain REVERSE: %s[%d] Conn1 → Distributor.%s (fallback - name '%s' not found)"),
							*ConveyorTypeName, ChainIndex, *Result.Conn1Target->GetName(), *ConnectorName.ToString());
					}
				}
			}
		}
	}

	// === CONN0: Connect to PREVIOUS conveyor (lower index, not built yet) or distributor ===
	if (ChainIndex == 0)
	{
		// First conveyor (index 0, built LAST in reverse order)
		if (bIsInputChain)
		{
			// INPUT chain: first conveyor's Conn0 ← Distributor (splitter) OUTPUT
			AFGBuildable* Distributor = ExtendService->GetBuiltDistributor(ChainId);
			FName ConnectorName = ExtendService->GetDistributorConnectorName(ChainId);

			if (Distributor)
			{
				bool bFoundByName = false;
				Result.Conn0Target = FindDistributorConnector(
					Distributor, ConnectorName, EFactoryConnectionDirection::FCD_OUTPUT, bFoundByName);

				if (Result.Conn0Target)
				{
					if (bFoundByName)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain REVERSE: %s[0] Conn0 ← Distributor.%s (by name)"),
							*ConveyorTypeName, *Result.Conn0Target->GetName());
					}
					else
					{
						SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Chain REVERSE: %s[0] Conn0 ← Distributor.%s (fallback - name '%s' not found)"),
							*ConveyorTypeName, *Result.Conn0Target->GetName(), *ConnectorName.ToString());
					}
				}
			}
		}
		else
		{
			// OUTPUT chain: first conveyor's Conn0 ← Factory OUTPUT
			// Factory connection lookup is complex - leave for post-build wiring
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain REVERSE: %s[0] is FIRST (OUTPUT) - Conn0←Factory deferred to post-build"),
				*ConveyorTypeName);
		}
	}
	// For non-first conveyors, Conn0 will be connected when the PREVIOUS conveyor builds
	// and sets its Conn1 to our Conn0

	return Result;
}

UFGFactoryConnectionComponent* FSFExtendChainHelper::FindDistributorConnector(
	AFGBuildable* Distributor,
	FName ConnectorName,
	EFactoryConnectionDirection Direction,
	bool& OutFoundByName)
{
	OutFoundByName = false;

	if (!Distributor)
	{
		return nullptr;
	}

	TArray<UFGFactoryConnectionComponent*> DistributorConns;
	Distributor->GetComponents<UFGFactoryConnectionComponent>(DistributorConns);

	// First try to find the connector by name (from source topology)
	if (!ConnectorName.IsNone())
	{
		for (UFGFactoryConnectionComponent* Conn : DistributorConns)
		{
			if (Conn && Conn->GetFName() == ConnectorName)
			{
				OutFoundByName = true;
				return Conn;
			}
		}
	}

	// Fallback: find any available connector with correct direction
	for (UFGFactoryConnectionComponent* Conn : DistributorConns)
	{
		if (Conn && Conn->GetDirection() == Direction && !Conn->IsConnected())
		{
			return Conn;
		}
	}

	return nullptr;
}

void FSFExtendChainHelper::RegisterBuiltConveyor(
	const FSFHologramData* HoloData,
	AFGBuildableConveyorBase* BuiltConveyor,
	USFExtendService* ExtendService)
{
	if (!HoloData || !BuiltConveyor || !ExtendService)
	{
		return;
	}

	if (!IsExtendChainMember(HoloData))
	{
		return;
	}

	ExtendService->RegisterBuiltConveyor(
		HoloData->ExtendChainId,
		HoloData->ExtendChainIndex,
		BuiltConveyor,
		HoloData->bIsInputChain
	);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Registered %s as Chain[%d] Index[%d] (Input=%s)"),
		*BuiltConveyor->GetName(),
		HoloData->ExtendChainId,
		HoloData->ExtendChainIndex,
		HoloData->bIsInputChain ? TEXT("Yes") : TEXT("No"));
}
