// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGBlueprintHologram;
class AFGVehiclePathNode;
class AFGVehiclePathSegment;
class AFGBuildable;
class AFGHologram;
struct FNetConstructionID;

/** A native directed tail/head choice. Names and transient network IDs are not identity. */
struct FSFBlueprintVehiclePair
{
    TWeakObjectPtr<AFGVehiclePathNode> SourceTail;
    TWeakObjectPtr<AFGVehiclePathNode> TargetHead;
    TWeakObjectPtr<AFGVehiclePathSegment> TargetSegment;
    FTransform SourceOriginal;
    FTransform TargetOriginal;
    FTransform SourceWorld;
    FTransform TargetWorld;
};

/** Owns the ordinary native connection manager and its temporary bridge previews. */
class SMARTFOUNDATIONS_API FSFBlueprintVehicleSelection
{
public:
    ~FSFBlueprintVehicleSelection();
    bool IsValid() const;
    const FSFBlueprintVehiclePair& GetPair() const;
    FString Describe() const;
    void GetBridges(TArray<AFGHologram*>& Out) const;
private:
    friend class FSFBlueprintVehicleAdapter;
    struct FImpl;
    TSharedPtr<FImpl> Impl;
};

/** Native Blueprint path predicates and construction, scoped to one directed endpoint. */
class SMARTFOUNDATIONS_API FSFBlueprintVehicleAdapter
{
public:
    static TSharedPtr<FSFBlueprintVehicleSelection> Preview(AFGBlueprintHologram* Source,
        AFGVehiclePathNode* Tail, AFGBlueprintHologram* Neighbor, AFGVehiclePathSegment* Segment);

    // [MP-AUTH] The caller supplies only nodes/segments from the exact constructed copies.
    // bAttempted distinguishes a failed predicate from a native mutation that must not replay.
    static bool Construct(AFGBlueprintHologram* Source, const TSharedPtr<FSFBlueprintVehicleSelection>& Selection,
        AFGVehiclePathNode* BuiltTail, AFGVehiclePathNode* BuiltHead, AFGVehiclePathSegment* BuiltTarget,
        FNetConstructionID ID, TArray<AFGBuildable*>& OutBridges, bool& bAttempted);
};
