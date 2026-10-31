// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "Features/Extend/SFExtendService.h"

/** Synchronous native request envelope. Never retain this scope across frames. */
class FSFExtendWiringCaptureScope
{
public:
    FSFExtendWiringCaptureScope(USFExtendService* InOwner, AFGHologram* Root, FNetConstructionID NativeId)
        : Owner(InOwner), Invocation(Owner ? Owner->BeginNativeWiringCapture(Root, NativeId) : nullptr) {}
    ~FSFExtendWiringCaptureScope()
    {
        if (Owner) Owner->EndNativeWiringCapture(Invocation);
    }
    bool IsValid() const { return Invocation.IsValid(); }
    FSFExtendWiringCaptureScope(const FSFExtendWiringCaptureScope&) = delete;
    FSFExtendWiringCaptureScope& operator=(const FSFExtendWiringCaptureScope&) = delete;

private:
    USFExtendService* Owner;
    TSharedPtr<FSFExtendWiringInvocation> Invocation;
};
