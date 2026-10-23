// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

/** Presentation capabilities derived from existing family/native eligibility. */
struct FSFInterfaceTaskCapability
{
    int32 Task = 0;
    FText Label;
    bool bSupported = false;
    bool bAvailable = false;
    bool bVisible = true;
    FText Reason;
};

struct FSFInterfaceCapabilities
{
    TArray<FSFInterfaceTaskCapability> Tasks;
    int32 DefaultTask = 0;
};
