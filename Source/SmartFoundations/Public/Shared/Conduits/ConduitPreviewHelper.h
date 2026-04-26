#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemAmount.h"

class UWorld;
class AFGHologram;
class AFGSplineHologram;
class AFGBuildable;
class USFSubsystem;

/**
 * FConduitPreviewHelper
 * 
 * Abstract base class for conduit preview helpers (belts, pipes, lifts, power lines, etc.).
 * Provides common lifecycle management, visibility control, and cost calculation.
 * 
 * Derived classes must implement:
 * - GetConnectorType() - Return connector component type name for logging
 * - GetHologramClass() - Return hologram class to spawn
 * - GetBuildClass() - Return buildable class for current tier
 * - ConfigureHologram() - Set build class, recipe, tags, etc.
 * - SetupSplineRouting() - Route spline between connectors
 * 
 * Optional overrides:
 * - ShouldEnableTick() - Enable tick for position correction (default: false)
 * - FinalizeSpawn() - Post-spawn setup like mesh generation
 * - GetCostPerMeter() - Cost multiplier per meter (default: 1.0)
 */
class SMARTFOUNDATIONS_API FConduitPreviewHelper
{
public:
	virtual ~FConduitPreviewHelper();

	// ========================================
	// Public Interface (Common)
	// ========================================

	/** Hide the preview without destroying it */
	void HidePreview();

	/** Destroy the preview actor completely */
	void DestroyPreview();

	/** Check if preview is visible */
	bool IsPreviewVisible() const;

	/** Check if preview is valid and properly positioned */
	bool IsPreviewValid() const;

	/** Get the hologram actor */
	AFGSplineHologram* GetHologram() const { return Hologram.Get(); }

	/** Get current tier */
	int32 GetTier() const { return Tier; }

	/** Get the length of the conduit spline in cm */
	float GetLength() const;

	/** Calculate cost for this preview (based on tier and length) */
	TArray<FItemAmount> GetPreviewCost() const;

protected:
	// ========================================
	// Protected Constructor (Abstract)
	// ========================================

	FConduitPreviewHelper(UWorld* InWorld, int32 InTier, AFGHologram* InParent);

	// ========================================
	// Virtual Hooks (Type-Specific Behavior)
	// ========================================

	/** Should tick be enabled for this conduit type? (Default: false) */
	virtual bool ShouldEnableTick() const { return false; }

	/** Get the connector component type name for logging */
	virtual FString GetConnectorType() const = 0;

	/** Get the hologram class to spawn */
	virtual TSubclassOf<AFGSplineHologram> GetHologramClass() const = 0;

	/** Get the buildable class for the current tier */
	virtual TSubclassOf<AFGBuildable> GetBuildClass(USFSubsystem* Subsystem) const = 0;

	/** Configure the hologram after spawn (set build class, recipe, tags, etc.) */
	virtual void ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem) = 0;

	/** Setup spline routing between connectors */
	virtual void SetupSplineRouting(AFGSplineHologram* SpawnedHologram) = 0;

	/** Finalize spawn (optional post-spawn setup like mesh generation) */
	virtual void FinalizeSpawn(AFGSplineHologram* SpawnedHologram) {}

	/** Get cost calculation multiplier per meter (default: 1.0) */
	virtual float GetCostPerMeter() const { return 1.0f; }

	// ========================================
	// Common Implementation Helpers
	// ========================================

	/** Ensure hologram is spawned and configured */
	void EnsureSpawned(const FVector& SpawnLocation);

	/** Configure tick behavior based on ShouldEnableTick() */
	void ConfigureTickBehavior(AFGSplineHologram* SpawnedHologram);

	/** Configure visibility and collision */
	void ConfigureVisibility(AFGSplineHologram* SpawnedHologram);

	/** Use queued destruction for safety */
	void QueueForDestruction(AFGSplineHologram* HologramToDestroy);

protected:
	// ========================================
	// Common Members
	// ========================================

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AFGSplineHologram> Hologram;
	TWeakObjectPtr<AFGHologram> ParentHologram;
	int32 Tier;
};
