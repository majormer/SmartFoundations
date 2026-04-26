#include "Holograms/Core/SFSmartChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Logging/LogMacros.h"

ASFSmartChildHologram::ASFSmartChildHologram() {
    // Minimal constructor - most behavior handled by base class
}

void ASFSmartChildHologram::CheckValidPlacement() {
    // Check data structure for validation control
    if (ShouldSkipValidation()) {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFSmartChildHologram::CheckValidPlacement: Skipping validation for %s"), 
            *GetName());
        return; // Skip validation - always valid
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

AActor* ASFSmartChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) {
    UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartChildHologram::Construct: Building from hologram %s"), *GetName());
    
    // Call base construction to create the actual building
    AActor* ConstructedActor = Super::Construct(out_children, constructionID);
    
    if (ConstructedActor) {
        UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartChildHologram::Construct: Successfully constructed %s"), 
            *ConstructedActor->GetName());
        
        // Check if we have a stored recipe to apply
        TSubclassOf<UFGRecipe> StoredRecipe = USFHologramDataService::GetStoredRecipe(this);
        if (StoredRecipe) {
            UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartChildHologram::Construct: Applying stored recipe %s to building"), 
                *StoredRecipe->GetName());
            
            // Apply the stored recipe to the constructed building
            if (AFGBuildableManufacturer* ManufacturerBuilding = Cast<AFGBuildableManufacturer>(ConstructedActor))
            {
                ManufacturerBuilding->SetRecipe(StoredRecipe);
                UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartChildHologram::Construct: Successfully applied recipe to manufacturer building"));
            }
            else
            {
                UE_LOG(LogSmartFoundations, Warning, TEXT("SFSmartChildHologram::Construct: Constructed building is not a manufacturer, cannot apply recipe"));
            }
        } else {
            UE_LOG(LogSmartFoundations, Verbose, TEXT("SFSmartChildHologram::Construct: No stored recipe found"));
        }
    } else {
        UE_LOG(LogSmartFoundations, Error, TEXT("SFSmartChildHologram::Construct: Failed to construct building"));
    }
    
    return ConstructedActor;
}

void ASFSmartChildHologram::Destroyed() {
    // Clean up data structure
    USFHologramDataRegistry::ClearData(this);
    
    Super::Destroyed();
}

bool ASFSmartChildHologram::ShouldSkipValidation() const {
    // Check if we have data structure with validation disabled
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this)) {
        return !Data->bNeedToCheckPlacement;
    }

    return false; // Default to validation if no data structure
}
