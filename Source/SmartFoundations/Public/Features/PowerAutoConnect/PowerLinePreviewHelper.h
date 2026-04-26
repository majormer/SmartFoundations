#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGPowerConnectionComponent.h"
#include "FGRecipe.h"

class UFGPowerConnectionComponent;
class AFGHologram;
class UWorld;

// Forward declare the Smart wire hologram class
class ASFWireHologram;

/**
 * FPowerLinePreviewHelper
 * 
 * Manages a single power line preview hologram between two UFGPowerConnectionComponent endpoints.
 * Uses ASFWireHologram (Smart's custom wire hologram with proper catenary and hologram materials).
 * 
 * Lifecycle:
 * - Create with world and parent power pole hologram
 * - Call UpdatePreview() to create/update the power line
 * - Call DestroyPreview() to clean up
 * 
 * Power Line Rules:
 * - Maximum distance: 100 meters (10,000cm)
 * - Cable cost: 1 Cable per 25 meters, rounded up
 * - All pole tiers use same distance (only connection count differs)
 */
class SMARTFOUNDATIONS_API FPowerLinePreviewHelper
{
public:
	FPowerLinePreviewHelper(UWorld* InWorld, AFGHologram* ParentPole = nullptr);
	~FPowerLinePreviewHelper();

	/** Update or create a preview between two power connection points */
	bool UpdatePreview(UFGPowerConnectionComponent* StartConnection, UFGPowerConnectionComponent* EndConnection);

	/** Hide the preview (does not destroy it) */
	void HidePreview();

	/** Destroy the preview actor completely */
	void DestroyPreview();

	/** Check if preview is visible */
	bool IsPreviewVisible() const;

	/** Check if preview is valid and properly positioned */
	bool IsPreviewValid() const;

	/** Get the connection endpoints */
	UFGPowerConnectionComponent* GetStartConnection() const { return StartConnection.Get(); }
	UFGPowerConnectionComponent* GetEndConnection() const { return EndConnection.Get(); }

	/** Get the hologram actor */
	ASFWireHologram* GetHologram() const { return PowerLineHologram.Get(); }

	/** Get the length of the power line in cm */
	float GetLineLength() const;

	/** Calculate cost for this power line preview (based on length) */
	TArray<FItemAmount> GetPreviewCost() const;

	/** Calculate cable cost based on distance */
	static int32 CalculateCableCost(float DistanceInCm);

private:
	/** Spawn the power line hologram at the specified location */
	void EnsureSpawned(const FVector& SpawnLocation, UFGPowerConnectionComponent* Start, UFGPowerConnectionComponent* End);

	/** Update the power line endpoints */
	void UpdateLineEndpoints(UFGPowerConnectionComponent* Start, UFGPowerConnectionComponent* End);

	/** Use queued destruction for safety (same pattern as FConduitPreviewHelper) */
	void QueueForDestruction(AActor* HologramToDestroy);

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ASFWireHologram> PowerLineHologram;
	TWeakObjectPtr<UFGPowerConnectionComponent> StartConnection;
	TWeakObjectPtr<UFGPowerConnectionComponent> EndConnection;
	TWeakObjectPtr<AFGHologram> ParentPole;  // Parent power pole hologram for child registration
	
	// Maximum distance for power connections (100m = 10,000cm)
	static constexpr float MAX_CONNECTION_DISTANCE = 10000.0f;
};
