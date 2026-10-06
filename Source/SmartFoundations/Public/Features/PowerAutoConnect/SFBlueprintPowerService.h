// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Features/PowerAutoConnect/Net/SFPowerWireEndpoint.h"

class AFGBlueprintHologram;
class UFGPowerConnectionComponent;

struct FSFBlueprintPowerPort
{
	UFGPowerConnectionComponent* Preview = nullptr;
	UFGPowerConnectionComponent* Original = nullptr;
	FString Identity;
	FString Network;
	int32 FreeSlots = 0;
};

struct FSFBlueprintPowerNode
{
	FIntVector Cell = FIntVector::ZeroValue;
	FString Identity;
	FString Network;
	FVector Position = FVector::ZeroVector;
	int32 FreeSlots = 0;
};

/** Blueprint-owned socket identity; this service alone reads the native duplicate/original map. */
class SMARTFOUNDATIONS_API FSFBlueprintPowerService
{
public:
	static TArray<FSFBlueprintPowerPort> CollectPorts(AFGBlueprintHologram* Blueprint);
	static FSFPowerWireEndpoint CaptureEndpoint(UFGPowerConnectionComponent* Connection);
	static FTransform OwnerTransformFromSocket(const FTransform& OriginalOwner, const FTransform& OriginalSocket, const FTransform& PreviewSocket);
	static TArray<TPair<int32, int32>> PlanSpans(const TArray<FSFBlueprintPowerNode>& Nodes, int32 GridMode, double MaxLength);
};
