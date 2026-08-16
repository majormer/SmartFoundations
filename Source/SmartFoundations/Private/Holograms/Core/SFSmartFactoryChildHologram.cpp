// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFSmartFactoryChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFRecipeManagementService.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Logging/LogMacros.h"

ASFSmartFactoryChildHologram::ASFSmartFactoryChildHologram() {
    // Minimal constructor - most behavior handled by base class
}

void ASFSmartFactoryChildHologram::CheckValidPlacement() {
    // Check data structure for validation control
    if (ShouldSkipValidation()) {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("SFSmartFactoryChildHologram::CheckValidPlacement: Skipping validation for %s"), 
            *GetName());
        return; // Skip validation - always valid
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

void ASFSmartFactoryChildHologram::Destroyed() {
    // Clean up data structure
    USFHologramDataRegistry::ClearData(this);
    
    Super::Destroyed();
}

bool ASFSmartFactoryChildHologram::ShouldSkipValidation() const {
    // Check if we have data structure with validation disabled
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this)) {
        return !Data->bNeedToCheckPlacement;
    }

    return false; // Default to validation if no data structure
}
