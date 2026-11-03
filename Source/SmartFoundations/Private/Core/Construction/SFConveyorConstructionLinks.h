// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class AFGHologram;
class AFGBuildable;
class UFGFactoryConnectionComponent;
struct FNetConstructionID;

/** Value intent captured before native Construct can reset or retire previews. */
struct FSFConveyorConstructionEndpoint
{
    TWeakObjectPtr<UWorld> World;
    FObjectKey WorldKey;
    FGuid Generation;
    TWeakObjectPtr<UFGFactoryConnectionComponent> ExistingPort;
    FObjectKey PreviewOwner;
    TWeakObjectPtr<UClass> ExpectedClass;
    FName PortName;
    FVector Position = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    float Clearance = 0;
    int32 Direction = INDEX_NONE;
    int8 NetPlayerId = -1;
    uint16 ServerId = 0;
    uint16 ClientId = 0;
    bool bPreview = false;
    bool bValid = false;
};

struct FSFConveyorConstructionEdge
{
    FSFConveyorConstructionEndpoint A;
    FSFConveyorConstructionEndpoint B;
};

/** Shared by Auto-Connect and Walk. Timers resolve identities and submit intent;
 * only the retained native-phase adapter may change already registered topology.
 * Native parent construction/payment remains a separate obligation. */
class FSFConveyorConstructionLinks
{
public:
    static void InstallHooks();
    static bool GetCurrentConstructionId(FNetConstructionID& OutId);
    /** Capture requester/root/world identity before native construction can retire the tree. */
    static bool Prepare(AFGHologram* Context, const FNetConstructionID& ConstructionId);
    static FSFConveyorConstructionEndpoint Capture(UFGFactoryConnectionComponent* Port,
        const FNetConstructionID* ConstructionId = nullptr);
    static void RecordConfigured(const AFGHologram* Preview, AFGBuildable* Built);
    static bool DefinePreviewPort(UFGFactoryConnectionComponent* Port, UClass* BuiltClass, FName BuiltPortName,
        const FNetConstructionID& ConstructionId);
    /** Caller must supply an actor from this exact native copy's returned set. */
    static bool RecordPreviewPort(const FSFConveyorConstructionEndpoint& Intent, AFGBuildable* Built);
    /** Resolve a preview peer only through its exact native construction identity. */
    static UFGFactoryConnectionComponent* ResolveConfigured(UFGFactoryConnectionComponent* Port,
        const FNetConstructionID* ConstructionId = nullptr);
    /** Accepted here means retained for admission, never physically linked or settled. */
    static bool Defer(AFGHologram* Context, const FNetConstructionID& ConstructionId,
        const TArray<FSFConveyorConstructionEdge>& Edges);
    /** Cancel pending link admission and retain native-parent effects as unresolved. */
    static void RejectPrepared(AFGHologram* Context, const FNetConstructionID& ConstructionId, const TCHAR* Reason);
};
