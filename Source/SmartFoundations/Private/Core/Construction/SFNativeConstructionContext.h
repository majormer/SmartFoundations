// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
class UWorld;
class AFGHologram;
class AFGBuildable;
class AFGBlueprintProxy;
struct FNetConstructionID;

/** Immutable native call context; never derives ownership from the active local preview. */
class FSFNativeConstructionContext
{
public:
    class FScope
    {
    public:
        FScope(AFGHologram* Preview, const FNetConstructionID& Id);
        ~FScope();
        FScope(const FScope&) = delete;
        FScope& operator=(const FScope&) = delete;
    private:
        bool bEntered = false;
    };
    static AFGHologram* GetRoot();
    static bool MatchesInput(const FObjectKey& Root, const FNetConstructionID& Id, const UWorld* World);
    static bool AdoptGroup(AFGHologram* Root, const FNetConstructionID& Id, AFGBlueprintProxy* Group);
    static AFGBlueprintProxy* GetGroup(const UWorld* World);
    /** Native Blueprint copies and Designers retain their native grouping ownership. */
    static AFGBlueprintProxy* GroupBuildable(AFGBuildable* Buildable, bool& bCreatedProxy);
};
