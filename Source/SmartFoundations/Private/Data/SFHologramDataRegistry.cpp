#include "Data/SFHologramDataRegistry.h"
#include "SmartFoundations.h"
#include "FGHologram.h"
#include "Logging/LogMacros.h"

// Static member initialization with deterministic weak pointers
TMap<TWeakObjectPtr<AFGHologram>, FSFHologramData> USFHologramDataRegistry::HologramDataMap;

FSFHologramData* USFHologramDataRegistry::AttachData(AFGHologram* Hologram) {
    if (!Hologram) return nullptr;
    
    // Create weak pointer for deterministic safety
    TWeakObjectPtr<AFGHologram> WeakHologram(Hologram);
    
    // Create or get existing data structure
    FSFHologramData& Data = HologramDataMap.FindOrAdd(WeakHologram);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("AttachData: Attached data to hologram %s"), 
        *Hologram->GetName());
    
    return &Data;
}

FSFHologramData* USFHologramDataRegistry::GetData(AFGHologram* Hologram) {
    if (!Hologram) return nullptr;

    // Use weak pointer for lookup (deterministic validation)
    TWeakObjectPtr<AFGHologram> WeakHologram(Hologram);
    return HologramDataMap.Find(WeakHologram);
}

const FSFHologramData* USFHologramDataRegistry::GetData(const AFGHologram* Hologram) {
    // Delegate to non-const version - the map lookup doesn't modify the hologram
    return GetData(const_cast<AFGHologram*>(Hologram));
}

bool USFHologramDataRegistry::HasData(AFGHologram* Hologram) {
    if (!Hologram) return false;

    // Check weak pointer (deterministic safety)
    TWeakObjectPtr<AFGHologram> WeakHologram(Hologram);
    return HologramDataMap.Contains(WeakHologram);
}

bool USFHologramDataRegistry::HasData(const AFGHologram* Hologram) {
    // Delegate to non-const version - the map lookup doesn't modify the hologram
    return HasData(const_cast<AFGHologram*>(Hologram));
}

void USFHologramDataRegistry::ClearData(AFGHologram* Hologram) {
    if (!Hologram) return;
    
    // Remove using weak pointer key
    TWeakObjectPtr<AFGHologram> WeakHologram(Hologram);
    HologramDataMap.Remove(WeakHologram);
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ClearData: Removed data from hologram %s"), 
        *Hologram->GetName());
}

const TMap<TWeakObjectPtr<AFGHologram>, FSFHologramData>& USFHologramDataRegistry::GetRegistry() {
    return HologramDataMap;
}

void USFHologramDataRegistry::ClearRegistry() {
    int32 Count = HologramDataMap.Num();
    HologramDataMap.Empty();
    
    UE_LOG(LogSmartFoundations, Log, TEXT("ClearRegistry: Cleared %d hologram entries"), Count);
}

void USFHologramDataRegistry::CleanupDeadEntries() {
    int32 RemovedCount = 0;
    
    // Remove entries where weak pointer is no longer valid (deterministic cleanup)
    for (auto It = HologramDataMap.CreateIterator(); It; ++It)
    {
        if (!It->Key.IsValid())
        {
            It.RemoveCurrent();
            RemovedCount++;
        }
    }
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("CleanupDeadEntries: Removed %d dead hologram entries"), RemovedCount);
    }
}

AFGBuildable* USFHologramDataRegistry::GetBuiltBuildable(AFGHologram* Hologram) {
    if (!Hologram) return nullptr;
    
    FSFHologramData* Data = GetData(Hologram);
    if (!Data) return nullptr;
    
    // Return the created actor if it was built and is still valid
    if (Data->bWasBuilt && Data->CreatedActor && IsValid(Data->CreatedActor))
    {
        return Data->CreatedActor;
    }
    
    return nullptr;
}

bool USFHologramDataRegistry::WasBuilt(AFGHologram* Hologram) {
    if (!Hologram) return false;
    
    FSFHologramData* Data = GetData(Hologram);
    return Data && Data->bWasBuilt;
}
