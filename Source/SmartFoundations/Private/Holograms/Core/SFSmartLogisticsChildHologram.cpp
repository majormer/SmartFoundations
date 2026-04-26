#include "Holograms/Core/SFSmartLogisticsChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Logging/LogMacros.h"

ASFSmartLogisticsChildHologram::ASFSmartLogisticsChildHologram() {
    // Minimal constructor - most behavior handled by base class
}

void ASFSmartLogisticsChildHologram::CheckValidPlacement() {
    // Check data structure for validation control
    if (ShouldSkipValidation()) {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFSmartLogisticsChildHologram::CheckValidPlacement: Skipping validation for %s"), 
            *GetName());
        return; // Skip validation - always valid
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

void ASFSmartLogisticsChildHologram::Destroyed() {
    // Clean up data structure
    USFHologramDataRegistry::ClearData(this);
    
    Super::Destroyed();
}

bool ASFSmartLogisticsChildHologram::ShouldSkipValidation() const {
    // Check if we have data structure with validation disabled
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this)) {
        return !Data->bNeedToCheckPlacement;
    }

    return false; // Default to validation if no data structure
}
