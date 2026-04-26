// Copyright (Coffee Stain Studios). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

/**
 * Utility class for detecting and handling multiplayer/network state
 * Use this to conditionally enable/disable features based on network mode
 */
class SMARTFOUNDATIONS_API FSFNetworkHelper
{
public:
	/**
	 * Check if the world is in any multiplayer mode (Listen Server, Client, or Dedicated Server)
	 * @param World - The world to check
	 * @return true if in any multiplayer mode, false if standalone or invalid
	 */
	static bool IsMultiplayer(const UWorld* World);

	/**
	 * Check if we're running as a Listen Server (hosting multiplayer)
	 * @param World - The world to check
	 * @return true if hosting as Listen Server
	 */
	static bool IsListenServer(const UWorld* World);

	/**
	 * Check if we're running as a multiplayer client
	 * @param World - The world to check
	 * @return true if connected as client
	 */
	static bool IsClient(const UWorld* World);

	/**
	 * Check if we're running in pure standalone mode (single-player)
	 * @param World - The world to check
	 * @return true if standalone (not multiplayer)
	 */
	static bool IsStandalone(const UWorld* World);

	/**
	 * Check if we're running as a dedicated server
	 * @param World - The world to check
	 * @return true if dedicated server
	 */
	static bool IsDedicatedServer(const UWorld* World);

	/**
	 * Get a human-readable string describing the current network mode
	 * @param World - The world to check
	 * @return String like "Standalone", "Listen Server", "Client", etc.
	 */
	static FString GetNetworkModeString(const UWorld* World);

	/**
	 * Get the raw ENetMode enum value
	 * @param World - The world to check
	 * @return The current network mode enum
	 */
	static ENetMode GetNetworkMode(const UWorld* World);

	/**
	 * Check if a feature should be enabled based on multiplayer state
	 * Use this for features that are known to have issues in multiplayer
	 * @param World - The world to check
	 * @param bAllowInListenServer - Should the feature work in Listen Server mode?
	 * @param bAllowInClient - Should the feature work in Client mode?
	 * @return true if the feature should be enabled
	 */
	static bool ShouldEnableFeature(const UWorld* World, bool bAllowInListenServer = true, bool bAllowInClient = true);

	/**
	 * Log current network state for debugging
	 * @param World - The world to check
	 * @param Context - Optional context string for the log message
	 */
	static void LogNetworkState(const UWorld* World, const FString& Context = TEXT(""));
};
