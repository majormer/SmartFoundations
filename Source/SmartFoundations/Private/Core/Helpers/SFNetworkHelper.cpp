// Copyright (Coffee Stain Studios). All Rights Reserved.

#include "SFNetworkHelper.h"
#include "SmartFoundations.h"

bool FSFNetworkHelper::IsMultiplayer(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	return NetMode != NM_Standalone;
}

bool FSFNetworkHelper::IsListenServer(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() == NM_ListenServer;
}

bool FSFNetworkHelper::IsClient(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() == NM_Client;
}

bool FSFNetworkHelper::IsStandalone(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() == NM_Standalone;
}

bool FSFNetworkHelper::IsDedicatedServer(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() == NM_DedicatedServer;
}

FString FSFNetworkHelper::GetNetworkModeString(const UWorld* World)
{
	if (!World)
	{
		return TEXT("Invalid World");
	}

	switch (World->GetNetMode())
	{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_ListenServer:
			return TEXT("Listen Server");
		case NM_Client:
			return TEXT("Client");
		case NM_DedicatedServer:
			return TEXT("Dedicated Server");
		default:
			return TEXT("Unknown");
	}
}

ENetMode FSFNetworkHelper::GetNetworkMode(const UWorld* World)
{
	if (!World)
	{
		return NM_Standalone; // Safe default
	}

	return World->GetNetMode();
}

bool FSFNetworkHelper::ShouldEnableFeature(const UWorld* World, bool bAllowInListenServer, bool bAllowInClient)
{
	if (!World)
	{
		return false; // Conservative: disable if no world
	}

	const ENetMode NetMode = World->GetNetMode();

	// Always enable in standalone and dedicated server
	if (NetMode == NM_Standalone || NetMode == NM_DedicatedServer)
	{
		return true;
	}

	// Check Listen Server permission
	if (NetMode == NM_ListenServer)
	{
		return bAllowInListenServer;
	}

	// Check Client permission
	if (NetMode == NM_Client)
	{
		return bAllowInClient;
	}

	// Unknown mode - conservative disable
	return false;
}

void FSFNetworkHelper::LogNetworkState(const UWorld* World, const FString& Context)
{
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[NetworkState%s] World is invalid"),
			Context.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | %s"), *Context));
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	const FString ModeString = GetNetworkModeString(World);
	const bool bIsMultiplayer = IsMultiplayer(World);

	FString ContextPrefix = Context.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" | %s"), *Context);

	UE_LOG(LogSmartFoundations, Log, TEXT("[NetworkState%s] Mode=%s (%d) | IsMultiplayer=%s"),
		*ContextPrefix,
		*ModeString,
		static_cast<int32>(NetMode),
		bIsMultiplayer ? TEXT("YES") : TEXT("NO"));
}
