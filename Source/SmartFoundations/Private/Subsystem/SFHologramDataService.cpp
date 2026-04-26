#include "Subsystem/SFHologramDataService.h"
#include "SmartFoundations.h"
#include "FGHologram.h"
#include "Logging/LogMacros.h"

FSFHologramData* USFHologramDataService::GetOrCreateData(AFGHologram* Hologram) {
    if (!Hologram) return nullptr;
    
    // Try to get existing data
    FSFHologramData* Data = USFHologramDataRegistry::GetData(Hologram);
    
    // Create if doesn't exist
    if (!Data) {
        Data = USFHologramDataRegistry::AttachData(Hologram);
    }
    
    return Data;
}

void USFHologramDataService::DisableValidation(AFGHologram* Hologram) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->bNeedToCheckPlacement = false;
        Data->bIgnoreLocationUpdates = true;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("DisableValidation: Disabled validation for %s"), 
            *Hologram->GetName());
    }
}

void USFHologramDataService::EnableValidation(AFGHologram* Hologram) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->bNeedToCheckPlacement = true;
        Data->bIgnoreLocationUpdates = false;
    }
}

void USFHologramDataService::MarkAsChild(AFGHologram* ChildHologram, AFGHologram* ParentHologram, ESFChildHologramType ChildType) {
    if (FSFHologramData* Data = GetOrCreateData(ChildHologram)) {
        Data->bIsChildHologram = true;
        Data->ParentHologram = ParentHologram;
        Data->ChildType = ChildType;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("MarkAsChild: Marked %s as child of %s (type: %d)"), 
            *ChildHologram->GetName(), *ParentHologram->GetName(), (int32)ChildType);
    }
}

void USFHologramDataService::StoreRecipe(AFGHologram* Hologram, TSubclassOf<UFGRecipe> Recipe) {
    if (FSFHologramData* Data = GetOrCreateData(Hologram)) {
        Data->StoredRecipe = Recipe;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StoreRecipe: Stored recipe %s in hologram %s"), 
            *Recipe->GetName(), *Hologram->GetName());
    }
}

TSubclassOf<UFGRecipe> USFHologramDataService::GetStoredRecipe(AFGHologram* Hologram) {
    if (FSFHologramData* Data = USFHologramDataRegistry::GetData(Hologram)) {
        return Data->StoredRecipe;
    }
    return nullptr;
}

void USFHologramDataService::OnHologramDestroyed(AFGHologram* Hologram) {
    USFHologramDataRegistry::ClearData(Hologram);
}
