#include "Holograms/Core/SFSmartFactoryChildHologram.h"
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
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFSmartFactoryChildHologram::CheckValidPlacement: Skipping validation for %s"), 
            *GetName());
        return; // Skip validation - always valid
    }
    
    // Normal validation
    Super::CheckValidPlacement();
}

void ASFSmartFactoryChildHologram::ConfigureActor(class AFGBuildable* inBuildable) const {
    Super::ConfigureActor(inBuildable);

    USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
    if (!Subsystem)
    {
        return;
    }

    // Apply stored recipe if available
    if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this)) {
        if (Data->StoredRecipe) {
            Subsystem->ApplyStoredProductionRecipeToBuilding(inBuildable);
            UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartFactoryChildHologram::ConfigureActor: Applied stored recipe to %s"),
                *inBuildable->GetName());
        }
    }

    // Issue #208/#209: Apply Power Shards and Somersloops from player inventory
    USFRecipeManagementService* RecipeService = Subsystem->GetRecipeManagementService();
    if (RecipeService && (RecipeService->HasStoredPotential() || RecipeService->HasStoredProductionBoost()))
    {
        // Get the player character to consume items from their inventory
        AFGCharacterPlayer* Player = nullptr;
        if (AFGPlayerController* PC = Subsystem->GetLastPlayerController())
        {
            Player = Cast<AFGCharacterPlayer>(PC->GetPawn());
        }
        
        if (Player)
        {
            bool bApplied = RecipeService->ApplyStoredPotentialToBuilding(inBuildable, Player);
            if (bApplied)
            {
                UE_LOG(LogSmartFoundations, Log, TEXT("SFSmartFactoryChildHologram::ConfigureActor: Applied Power Shards/Somersloops to %s"),
                    *inBuildable->GetName());
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("SFSmartFactoryChildHologram::ConfigureActor: No player found for shard/somersloop transfer"));
        }
    }
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
