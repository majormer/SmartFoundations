// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "Engine/EngineBaseTypes.h"

/** One permission to enter the named native dispatcher; never an SML continuation. */
class FSFExtendNativeInputTicket
{
public:
    static bool CanOwnLocalInput(ENetMode Mode, bool bControllerAuthority, bool bLocalController, bool bFinalStep)
    {
        return (Mode == NM_Standalone || Mode == NM_ListenServer)
            && bControllerAuthority && bLocalController && bFinalStep;
    }
    FSFExtendNativeInputTicket(const FGuid& InWorld, FObjectKey InReceiver, FObjectKey InRoot)
        : World(InWorld), Receiver(InReceiver), Root(InRoot) {}
    bool Permit(const FGuid& CurrentWorld, FObjectKey CurrentReceiver, FObjectKey CurrentRoot, bool bIntentMatches)
    {
        if (bPermitted || bConsumed || !World.IsValid() || CurrentWorld != World
            || Receiver == FObjectKey() || Root == FObjectKey()
            || Receiver != CurrentReceiver || Root != CurrentRoot || !bIntentMatches) return false;
        bPermitted = true;
        return true;
    }
    bool Consume(const FGuid& CurrentWorld, FObjectKey CurrentReceiver, FObjectKey CurrentRoot)
    {
        if (!bPermitted || bConsumed || CurrentWorld != World || Receiver != CurrentReceiver || Root != CurrentRoot) return false;
        bConsumed = true;
        return true;
    }
    bool WasConsumed() const { return bConsumed; }
private:
    const FGuid World;
    const FObjectKey Receiver;
    const FObjectKey Root;
    bool bPermitted = false;
    bool bConsumed = false;
};
