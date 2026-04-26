// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendHologramService - Hologram Spawning and Preview Management
 * 
 * Handles:
 * - Child hologram tracking and position management
 * - Preview creation and cleanup
 * - Hologram swapping (vanilla → Smart! factory hologram)
 * - Per-frame position refresh for child holograms
 * 
 * Part of EXTEND feature refactor (Dec 2025).
 * Extracted from SFExtendService for separation of concerns.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFExtendHologramService.generated.h"

class AFGHologram;
class AFGBuildable;
class AFGBuildGun;
class UFGBuildGunStateBuild;
class ASFFactoryHologram;
class USFSubsystem;
class USFExtendService;
class USFExtendTopologyService;
struct FSFCloneTopology;

/**
 * Service for managing EXTEND child holograms and previews.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendHologramService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendHologramService();

    /** Initialize with owning subsystem and extend service references */
    void Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService);

    /** Shutdown service */
    void Shutdown();

    // ==================== Child Hologram Tracking ====================

    /** Add a child hologram to tracking with its intended position/rotation */
    void TrackChildHologram(AFGHologram* Child, const FVector& IntendedPosition, const FRotator& IntendedRotation);

    /** Get all tracked child holograms */
    const TArray<AFGHologram*>& GetTrackedChildren() const { return TrackedChildren; }

    /** Get intended position for a child hologram */
    FVector* GetIntendedPosition(AFGHologram* Child);

    /** Get intended rotation for a child hologram */
    FRotator* GetIntendedRotation(AFGHologram* Child);

    /** Clear all tracked children (does NOT destroy them) */
    void ClearTracking();

    // ==================== Preview Management ====================

    /** Create belt preview holograms for all connection chains */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Hologram")
    void CreateBeltPreviews(AFGHologram* ParentHologram);

    /** Clear all belt preview holograms (destroys them) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Hologram")
    void ClearBeltPreviews();

    /** Refresh child hologram positions (called every frame during EXTEND) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Hologram")
    void RefreshChildPositions();

    // ==================== Hologram Swapping ====================

    /** 
     * Swap a vanilla factory hologram for our custom Smart! hologram.
     * This allows us to intercept SetHologramLocationAndRotation calls.
     * @return The new custom hologram, or nullptr if swap failed
     */
    ASFFactoryHologram* SwapToSmartFactoryHologram(AFGHologram* VanillaHologram);

    /** Restore state after hologram swap (cleanup tracking) */
    void RestoreOriginalHologram();

    /** Check if we've swapped the hologram */
    bool HasSwappedHologram() const { return bHasSwappedHologram; }

    /** Get the swapped hologram */
    ASFFactoryHologram* GetSwappedHologram() const;

    // ==================== State Access ====================

    /** Set the current parent hologram (for child management) */
    void SetCurrentParentHologram(AFGHologram* Parent) { CurrentParentHologram = Parent; }

    /** Get the current parent hologram */
    AFGHologram* GetCurrentParentHologram() const { return CurrentParentHologram.Get(); }

    // ==================== JSON Spawning Support ====================

    /** Store spawned holograms from JSON for post-build wiring */
    void StoreJsonSpawnedHolograms(const TMap<FString, AFGHologram*>& SpawnedHolograms);

    /** Get JSON spawned holograms map */
    const TMap<FString, AFGHologram*>& GetJsonSpawnedHolograms() const { return JsonSpawnedHolograms; }

    /** Store clone topology for post-build wiring */
    void StoreCloneTopology(TSharedPtr<FSFCloneTopology> CloneTopology);

    /** Get stored clone topology */
    TSharedPtr<FSFCloneTopology> GetStoredCloneTopology() const { return StoredCloneTopology; }

private:
    /** Get the player's build gun */
    AFGBuildGun* GetPlayerBuildGun() const;

    /** Get the build gun's build state */
    UFGBuildGunStateBuild* GetBuildGunBuildState(AFGBuildGun* BuildGun) const;

    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Parent extend service (for topology access) */
    UPROPERTY()
    USFExtendService* ExtendService = nullptr;

    /** Current parent hologram for child management */
    UPROPERTY()
    TWeakObjectPtr<AFGHologram> CurrentParentHologram;

    /** Tracked child holograms */
    UPROPERTY()
    TArray<AFGHologram*> TrackedChildren;

    /** Intended world positions for child holograms */
    TMap<AFGHologram*, FVector> ChildIntendedPositions;

    /** Intended world rotations for child holograms */
    TMap<AFGHologram*, FRotator> ChildIntendedRotations;

    /** Our custom hologram that replaced the vanilla one */
    UPROPERTY()
    TWeakObjectPtr<ASFFactoryHologram> SwappedHologram;

    /** Whether we've swapped the vanilla hologram for our custom one */
    bool bHasSwappedHologram = false;

    /** JSON spawned holograms for post-build wiring */
    TMap<FString, AFGHologram*> JsonSpawnedHolograms;

    /** Stored clone topology for post-build wiring */
    TSharedPtr<FSFCloneTopology> StoredCloneTopology;
};
