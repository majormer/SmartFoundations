// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Data/SFHologramData.h"
#include "Data/SFHologramDataRegistry.h"
#include "Hologram/FGHologram.h"   // [#497] EHologramMaterialState for GetRawPlacementMaterialState
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

    /**
     * [#497] O(1) raw read of a hologram's own mPlacementMaterialState via cached reflection.
     * Vanilla AFGHologram::GetHologramMaterialState() AGGREGATES: it walks
     * GetHologramsToShareMaterialStateWith — the CHILD ARRAY — on every call. Any per-child or
     * per-pair path that queries the PARENT's state through it goes quadratic (8,238-child
     * stackable grid: 13-second frames from the belt/pipe material guards alone). Use this
     * whenever the parent's own last-set state is what's wanted.
     */
    static EHologramMaterialState GetRawPlacementMaterialState(const AFGHologram* Hologram);

    /**
     * [#497] Disable a hologram's clearance-detector box. Every hologram spawns one at BeginPlay
     * (SetupClearanceDetector) and it generates pairwise overlap EVENTS against every other
     * detector in range — at thousands of Smart preview children the pair database goes
     * quadratic, and every spawn/collision-toggle pays linear list scans over it (capture 13:
     * belt-create overlap storms + the root's per-frame hidden-toggle re-registration were the
     * frame). Smart children never consume detector overlaps; kill the component's collision.
     */
    static void DisableClearanceDetector(AFGHologram* Hologram);
    
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
