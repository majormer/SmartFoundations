// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

class APlayerController;

namespace SFPanelInput
{
    /** Modal panels receive UI keys instead of Enhanced Input actions. Preserve remaps and text entry. */
    SMARTFOUNDATIONS_API bool IsToggleKey(APlayerController* Controller, const FKeyEvent& Event);
}
