#pragma once

#include "CoreMinimal.h"
#include "FGFactoryConnectionComponent.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildable.h"

/**
 * Utility class for handling conveyor belt and lift connections.
 * 
 * Consolidates connection logic discovered during EXTEND development:
 * - Snapped connections work for lifts but not belts
 * - SetConnection() must be called during ConfigureActor for proper chain integration
 * - Connection direction matters (INPUT chains vs OUTPUT chains)
 * - Distributor connections require finding the correct output/input based on chain type
 */
class SMARTFOUNDATIONS_API FSFConveyorConnectionHelper
{
public:
    /**
     * Connect a conveyor's Conn0 to a previous conveyor's Conn1.
     * Used for chaining conveyors together (belt-to-belt, belt-to-lift, lift-to-belt).
     * 
     * @param CurrentConveyor The conveyor being connected (its Conn0 will be connected)
     * @param PreviousConveyor The previous conveyor in the chain (its Conn1 will be connected)
     * @return true if connection was established, false if already connected or invalid
     */
    static bool ConnectToPreviousConveyor(
        AFGBuildableConveyorBase* CurrentConveyor,
        AFGBuildableConveyorBase* PreviousConveyor);
    
    /**
     * Connect a conveyor's Conn0 to a distributor (splitter/merger).
     * Automatically finds the correct connection based on chain direction.
     * 
     * @param Conveyor The conveyor to connect (its Conn0 will be connected)
     * @param Distributor The splitter or merger to connect to
     * @param bIsInputChain true if items flow FROM distributor TO conveyor (splitter output)
     *                      false if items flow FROM conveyor TO distributor (merger input)
     * @return true if connection was established, false if no suitable connection found
     */
    static bool ConnectToDistributor(
        AFGBuildableConveyorBase* Conveyor,
        AFGBuildable* Distributor,
        bool bIsInputChain);
    
    /**
     * Connect a conveyor's Conn1 to a factory building.
     * Used for the last conveyor in an INPUT chain or first conveyor in an OUTPUT chain.
     * 
     * @param Conveyor The conveyor to connect
     * @param Factory The factory building to connect to
     * @param bConnectConn1 true to connect Conn1 (end of conveyor), false for Conn0 (start)
     * @param bNeedsInput true if factory needs an input connection (end of INPUT chain)
     *                    false if factory needs an output connection (start of OUTPUT chain)
     * @return true if connection was established
     */
    static bool ConnectToFactory(
        AFGBuildableConveyorBase* Conveyor,
        AFGBuildable* Factory,
        bool bConnectConn1,
        bool bNeedsInput);
    
    /**
     * Find the best matching connection on a buildable based on direction and proximity.
     * 
     * @param Buildable The buildable to search for connections
     * @param ReferenceLocation Location to measure distance from
     * @param NeededDirection The connection direction needed (INPUT or OUTPUT)
     * @param bMustBeUnconnected If true, only returns unconnected connections
     * @return The best matching connection, or nullptr if none found
     */
    static UFGFactoryConnectionComponent* FindBestConnection(
        AFGBuildable* Buildable,
        const FVector& ReferenceLocation,
        EFactoryConnectionDirection NeededDirection,
        bool bMustBeUnconnected = true);
    
    /**
     * Check if two connections can be connected (compatible directions, not already connected).
     * 
     * @param Conn1 First connection
     * @param Conn2 Second connection
     * @return true if connections are compatible and can be connected
     */
    static bool CanConnect(
        UFGFactoryConnectionComponent* Conn1,
        UFGFactoryConnectionComponent* Conn2);
    
    /**
     * Establish a connection between two factory connection components.
     * Includes validation and logging.
     * 
     * @param FromConn The "from" connection (typically output)
     * @param ToConn The "to" connection (typically input)
     * @param ContextDescription Description for logging (e.g., "CHAIN LINK", "DISTRIBUTOR")
     * @return true if connection was established
     */
    static bool EstablishConnection(
        UFGFactoryConnectionComponent* FromConn,
        UFGFactoryConnectionComponent* ToConn,
        const FString& ContextDescription = TEXT(""));
};
