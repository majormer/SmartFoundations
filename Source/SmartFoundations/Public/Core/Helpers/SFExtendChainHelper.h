#pragma once

#include "CoreMinimal.h"
#include "FGFactoryConnectionComponent.h"

class AFGBuildableConveyorBase;
class AFGBuildable;
class USFExtendService;
struct FSFHologramData;

/**
 * FSFExtendChainHelper
 *
 * Utility class for EXTEND chain connection logic shared by belt and lift holograms.
 * Handles finding neighboring conveyors, connecting to distributors, and registering
 * built conveyors with the ExtendService.
 *
 * This consolidates ~170 lines of duplicated code from SFConveyorBeltHologram and
 * SFConveyorLiftHologram into a single, testable location.
 */
class SMARTFOUNDATIONS_API FSFExtendChainHelper
{
public:
	/**
	 * Result of chain connection resolution.
	 * Contains the target connectors for Conn0 and Conn1.
	 */
	struct FChainConnectionTargets
	{
		UFGFactoryConnectionComponent* Conn0Target = nullptr;
		UFGFactoryConnectionComponent* Conn1Target = nullptr;

		bool bHasValidTargets() const { return Conn0Target != nullptr || Conn1Target != nullptr; }
	};

	/**
	 * Resolve chain connection targets for a conveyor being built.
	 *
	 * Implements the "reverse build order" connection strategy:
	 * - Conn1 → already-built NEXT conveyor (higher index)
	 * - Conn0 → distributor (for index 0) or deferred
	 *
	 * @param HoloData - Hologram data with ExtendChainId, ExtendChainIndex, etc.
	 * @param ExtendService - Service for accessing built conveyors and distributors
	 * @param ConveyorTypeName - "Belt" or "Lift" for logging
	 * @return Connection targets for Conn0 and Conn1
	 */
	static FChainConnectionTargets ResolveChainConnections(
		const FSFHologramData* HoloData,
		USFExtendService* ExtendService,
		const FString& ConveyorTypeName = TEXT("Conveyor")
	);

	/**
	 * Find a distributor connector by name, with fallback to direction.
	 *
	 * @param Distributor - The distributor buildable
	 * @param ConnectorName - Preferred connector name from source topology
	 * @param Direction - Fallback direction (INPUT or OUTPUT)
	 * @param OutFoundByName - Set to true if found by name, false if fallback
	 * @return The found connector, or nullptr if none available
	 */
	static UFGFactoryConnectionComponent* FindDistributorConnector(
		AFGBuildable* Distributor,
		FName ConnectorName,
		EFactoryConnectionDirection Direction,
		bool& OutFoundByName
	);

	/**
	 * Register a built conveyor with the ExtendService.
	 *
	 * @param HoloData - Hologram data with chain info
	 * @param BuiltConveyor - The conveyor that was just built
	 * @param ExtendService - Service to register with
	 */
	static void RegisterBuiltConveyor(
		const FSFHologramData* HoloData,
		AFGBuildableConveyorBase* BuiltConveyor,
		USFExtendService* ExtendService
	);

	/**
	 * Check if HoloData indicates this is part of an EXTEND chain.
	 *
	 * @param HoloData - Hologram data to check
	 * @return true if this hologram is part of an EXTEND chain
	 */
	static bool IsExtendChainMember(const FSFHologramData* HoloData);
};
