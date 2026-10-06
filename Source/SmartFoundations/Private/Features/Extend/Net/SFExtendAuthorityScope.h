// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Templates/UniquePtr.h"

class USFSubsystem;
class AFGHologram;
class APawn;
class UClass;

/** Remote construction owns separate services through validation, wiring and payment.
 * Local preview objects survive every exit, including rejected requests. */
class FSFExtendAuthorityScope
{
public:
    explicit FSFExtendAuthorityScope(USFSubsystem* Subsystem, AFGHologram* Root = nullptr);
    ~FSFExtendAuthorityScope();
    FSFExtendAuthorityScope(const FSFExtendAuthorityScope&) = delete;
    FSFExtendAuthorityScope& operator=(const FSFExtendAuthorityScope&) = delete;
    static bool IsActive(const USFSubsystem* Subsystem);
    static bool ShouldIsolate(USFSubsystem* Subsystem, APawn* Instigator, UClass* BuildClass);
    static void BeginReconstruction(USFSubsystem* Subsystem, AFGHologram* Root);
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
    FSFExtendAuthorityScope* Previous = nullptr;
};
