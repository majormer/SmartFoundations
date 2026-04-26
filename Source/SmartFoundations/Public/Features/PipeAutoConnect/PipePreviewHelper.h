#pragma once

#include "Shared/Conduits/ConduitPreviewHelperTypes.h"
#include "FGPipeConnectionComponent.h"
#include "Holograms/Logistics/SFPipelineHologram.h"

/**
 * FPipePreviewHelper
 * 
 * Manages a single pipe preview hologram between two UFGPipeConnectionComponent endpoints.
 * Similar to FBeltPreviewHelper but for pipes.
 * 
 * Lifecycle:
 * - Create with world, pipe tier, and parent junction hologram
 * - Call UpdatePreview() to create/update the pipe spline
 * - Call DestroyPreview() to clean up
 */
class SMARTFOUNDATIONS_API FPipePreviewHelper
    : public TConduitPreviewHelper<UFGPipeConnectionComponent, ASFPipelineHologram>
{
    using Super = TConduitPreviewHelper<UFGPipeConnectionComponent, ASFPipelineHologram>;

public:
	FPipePreviewHelper(UWorld* InWorld, int32 PipeTier = 1, bool bWithIndicator = true, AFGHologram* ParentJunction = nullptr);
	virtual ~FPipePreviewHelper() = default;

	/** Update pipe tier and style (forces hologram recreation) */
	void UpdatePipeTier(int32 NewTier, bool bWithIndicator);

	/** Get current pipe tier */
	int32 GetPipeTier() const { return GetTier(); }

	/** Get current pipe style */
	bool GetPipeWithIndicator() const { return bPipeWithIndicator; }

	/** Get the length of the pipe spline in cm */
	float GetPipeLength() const { return GetLength(); }

	/** Get the connection endpoints (backward compatibility) */
	UFGPipeConnectionComponent* GetStartConnection() const { return GetStartConnector(); }
	UFGPipeConnectionComponent* GetEndConnection() const { return GetEndConnector(); }

protected:
	// ========================================
	// Virtual Hook Implementations
	// ========================================

	virtual bool ShouldEnableTick() const override;

	virtual FString GetConnectorType() const override;

	virtual TSubclassOf<AFGSplineHologram> GetHologramClass() const override;

	virtual TSubclassOf<AFGBuildable> GetBuildClass(USFSubsystem* Subsystem) const override;

	virtual void ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem) override;

	virtual void SetupSplineRouting(AFGSplineHologram* SpawnedHologram) override;

	virtual FVector GetConnectorLocation(UFGPipeConnectionComponent* Connector) const override;

private:
	void UpdateSplineEndpoints(UFGPipeConnectionComponent* Start, UFGPipeConnectionComponent* End);

private:
	bool bPipeWithIndicator = true;  // true=Normal (with indicators), false=Clean (no indicators)
};
