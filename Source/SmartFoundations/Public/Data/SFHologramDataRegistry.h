#pragma once

#include "CoreMinimal.h"
#include "Data/SFHologramData.h"
#include "UObject/NoExportTypes.h"
#include "SFHologramDataRegistry.generated.h"

// Forward declaration
class AFGHologram;

// Registry for attaching data structures to vanilla holograms
UCLASS()
class SMARTFOUNDATIONS_API USFHologramDataRegistry : public UObject {
    GENERATED_BODY()
    
public:
    // Attach data structure to hologram
    static FSFHologramData* AttachData(AFGHologram* Hologram);
    
    // Retrieve data structure from hologram (const-correct overloads)
    static FSFHologramData* GetData(AFGHologram* Hologram);
    static const FSFHologramData* GetData(const AFGHologram* Hologram);
    
    // Check if hologram has data structure (const-correct overloads)
    static bool HasData(AFGHologram* Hologram);
    static bool HasData(const AFGHologram* Hologram);
    
    // Clear data structure (on hologram destruction)
    static void ClearData(AFGHologram* Hologram);
    
    // Get access to the internal registry map (for iteration)
    static const TMap<TWeakObjectPtr<AFGHologram>, FSFHologramData>& GetRegistry();
    
    // Clear the entire registry (for world transitions)
    static void ClearRegistry();
    
    // Cleanup dead weak pointers (call periodically)
    static void CleanupDeadEntries();
    
    // Get the built buildable from a hologram (returns nullptr if not built or invalid)
    static AFGBuildable* GetBuiltBuildable(AFGHologram* Hologram);
    
    // Check if a hologram was successfully built
    static bool WasBuilt(AFGHologram* Hologram);
    
private:
    // Map of weak hologram pointers to data structures (deterministic safety)
    static TMap<TWeakObjectPtr<AFGHologram>, FSFHologramData> HologramDataMap;
};
