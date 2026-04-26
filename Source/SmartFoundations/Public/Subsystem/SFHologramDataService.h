#pragma once

#include "CoreMinimal.h"
#include "Data/SFHologramData.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFHologramDataService.generated.h"

// Forward declarations
class AFGHologram;
class UFGRecipe;

// Service for hologram data management
UCLASS()
class SMARTFOUNDATIONS_API USFHologramDataService : public UObject {
    GENERATED_BODY()
    
public:
    // Get or create data structure for hologram
    static FSFHologramData* GetOrCreateData(AFGHologram* Hologram);
    
    // Set validation control flags
    static void DisableValidation(AFGHologram* Hologram);
    static void EnableValidation(AFGHologram* Hologram);
    
    // Set child hologram metadata
    static void MarkAsChild(AFGHologram* ChildHologram, AFGHologram* ParentHologram, ESFChildHologramType ChildType);
    
    // Recipe copying support
    static void StoreRecipe(AFGHologram* Hologram, TSubclassOf<UFGRecipe> Recipe);
    static TSubclassOf<UFGRecipe> GetStoredRecipe(AFGHologram* Hologram);
    
    // Cleanup on destruction
    static void OnHologramDestroyed(AFGHologram* Hologram);
};
